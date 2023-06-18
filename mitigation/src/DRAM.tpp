#pragma once
#include "DRAM.h"

namespace Ramulator
{
  template <typename T>
  void DRAM<T>::finish(long dram_cycles, uint64_t clk) 
  {
    cur_clk = clk;

    // finalize busy cycles
    busy_cycles = active_cycles.value() + refresh_cycles.value() - active_refresh_overlap_cycles.value();

    // finalize average serving requests
    average_serving_requests = serving_requests.value() / dram_cycles;

    #if defined(RAMULATOR_POWER)
    // epilogue for the power model
    if (level == T::Level::Rank)
    {
      bool is_rank_idle = true;
      for (auto bg : children)
      {
        for (auto bank : bg->children) 
        {
          if (bank->state == T::State::Opened)
            is_rank_idle = false;
        }
      }

      power.epilogue_rank(is_rank_idle, cur_clk);
      power.calculate_energy();
      power.print_energy();
    }
    // else if (level == T::Level::Bank)
    // {
    //   power.epilogue_bank(cur_clk);
    //   power.calculate_energy();
    //   power.print_energy();
    // }
    #endif

    if (!children.size())
      return;

    for (auto child : children)
      child->finish(dram_cycles, clk);
  }

  template <typename T>
  DRAM<T>::DRAM(T* spec, typename T::Level level):
  spec(spec), level(level), id(0), parent(nullptr)
  {
    // Populate the tables from the spec
    state = spec->start[(int)level];
    prereq = spec->prereq[int(level)];
    rowhit = spec->rowhit[int(level)];
    rowopen = spec->rowopen[int(level)];
    lambda = spec->lambda[int(level)];
    timing = spec->timing[int(level)];

    #if defined(RAMULATOR_POWER)
    power_lambda = spec->power_lambda[int(level)];
    power.dram_node = this;
    power.level = level;
    #endif

    // Initialize next (earliest future time when a command can be scheduled). 
    std::fill_n(next, int(T::Command::MAX), -1);

    // Initialize the history of commands.  
    for (int cmd = 0; cmd < int(T::Command::MAX); cmd++) 
    {
      // For each command, determine the max number of histories to track across all timing constraints.
      int dist = 0;
      for (auto& t : timing[cmd])
        dist = std::max(dist, t.dist);

      if (dist)
        prev[cmd].resize(dist, -1);
    }

    // try to recursively construct my children
    int child_level = int(level) + 1;
    if (child_level == int(T::Level::Row))
      // stop recursion: rows are not instantiated as nodes
      return; 

    int child_max = spec->org_entry.count[child_level];
    if (!child_max)
      // stop recursion: the number of children is unspecified
      return; 

    // recursively construct my children
    for (int i = 0; i < child_max; i++) 
    {
      DRAM<T>* child = new DRAM<T>(spec, typename T::Level(child_level));
      child->parent = this;
      child->id = i;
      children.push_back(child);
    }
  }

  template <typename T>
  DRAM<T>::~DRAM()
  {
    for (auto child: children)
      delete child;
  }

  template <typename T>
  void DRAM<T>::insert(DRAM<T>* child)
  {
    child->parent = this;
    child->id = children.size();
    children.push_back(child);
  }

  template <typename T>
  typename T::Command DRAM<T>::decode(typename T::Command cmd, const int* addr)
  {
    int child_id = addr[int(level)+1];
    if (prereq[int(cmd)]) 
    {
      typename T::Command prereq_cmd = prereq[int(cmd)](this, cmd, child_id);
      if (prereq_cmd != T::Command::MAX)
        // stop recursion: there is a prerequisite at this level
        return prereq_cmd; 
    }

    if (child_id < 0 || !children.size())
      // stop recursion: there were no prequisites at any level
      return cmd; 

    // recursively decode at my child
    return children[child_id]->decode(cmd, addr);
  }

  template <typename T>
  bool DRAM<T>::check(typename T::Command cmd, const int* addr, long clk)
  {
    if (next[int(cmd)] != -1 && clk < next[int(cmd)])
      // stop recursion: the check failed at this level
      return false; 

    int child_id = addr[int(level)+1];
    if (child_id < 0 || level == spec->scope[int(cmd)] || !children.size())
      // stop recursion: the check passed at all levels
      return true; 

    // recursively check my child
    return children[child_id]->check(cmd, addr, clk);
  }

