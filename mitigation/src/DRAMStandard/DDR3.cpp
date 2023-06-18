#include "DDR3.h"

#include <vector>
#include <functional>
#include <cassert>

using namespace Ramulator;

#define TS(timing) (speed_entry[int(TimingCons::timing)])

DDR3::DDR3(const YAML::Node& config): config(config)
{
  if (!config["spec"])
    throw std::runtime_error("DRAM Spec configuration (spec:) not found!");

  const YAML::Node& spec_config = config["spec"];

  if (spec_config["suffix"])
    suffix = spec_config["suffix"].as<std::string>();

  if (!spec_config["org"])
    throw std::runtime_error("DRAM organization configuration (org:) not found!");
  init_org(spec_config["org"]);

  if (!spec_config["speed"])
    throw std::runtime_error("DRAM speed configuration (speed:) not found!");
  init_speed(spec_config["speed"]);

  // Fill all the function & timing tables
  init_lambda();
  init_prereq();
  init_rowhit();
  init_rowopen();
  init_timing_table();

  #if defined(RAMULATOR_POWER)
  if (!spec_config["power"])
    throw std::runtime_error("DRAM power configuration (power:) not found!");
  init_power_parameters(spec_config["power"]);
  init_power_lambda();
  #endif
}

void DDR3::init_org(const YAML::Node& org_config)
{
  // Load organization preset if provided
  if (org_config["preset"])
  {
    std::string preset_string = org_config["preset"].as<std::string>();
    if (org_presets_map.count(preset_string) > 0)
      org_entry = org_presets[int(org_presets_map.at(preset_string))];
    else
    {
      std::cerr << "Organization preset " << preset_string;
      std::cerr << " does not exist!" << std::endl;
      exit(-1);
    }
  }

  // Override the preset with any provided settings
  for (int i = 0; i < int(Level::MAX); i++)
    if (org_config[level_str[i]])
      org_entry.count[i] = org_config[level_str[i]].as<int>();
      
  if (org_config["width"])
    org_entry.dq = org_config["width"].as<int>();

  // Sanity check: is the calculated chip density the same as the provided one?
  uint64_t density_calc = uint64_t(org_entry.count[int(Level::Bank)]) *
                          uint64_t(org_entry.count[int(Level::Row)]) *
                          uint64_t(org_entry.count[int(Level::Column)]) *
                          uint64_t(org_entry.dq);
  density_calc >>= 20;

  if (org_config["density"])
    org_entry.density = org_config["density"].as<uint64_t>();

  if (org_entry.density != density_calc)
  {
    std::cerr << "Calculated chip density (";
    std::cerr << density_calc << " Mb) ";
    std::cerr << "does not equal provided density (";
    std::cerr << org_entry.density << " Mb)!";
    std::cerr << std::endl;
    exit(-1);
  }
}

