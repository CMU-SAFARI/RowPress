/*
 * Refresh.h
 *
 * This is a refresh scheduler. A list of refresh policies implemented:
 *
 * 1. All-bank refresh
 * 2. Per-bank refresh (only DSARP memory module has been completed to work with REFpb).
 *     The other modules (LPDDRx) have not been updated to pass a knob to turn on/off REFpb.
 * 3. A re-implementation of DSARP from the refresh mechanisms proposed in Chang et al.,
 * "Improving DRAM Performance by Parallelizing Refreshes with Accesses", HPCA 2014.
 *
 *  Created on: Mar 17, 2015
 *      Author: kevincha
 */

#pragma once

#include <stddef.h>
#include <cassert>
#include <iostream>
#include <vector>

#include "Traits.h"
#include "Request.h"
#include "Controller.h"
#include "DRAM.h"
#include "Standards.h"

namespace Ramulator 
{
  template <class T>
  class Controller;

  template <class T>
  class RefreshBase
  {
    public:
      Controller<T>* ctrl;
      long clk = 0;                 ///< The current tick of the refresh scheduler.

      int refresh_interval = 0;     ///< Interval between two REF commands (e.g., nREFI).
      long refreshed = 0;           ///< The last time refresh has been issued.

      uint64_t ref_count = 0;       ///< The number of REF commands issued.


    public:
      RefreshBase(const YAML::Node& config, Controller<T>* ctrl): ctrl(ctrl) {};
      virtual ~RefreshBase() = default;
      virtual std::string to_string() = 0;

    public:
      virtual void tick() = 0;
      virtual void update_refresh_scheduler(typename T::Command cmd, const std::vector<int>& addr_vec) {};
  };


  template <class T>
  class GenericRefresh : public RefreshBase<T>
  {
    public:
      GenericRefresh(const YAML::Node& config, Controller<T>* ctrl);
      virtual ~GenericRefresh() = default;
      virtual std::string to_string()      
      {
        return fmt::format("Generic (All-bank)\n"
                           "  └  "
                           "refresh interval = {}\n", this->refresh_interval); 
      }

    public:
      virtual void tick() override;

    protected:
      void refresh_rank(int rank);
  };
}

#include "Refresh.tpp"
