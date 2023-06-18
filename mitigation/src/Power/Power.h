#pragma once

#include <cstdint>

#include "DRAM.h"
#include "PowerCounters.h"
#include "PowerComponents.h"

namespace Ramulator
{
  template <class T>
  class DRAM;

  template <class T>
  class Power
  {
    public:
      PowerCounters   counters;
      PowerComponents components;

    public:
      typename T::Level level = T::Level::MAX;
      DRAM<T>* dram_node      = nullptr;

    public:
      Power();
      Power(const DRAM<T>* node, typename T::Level level);

    public:
      void handle_ACT_rank(bool is_rank_idle, int64_t clk);
      void handle_ACT_bank(int64_t clk);

      void handle_PRE_rank(bool is_rank_idle, int64_t clk);
      void handle_PRE_bank(int64_t clk);

      void handle_RD_rank(int64_t clk);
      void handle_WR_rank(int64_t clk);

      void handle_REF_rank(int64_t clk);
      void handle_REF_bank(int64_t clk);

      void epilogue_rank(bool is_rank_idle, int64_t clk);
      void epilogue_bank(int64_t clk);

    private:
      void update_act_idle_cycles(int64_t clk);

    public:
      void calculate_energy();
      void print_energy();
  };
};

#include "Power.tpp"