void DDR3::init_speed(const YAML::Node& speed_config)
{
  std::fill_n(speed_entry, int(TimingCons::MAX), -1);

  // Load organization preset if provided
  if (speed_config["preset"])
  {
    std::string preset_string = speed_config["preset"].as<std::string>();
    if (speed_presets_map.count(preset_string) > 0)
      std::copy(std::begin(speed_presets[int(speed_presets_map.at(preset_string))]), 
                std::end(speed_presets[int(speed_presets_map.at(preset_string))]),
                speed_entry);
      // speed_entry = speed_presets[int(speed_presets_map.at(preset_string))];
    else
    {
      std::cerr << "Speed preset " << preset_string;
      std::cerr << " does not exist!" << std::endl;
      exit(-1);
    }
  }

  // Check for rate (in MT/s), and if provided, calculate and set tCK (in picosecond)
  if (speed_config["rate"])
  {
    assert(!speed_config["preset"] && "You cannot change the transfer rate when using a preset!");
    speed_entry[int(TimingCons::rate)] = speed_config["rate"].as<double>();
  }
  assert(speed_entry[int(TimingCons::rate)] != -1 && "Transfer rate (in MT/s) must be provided!");

  int tCK_ps = 1E6 / (speed_entry[int(TimingCons::rate)] / 2);
  speed_entry[int(TimingCons::tCK_ps)] = tCK_ps;

  // Load the organization specific timings
  int dq_id = get_dq_index(org_entry.dq);
  int rate_id = get_rate_index(speed_entry[int(TimingCons::rate)]);
  if (rate_id != -1 && dq_id != -1)
  {
    speed_entry[int(TimingCons::nRRD)] = RRD_TABLE[dq_id][rate_id];
    speed_entry[int(TimingCons::nFAW)] = FAW_TABLE [dq_id][rate_id];
  }
  int density_id = get_density_index(org_entry.density);
  if (density_id != -1)
    // tXS = tRFC1 + 10ns
    speed_entry[int(TimingCons::nXS)] = get_nCK_JEDEC_int(RFC_TABLE[density_id] + 10, tCK_ps);

  speed_entry[int(TimingCons::nRTRS)] = get_nCK_JEDEC_int(tRTRS, tCK_ps);


  // Override the preset with any provided settings
  for (int i = 1; i < int(TimingCons::MAX); i++)
  {
    std::string timing_cons_key = timingcons_str[i];

    if (speed_config[timing_cons_key])
      speed_entry[i] = speed_config[timing_cons_key].as<int>();
    else if (speed_config[timing_cons_key.replace(0, 1, "t")])
  // Check for timings specified in ns (keys starts with "t")
      speed_entry[i] = get_nCK_JEDEC_int(speed_config[timing_cons_key.replace(0, 1, "t")].as<float>(), tCK_ps);
  }

  speed_entry[int(TimingCons::nRFC)] = get_nCK_JEDEC_int(RFC_TABLE[density_id], tCK_ps);
  speed_entry[int(TimingCons::nREFI)] = get_nCK_JEDEC_int(REFI_BASE_TABLE[density_id], tCK_ps);

  // Check for any undefined timings (except for refresh timings, which are determined by the memory controller)
  for (int i = 1; i < int(TimingCons::MAX); i++)
  {
    if (speed_entry[i] == -1)
    {
      std::cerr << "ERROR! Timing constraint " << timingcons_str[i] << " not provided!" << std::endl;
      exit(-1);
    }
  }

  read_latency = speed_entry[int(TimingCons::nCL)] + speed_entry[int(TimingCons::nBL)];
}


void DDR3::init_prereq()
{
  // RD
  prereq[int(Level::Rank)][int(Command::RD)] = 
  [] (DRAM<DDR3>* node, Command cmd, int id) 
  {
    switch (int(node->state)) 
    {
      case int(State::PowerUp):       return Command::MAX;
      case int(State::ActPowerDown):  return Command::PDX;
      case int(State::PrePowerDown):  return Command::PDX;
      case int(State::SelfRefresh):   return Command::SRX;
      default: assert(false && "[PREQ] Invalid rank state!"); return Command::MAX;
    }
  };

  prereq[int(Level::Bank)][int(Command::RD)] = 
  [] (DRAM<DDR3>* node, Command cmd, int id) 
  {
    switch (int(node->state)) 
    {
      case int(State::ClosedPending): return Command::ACT;
      case int(State::Closed): return Command::ACT;
      case int(State::Opened):
      {
        if (node->row_state.find(id) != node->row_state.end())
          return cmd;
        else 
          return Command::PRE;
      }    
      default: assert(false && "[PREQ] Invalid bank state!"); return Command::MAX;
    }
  };

  // WR
  prereq[int(Level::Rank)][int(Command::WR)] = prereq[int(Level::Rank)][int(Command::RD)];
  prereq[int(Level::Bank)][int(Command::WR)] = prereq[int(Level::Bank)][int(Command::RD)];

  // REF
  prereq[int(Level::Rank)][int(Command::REF)] = 
  [] (DRAM<DDR3>* node, Command cmd, int id) 
  {
    for (auto bank : node->children)
      if (bank->state == State::Closed)
        continue;
      else 
        return Command::PREab;
    return Command::REF;
  };

  // PD
  prereq[int(Level::Rank)][int(Command::PDE)] = 
  [] (DRAM<DDR3>* node, Command cmd, int id) 
  {
    switch (int(node->state)) 
    {
      case int(State::PowerUp):       return Command::PDE;
      case int(State::ActPowerDown):  return Command::PDE;
      case int(State::PrePowerDown):  return Command::PDE;
      case int(State::SelfRefresh):   return Command::SRX;
      default: assert(false && "[PREQ] Invalid rank state!"); return Command::MAX;
    }
  };

  // SR
  prereq[int(Level::Rank)][int(Command::SRE)] = 
  [] (DRAM<DDR3>* node, Command cmd, int id) 
  {
    switch (int(node->state)) 
    {
      case int(State::PowerUp):       return Command::SRE;
      case int(State::ActPowerDown):  return Command::PDX;
      case int(State::PrePowerDown):  return Command::PDX;
      case int(State::SelfRefresh):   return Command::SRE;
      default: assert(false && "[PREQ] Invalid rank state!"); return Command::MAX;
    }
  };
}

