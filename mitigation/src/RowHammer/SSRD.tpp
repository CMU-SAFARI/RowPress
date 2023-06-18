#include "SSRD.h"

namespace Ramulator
{
  template <class T>
  SSRD<T>::SSRD(const YAML::Node &config, Controller<T>* ctrl) : RefreshBasedDefense<T>(config, ctrl)
  {
    row_address_index_bits = config["row_address_index_bits"].as<std::vector<int>>();
    table_size = 1 << row_address_index_bits.size();
    timeout_cycles = config["timeout_cycles"].as<int>();
    probability_multiplier = config["probability_multiplier"].as<float>();
    probability_divider = config["probability_divider"].as<float>();
    probability_lower_limit = config["probability_lower_limit"].as<float>();
    debug = config["debug"].as<bool>();
    debug_verbose = config["debug_verbose"].as<bool>();

    // Get organization configuration
    no_rows_per_bank = ctrl->channel->spec->org_entry.count[int(T::Level::Row)];
    no_bank_groups = ctrl->channel->spec->org_entry.count[int(T::Level::BankGroup)];
    no_banks = ctrl->channel->spec->org_entry.count[int(T::Level::Bank)];
    no_ranks = ctrl->channel->spec->org_entry.count[int(T::Level::Rank)];

    if (debug)
    {
      std::cout << "SSRD configuration:" << std::endl;
      std::cout << "  └  " << "row_address_index_bits: ";
      for (auto bit : row_address_index_bits)
        std::cout << bit << " ";
      std::cout << std::endl;
      std::cout << "  └  " << "timeout_cycles: " << timeout_cycles << std::endl;
      std::cout << "  └  " << "probability_multiplier: " << probability_multiplier << std::endl;
      std::cout << "  └  " << "probability_divider: " << probability_divider << std::endl;
      std::cout << "  └  " << "no_rows_per_bank: " << no_rows_per_bank << std::endl;
      std::cout << "  └  " << "no_bank_groups: " << no_bank_groups << std::endl;
      std::cout << "  └  " << "no_banks: " << no_banks << std::endl;
      std::cout << "  └  " << "no_ranks: " << no_ranks << std::endl;
    }

    // assert if any of the row_address_index_bits
    // is larger than log2 of no_rows_per_bank
    for (auto bit : row_address_index_bits)
      if (bit >= (int) log2(no_rows_per_bank))
        throw std::runtime_error("Not enough bits in the row address!");

    // Initialize the refresh probability table
    refresh_probability_table.resize(no_ranks * no_bank_groups * no_banks);
    for (int i = 0; i < no_ranks * no_bank_groups * no_banks; i++)
    {
      refresh_probability_table[i].resize(table_size);
      for (int j = 0; j < table_size; j++)
        refresh_probability_table[i][j] = {1, 0ULL};
    }

    // initialize random number generator
    generator = std::mt19937(1337);
    // initialize uniform distribution
    distribution = std::uniform_real_distribution<float>(0.0, 1.0);
  }
  
  template <class T>
  void SSRD<T>::schedule_preventive_refresh(const std::vector<int> addr_vec)
  {
    // create two new preventive refreshes targeting addr_vec
    std::vector<int> m1_addr_vec = addr_vec;
    std::vector<int> m2_addr_vec = addr_vec;
    m1_addr_vec[int(T::Level::Row)] = (m1_addr_vec[int(T::Level::Row)] + 1) % no_rows_per_bank;
    m2_addr_vec[int(T::Level::Row)] = (m2_addr_vec[int(T::Level::Row)] - 1) % no_rows_per_bank;

    if (debug)
    {
      std::cout << "Scheduling preventive refreshes for row " << addr_vec[int(T::Level::Row)] << std::endl;
      std::cout << "  └  " << "m1: " << m1_addr_vec[int(T::Level::Row)] << std::endl;
      std::cout << "  └  " << "m2: " << m2_addr_vec[int(T::Level::Row)] << std::endl;
    }

    Request m1(m1_addr_vec, Request::Type::PREVENTIVE_REFRESH, nullptr);
    Request m2(m2_addr_vec, Request::Type::PREVENTIVE_REFRESH, nullptr);
    
    // schedule the requests
    bool sched_m1 = this->ctrl->enqueue(m1);
    bool sched_m2 = this->ctrl->enqueue(m2);

    // if we could not schedule any of the requests, warn the user
    if (!sched_m1 || !sched_m2)
    {
      std::cout << "SSRD: Could not schedule preventive refreshes for row " << addr_vec[int(T::Level::Row)] << "!" << std::endl;
      std::cout << "  └  " << "m1: " << m1_addr_vec[int(T::Level::Row)] << std::endl;
      std::cout << "  └  " << "m2: " << m2_addr_vec[int(T::Level::Row)] << std::endl;
    }
  }

