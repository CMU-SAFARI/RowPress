#include "Power.h"
#include "Standards.h"

namespace Ramulator
{
  template <>
  void Power<DDR4>::calculate_energy()
  {
    #define TS(timing)  (dram_node->spec->speed_entry  [int(DDR4::TimingCons::timing)])
    #define VE(voltage) (dram_node->spec->voltage_entry[int(DDR4::Voltage::voltage)])
    #define CE(current) (dram_node->spec->current_entry[int(DDR4::Current::current)])

    PowerCounters&   ctrs = counters;
    PowerComponents& c = components;

    double tCK_ns = (double) TS(tCK_ps) / 1000.0;

    c.ACT_stdby_energy = (VE(VDD) * CE(IDD3N) + VE(VPP) * CE(IPP3N)) * ctrs.act_cycles * tCK_ns / 1E3;
    c.PRE_stdby_energy = (VE(VDD) * CE(IDD2N) + VE(VPP) * CE(IPP2N)) * ctrs.pre_cycles * tCK_ns / 1E3;

    c.ACT_idle_energy = (VE(VDD) * CE(IDD3N) + VE(VPP) * CE(IPP3N)) * ctrs.act_idle_cycles * tCK_ns / 1E3;
    c.PRE_idle_energy = (VE(VDD) * CE(IDD2N) + VE(VPP) * CE(IPP2N)) * ctrs.pre_idle_cycles * tCK_ns / 1E3;

    c.total_idle_energy = c.ACT_idle_energy + c.PRE_idle_energy;

    c.ACT_cmd_energy  = (VE(VDD) * (CE(IDD0) - CE(IDD3N)) + VE(VPP) * (CE(IPP0) - CE(IPP3N))) * ctrs.num_act * TS(nRAS) * tCK_ns / 1E3;
    c.PRE_cmd_energy  = (VE(VDD) * (CE(IDD0) - CE(IDD2N)) + VE(VPP) * (CE(IPP0) - CE(IPP2N))) * ctrs.num_pre * TS(nRP)  * tCK_ns / 1E3;
    c.RD_cmd_energy   = (VE(VDD) * CE(IDD4R) + VE(VPP) * CE(IPP4R)) * ctrs.num_rd * TS(nBL) * tCK_ns / 1E3;
    c.WR_cmd_energy   = (VE(VDD) * CE(IDD4W) + VE(VPP) * CE(IPP4W)) * ctrs.num_wr * TS(nBL) * tCK_ns / 1E3;
    c.REF_cmd_energy  = (VE(VDD) * (CE(IDD5B) - CE(IDD3N)) + VE(VPP) * (CE(IPP5B) - CE(IPP3N))) * ctrs.num_ref * TS(nRFC) * tCK_ns / 1E3;

    #undef TS
    #undef VE
    #undef CE
  };

  template <>
  void Power<DDR3>::calculate_energy()
  {
    #define TS(timing)  (dram_node->spec->speed_entry  [int(DDR3::TimingCons::timing)])
    #define VE(voltage) (dram_node->spec->voltage_entry[int(DDR3::Voltage::voltage)])
    #define CE(current) (dram_node->spec->current_entry[int(DDR3::Current::current)])

    PowerCounters&   ctrs = counters;
    PowerComponents& c = components;

    double tCK_ns = (double) TS(tCK_ps) / 1000.0;

    c.ACT_stdby_energy = VE(VDD) * CE(IDD3N) * ctrs.act_cycles * tCK_ns / 1E3;
    c.PRE_stdby_energy = VE(VDD) * CE(IDD2N) * ctrs.pre_cycles * tCK_ns / 1E3;

    c.ACT_idle_energy = VE(VDD) * CE(IDD3N) * ctrs.act_idle_cycles * tCK_ns / 1E3;
    c.PRE_idle_energy = VE(VDD) * CE(IDD2N) * ctrs.pre_idle_cycles * tCK_ns / 1E3;

    c.total_idle_energy = c.ACT_idle_energy + c.PRE_idle_energy;

    c.ACT_cmd_energy  = VE(VDD) * (CE(IDD0) - CE(IDD3N)) * ctrs.num_act * TS(nRAS) * tCK_ns / 1E3;
    c.PRE_cmd_energy  = VE(VDD) * (CE(IDD0) - CE(IDD2N)) * ctrs.num_pre * TS(nRP)  * tCK_ns / 1E3;
    c.RD_cmd_energy   = VE(VDD) * CE(IDD4R) * ctrs.num_rd * TS(nBL) * tCK_ns / 1E3;
    c.WR_cmd_energy   = VE(VDD) * CE(IDD4W) * ctrs.num_wr * TS(nBL) * tCK_ns / 1E3;
    c.REF_cmd_energy  = VE(VDD) * (CE(IDD5B) - CE(IDD3N)) * ctrs.num_ref * TS(nRFC) * tCK_ns / 1E3;

    #undef TS
    #undef VE
    #undef CE
  };
}
