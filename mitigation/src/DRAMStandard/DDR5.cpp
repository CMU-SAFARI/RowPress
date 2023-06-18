#include "DDR5.h"

#include <vector>
#include <functional>
#include <cassert>

using namespace Ramulator;

DDR5::DDR5(const YAML::Node& config): config(config)
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
}

void DDR5::init_org(const YAML::Node& org_config)
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
  uint64_t density_calc = uint64_t(org_entry.count[int(Level::BankGroup)]) *
                          uint64_t(org_entry.count[int(Level::Bank)]) *
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

void DDR5::init_speed(const YAML::Node& speed_config)
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
    speed_entry[int(TimingCons::nRRDS)] = RRDS_TABLE[dq_id][rate_id];
    speed_entry[int(TimingCons::nRRDL)] = RRDL_TABLE[dq_id][rate_id];
    speed_entry[int(TimingCons::nFAW)]  = FAW_TABLE [dq_id][rate_id];
  }
  int density_id = get_density_index(org_entry.density);
  if (density_id != -1)
    // tXS = tRFC1 + 10ns
    speed_entry[int(TimingCons::nXS)] = get_nCK_JEDEC_int(RFC_TABLE[0][density_id] + 10, tCK_ps);

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

  std::string refresh_type_str = config["refresh"].as<std::string>("1X");

  if (refresh_type_str == "1X")
    refresh_mode = RefreshMode::Refresh_1X;
  else if (refresh_type_str == "2X")
    refresh_mode = RefreshMode::Refresh_2X;
  else if (refresh_type_str == "4X")
    refresh_mode = RefreshMode::Refresh_4X;
  else
    assert(false && "Unknown refresh type!");

  speed_entry[int(TimingCons::nRFC)] = get_nCK_JEDEC_int(RFC_TABLE[int(refresh_mode)][density_id], tCK_ps);
  speed_entry[int(TimingCons::nREFI)] = get_nCK_JEDEC_int(REFI_BASE_TABLE[density_id] / (1 << int(refresh_mode)), tCK_ps);

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