// SAUGATA: added row hit check functions to see if the desired location is currently open
void DDR3::init_rowhit()
{
  // RD
  rowhit[int(Level::Bank)][int(Command::RD)] = 
  [] (DRAM<DDR3>* node, Command cmd, int id) 
  {
    switch (int(node->state)) 
    {
      case int(State::Closed): return false;
      case int(State::Opened):
        if (node->row_state.find(id) != node->row_state.end())
          return true;
        else
          return false;
      default: assert(false && "[ROWHIT] Invalid bank state!");  return false;
    }
  };

  // WR
  rowhit[int(Level::Bank)][int(Command::WR)] = rowhit[int(Level::Bank)][int(Command::RD)];
}

void DDR3::init_rowopen()
{
    // RD
    rowopen[int(Level::Bank)][int(Command::RD)] = 
    [] (DRAM<DDR3>* node, Command cmd, int id) 
    {
      switch (int(node->state)) 
      {
        case int(State::Closed): return false;
        case int(State::Opened): return true;
        default: assert(false && "[ROWOPEN] Invalid bank state!"); return false;
      }
    };

    // WR
    rowopen[int(Level::Bank)][int(Command::WR)] = rowopen[int(Level::Bank)][int(Command::RD)];
}

void DDR3::init_lambda()
{
  /************************************************
   *                 Bank Behavior
   ***********************************************/

  lambda[int(Level::Bank)][int(Command::ACT)] = 
  [] (DRAM<DDR3>* node, int id) 
  {
    node->state = State::Opened;
    node->row_state[id] = State::Opened;
  };

  lambda[int(Level::Bank)][int(Command::PRE)] = 
  [] (DRAM<DDR3>* node, int id) 
  {
    node->state = State::Closed;
    node->row_state.clear();
  };

  // Auto-precharge
  lambda[int(Level::Bank)][int(Command::RDA)] = [] (DRAM<DDR3>* node, int id)
  {
    node->state = State::ClosedPending;
  };
  lambda[int(Level::Bank)][int(Command::WRA)] = lambda[int(Level::Bank)][int(Command::RDA)];


  /************************************************
   *                 Rank Behavior
   ***********************************************/

  // Precharge all
  lambda[int(Level::Rank)][int(Command::PREab)] = 
  [] (DRAM<DDR3>* node, int id) 
  {
    for (auto bank : node->children)
    {
      bank->state = State::Closed;
      bank->row_state.clear();
    }
  };

  lambda[int(Level::Rank)][int(Command::PDE)] = 
  [] (DRAM<DDR3>* node, int id) 
  {
    for (auto bank : node->children)
    {
      if (bank->state == State::Closed)
        continue;
      node->state = State::ActPowerDown;
      return;
    }
    node->state = State::PrePowerDown;
  };
  
  lambda[int(Level::Rank)][int(Command::PDX)] = [] (DRAM<DDR3>* node, int id) { node->state = State::PowerUp; };
  lambda[int(Level::Rank)][int(Command::SRE)] = [] (DRAM<DDR3>* node, int id) { node->state = State::SelfRefresh; };
  lambda[int(Level::Rank)][int(Command::SRX)] = [] (DRAM<DDR3>* node, int id) { node->state = State::PowerUp; };
}


