#pragma once

#include <iostream>
#include <vector>
#include <deque>
#include <map>
#include <functional>
#include <algorithm>
#include <cassert>
#include <type_traits>

#include "Statistics.h"
#if defined(RAMULATOR_POWER)
#include "Power.h"
#endif

namespace Ramulator
{
  template <class T>
  class Power;

  template <typename T>
  class DRAM
  {
    public:
      /**
       * @brief    Construct a new DRAM object.
       * 
       * @param    spec           A pointer to a DRAM spec object.
       * @param    level          The level of the DRAM object in the hierarchy.
       */
      DRAM(T* spec, typename T::Level level);
      ~DRAM();

      ScalarStat active_cycles;
      ScalarStat refresh_cycles;
      ScalarStat busy_cycles;
      ScalarStat active_refresh_overlap_cycles;
      ScalarStat serving_requests;
      ScalarStat average_serving_requests;
      void reg_stats(const std::string& identifier);

      /// Specification (e.g., DDR3)
      T* spec;

      long cur_clk = 0;

      /// The DRAM hierarchy (e.g., Channel->Rank->Bank->Row->Column)
      typename T::Level level;
      int id;
      long size;
      DRAM* parent;
      std::vector<DRAM*> children;

      #if defined(RAMULATOR_POWER)
      Power<T> power;
      #endif
      
      /// Insert a node as one of my child nodes
      void insert(DRAM<T>* child);

      /// The state (e.g., Opened, Closed) of the current node
      typename T::State state;

      /**
       * \brief The state of rows (if the current node is a bank).
       * 
       * \details There are too many rows for them to be instantiated individually. 
       * Instead, their bank (or an equivalent entity) tracks their state for them
       */
      std::map<int, typename T::State> row_state;

      /// Decode a command into its "prerequisite" command (if any is needed) using the prereq table.
      typename T::Command decode(typename T::Command cmd, const int* addr);

      /**
       * \brief Recursively check whether a command is ready to be scheduled
       * 
       * \details Recursively check myself and my children to see whether a command is ready.
       * A command is ready if, at all levels, the next earliest schedulable cycle (if not -1)
       * is no smaller than the current clock cycle.
       */
      bool check(typename T::Command cmd, const int* addr, long clk);

      /**
       * \brief Recursively check whether a command is a row hit.
       * 
       * \details Recursively check myself and my children to see whether a command is a row hit.
       * Nodes that do not apply to row-hit checks are skipped.
       */
      bool check_row_hit(typename T::Command cmd, const int* addr);

      /**
       * \brief Recursively check whether a node is open for the command.
       * 
       * \details Recursively check myself and my children to see whether a node is open for the command (subject to the scope of the command).
       * Nodes that do not apply to node-open checks are skipped.
       */
      bool check_node_open(typename T::Command cmd, const int* addr);

      /**
       * \brief (Decrepated?) Return the earliest clock when the command is ready to be scheduled
       * 
       * \details .
       */
      long get_next(typename T::Command cmd, const int* addr);

      /**
       * \brief Recursively update the state and timing info of the hierarchy, signifying that a command has been issued.
       */
      void update(typename T::Command cmd, const int* addr, long clk);

      // TIANSHI: current serving requests count
      int cur_serving_requests = 0;
      long begin_of_serving = -1;
      long end_of_serving = -1;
      long begin_of_cur_reqcnt = -1;
      long begin_of_refreshing = -1;
      long end_of_refreshing = -1;
      std::vector<std::pair<long, long>> refresh_intervals;

      /// Update the number of requests it serves currently
      void update_serving_requests(const int* addr, int delta, long clk);

      /// Finalizing statistics
      void finish(long dram_cycles, uint64_t clk);

    private:
      DRAM(){}

      /// The earliest clock cycles in the future when commands could be ready
      long next[int(T::Command::MAX)]; 

      /** 
       * \brief The most recent history of when commands were issued.
       * \details For each command, a deque is used to maintain the history of (potential) multiple issued commands. 
       */ 
      std::deque<long> prev[int(T::Command::MAX)]; 

      /// A lookup table of functions that returns the prerequisite of each command at each level.
      std::function<typename T::Command(DRAM<T>*, typename T::Command cmd, int)>* prereq;

      /// A lookup table of functions that determines whether a command can hit an opened row.
      std::function<bool(DRAM<T>*, typename T::Command cmd, int)>* rowhit;

      /// A lookup table of functions that determines whether a node is open for the command.
      std::function<bool(DRAM<T>*, typename T::Command cmd, int)>* rowopen;

      /// A lookup table of functions that models the state changes performed by the commands. 
      std::function<void(DRAM<T>*, int)>* lambda;

      #if defined(RAMULATOR_POWER)
      /// A lookup table of functions that models the energy consumption caused by the commands. 
      std::function<void(DRAM<T>*)>* power_lambda;
      #endif

      /**
       * \brief The timing table that stores all the timing constraints for all levels in the hierarchy and all commands.
       * 
       * \details Each element in the table is a vector of TimingEntry that stores the timing constraints for this command at this level.
       * A TimingEntry element in the vector essentially regulates the mimimum number of cycles between the command and the command stored in this TimingEntry at this level.
       * For example, the timing constraint nRCD should be modeled by a TimingEntry {Command::RCD, 1, nRCD, false} stored in the vector at timing[Level::Bank][Command::ACT].
       */
      std::vector<typename T::TimingEntry>* timing;

      /// Helper function used in update()
      void update_state(typename T::Command cmd, const int* addr);
      /// Helper function used in update()
      void update_timing(typename T::Command cmd, const int* addr, long clk);

      #if defined(RAMULATOR_POWER)
      /// Helper function used in update()
      void update_power(typename T::Command cmd, const int* addr, uint64_t clk);
      #endif
  };

};

#include "DRAM.tpp"