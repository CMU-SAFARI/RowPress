#pragma once

namespace Ramulator
{
  class PowerComponents
  {
    public:
      double ACT_stdby_energy = 0.0;
      double ACT_idle_energy  = 0.0;
      double ACT_cmd_energy   = 0.0;

      double PRE_stdby_energy = 0.0;
      double PRE_idle_energy  = 0.0;
      double PRE_cmd_energy   = 0.0;

      double total_idle_energy  = 0.0;

      double RD_cmd_energy    = 0.0;
      double WR_cmd_energy    = 0.0;

      double REF_cmd_energy   = 0.0;
  };
};