void DDR5::init_prereq()
{
  // RD
  prereq[int(Level::Rank)][int(Command::RD)] = 
  [] (DRAM<DDR5>* node, Command cmd, int id) 
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
  [] (DRAM<DDR5>* node, Command cmd, int id) 
  {
    switch (int(node->state)) 
    {
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
  prereq[int(Level::Rank)][int(Command::REFab)] = 
  [] (DRAM<DDR5>* node, Command cmd, int id) 
  {
    for (auto bg : node->children)
      for (auto bank: bg->children) 
        if (bank->state == State::Closed)
          continue;
        else 
          return Command::PREab;
    return Command::REFab;
  };
  prereq[int(Level::Rank)][int(Command::RFMab)] = prereq[int(Level::Rank)][int(Command::REFab)];

  prereq[int(Level::Rank)][int(Command::REFsb)] = 
  [] (DRAM<DDR5>* node, Command cmd, int id) 
  {
    for (auto bg : node->children)
      for (auto bank: bg->children) 
        if (bank->id == id && bank->state == State::Closed)
          continue;
        else 
          return Command::PREsb;
    return Command::REFsb;
  };
  prereq[int(Level::Rank)][int(Command::RFMsb)] = prereq[int(Level::BankGroup)][int(Command::REFsb)];

  // PD
  prereq[int(Level::Rank)][int(Command::PDE)] = 
  [] (DRAM<DDR5>* node, Command cmd, int id) 
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
  [] (DRAM<DDR5>* node, Command cmd, int id) 
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
void DDR5::init_rowhit()
{
  // RD
  rowhit[int(Level::Bank)][int(Command::RD)] = 
  [] (DRAM<DDR5>* node, Command cmd, int id) 
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

void DDR5::init_rowopen()
{
    // RD
    rowopen[int(Level::Bank)][int(Command::RD)] = 
    [] (DRAM<DDR5>* node, Command cmd, int id) 
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

void DDR5::init_lambda()
{
  /************************************************
   *                 Bank Behavior
   ***********************************************/

  lambda[int(Level::Bank)][int(Command::ACT)] = 
  [] (DRAM<DDR5>* node, int id) 
  {
    node->state = State::Opened;
    node->row_state[id] = State::Opened;
  };

  lambda[int(Level::Bank)][int(Command::PRE)] = 
  [] (DRAM<DDR5>* node, int id) 
  {
    node->state = State::Closed;
    node->row_state.clear();
  };

  // Auto-precharge
  lambda[int(Level::Bank)][int(Command::RDA)] = lambda[int(Level::Bank)][int(Command::PRE)];
  lambda[int(Level::Bank)][int(Command::WRA)] = lambda[int(Level::Bank)][int(Command::PRE)];

  /************************************************
   *                 Rank Behavior
   ***********************************************/

  // Precharge all
  lambda[int(Level::Rank)][int(Command::PREab)] = 
  [] (DRAM<DDR5>* node, int id) 
  {
    for (auto bg : node->children)
      for (auto bank : bg->children) 
      {
        bank->state = State::Closed;
        bank->row_state.clear();
      }
  };

  lambda[int(Level::Rank)][int(Command::PREsb)] = 
  [] (DRAM<DDR5>* node, int id) 
  {
    for (auto bg : node->children)
    {
      for (auto bank : bg->children) 
      {
        if (bank->id == id)
        {
          bank->state = State::Closed;
          bank->row_state.clear();
        }
      }
    }
  };

  lambda[int(Level::Rank)][int(Command::PDE)] = 
  [] (DRAM<DDR5>* node, int id) 
  {
    for (auto bg : node->children)
      for (auto bank : bg->children) 
      {
        if (bank->state == State::Closed)
          continue;
        node->state = State::ActPowerDown;
        return;
      }
    node->state = State::PrePowerDown;
  };
  
  lambda[int(Level::Rank)][int(Command::PDX)] = [] (DRAM<DDR5>* node, int id) { node->state = State::PowerUp; };
  lambda[int(Level::Rank)][int(Command::SRE)] = [] (DRAM<DDR5>* node, int id) { node->state = State::SelfRefresh; };
  lambda[int(Level::Rank)][int(Command::SRX)] = [] (DRAM<DDR5>* node, int id) { node->state = State::PowerUp; };
}

#define TS(timing) (speed_entry[int(TimingCons::timing)])

void DDR5::init_timing_table()
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
  t[int(Command::RD)]. push_back({Command::RD,  1, TS(nCCDS)});
  t[int(Command::RD)]. push_back({Command::RDA, 1, TS(nCCDS)});
  t[int(Command::RDA)].push_back({Command::RD,  1, TS(nCCDS)});
  t[int(Command::RDA)].push_back({Command::RDA, 1, TS(nCCDS)});
  t[int(Command::WR)]. push_back({Command::WR,  1, TS(nCCDS)});
  t[int(Command::WR)]. push_back({Command::WRA, 1, TS(nCCDS)});
  t[int(Command::WRA)].push_back({Command::WR,  1, TS(nCCDS)});
  t[int(Command::WRA)].push_back({Command::WRA, 1, TS(nCCDS)});
  t[int(Command::RD)]. push_back({Command::WR,  1, TS(nCL)  + TS(nBL) + 2 - TS(nCWL)});
  t[int(Command::RD)]. push_back({Command::WRA, 1, TS(nCL)  + TS(nBL) + 2 - TS(nCWL)});
  t[int(Command::RDA)].push_back({Command::WR,  1, TS(nCL)  + TS(nBL) + 2 - TS(nCWL)});
  t[int(Command::RDA)].push_back({Command::WRA, 1, TS(nCL)  + TS(nBL) + 2 - TS(nCWL)});
  t[int(Command::WR)]. push_back({Command::RD,  1, TS(nCWL) + TS(nBL) + TS(nWTRS)});
  t[int(Command::WR)]. push_back({Command::RDA, 1, TS(nCWL) + TS(nBL) + TS(nWTRS)});
  t[int(Command::WRA)].push_back({Command::RD,  1, TS(nCWL) + TS(nBL) + TS(nWTRS)});
  t[int(Command::WRA)].push_back({Command::RDA, 1, TS(nCWL) + TS(nBL) + TS(nWTRS)});

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
  t[int(Command::ACT)].  push_back({Command::ACT,   1, TS(nRRDS)});
  t[int(Command::ACT)].  push_back({Command::ACT,   4, TS(nFAW)});
  t[int(Command::ACT)].  push_back({Command::PREab, 1, TS(nRAS)});
  t[int(Command::PREab)].push_back({Command::ACT,   1, TS(nRP)});

  t[int(Command::PREab)].push_back({Command::PREab, 1, TS(nPPD)});
  t[int(Command::PREab)].push_back({Command::PREsb, 1, TS(nPPD)});
  t[int(Command::PREab)].push_back({Command::PRE, 1, TS(nPPD)});
  t[int(Command::PREsb)].push_back({Command::PREab, 1, TS(nPPD)});
  t[int(Command::PREsb)].push_back({Command::PREsb, 1, TS(nPPD)});
  t[int(Command::PREsb)].push_back({Command::PRE, 1, TS(nPPD)});
  t[int(Command::PRE)].  push_back({Command::PREab, 1, TS(nPPD)});
  t[int(Command::PRE)].  push_back({Command::PREsb, 1, TS(nPPD)});
  t[int(Command::PRE)].  push_back({Command::PRE, 1, TS(nPPD)});


  // RAS <-> REF
  t[int(Command::ACT)].   push_back({Command::REFab,  1, TS(nRC)});
  t[int(Command::PRE)].   push_back({Command::REFab,  1, TS(nRP)});
  t[int(Command::PREab)]. push_back({Command::REFab,  1, TS(nRP)});
  t[int(Command::RDA)].   push_back({Command::REFab,  1, TS(nRTP) + TS(nRP)});
  t[int(Command::WRA)].   push_back({Command::REFab,  1, TS(nCWL) + TS(nBL) + TS(nWR) + TS(nRP)});
  t[int(Command::REFab)]. push_back({Command::ACT,    1, TS(nRFC)});

  // RAS <-> PD
  t[int(Command::ACT)].push_back({Command::PDE,   1, 1});
  t[int(Command::PDX)].push_back({Command::ACT,   1, TS(nXP)});
  t[int(Command::PDX)].push_back({Command::PRE, 1, TS(nXP)});
  t[int(Command::PDX)].push_back({Command::PREab, 1, TS(nXP)});

  // RAS <-> SR
  t[int(Command::PRE)].  push_back({Command::SRE, 1, TS(nRP)});
  t[int(Command::PREab)].push_back({Command::SRE, 1, TS(nRP)});
  t[int(Command::SRX)].  push_back({Command::ACT, 1, TS(nXS)});

  // REF <-> REF
  t[int(Command::REFab)].push_back({Command::REFab, 1, TS(nRFC)});
  t[int(Command::REFab)].push_back({Command::REFsb, 1, TS(nRFC)});

  // REF <-> PD
  t[int(Command::REFab)].push_back({Command::PDE,   1, 1});
  t[int(Command::PDX)]  .push_back({Command::REFab, 1, TS(nXP)});

  // REF <-> SR
  t[int(Command::SRX)].push_back({Command::REFab, 1, TS(nXS)});
  
  // PD <-> PD
  t[int(Command::PDE)].push_back({Command::PDX, 1, TS(nPD)});
  t[int(Command::PDX)].push_back({Command::PDE, 1, TS(nXP)});

  // PD <-> SR
  t[int(Command::PDX)].push_back({Command::SRE, 1, TS(nXP)});
  t[int(Command::SRX)].push_back({Command::PDE, 1, TS(nXS)});
  
  // SR <-> SR
  t[int(Command::SRE)].push_back({Command::SRX, 1, TS(nCKESR)});
  t[int(Command::SRX)].push_back({Command::SRE, 1, TS(nXS)});


  /*** Bank Group ***/ 
  t = timing[int(Level::BankGroup)];
  // CAS <-> CAS
  t[int(Command::RD)]. push_back({Command::RD,  1, TS(nCCDL)});
  t[int(Command::RD)]. push_back({Command::RDA, 1, TS(nCCDL)});
  t[int(Command::RDA)].push_back({Command::RD,  1, TS(nCCDL)});
  t[int(Command::RDA)].push_back({Command::RDA, 1, TS(nCCDL)});
  t[int(Command::WR)]. push_back({Command::WR,  1, TS(nCCDLwr)});
  t[int(Command::WR)]. push_back({Command::WRA, 1, TS(nCCDLwr)});
  t[int(Command::WRA)].push_back({Command::WR,  1, TS(nCCDLwr)});
  t[int(Command::WRA)].push_back({Command::WRA, 1, TS(nCCDLwr)});
  t[int(Command::WR)]. push_back({Command::RD,  1, TS(nCWL) + TS(nBL) + TS(nWTRL)});
  t[int(Command::WR)]. push_back({Command::RDA, 1, TS(nCWL) + TS(nBL) + TS(nWTRL)});
  t[int(Command::WRA)].push_back({Command::RD,  1, TS(nCWL) + TS(nBL) + TS(nWTRL)});
  t[int(Command::WRA)].push_back({Command::RDA, 1, TS(nCWL) + TS(nBL) + TS(nWTRL)});

  // RAS <-> RAS
  t[int(Command::ACT)].push_back({Command::ACT, 1, TS(nRRDL)});


  /*** Bank ***/ 
  t = timing[int(Level::Bank)];

  // CAS <-> RAS
  t[int(Command::ACT)].push_back({Command::RD,  1, TS(nRCD)});
  t[int(Command::ACT)].push_back({Command::RDA, 1, TS(nRCD)});
  t[int(Command::ACT)].push_back({Command::WR,  1, TS(nRCD)});
  t[int(Command::ACT)].push_back({Command::WRA, 1, TS(nRCD)});

  t[int(Command::RD)]. push_back({Command::PRE,   1, TS(nRTP)});
  t[int(Command::RD)]. push_back({Command::PREsb, 1, TS(nRTP)});
  t[int(Command::WR)]. push_back({Command::PRE,   1, TS(nCWL) + TS(nBL) + TS(nWR)});
  t[int(Command::WR)]. push_back({Command::PREsb, 1, TS(nCWL) + TS(nBL) + TS(nWR)});

  t[int(Command::RDA)].push_back({Command::ACT, 1, TS(nRTP) + TS(nRP)});
  t[int(Command::WRA)].push_back({Command::ACT, 1, TS(nCWL) + TS(nBL) + TS(nWR) + TS(nRP)});

  // RAS <-> RAS
  t[int(Command::ACT)]  .push_back({Command::ACT,   1, TS(nRC)});
  t[int(Command::ACT)]  .push_back({Command::PRE,   1, TS(nRAS)});
  t[int(Command::ACT)]  .push_back({Command::PREsb, 1, TS(nRAS)});
  t[int(Command::PRE)].  push_back({Command::ACT,   1, TS(nRP)});
  t[int(Command::PREsb)].push_back({Command::ACT,   1, TS(nRP)});

  // REF <-> RAS
  t[int(Command::REFsb)].push_back({Command::ACT, 1, TS(nRFCsb)});
  t[int(Command::REFsb)].push_back({Command::ACT, 1, TS(nREFSBRD), true});

  t[int(Command::ACT)].   push_back({Command::REFsb,  1, TS(nRC)});
  t[int(Command::ACT)].   push_back({Command::REFsb,  1, TS(nRRDL), true});
  t[int(Command::PREab)]. push_back({Command::REFsb,  1, TS(nRP)});
  t[int(Command::PRE)].   push_back({Command::REFsb,  1, TS(nRP)});
  t[int(Command::PREsb)]. push_back({Command::REFsb,  1, TS(nRP)});
  t[int(Command::RDA)].   push_back({Command::REFsb,  1, TS(nRTP) + TS(nRP)});
  t[int(Command::WRA)].   push_back({Command::REFsb,  1, TS(nCWL) + TS(nBL) + TS(nWR) + TS(nRP)});
  t[int(Command::REFab)]. push_back({Command::ACT,    1, TS(nRFC)});

  // REF <-> REF
  t[int(Command::REFsb)].push_back({Command::REFsb, 1, TS(nRFCsb)});
}

uint64_t DDR5::get_nCK_JEDEC_int(float t_ns, int tCK_ps)
{
  // Turn timing in nanosecond to picosecond
  uint64_t t_ps = t_ns * 1000;

  // Apply correction factor 974
  uint64_t nCK = ((t_ps * 1000 / tCK_ps) + 974) / 1000;
  return nCK;
}

#undef TS