  template <typename T>
  bool DRAM<T>::check_row_hit(typename T::Command cmd, const int* addr)
  {
    int child_id = addr[int(level)+1];
    if (rowhit[int(cmd)]) 
      // stop recursion: there is a row hit at this level
      return rowhit[int(cmd)](this, cmd, child_id);  

    if (child_id < 0 || !children.size())
      // stop recursion: there were no row hits at any level
      return false; 

    // recursively check for row hits at my child
    return children[child_id]->check_row_hit(cmd, addr);
  }

  template <typename T>
  bool DRAM<T>::check_node_open(typename T::Command cmd, const int* addr)
  {
    int child_id = addr[int(level)+1];
    if (rowopen[int(cmd)])
      // stop recursion: there is a row open at this level
      return rowopen[int(cmd)](this, cmd, child_id); 

    if (child_id < 0 || !children.size())
      // stop recursion: there were no row hits at any level
      return false; 

    // recursively check for row hits at my child
    return children[child_id]->check_node_open(cmd, addr);
  }

  template <typename T>
  long DRAM<T>::get_next(typename T::Command cmd, const int* addr)
  {
    long next_clk = std::max(cur_clk, next[int(cmd)]);

    // Iterate through all my children to get the earliest clock when the command is ready to be scheduled.
    auto node = this;
    for (int l = int(level); l < int(spec->scope[int(cmd)]) && node->children.size() && addr[l + 1] >= 0; l++)
    {
      node = node->children[addr[l + 1]];
      next_clk = std::max(next_clk, node->next[int(cmd)]);
    }
    return next_clk;
  }

  template <typename T>
  void DRAM<T>::update(typename T::Command cmd, const int* addr, long clk)
  {
    cur_clk = clk;

    #if defined(RAMULATOR_POWER)
    update_power(cmd, addr, clk);
    #endif

    update_state(cmd, addr);
    update_timing(cmd, addr, clk);
  }

  // Update (State)
  template <typename T>
  void DRAM<T>::update_state(typename T::Command cmd, const int* addr)
  {
    int child_id = addr[int(level)+1];
    if (lambda[int(cmd)])
      // update this level
      lambda[int(cmd)](this, child_id); 

    if (level == spec->scope[int(cmd)] || !children.size())
      // stop recursion: updated all levels
      return; 

    // recursively update my child
    children[child_id]->update_state(cmd, addr);
  }


  // Update (Timing)
  template <typename T>
  void DRAM<T>::update_timing(typename T::Command cmd, const int* addr, long clk)
  {
    // I am a sibling of the target node.
    // Update timing info based on sibling timing constraints.
    if (id != addr[int(level)]) 
    {
      for (auto& t : timing[int(cmd)]) 
      {
        if (!t.sibling)
          // not an applicable timing parameter
          continue; 

          assert (t.dist == 1);

          // update earliest schedulable time of every command
          long future = clk + t.val;
          next[int(t.cmd)] = std::max(next[int(t.cmd)], future); 
        }
      // stop recursion: only target nodes should be recursed
      return; 
    }

    // I am a target node

    // Update history
    if (prev[int(cmd)].size()) 
    {
      // Remove oldest history entry
      prev[int(cmd)].pop_back();
      // Push the latest time when the command is issued.
      prev[int(cmd)].push_front(clk); 
    }

    for (auto& t : timing[int(cmd)]) 
    {
      if (t.sibling)
        // sibling timing constraint not applicable here
        continue; 

      // Get the oldest history
      long past = prev[int(cmd)][t.dist-1];
      if (past < 0)
        // not enough history
        continue; 

      // update earliest schedulable time of every command
      long future = past + t.val;
      next[int(t.cmd)] = std::max(next[int(t.cmd)], future);

      // TIANSHI: for refresh statistics
      if (spec->is_refreshing(cmd) && spec->is_opening(t.cmd)) 
      {
        assert(past == clk);
        begin_of_refreshing = clk;
        end_of_refreshing = std::max(end_of_refreshing, next[int(t.cmd)]);
        refresh_cycles += end_of_refreshing - clk;
        if (cur_serving_requests > 0) 
          refresh_intervals.push_back(std::make_pair(begin_of_refreshing, end_of_refreshing));
      }
    }

    if (!children.size())
      // stop recursion: updated all levels
      return; 

    // recursively update all of my children
    for (auto child : children)
      child->update_timing(cmd, addr, clk);

  }


