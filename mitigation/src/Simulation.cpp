#include "Simulation.h"

namespace Ramulator
{
  void warmup_processor(MemoryBase* mem, Processor* proc)
  {
    uint cpu_tick = proc->tick_unit;
    uint mem_tick = mem->tick_unit;

    uint64_t warmup_insts = proc->warmup_insts;
    bool is_warming_up = (warmup_insts != 0);

    for(uint64_t i = 0; is_warming_up; i++)
    {
      proc->tick();
      Stats::curTick++;
      if (i % cpu_tick == (cpu_tick - 1))
        for (uint j = 0; j < mem_tick; j++)
          mem->tick();

      is_warming_up = false;
      for(uint c = 0; c < proc->cores.size(); c++)
      {
        if(proc->cores[c]->get_insts() < warmup_insts)
          is_warming_up = true;
      }

      if (is_warming_up && proc->has_reached_limit()) 
      {
        printf("WARNING: The end of the input trace file was reached during warmup. "
                "Consider changing warmup_insts in the config file. \n");
        break;
      }
    }

    printf("Warmup complete! Resetting stats...\n");
    Stats::reset_stats();
    proc->reset_stats();
    assert(proc->get_insts() == 0);
  }

  void start_sim_processor(MemoryBase* mem, Processor* proc)
  {
    printf("Starting the simulation...\n");

    uint cpu_tick = proc->tick_unit;
    uint mem_tick = mem->tick_unit;
    uint tick_mult = cpu_tick * mem_tick;

    for (uint64_t i = 0; ; i++) 
    {
      if (((i % tick_mult) % mem_tick) == 0) 
      { // When the CPU is ticked cpu_tick times,
        // the memory controller should be ticked mem_tick times
          proc->tick();
          Stats::curTick++; // processor clock, global, for Statistics

          if(proc->time_limit != 0)
          {
            if (proc->finished())
            {
              proc->finalize();
              break;
            }            
          }
          else if (proc->expected_limit_insts != 0) 
          {
            if (proc->has_reached_limit())
            {
              proc->finalize();
              break;
            }
          }
          else 
          {
            if (proc->early_exit) 
            {
              if (proc->finished())
              {
                proc->finalize();
                break;
              }
            }
            else 
              if (proc->finished() && (mem->pending_requests() == 0))
              {
                proc->finalize();
                break;
              }
          }
      }

      if (((i % tick_mult) % cpu_tick) == 0)
          mem->tick();
    }

    mem->finish();
    Stats::statlist.printall();
  }

  void start_sim_memory(MemoryBase* mem, std::string _trace)
  {
    Trace trace(_trace);

    bool stall = false, end = false;
    int reads = 0, writes = 0, clks = 0;
    long addr = 0;
    Request::Type type = Request::Type::READ;

    std::map<int, int> latencies;
    auto read_complete = [&latencies](Request& r){latencies[r.depart - r.arrive]++;};

    Request req(addr, type, read_complete);

    while (!end || mem->pending_requests())
    {
      if (!end && !stall)
        end = !trace.get_dramtrace_request(addr, type);

      if (!end)
      {
        req.addr = addr;
        req.type = type;
        stall = !mem->send(req);
        if (!stall)
        {
          if (type == Request::Type::READ) reads++;
          else if (type == Request::Type::WRITE) writes++;
        }
      }
      else
        // make sure that all write requests in the writeq are draind
        mem->set_writeq_watermark(0.0f, -1.0f); 

      mem->tick();
      clks ++;
      Stats::curTick++; // memory clock, global, for Statistics
    }

    mem->finish();
    Stats::statlist.printall();
  }

  void reload_options(const YAML::Node& config, MemoryBase* mem, Processor* proc)
  {
    if (config["post_warmup_settings"])
    {
      mem->reload_options(config["post_warmup_settings"]);
    }
  }

  void run_cputraces(const YAML::Node& config)
  {
    MemoryBase* memory = make_memory(config);
    Processor* proc = make_processor(config, memory);

    warmup_processor(memory, proc);
    
    reload_options(config, memory, proc);

    start_sim_processor(memory, proc);
  }  

  void run_memorytraces(const YAML::Node& config)
  {
    MemoryBase* memory = make_memory(config);

    const YAML::Node& memory_config = config["memory"];
    if (!memory_config["trace"])
      throw std::runtime_error("Trace configuration (trace:) not found!");
    std::string trace = memory_config["trace"].as<std::string>();

    start_sim_memory(memory, trace);
  }  
}