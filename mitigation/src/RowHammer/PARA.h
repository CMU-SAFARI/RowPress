#pragma once

#include "RefreshBasedDefense.h"
#include "Config.h"
#include "Controller.h"

#include <vector>
#include <random>

namespace Ramulator
{
  template <class T>
  class PARA : public RefreshBasedDefense<T>
  {
  public:
    PARA(const YAML::Node &config, Controller<T>* ctrl);
    ~PARA() = default;
    /**
     * @brief: Schedule preventive refresh to victims of the aggressor row at addr_vec
     */
    void schedule_preventive_refresh(const std::vector<int> addr_vec);
    void tick();
    void update(typename T::Command cmd, const std::vector<int> &addr_vec, uint64_t open_for_nclocks);
    
    std::string to_string()
    {
      return fmt::format("Refresh-based RowHammer Defense\n"
                         "  └  "
                         "PARA\n"
                         "  └  "
                         "Probability threshold: {}\n", probability_threshold);
    }

  private:
    bool trigger_on_rdwra = false;
    int no_rows_per_bank;
    // probability threshold for PARA
    float probability_threshold;
    // address of the latest row that was opened
    std::vector<int> last_addr_vec;
    // a random number generator
    std::mt19937 generator;
    // a uniform distribution between 0 and 1
    std::uniform_real_distribution<float> distribution;
    // true if we need to schedule a preventive refresh
    bool pending_preventive_refresh = false;
    bool debug = false;

    // take rowpress into account
    bool rowpress = false;
    int rowpress_increment_nticks = 0;
    int nRAS = 0;
  };
};

#include "PARA.tpp"