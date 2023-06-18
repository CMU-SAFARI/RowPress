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
  /**
   * Slow Start RowHammer Defense (SSRD)
  */
  class SSRD : public RefreshBasedDefense<T>
  {
  public:
    SSRD(const YAML::Node &config, Controller<T>* ctrl);
    ~SSRD() = default;
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
                        "SSRD\n");
    }

  private:
    struct RPT_ENTRY{
      float probability;
      uint64_t last_activation_timestamp;
    };

    uint64_t clk;
    std::vector<int> row_address_index_bits;
    int timeout_cycles;
    float probability_multiplier;
    float probability_divider;
    float probability_lower_limit;
    bool debug;
    bool debug_verbose;
    int no_rows_per_bank;
    int no_banks; // b per bg
    int no_bank_groups; // bg per rank
    int no_ranks;
    bool pending_preventive_refresh = false;
    std::vector<int> last_addr_vec;
    // per bank refresh probability table
    std::vector<std::vector<RPT_ENTRY>> refresh_probability_table;
    int table_size;

    // a random number generator
    std::mt19937 generator;
    // a uniform distribution between 0 and 1
    std::uniform_real_distribution<float> distribution;

  };
}

#include "SSRD.tpp"