#if defined(RAMULATOR_POWER)
void DDR3::init_power_parameters(const YAML::Node& power_config)
{
  std::fill_n(voltage_entry, int(Voltage::MAX), -1.0);
  for (int i = 0; i < int(Voltage::MAX); i++)
  {
    std::string voltage_key = voltage_str[i];
    if (power_config[voltage_key])
      voltage_entry[i] = power_config[voltage_key].as<double>(-1.0);
  }

  std::fill_n(current_entry, int(Current::MAX), -1.0);
  for (int i = 0; i < int(Current::MAX); i++)
  {
    std::string current_key = current_str[i];
    if (power_config[current_key])
      current_entry[i] = power_config[current_key].as<double>(-1.0);
  }
};


void DDR3::init_power_lambda()
{
  /************************************************
   *                 ACT Behavior
   ***********************************************/

  power_lambda[int(Level::Bank)][int(Command::ACT)] = 
  [] (DRAM<DDR3>* node) 
  {
    uint clk =  node->cur_clk;
    node->power.handle_ACT_bank(clk);
  };

  power_lambda[int(Level::Rank)][int(Command::ACT)] = 
  [] (DRAM<DDR3>* node) 
  {
    bool is_rank_idle = true;
    for (auto bank : node->children)
    {
      if (bank->state == State::Opened)
        is_rank_idle = false;
    }
    
    uint clk =  node->cur_clk;
    node->power.handle_ACT_rank(is_rank_idle, clk);
  };

  /************************************************
   *                 PRE Behavior
   ***********************************************/

  power_lambda[int(Level::Bank)][int(Command::PRE)] = 
  [] (DRAM<DDR3>* node) 
  {
    uint clk =  node->cur_clk;
    node->power.handle_PRE_bank(clk);
  };

  power_lambda[int(Level::Rank)][int(Command::PRE)] = 
  [] (DRAM<DDR3>* node) 
  {
    bool is_rank_idle = true;
    uint num_opened_banks = 0;
    for (auto bank : node->children)
    {
      if (bank->state == State::Opened)
        num_opened_banks++;
      
      if (num_opened_banks > 1)
        is_rank_idle = false;
    }
    
    uint clk =  node->cur_clk;
    node->power.handle_PRE_rank(is_rank_idle, clk);
  };


  /************************************************
   *                 PREab Behavior
   ***********************************************/
  power_lambda[int(Level::Rank)][int(Command::PREab)] = 
  [] (DRAM<DDR3>* node) 
  {
    uint clk =  node->cur_clk;
    bool is_rank_idle = true;
    node->power.handle_PRE_rank(is_rank_idle, clk);

    for (auto bank : node->children)
      bank->power.handle_PRE_bank(bank->cur_clk);
  };


  /************************************************
   *                 RD Behavior
   ***********************************************/
  power_lambda[int(Level::Bank)][int(Command::RD)] = 
  [] (DRAM<DDR3>* node) 
  {
    node->power.counters.num_rd++;
  };

  power_lambda[int(Level::Rank)][int(Command::RD)] = 
  [] (DRAM<DDR3>* node) 
  {
    uint clk =  node->cur_clk;
    node->power.handle_RD_rank(clk);
  };


  /************************************************
   *                 RDA Behavior
   ***********************************************/
  power_lambda[int(Level::Bank)][int(Command::RDA)] = 
  [] (DRAM<DDR3>* node) 
  {
    node->power.counters.num_rd++;
  };

  power_lambda[int(Level::Rank)][int(Command::RDA)] = 
  [] (DRAM<DDR3>* node) 
  {
    uint clk =  node->cur_clk;
    node->power.handle_RD_rank(clk);
  };


  /************************************************
   *                 WR Behavior
   ***********************************************/
  power_lambda[int(Level::Bank)][int(Command::WR)] = 
  [] (DRAM<DDR3>* node) 
  {
    node->power.counters.num_wr++;
  };

  power_lambda[int(Level::Rank)][int(Command::WR)] = 
  [] (DRAM<DDR3>* node) 
  {
    uint clk =  node->cur_clk;
    node->power.handle_WR_rank(clk);
  };


  /************************************************
   *                 REF Behavior
   ***********************************************/
  power_lambda[int(Level::Rank)][int(Command::REF)] = 
  [] (DRAM<DDR3>* node) 
  {
    uint clk =  node->cur_clk;
    node->power.handle_REF_rank(clk);

    // Update all banks
    for (auto bank : node->children)
      bank->power.handle_REF_bank(bank->cur_clk);
  };
}
#endif