  #if defined(RAMULATOR_POWER)
  // Update (Power)
  template <typename T>
  void DRAM<T>::update_power(typename T::Command cmd, const int* addr, uint64_t clk)
  {
    cur_clk = clk;
    
    if (power_lambda[int(cmd)])
      // update this level
      power_lambda[int(cmd)](this); 

    if (level == spec->scope[int(cmd)] || !children.size())
      // stop recursion: updated all levels
      return; 

    // recursively update my child
    int child_id = addr[int(level)+1];
    children[child_id]->update_power(cmd, addr, clk);
  }
  #endif

  template <typename T>
  void DRAM<T>::update_serving_requests(const int* addr, int delta, long clk) 
  {
    assert(id == addr[int(level)]);
    assert(delta == 1 || delta == -1);
    // update total serving requests
    if (begin_of_cur_reqcnt != -1 && cur_serving_requests > 0) 
    {
      serving_requests += (clk - begin_of_cur_reqcnt) * cur_serving_requests;
      active_cycles += clk - begin_of_cur_reqcnt;
    }
    
    // update begin of current request number
    begin_of_cur_reqcnt = clk;
    cur_serving_requests += delta;
    assert(cur_serving_requests >= 0);

    if (delta == 1 && cur_serving_requests == 1) 
    {
      // transform from inactive to active
      begin_of_serving = clk;
      if (end_of_refreshing > begin_of_serving)
        active_refresh_overlap_cycles += end_of_refreshing - begin_of_serving;
    } 
    else if (cur_serving_requests == 0) 
    {
      // transform from active to inactive
      assert(begin_of_serving != -1);
      assert(delta == -1);
      active_cycles += clk - begin_of_cur_reqcnt;
      end_of_serving = clk;

      for (const auto& ref: refresh_intervals) 
        active_refresh_overlap_cycles += std::min(end_of_serving, ref.second) - ref.first;
      refresh_intervals.clear();
    }

    // We only count the level bank or the level higher than bank
    int child_id = addr[int(level) + 1];
    if (child_id < 0 || !children.size() || (int(level) > int(T::Level::Bank)) ) 
      return;

    // Recursively update my children
    children[child_id]->update_serving_requests(addr, delta, clk);
  }

  template <typename T>
  void DRAM<T>::reg_stats(const std::string& identifier) 
  {
    active_cycles
      .name("active_cycles" + identifier + "_" + std::to_string(id))
      .desc("Total active cycles for level " + identifier + "_" + std::to_string(id))
      .precision(0)
      ;
    refresh_cycles
      .name("refresh_cycles" + identifier + "_" + std::to_string(id))
      .desc("(All-bank refresh only, only valid for rank level) The sum of cycles that is under refresh per memory cycle for level " + identifier + "_" + std::to_string(id))
      .precision(0)
      .flags(Stats::nozero)
      ;
    busy_cycles
      .name("busy_cycles" + identifier + "_" + std::to_string(id))
      .desc("(All-bank refresh only. busy cycles only include refresh time in rank level) The sum of cycles that the DRAM part is active or under refresh for level " + identifier + "_" + std::to_string(id))
      .precision(0)
      ;
    active_refresh_overlap_cycles
      .name("active_refresh_overlap_cycles" + identifier + "_" + std::to_string(id))
      .desc("(All-bank refresh only, only valid for rank level) The sum of cycles that are both active and under refresh per memory cycle for level " + identifier + "_" + std::to_string(id))
      .precision(0)
      .flags(Stats::nozero)
      ;
    serving_requests
      .name("serving_requests" + identifier + "_" + std::to_string(id))
      .desc("The sum of read and write requests that are served in this DRAM element per memory cycle for level " + identifier + "_" + std::to_string(id))
      .precision(0)
      ;
    average_serving_requests
      .name("average_serving_requests" + identifier + "_" + std::to_string(id))
      .desc("The average of read and write requests that are served in this DRAM element per memory cycle for level " + identifier + "_" + std::to_string(id))
      .precision(6)
      ;

    if (!children.size())
      return;

    // recursively register children statistics
    for (auto child : children)
      child->reg_stats(identifier + "_" + std::to_string(id));
  }
};