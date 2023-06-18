#include "GDDR5.h"

#include <vector>
#include <functional>
#include <cassert>

using namespace Ramulator;

GDDR5::GDDR5(const YAML::Node& config): config(config)
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

  if (spec_config["bankgroup_enabled"])
    bankgroup_enabled = spec_config["bankgroup_enabled"].as<bool>(false);

  // Fill all the function & timing tables
  init_lambda();
  init_prereq();
  init_rowhit();
  init_rowopen();
  init_timing_table();
}

void GDDR5::init_org(const YAML::Node& org_config)
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

void GDDR5::init_speed(const YAML::Node& speed_config)
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


  // GDDR5 is QDR
  int tCK_ps = 1E6 / (speed_entry[int(TimingCons::rate)] / 4);
  speed_entry[int(TimingCons::tCK_ps)] = tCK_ps;


  // Set nRTW
  speed_entry[int(TimingCons::nRTW)] = speed_entry[int(TimingCons::nCL)] + 
                                       speed_entry[int(TimingCons::nBL)] + 2 -
                                       speed_entry[int(TimingCons::nCWL)];


  // Load refresh timings
  int density_id = get_density_index(org_entry.density);
  speed_entry[int(TimingCons::nRFC)]    = get_nCK_JEDEC_int(RFC_TABLE[density_id], tCK_ps);
  speed_entry[int(TimingCons::nRFCpb)]  = speed_entry[int(TimingCons::nRFC)];
  speed_entry[int(TimingCons::nREFI)]   = get_nCK_JEDEC_int(REFI_BASE_TABLE[0][density_id], tCK_ps);
  speed_entry[int(TimingCons::nREFIpb)] = get_nCK_JEDEC_int(REFI_BASE_TABLE[0][density_id], tCK_ps);

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