  template <class T>
  void SSRD<T>::tick()
  {
    if (pending_preventive_refresh)
    {
      schedule_preventive_refresh(last_addr_vec);
      pending_preventive_refresh = false;
    }

    // update the refresh probability table
    // TODO: This does not have to be done every tick
    // We can offload this to update(), it would be done
    // only once for only a single entry in one of the
    // tables (the one that is being updated)
    for (int i = 0; i < no_banks * no_bank_groups * no_ranks; i++)
    {
      for (int j = 0; j < table_size; j++)
      {
        if (clk > refresh_probability_table[i][j].last_activation_timestamp + timeout_cycles)
        {
          float new_probability = refresh_probability_table[i][j].probability * probability_divider;
          if (new_probability > 1.0f)
            new_probability = 1.0f;
          refresh_probability_table[i][j].probability = new_probability;
        }
      }
    }

    clk++;
  }

  template <class T>
  void SSRD<T>::update(typename T::Command cmd, const std::vector<int> &addr_vec, uint64_t open_for_nclocks)
  {
    if (cmd != T::Command::PRE)
      return;
    
    int bank_group_id = addr_vec[int(T::Level::BankGroup)];
    int bank_id = addr_vec[int(T::Level::Bank)];
    int rank_id = addr_vec[int(T::Level::Rank)];
    int row_id = addr_vec[int(T::Level::Row)];

    // which refresh probability table are we accessing
    int primary_index = rank_id * no_banks * no_bank_groups + bank_group_id * no_banks + bank_id;
    // which entry in the probability table are we accessing
    int secondary_index = 0;
    // extract the row address index bits from the row address
    // and concatenate them
    for (auto bit : row_address_index_bits)
      secondary_index = (secondary_index << 1) | ((row_id & (1 << bit)) >> bit);    

    // update the last activation timestamp
    refresh_probability_table[primary_index][secondary_index].last_activation_timestamp = clk;

    // check if a refresh needs to be scheduled
    float probability_threshold = refresh_probability_table[primary_index][secondary_index].probability;

    if (debug_verbose)
    {
      std::cout << "SSRD Bank" << primary_index<< " row activation: " << row_id << std::endl;
      std::cout << "  └  " << "secondary_index (from row address): " << secondary_index << std::endl;
      std::cout << "  └  " << "probability value: " << probability_threshold << std::endl;
      std::cout << "  └  " << "last activation timestamp: " << 
        refresh_probability_table[primary_index][secondary_index].last_activation_timestamp << std::endl;
      std::cout << "  └  " << "current cycle: " << clk << std::endl;
    }

    // NOTE: When the probability threshold decreases, we increase
    // the chances that victim rows get refreshed.
    // This is in contrast to implementation of PARA.
    if (distribution(generator) > probability_threshold)
    {
      if (pending_preventive_refresh)
      {
        throw std::runtime_error("Pending preventive refresh could not be scheduled\
                                  before another one needs to be scheduled");
      }
      pending_preventive_refresh = true;
      last_addr_vec = addr_vec;
    }

    // update the refresh probability table
    float new_probability = refresh_probability_table[primary_index][secondary_index].probability * probability_multiplier;
    if (new_probability < probability_lower_limit)
      new_probability = probability_lower_limit;
    refresh_probability_table[primary_index][secondary_index].probability = new_probability;
  }

}