void DDR3::init_timing_table()
{
  std::vector<TimingEntry> *t;

  /*** Channel ***/ 
  t = timing[int(Level::Channel)];

  // CAS <-> CAS
  t[int(Command::RD)] .push_back({Command::RD,  1, TS(nBL)});
  t[int(Command::RD)] .push_back({Command::RDA, 1, TS(nBL)});
  t[int(Command::RDA)].push_back({Command::RD,  1, TS(nBL)});
  t[int(Command::RDA)].push_back({Command::RDA, 1, TS(nBL)});
  t[int(Command::WR)] .push_back({Command::WR,  1, TS(nBL)});
  t[int(Command::WR)] .push_back({Command::WRA, 1, TS(nBL)});
  t[int(Command::WRA)].push_back({Command::WR,  1, TS(nBL)});
  t[int(Command::WRA)].push_back({Command::WRA, 1, TS(nBL)});


  /*** Rank ***/ 
  t = timing[int(Level::Rank)];

  // CAS <-> CAS
  t[int(Command::RD)]. push_back({Command::RD,  1, TS(nCCD)});
  t[int(Command::RD)]. push_back({Command::RDA, 1, TS(nCCD)});
  t[int(Command::RDA)].push_back({Command::RD,  1, TS(nCCD)});
  t[int(Command::RDA)].push_back({Command::RDA, 1, TS(nCCD)});
  t[int(Command::WR)]. push_back({Command::WR,  1, TS(nCCD)});
  t[int(Command::WR)]. push_back({Command::WRA, 1, TS(nCCD)});
  t[int(Command::WRA)].push_back({Command::WR,  1, TS(nCCD)});
  t[int(Command::WRA)].push_back({Command::WRA, 1, TS(nCCD)});
  t[int(Command::RD)]. push_back({Command::WR,  1, TS(nCL)  + TS(nBL) + 2 - TS(nCWL)});
  t[int(Command::RD)]. push_back({Command::WRA, 1, TS(nCL)  + TS(nBL) + 2 - TS(nCWL)});
  t[int(Command::RDA)].push_back({Command::WR,  1, TS(nCL)  + TS(nBL) + 2 - TS(nCWL)});
  t[int(Command::RDA)].push_back({Command::WRA, 1, TS(nCL)  + TS(nBL) + 2 - TS(nCWL)});
  t[int(Command::WR)]. push_back({Command::RD,  1, TS(nCWL) + TS(nBL) + TS(nWTR)});
  t[int(Command::WR)]. push_back({Command::RDA, 1, TS(nCWL) + TS(nBL) + TS(nWTR)});
  t[int(Command::WRA)].push_back({Command::RD,  1, TS(nCWL) + TS(nBL) + TS(nWTR)});
  t[int(Command::WRA)].push_back({Command::RDA, 1, TS(nCWL) + TS(nBL) + TS(nWTR)});

  // CAS <-> CAS (between sibling ranks)
  t[int(Command::RD)]. push_back({Command::RD,  1, TS(nBL)  + TS(nRTRS), true});
  t[int(Command::RD)]. push_back({Command::RDA, 1, TS(nBL)  + TS(nRTRS), true});
  t[int(Command::RDA)].push_back({Command::RD,  1, TS(nBL)  + TS(nRTRS), true});
  t[int(Command::RDA)].push_back({Command::RDA, 1, TS(nBL)  + TS(nRTRS), true});
  t[int(Command::RD)]. push_back({Command::WR,  1, TS(nBL)  + TS(nRTRS), true});
  t[int(Command::RD)]. push_back({Command::WRA, 1, TS(nBL)  + TS(nRTRS), true});
  t[int(Command::RDA)].push_back({Command::WR,  1, TS(nBL)  + TS(nRTRS), true});
  t[int(Command::RDA)].push_back({Command::WRA, 1, TS(nBL)  + TS(nRTRS), true});
  t[int(Command::RD)]. push_back({Command::WR,  1, TS(nCL)  + TS(nBL) + TS(nRTRS) - TS(nCWL), true});
  t[int(Command::RD)]. push_back({Command::WRA, 1, TS(nCL)  + TS(nBL) + TS(nRTRS) - TS(nCWL), true});
  t[int(Command::RDA)].push_back({Command::WR,  1, TS(nCL)  + TS(nBL) + TS(nRTRS) - TS(nCWL), true});
  t[int(Command::RDA)].push_back({Command::WRA, 1, TS(nCL)  + TS(nBL) + TS(nRTRS) - TS(nCWL), true});
  t[int(Command::WR)]. push_back({Command::RD,  1, TS(nCWL) + TS(nBL) + TS(nRTRS) - TS(nCL), true});
  t[int(Command::WR)]. push_back({Command::RDA, 1, TS(nCWL) + TS(nBL) + TS(nRTRS) - TS(nCL), true});
  t[int(Command::WRA)].push_back({Command::RD,  1, TS(nCWL) + TS(nBL) + TS(nRTRS) - TS(nCL), true});
  t[int(Command::WRA)].push_back({Command::RDA, 1, TS(nCWL) + TS(nBL) + TS(nRTRS) - TS(nCL), true});

  t[int(Command::RD)].push_back({Command::PREab, 1, TS(nRTP)});
  t[int(Command::WR)].push_back({Command::PREab, 1, TS(nCWL) + TS(nBL) + TS(nWR)});

  // CAS <-> PD
  t[int(Command::RD)]. push_back({Command::PDE, 1, TS(nCL)  + TS(nBL) + 1});
  t[int(Command::RDA)].push_back({Command::PDE, 1, TS(nCL)  + TS(nBL) + 1});
  t[int(Command::WR)]. push_back({Command::PDE, 1, TS(nCWL) + TS(nBL) + TS(nWR)});
  t[int(Command::WRA)].push_back({Command::PDE, 1, TS(nCWL) + TS(nBL) + TS(nWR) + 1}); // +1 for pre
  t[int(Command::PDX)].push_back({Command::RD,  1, TS(nXP)});
  t[int(Command::PDX)].push_back({Command::RDA, 1, TS(nXP)});
  t[int(Command::PDX)].push_back({Command::WR,  1, TS(nXP)});
  t[int(Command::PDX)].push_back({Command::WRA, 1, TS(nXP)});
  
  // CAS <-> SR: none (all banks have to be precharged)

  // RAS <-> RAS
  t[int(Command::ACT)]. push_back({Command::ACT,  1, TS(nRRD)});
  t[int(Command::ACT)]. push_back({Command::ACT,  4, TS(nFAW)});
  t[int(Command::ACT)]. push_back({Command::PREab, 1, TS(nRAS)});
  t[int(Command::PREab)].push_back({Command::ACT,  1, TS(nRP)});

  // RAS <-> REF
  t[int(Command::ACT)]. push_back({Command::REF, 1, TS(nRC)});
  t[int(Command::PRE)]. push_back({Command::REF, 1, TS(nRP)});
  t[int(Command::PREab)].push_back({Command::REF, 1, TS(nRP)});
  t[int(Command::RDA)]. push_back({Command::REF, 1, TS(nRTP) + TS(nRP)});
  t[int(Command::WRA)]. push_back({Command::REF, 1, TS(nCWL) + TS(nBL) + TS(nWR) + TS(nRP)});
  t[int(Command::REF)]. push_back({Command::ACT, 1, TS(nRFC)});

  // RAS <-> PD
  t[int(Command::ACT)].push_back({Command::PDE,  1, 1});
  t[int(Command::PDX)].push_back({Command::ACT,  1, TS(nXP)});
  t[int(Command::PDX)].push_back({Command::PRE,  1, TS(nXP)});
  t[int(Command::PDX)].push_back({Command::PREab, 1, TS(nXP)});

  // RAS <-> SR
  t[int(Command::PRE)]. push_back({Command::SRE, 1, TS(nRP)});
  t[int(Command::PREab)].push_back({Command::SRE, 1, TS(nRP)});
  t[int(Command::SRX)]. push_back({Command::ACT, 1, TS(nXS)});

  // REF <-> REF
  t[int(Command::REF)].push_back({Command::REF, 1, TS(nRFC)});

  // REF <-> PD
  t[int(Command::REF)].push_back({Command::PDE, 1, 1});
  t[int(Command::PDX)].push_back({Command::REF, 1, TS(nXP)});

  // REF <-> SR
  t[int(Command::SRX)].push_back({Command::REF, 1, TS(nXS)});
  
  // PD <-> PD
  t[int(Command::PDE)].push_back({Command::PDX, 1, TS(nPD)});
  t[int(Command::PDX)].push_back({Command::PDE, 1, TS(nXP)});

  // PD <-> SR
  t[int(Command::PDX)].push_back({Command::SRE, 1, TS(nXP)});
  t[int(Command::SRX)].push_back({Command::PDE, 1, TS(nXS)});
  
  // SR <-> SR
  t[int(Command::SRE)].push_back({Command::SRX, 1, TS(nCKESR)});
  t[int(Command::SRX)].push_back({Command::SRE, 1, TS(nXS)});


  /*** Bank ***/ 
  t = timing[int(Level::Bank)];

  // CAS <-> RAS
  t[int(Command::ACT)].push_back({Command::RD,  1, TS(nRCD)});
  t[int(Command::ACT)].push_back({Command::RDA, 1, TS(nRCD)});
  t[int(Command::ACT)].push_back({Command::WR,  1, TS(nRCD)});
  t[int(Command::ACT)].push_back({Command::WRA, 1, TS(nRCD)});

  t[int(Command::RD)]. push_back({Command::PRE, 1, TS(nRTP)});
  t[int(Command::WR)]. push_back({Command::PRE, 1, TS(nCWL) + TS(nBL) + TS(nWR)});

  // RDA and WRA has the same timing constraints as RD/WR as PRE is explicitly handled.
  t[int(Command::RDA)].push_back({Command::PRE, 1, TS(nRTP)});
  t[int(Command::WRA)].push_back({Command::PRE, 1, TS(nCWL) + TS(nBL) + TS(nWR)});

  // RAS <-> RAS
  t[int(Command::ACT)].push_back({Command::ACT, 1, TS(nRC)});
  t[int(Command::ACT)].push_back({Command::PRE, 1, TS(nRAS)});
  t[int(Command::PRE)].push_back({Command::ACT, 1, TS(nRP)});
}

uint64_t DDR3::get_nCK_JEDEC_int(float t_ns, int tCK_ps)
{
  // Turn timing in nanosecond to picosecond
  uint64_t t_ps = t_ns * 1000;

  // Apply correction factor 974
  uint64_t nCK = ((t_ps * 1000 / tCK_ps) + 974) / 1000;
  return nCK;
}

#undef TS