void GDDR5::init_prereq()
{
  // RD
  prereq[int(Level::Channel)][int(Command::RD)] = 
  [] (DRAM<GDDR5>* node, Command cmd, int id) 
  {
    switch (int(node->state)) 
    {
      case int(State::PowerUp):       return Command::MAX;
      case int(State::ActPowerDown):  return Command::PDX;
      case int(State::PrePowerDown):  return Command::PDX;
      case int(State::SelfRefresh):   return Command::SRX;
      default: assert(false && "[PREQ] Invalid Channel state!"); return Command::MAX;
    }
  };

  prereq[int(Level::Bank)][int(Command::RD)] = 
  [] (DRAM<GDDR5>* node, Command cmd, int id) 
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
  prereq[int(Level::Channel)][int(Command::WOM)] = prereq[int(Level::Channel)][int(Command::RD)];
  prereq[int(Level::Channel)][int(Command::WDM)] = prereq[int(Level::Channel)][int(Command::RD)];
  prereq[int(Level::Channel)][int(Command::WSM)] = prereq[int(Level::Channel)][int(Command::RD)];
  prereq[int(Level::Bank)][int(Command::WOM)] = prereq[int(Level::Bank)][int(Command::RD)];
  prereq[int(Level::Bank)][int(Command::WDM)] = prereq[int(Level::Bank)][int(Command::RD)];
  prereq[int(Level::Bank)][int(Command::WSM)] = prereq[int(Level::Bank)][int(Command::RD)];

  // REF
  prereq[int(Level::Channel)][int(Command::REFab)] = 
  [] (DRAM<GDDR5>* node, Command cmd, int id) 
  {
    for (auto bg : node->children)
      for (auto bank: bg->children) 
        if (bank->state == State::Closed)
          continue;
        else 
          return Command::PREab;
    return Command::REFab;
  };

  prereq[int(Level::Bank)][int(Command::REFpb)] = 
  [] (DRAM<GDDR5>* node, Command cmd, int id) 
  {
    switch (int(node->state)) 
    {
      case int(State::Closed): return Command::REFpb;
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

  // PD
  prereq[int(Level::Channel)][int(Command::PDE)] = 
  [] (DRAM<GDDR5>* node, Command cmd, int id) 
  {
    switch (int(node->state)) 
    {
      case int(State::PowerUp):       return Command::PDE;
      case int(State::ActPowerDown):  return Command::PDE;
      case int(State::PrePowerDown):  return Command::PDE;
      case int(State::SelfRefresh):   return Command::SRX;
      default: assert(false && "[PREQ] Invalid Channel state!"); return Command::MAX;
    }
  };

  // SR
  prereq[int(Level::Channel)][int(Command::SRE)] = 
  [] (DRAM<GDDR5>* node, Command cmd, int id) 
  {
    switch (int(node->state)) 
    {
      case int(State::PowerUp):       return Command::SRE;
      case int(State::ActPowerDown):  return Command::PDX;
      case int(State::PrePowerDown):  return Command::PDX;
      case int(State::SelfRefresh):   return Command::SRE;
      default: assert(false && "[PREQ] Invalid Channel state!"); return Command::MAX;
    }
  };
}

// SAUGATA: added row hit check functions to see if the desired location is currently open
void GDDR5::init_rowhit()
{
  // RD
  rowhit[int(Level::Bank)][int(Command::RD)] = 
  [] (DRAM<GDDR5>* node, Command cmd, int id) 
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
  rowhit[int(Level::Bank)][int(Command::WOM)] = rowhit[int(Level::Bank)][int(Command::RD)];
  rowhit[int(Level::Bank)][int(Command::WDM)] = rowhit[int(Level::Bank)][int(Command::RD)];
  rowhit[int(Level::Bank)][int(Command::WSM)] = rowhit[int(Level::Bank)][int(Command::RD)];
}

void GDDR5::init_rowopen()
{
    // RD
    rowopen[int(Level::Bank)][int(Command::RD)] = 
    [] (DRAM<GDDR5>* node, Command cmd, int id) 
    {
      switch (int(node->state)) 
      {
        case int(State::Closed): return false;
        case int(State::Opened): return true;
        default: assert(false && "[ROWOPEN] Invalid bank state!"); return false;
      }
    };

    // WR
    rowopen[int(Level::Bank)][int(Command::WOM)] = rowopen[int(Level::Bank)][int(Command::RD)];
    rowopen[int(Level::Bank)][int(Command::WDM)] = rowopen[int(Level::Bank)][int(Command::RD)];
    rowopen[int(Level::Bank)][int(Command::WSM)] = rowopen[int(Level::Bank)][int(Command::RD)];
}

void GDDR5::init_lambda()
{
  /************************************************
   *                 Bank Behavior
   ***********************************************/

  lambda[int(Level::Bank)][int(Command::ACT)] = 
  [] (DRAM<GDDR5>* node, int id) 
  {
    node->state = State::Opened;
    node->row_state[id] = State::Opened;
  };

  lambda[int(Level::Bank)][int(Command::PRE)] = 
  [] (DRAM<GDDR5>* node, int id) 
  {
    node->state = State::Closed;
    node->row_state.clear();
  };

  // Auto-precharge
  lambda[int(Level::Bank)][int(Command::RDA)]  = lambda[int(Level::Bank)][int(Command::PRE)];
  lambda[int(Level::Bank)][int(Command::WOMA)] = lambda[int(Level::Bank)][int(Command::PRE)];
  lambda[int(Level::Bank)][int(Command::WDMA)] = lambda[int(Level::Bank)][int(Command::PRE)];
  lambda[int(Level::Bank)][int(Command::WSMA)] = lambda[int(Level::Bank)][int(Command::PRE)];


  /************************************************
   *                 Channel Behavior
   ***********************************************/

  // Precharge all
  lambda[int(Level::Channel)][int(Command::PREab)] = 
  [] (DRAM<GDDR5>* node, int id) 
  {
    for (auto bg : node->children)
      for (auto bank : bg->children) 
      {
        bank->state = State::Closed;
        bank->row_state.clear();
      }
  };

  lambda[int(Level::Channel)][int(Command::PDE)] = 
  [] (DRAM<GDDR5>* node, int id) 
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
  
  lambda[int(Level::Channel)][int(Command::PDX)] = [] (DRAM<GDDR5>* node, int id) { node->state = State::PowerUp; };
  lambda[int(Level::Channel)][int(Command::SRE)] = [] (DRAM<GDDR5>* node, int id) { node->state = State::SelfRefresh; };
  lambda[int(Level::Channel)][int(Command::SRX)] = [] (DRAM<GDDR5>* node, int id) { node->state = State::PowerUp; };
}

#define TS(timing) (speed_entry[int(TimingCons::timing)])

void GDDR5::init_timing_table()
{
  std::vector<TimingEntry> *t;

  /*** Channel ***/ 
  t = timing[int(Level::Channel)];

  // CAS <-> CAS
  t[int(Command::RD)] .push_back({Command::RD,  1, TS(nBL)});
  t[int(Command::RD)] .push_back({Command::RDA, 1, TS(nBL)});
  t[int(Command::RDA)].push_back({Command::RD,  1, TS(nBL)});
  t[int(Command::RDA)].push_back({Command::RDA, 1, TS(nBL)});

  t[int(Command::WOM)] .push_back({Command::WOM,  1, TS(nBL)});
  t[int(Command::WOM)] .push_back({Command::WOMA, 1, TS(nBL)});
  t[int(Command::WOMA)].push_back({Command::WOM,  1, TS(nBL)});
  t[int(Command::WOMA)].push_back({Command::WOMA, 1, TS(nBL)});

  // WDM and WSM commands require some trailing NOPs for data masks
  for (int c = int(Command::ACT); c < int(Command::MAX); c++)
  {
    // 2 NOPs for double-byte mask
    t[int(Command::WDM)] .push_back({static_cast<Command>(c), 1, 2});
    t[int(Command::WSM)] .push_back({static_cast<Command>(c), 1, 2});
    // 3 NOPs for single-byte mask
    t[int(Command::WDMA)].push_back({static_cast<Command>(c), 1, 3});
    t[int(Command::WSMA)].push_back({static_cast<Command>(c), 1, 3});    
  }

  // CAS <-> CAS
  t[int(Command::RD)]. push_back({Command::RD,  1, TS(nCCDS)});
  t[int(Command::RD)]. push_back({Command::RDA, 1, TS(nCCDS)});
  t[int(Command::RDA)].push_back({Command::RD,  1, TS(nCCDS)});
  t[int(Command::RDA)].push_back({Command::RDA, 1, TS(nCCDS)});

  Command wr_cmds[6] = 
  {
    Command::WOM,  Command::WDM,  Command::WSM,
    Command::WOMA, Command::WDMA, Command::WSMA
  };
  for (auto from_cmd : wr_cmds)
    for (auto to_cmd : wr_cmds)
      t[int(from_cmd)].push_back({to_cmd,  1, TS(nCCDS)});

  for (auto wr_cmd : wr_cmds)
  {
    t[int(Command::RD)].  push_back({wr_cmd, 1, TS(nRTW)});
    t[int(Command::RDA)]. push_back({wr_cmd, 1, TS(nRTW)});
    t[int(wr_cmd)].push_back({Command::RD,  1, TS(nCWL) + TS(nBL) + TS(nWTRS)});
    t[int(wr_cmd)].push_back({Command::RDA, 1, TS(nCWL) + TS(nBL) + TS(nWTRS)});
  }

  t[int(Command::RD)]. push_back({Command::PREab, 1, TS(nRTPS)});
  t[int(Command::WOM)].push_back({Command::PREab, 1, TS(nCWL) + TS(nBL) + TS(nWR)});
  t[int(Command::WDM)].push_back({Command::PREab, 1, TS(nCWL) + TS(nBL) + TS(nWR)});
  t[int(Command::WSM)].push_back({Command::PREab, 1, TS(nCWL) + TS(nBL) + TS(nWR)});

  // CAS <-> PD
  t[int(Command::RD)]. push_back({Command::PDE, 1, TS(nCL)  + TS(nBL) + 1});
  t[int(Command::RDA)].push_back({Command::PDE, 1, TS(nCL)  + TS(nBL) + 1});
  t[int(Command::PDX)].push_back({Command::RD,  1, TS(nXP)});
  t[int(Command::PDX)].push_back({Command::RDA, 1, TS(nXP)});

  for (auto wr_cmd : wr_cmds)
  {
    t[int(wr_cmd)]. push_back({Command::PDE, 1, TS(nCWL) + TS(nBL) + TS(nWR)});
    t[int(Command::PDX)].push_back({wr_cmd,  1, TS(nXP)});
  }

  // RAS <-> RAS
  t[int(Command::ACT)].  push_back({Command::ACT,   1,  TS(nRRDS)});
  t[int(Command::ACT)].  push_back({Command::ACT,   4,  TS(nFAW)});
  t[int(Command::ACT)].  push_back({Command::ACT,   32, TS(n32AW)});
  t[int(Command::ACT)].  push_back({Command::PREab, 1,  TS(nRAS)});
  t[int(Command::PREab)].push_back({Command::ACT,   1,  TS(nRP)});

  t[int(Command::PREab)].push_back({Command::PREab, 1, TS(nPPD)});
  t[int(Command::PREab)].push_back({Command::PRE,   1, TS(nPPD)});
  t[int(Command::PRE)].  push_back({Command::PREab, 1, TS(nPPD)});
  t[int(Command::PRE)].  push_back({Command::PRE,   1, TS(nPPD)});


  // RAS <-> REF
  t[int(Command::ACT)].   push_back({Command::REFab,  1, TS(nRC)});
  t[int(Command::PRE)].   push_back({Command::REFab,  1, TS(nRP)});
  t[int(Command::PREab)]. push_back({Command::REFab,  1, TS(nRP)});
  t[int(Command::RDA)].   push_back({Command::REFab,  1, TS(nRTPS) + TS(nRP)});
  t[int(Command::WOMA)].  push_back({Command::REFab,  1, TS(nCWL)  + TS(nBL) + TS(nWR) + TS(nRP)});
  t[int(Command::WDMA)].  push_back({Command::REFab,  1, TS(nCWL)  + TS(nBL) + TS(nWR) + TS(nRP)});
  t[int(Command::WSMA)].  push_back({Command::REFab,  1, TS(nCWL)  + TS(nBL) + TS(nWR) + TS(nRP)});
  t[int(Command::REFab)]. push_back({Command::ACT,    1, TS(nRFC)});

  // RAS <-> PD
  t[int(Command::ACT)].push_back({Command::PDE,   1, 1});
  t[int(Command::PDX)].push_back({Command::ACT,   1, TS(nXP)});
  t[int(Command::PDX)].push_back({Command::PRE,   1, TS(nXP)});
  t[int(Command::PDX)].push_back({Command::PREab, 1, TS(nXP)});

  // RAS <-> SR
  t[int(Command::PRE)].  push_back({Command::SRE, 1, TS(nRP)});
  t[int(Command::PREab)].push_back({Command::SRE, 1, TS(nRP)});
  t[int(Command::SRX)].  push_back({Command::ACT, 1, TS(nXS)});

  // REF <-> REF
  t[int(Command::REFab)].push_back({Command::REFab, 1, TS(nRFC)});
  t[int(Command::REFab)].push_back({Command::REFpb, 1, TS(nRFC)});

  // REF <-> PD
  t[int(Command::REFab)].push_back({Command::PDE,   1, 1});
  t[int(Command::REFpb)].push_back({Command::PDE,   1, 1});
  t[int(Command::PDX)]  .push_back({Command::REFab, 1, TS(nXP)});
  t[int(Command::PDX)]  .push_back({Command::REFpb, 1, TS(nXP)});

  // REF <-> SR
  t[int(Command::SRX)].push_back({Command::REFab, 1, TS(nXS)});
  t[int(Command::SRX)].push_back({Command::REFpb, 1, TS(nXS)});
  
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
  if (bankgroup_enabled)
  {
    // CAS <-> CAS
    t[int(Command::RD)]. push_back({Command::RD,  1, TS(nCCDL)});
    t[int(Command::RD)]. push_back({Command::RDA, 1, TS(nCCDL)});
    t[int(Command::RDA)].push_back({Command::RD,  1, TS(nCCDL)});
    t[int(Command::RDA)].push_back({Command::RDA, 1, TS(nCCDL)});
    t[int(Command::WOM)]. push_back({Command::WOM,  1, TS(nCCDL)});
    t[int(Command::WOM)]. push_back({Command::WOMA, 1, TS(nCCDL)});
    t[int(Command::WOMA)].push_back({Command::WOM,  1, TS(nCCDL)});
    t[int(Command::WOMA)].push_back({Command::WOMA, 1, TS(nCCDL)});
    t[int(Command::WOM)]. push_back({Command::RD,  1, TS(nCWL) + TS(nBL) + TS(nWTRL)});
    t[int(Command::WOM)]. push_back({Command::RDA, 1, TS(nCWL) + TS(nBL) + TS(nWTRL)});
    t[int(Command::WOMA)].push_back({Command::RD,  1, TS(nCWL) + TS(nBL) + TS(nWTRL)});
    t[int(Command::WOMA)].push_back({Command::RDA, 1, TS(nCWL) + TS(nBL) + TS(nWTRL)});

    // RAS <-> RAS
    t[int(Command::ACT)].push_back({Command::ACT, 1, TS(nRRDL)});
  }


  /*** Bank ***/ 
  t = timing[int(Level::Bank)];

  // CAS <-> RAS
  t[int(Command::ACT)].push_back({Command::RD,   1, TS(nRCDrd)});
  t[int(Command::ACT)].push_back({Command::RDA,  1, TS(nRCDrd)});
  t[int(Command::ACT)].push_back({Command::WOM,  1, TS(nRCDwr)});
  t[int(Command::ACT)].push_back({Command::WOMA, 1, TS(nRCDwr)});
  t[int(Command::ACT)].push_back({Command::WDM,  1, TS(nRCDwr)});
  t[int(Command::ACT)].push_back({Command::WDMA, 1, TS(nRCDwr)});
  t[int(Command::ACT)].push_back({Command::WSM,  1, TS(nRCDwr)});
  t[int(Command::ACT)].push_back({Command::WSMA, 1, TS(nRCDwr)});

  if (bankgroup_enabled)
    t[int(Command::RD)].  push_back({Command::PRE,   1, TS(nRTPL)});
  else
    t[int(Command::RD)].  push_back({Command::PRE,   1, TS(nRTPS)});

  t[int(Command::WOM)]. push_back({Command::PRE,   1, TS(nCWL) + TS(nBL) + TS(nWR)});
  t[int(Command::WDM)]. push_back({Command::PRE,   1, TS(nCWL) + TS(nBL) + TS(nWR)});
  t[int(Command::WSM)]. push_back({Command::PRE,   1, TS(nCWL) + TS(nBL) + TS(nWR)});

  t[int(Command::RDA)]. push_back({Command::ACT, 1, TS(nRTPS) + TS(nRP)});
  t[int(Command::WOMA)].push_back({Command::ACT, 1, TS(nCWL) + TS(nBL) + TS(nWR) + TS(nRP)});
  t[int(Command::WDMA)].push_back({Command::ACT, 1, TS(nCWL) + TS(nBL) + TS(nWR) + TS(nRP)});
  t[int(Command::WSMA)].push_back({Command::ACT, 1, TS(nCWL) + TS(nBL) + TS(nWR) + TS(nRP)});

  // RAS <-> RAS
  t[int(Command::ACT)]  .push_back({Command::ACT,   1, TS(nRC)});
  t[int(Command::ACT)]  .push_back({Command::PRE,   1, TS(nRAS)});
  t[int(Command::PRE)].  push_back({Command::ACT,   1, TS(nRP)});

  // REF <-> RAS
  t[int(Command::REFab)]. push_back({Command::ACT,    1, TS(nRFC)});
  t[int(Command::REFpb)]. push_back({Command::ACT,    1, TS(nRFCpb), true});
  t[int(Command::REFpb)]. push_back({Command::ACT,    1, TS(nRREFD), true});

  // REF <-> REF
  t[int(Command::REFpb)]. push_back({Command::REFpb,    1, TS(nRREFD), true});


  // Sibling bank timings
  int nWTR = TS(nWTRL);
  if (bankgroup_enabled)
    nWTR = TS(nWTRL);
  else
    nWTR = TS(nWTRS);

  t[int(Command::WOMA)].push_back({Command::RD,   1, TS(nCWL) + TS(nBL) + nWTR, true});
  t[int(Command::WOMA)].push_back({Command::RDA,  1, TS(nCWL) + TS(nBL) + nWTR, true});
  t[int(Command::WDMA)].push_back({Command::RD,   1, TS(nCWL) + TS(nBL) + nWTR, true});
  t[int(Command::WDMA)].push_back({Command::RDA,  1, TS(nCWL) + TS(nBL) + nWTR, true});
  t[int(Command::WSMA)].push_back({Command::RD,   1, TS(nCWL) + TS(nBL) + nWTR, true});
  t[int(Command::WSMA)].push_back({Command::RDA,  1, TS(nCWL) + TS(nBL) + nWTR, true});

  t[int(Command::WOMA)].push_back({Command::ACT,  1, TS(nRC)});
  t[int(Command::WDMA)].push_back({Command::ACT,  1, TS(nRC)});
  t[int(Command::WSMA)].push_back({Command::ACT,  1, TS(nRC)});

}

uint64_t GDDR5::get_nCK_JEDEC_int(float t_ns, int tCK_ps)
{
  // Turn timing in nanosecond to picosecond
  uint64_t t_ps = t_ns * 1000;

  // Apply correction factor 974
  uint64_t nCK = ((t_ps * 1000 / tCK_ps) + 974) / 1000;
  return nCK;
}

#undef TS