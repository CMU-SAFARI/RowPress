#pragma once

namespace Ramulator
{
  class PowerCounters
  {
    public:
      int64_t num_act  = 0;                      ///< The number of ACT commands
      int64_t num_pre  = 0;                      ///< The number of PRE commands
      int64_t num_rd   = 0;                      ///< The number of RD commands
      int64_t num_wr   = 0;                      ///< The number of RD commands
      int64_t num_ref  = 0;                      ///< The number of REF commands

      int64_t act_cycles       = 0;                  
      int64_t act_idle_cycles  = 0;             

      int64_t pre_cycles       = 0;                  
      int64_t pre_idle_cycles  = 0;             


      int64_t first_act_cycle      = 0;
      int64_t latest_act_cmd_cycle = 0;

      int64_t last_pre_cycle       = 0;
      int64_t latest_pre_cmd_cycle = 0;

      int64_t latest_read_cmd_cycle  = 0;
      int64_t latest_write_cmd_cycle = 0;
  };
};