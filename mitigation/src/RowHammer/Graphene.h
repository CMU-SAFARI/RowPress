#pragma once

#include "RefreshBasedDefense.h"
#include "Config.h"
#include "Controller.h"

#include <vector>
#include <unordered_map>
#include <random>

namespace Ramulator
{
  template <class T>
  class Graphene : public RefreshBasedDefense<T>
  {
  public:
    Graphene(const YAML::Node &config, Controller<T>* ctrl);
    ~Graphene() = default;
    /**
     * @brief: Schedule preventive refresh to victims of the aggressor row at addr_vec
     * TODO: Think about moving this to the parent class
     */
    void schedule_preventive_refresh(const std::vector<int> addr_vec);
    void tick();
    void update(typename T::Command cmd, const std::vector<int> &addr_vec, uint64_t open_for_nclocks);
    
    std::string to_string()
    {
    return fmt::format("Refresh-based RowHammer Defense\n"
                        "  └  "
                        "Graphene\n");
    }

  private:
    int clk;
    int no_table_entries;
    int activation_threshold;
    int reset_period; // in nanoseconds
    int reset_period_clk; // in clock cycles
    bool debug = false;
    bool debug_verbose = false;
    bool trigger_on_rdwra = false;
    int no_rows_per_bank;
    int no_banks; // b per bg
    int no_bank_groups; // bg per rank
    int no_ranks;
    bool pending_preventive_refresh = false;
    std::vector<int> last_addr_vec;
    // per bank activation count table
    // indexed using rank id, bank id
    // e.g., if rank 0, bank 4, index is 4
    // if rank 1, bank 5, index is 16 (assuming 16 banks/rank) + 5
    std::vector<std::unordered_map<int, int>> activation_count_table;
    // spillover counter per bank
    std::vector<int> spillover_counter;

    // take rowpress into account
    bool rowpress = false;
    int rowpress_increment_nticks = 0;
    int nRAS = 0;
  };
}

#include "Graphene.tpp"