#include "PARA.h"

namespace Ramulator
{
  
  template <class T>
  PARA<T>::PARA(const YAML::Node &config, Controller<T>* ctrl) : RefreshBasedDefense<T>(config, ctrl)
  {
    probability_threshold = config["probability_threshold"].as<float>();
    // check if probability threshold is valid
    if (probability_threshold < 0 || probability_threshold > 1)
      throw std::runtime_error("Invalid probability threshold for PARA defense mechanism!");

    rowpress = config["rowpress"].as<bool>();
    rowpress_increment_nticks = config["rowpress_increment_nticks"].as<int>(0);
    trigger_on_rdwra = config["trigger_on_rdwra"].as<bool>(false);
    nRAS = ctrl->channel->spec->speed_entry[int(T::TimingCons::nRAS)];
    // initialize random number generator
    generator = std::mt19937(1337);
    // initialize uniform distribution
    distribution = std::uniform_real_distribution<float>(0.0, 1.0);

    no_rows_per_bank = ctrl->channel->spec->org_entry.count[int(T::Level::Row)];

    debug = config["debug"].as<bool>();
  }

  template <class T>
  void PARA<T>::schedule_preventive_refresh(const std::vector<int> addr_vec)
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

    // if we could not schedule both requests, throw an error
    if (!sched_m1 || !sched_m2)
      throw std::runtime_error("Could not schedule preventive refreshes!");
  }

  template <class T>
  void PARA<T>::tick()
  {
    if (pending_preventive_refresh)
    {
      schedule_preventive_refresh(last_addr_vec);
      pending_preventive_refresh = false;
    }
  }

  template <class T>
  void PARA<T>::update(typename T::Command cmd, const std::vector<int> &addr_vec, uint64_t open_for_nclocks)
  {
    if (!trigger_on_rdwra && (cmd != T::Command::PRE))
      return;
    else if (trigger_on_rdwra && ((cmd != T::Command::RDA) && (cmd != T::Command::WRA)))
      return;

    int threshold_multiplier = rowpress ? ((open_for_nclocks - 1) + rowpress_increment_nticks - nRAS)/rowpress_increment_nticks + 1 : 1;

    // increase the probability threshold by the factor
    // of time that row remains open over row cycle time
    float _probability_threshold = probability_threshold * threshold_multiplier;

    if (_probability_threshold < probability_threshold)                                            
      _probability_threshold = probability_threshold;

    if (_probability_threshold > 1)
      _probability_threshold = 1;

    if (debug)
    {
      if (rowpress)
      {
        std::cout << "Rowpress adjusted probability threshold from " << probability_threshold << " to " << _probability_threshold 
          << "\nbecause the row was open for " << open_for_nclocks << " cycles" << std::endl;
      }
    }

    // schedule a preventive refresh to last_addr_vec
    // if RNG produces a number less than probability_threshold
    if (distribution(generator) < _probability_threshold)
    {
      if (pending_preventive_refresh)
      {
        throw std::runtime_error("Pending preventive refresh could not be scheduled\
                                  before another one needs to be scheduled");
      }
      pending_preventive_refresh = true;
    }

    // save the address of the latest row that was opened
    last_addr_vec = addr_vec;
  }
};