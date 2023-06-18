#include "Graphene.h"

namespace Ramulator
{
  template <class T>
  Graphene<T>::Graphene(const YAML::Node &config, Controller<T>* ctrl) : RefreshBasedDefense<T>(config, ctrl)
  {
    no_table_entries = config["no_table_entries"].as<int>();
    activation_threshold = config["activation_threshold"].as<int>();
    reset_period = config["reset_period"].as<int>();
    debug = config["debug"].as<bool>();
    debug_verbose = config["debug_verbose"].as<bool>();
    trigger_on_rdwra = config["trigger_on_rdwra"].as<bool>(false);
    rowpress = config["rowpress"].as<bool>();
    nRAS = ctrl->channel->spec->speed_entry[int(T::TimingCons::nRAS)];
    rowpress_increment_nticks = config["rowpress_increment_nticks"].as<int>(0);
    reset_period_clk = (int) (reset_period / (((float) ctrl->channel->spec->speed_entry[int(T::TimingCons::tCK_ps)])/1000));

    // Get organization configuration
    no_rows_per_bank = ctrl->channel->spec->org_entry.count[int(T::Level::Row)];
    no_bank_groups = ctrl->channel->spec->org_entry.count[int(T::Level::BankGroup)];
    no_banks = ctrl->channel->spec->org_entry.count[int(T::Level::Bank)];
    no_ranks = ctrl->channel->spec->org_entry.count[int(T::Level::Rank)];

    if (debug)
    {
      std::cout << "Graphene: no_table_entries: " << no_table_entries << std::endl;
      std::cout << "Graphene: activation_threshold: " << activation_threshold << std::endl;
      std::cout << "Graphene: reset_period: " << reset_period << std::endl;
      std::cout << "Graphene: reset_period_clk: " << reset_period_clk << std::endl;
      std::cout << "  └  tCK: " << ((float) ctrl->channel->spec->speed_entry[int(T::TimingCons::tCK_ps)]) << std::endl;
      std::cout << "Graphene: no_rows_per_bank: " << no_rows_per_bank << std::endl;
      std::cout << "Graphene: no_bank_groups: " << no_bank_groups << std::endl;
      std::cout << "Graphene: no_banks: " << no_banks << std::endl;
      std::cout << "Graphene: no_ranks: " << no_ranks << std::endl;
    }

    // Initialize activation count table
    // each table has no_table_entries entries
    for (int i = 0; i < no_banks * no_bank_groups * no_ranks; i++)
    {
      std::unordered_map<int, int> table;
      for (int j = -65536; j < -65536 + no_table_entries; j++)
        table.insert(std::make_pair(j, 0));
      activation_count_table.push_back(table);
    }

    // Initialize spillover counter
    spillover_counter = std::vector<int>(no_banks * no_bank_groups * no_ranks, 0);
  }
  
  template <class T>
  void Graphene<T>::schedule_preventive_refresh(const std::vector<int> addr_vec)
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

    // if we could not schedule both requests, warn the user
    if (!sched_m1 || !sched_m2)
    {
      std::cout << "Graphene: Could not schedule preventive refreshes for row " << addr_vec[int(T::Level::Row)] << "!" << std::endl;
      std::cout << "  └  " << "m1: " << m1_addr_vec[int(T::Level::Row)] << std::endl;
      std::cout << "  └  " << "m2: " << m2_addr_vec[int(T::Level::Row)] << std::endl;
    }
  }

  template <class T>
  void Graphene<T>::tick()
  {
    if (pending_preventive_refresh)
    {
      schedule_preventive_refresh(last_addr_vec);
      pending_preventive_refresh = false;
    }

    // reset activation count table every reset_period
    // by setting every element to 0
    // and reset spillover counter
    if (clk % reset_period_clk == 0)
    {
      for (int i = 0; i < no_banks * no_bank_groups * no_ranks; i++)
      {
        for (auto it = activation_count_table[i].begin(); it != activation_count_table[i].end(); it++)
          it->second = 0;
        spillover_counter[i] = 0;
      }
    }

    clk++;
  }

  template <class T>
  void Graphene<T>::update(typename T::Command cmd, const std::vector<int> &addr_vec, uint64_t open_for_nclocks)
  {
    if (!trigger_on_rdwra && (cmd != T::Command::PRE))
      return;
    else if (trigger_on_rdwra && ((cmd != T::Command::RDA) && (cmd != T::Command::WRA)))
      return;
      
    int bank_group_id = addr_vec[int(T::Level::BankGroup)];
    int bank_id = addr_vec[int(T::Level::Bank)];
    int rank_id = addr_vec[int(T::Level::Rank)];
    int row_id = addr_vec[int(T::Level::Row)];

    int index = rank_id * no_banks * no_bank_groups + bank_group_id * no_banks + bank_id;

    if (debug_verbose)
    {
      std::cout << "Graphene: ACT on row " << row_id << std::endl;
      std::cout << "  └  " << "rank: " << rank_id << std::endl;
      std::cout << "  └  " << "bank_group: " << bank_group_id << std::endl;
      std::cout << "  └  " << "bank: " << bank_id << std::endl;
      std::cout << "  └  " << "index: " << index << std::endl;
    }

    // check if the row is already in the table
    if (activation_count_table[index].find(row_id) == activation_count_table[index].end())
    {
      // if row is not in the table, find an entry 
      // with a count equal to that of the spillover counter
      bool found = false;
      int to_remove = -1;
      int spillover_value = -1;

      for (auto it = activation_count_table[index].begin(); it != activation_count_table[index].end(); it++)
      {
        if (debug_verbose)
          std::cout << "  └  " << "checking row " << it->first << " with count " << it->second << std::endl;

        if (it->second == spillover_counter[index])
        {
          // if we find an entry, record it
          spillover_value = it->second;
          to_remove = it->first;
          found = true;
          break;
        }
      }
      if (found)
      {
        // for debug
        if (debug_verbose)
        {
          // print the row that is being removed
          std::cout << "Removing row " << to_remove << " from table " << index << std::endl;
          // print the row that is being added
          std::cout << "Adding row " << row_id << " to table " << index << std::endl;
          std::cout << "  └  " << "spillover counter: " << spillover_counter[index] << std::endl;
        }
        // remove to_remove from the table
        activation_count_table[index].erase(to_remove);
        // add row_id to the table
        activation_count_table[index][row_id] = spillover_value;
      }
      // if we did not find such an entry, increment spillover counter by one
      else
      {
        int increment = rowpress ? ((open_for_nclocks - nRAS + rowpress_increment_nticks)/rowpress_increment_nticks) + 1 
                                  : 1;
        spillover_counter[index] += increment;
      }
    }
    else
    {
      // if row in table, increment its activation count
      activation_count_table[index][row_id] += rowpress ? ((open_for_nclocks - nRAS + rowpress_increment_nticks)/rowpress_increment_nticks) + 1 
                                                        : 1;
      
      if (debug_verbose)
      {
        std::cout << "Row " << row_id << " in table[" << index << "]" << std::endl;
        std::cout << "  └  " << "threshold: " << activation_threshold << std::endl;
        std::cout << "  └  " << "count: " << activation_count_table[index][row_id] << std::endl;
      }

      // check if the count exceeds the threshold
      if (activation_count_table[index][row_id] >= activation_threshold)
      {
        if (debug)
          std::cout << "Row " << row_id << " in table " << index << " has exceeded the threshold!" << std::endl;
        // if yes, schedule preventive refreshes
        pending_preventive_refresh = true;
        last_addr_vec = addr_vec;
        activation_count_table[index][row_id] = spillover_counter[index];
      }
    }


  }

}