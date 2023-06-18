#ifndef __GDDR5_H
#define __GDDR5_H

#include "DRAM.h"
#include "Request.h"
#include "Config.h"
#include <vector>
#include <functional>

namespace Ramulator
{

class GDDR5
{
  public:
    GDDR5(const YAML::Node& config);

    /// The name of the DRAM standard.
    inline static const std::string standard_name = "GDDR5";
    /// A suffix string to distinguish between different instances of the standard.
    std::string suffix;

    int prefetch_size = 8;
    int channel_width = 32;

    /// Depending on implementation, certain GDDR5 chips can operate as if bankgroups are disabled
    bool bankgroup_enabled = false;

    /************************************************
     *                Organization
     ***********************************************/

    /// The enumeration of different levels in the hierarchy of the memory standard.
    enum class Level : int {Channel, BankGroup, Bank, Row, Column, MAX};
    inline static const std::string level_str[int(Level::MAX)] = {"Ch", "Bg", "Ba", "Ro", "Co"};

    /// A struct that stores the organization hierarchy of the memory.
    struct OrgEntry
    {
      /// The density of the chip in Mb.
      uint64_t density;
      /// The DQ width.
      int dq;
      /// The size of different levels in the hierarchy.
      int count[int(Level::MAX)];
    } org_entry;


    /************************************************
     *                  Commands
     ***********************************************/

    /// The enumeration of different commands in the memory standard.
    enum class Command : int
    {
      ACT,    PRE,    PREab,
      RD,     WOM,    WDM,    WSM,  RDA,  WOMA,   WDMA,   WSMA,
      REFab,  REFpb,  PDE,    PDX,  SRE,  SRX,
      MAX
    };
    inline static const std::string command_str[int(Command::MAX)] =
    {
      "ACT",    "PRE",   "PREab",
      "RD",     "WOM",   "WDM",   "WSM",  "RDA", "WOMA",  "WDMA", "WSMA"
      "REFab",  "REFpb", "PDE",   "PDX",  "SRE", "SRX"
    };

    /// A lookup table of the scope of each command.
    Level scope[int(Command::MAX)] =
    {
      Level::Row,    Level::Bank,   Level::Channel,
      Level::Column, Level::Column, Level::Column, Level::Column, Level::Column, Level::Column, Level::Column, Level::Column,
      Level::Channel,   Level::Bank,   Level::Channel,   Level::Channel,   Level::Channel,   Level::Channel
    };

    /// A lookup table of which final command each kind of memory request is translated into.
    Command translate[int(Request::Type::MAX)] =
    {
      Command::RD,    Command::WOM,
      Command::REFab, Command::REFpb, 
      Command::MAX,   Command::MAX, 
      Command::SRE,
      Command::PDE, 
    };

    /// All the states in the state machine of the memory standard.
    enum class State : int
    {
      Opened, Closed, PowerUp, ActPowerDown, PrePowerDown, SelfRefresh, MAX
    };
    /// A lookup table of the initial states of all the levels.
    const State start[int(Level::MAX)] =
    {
      State::PowerUp, State::MAX, State::Closed, State::Closed, State::MAX
    };

    /// A lookup table of functions that returns the prerequisite of each command at each level.
    std::function<Command(DRAM<GDDR5>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    /// A lookup table of functions that determines whether a command can hit an opened row.
    std::function<bool(DRAM<GDDR5>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];

    /// A lookup table of functions that determines whether a node is open for the command.
    std::function<bool(DRAM<GDDR5>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];

    /// A lookup table of functions that models the state changes performed by the commands.
    std::function<void(DRAM<GDDR5>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    #if defined(RAMULATOR_POWER)
    /// A lookup table of functions that updates the power modeler by the commands.
    std::function<void(DRAM<GDDR5>*)> power_lambda[int(Level::MAX)][int(Command::MAX)];
    #endif
    
    /// Returns true if the given command can open a row.
    bool is_opening(Command cmd)
    {
      switch(int(cmd))
      {
        case int(Command::ACT):
          return true;
        default:
          return false;
      }
    }

    /// Returns true if the given command is accessing a row.
    bool is_accessing(Command cmd)
    {
      switch(int(cmd))
      {
        case int(Command::RD):    [[fallthrough]];
        case int(Command::RDA):   [[fallthrough]];
        case int(Command::WOM):   [[fallthrough]];
        case int(Command::WOMA):  [[fallthrough]];
        case int(Command::WDM):   [[fallthrough]];
        case int(Command::WDMA):  [[fallthrough]];
        case int(Command::WSM):   [[fallthrough]];
        case int(Command::WSMA):  //[[fallthrough]];
          return true;
        default:
          return false;
      }
    }

    /// Returns true if the given command is closing a row.
    bool is_closing(Command cmd)
    {
      switch(int(cmd))
      {
        case int(Command::RDA):   [[fallthrough]];
        case int(Command::WOMA):  [[fallthrough]];
        case int(Command::WDMA):  [[fallthrough]];
        case int(Command::WSMA):  [[fallthrough]];
        case int(Command::PRE):   [[fallthrough]];
        case int(Command::PREab): //[[fallthrough]];
          return true;
        default:
          return false;
      }
    }

    /// Returns true if the given command is refreshing.
    bool is_refreshing(Command cmd)
    {
      switch(int(cmd))
      {
        case int(Command::REFab): [[fallthrough]];
        case int(Command::REFpb): //[[fallthrough]];
          return true;
        default:
          return false;
      }
    }

    /************************************************
     *                    Timing
     ***********************************************/
    enum class TimingCons : int
    {
      rate, 
      nBL, nCL, nRCDrd, nRCDwr, nRP, nRAS, nRC, nWR, nRTPS, nRTPL, nCWL, 
      nPPD,
      nCCDS, nCCDL,
      nRRDS, nRRDL,
      nWTRS, nWTRL,
      nRTW,
      nFAW, n32AW,
      nRFC, nRFCpb, nREFI, nREFIpb, nRREFD,
      nPD, nXP, nCKESR, nXS,
      tCK_ps,
      MAX
    };
    inline static const std::string timingcons_str[int(TimingCons::MAX)] = 
    {
      "rate", 
      "nBL", "nCL", "nRCDrd", "nRCDwr", "nRP", "nRAS", "nRC", "nWR", "nRTPS", "nRTPL", "nCWL", 
      "nPPD",
      "nCCDS", "nCCDL",
      "nRRDS", "nRRDL",
      "nWTRS", "nWTRL",
      "nRTW",
      "nFAW", "n32AW",
      "nRFC", "nRFCpb", "nREFI", "nREFIpb", "nRREFD",
      "nPD", "nXP", "nCKESR", "nXS",
      "tCK_ps"
    };

    // Array for all timing constraints
    int speed_entry[int(TimingCons::MAX)];

    /// The total latency of transferring a cache line after the READ command is issued (i.e., nCL + nBL).
    int read_latency;

    /// A struct that defines a timing constraint (e.g., nRCD) entry in the timing table.
    struct TimingEntry
    {
      /// The command that the timing constraint is constraining.
      Command cmd;
      ///
      int dist;
      /// The value of the timing constraint (in number of cycles).
      int val;
      /// Whether this timing constraint is affecting siblings in the same level.
      bool sibling = false;
    };

    /**
     * \brief The timing table that stores all the timing constraints for all levels in the hierarchy and all commands.
     *
     * \details Each element in the table is a vector of TimingEntry that stores the timing constraints for this command at this level.
     * A TimingEntry element in the vector essentially regulates the mimimum number of cycles between the command and the command stored in this TimingEntry at this level.
     * For example, the timing constraint nRCD should be modeled by a TimingEntry {Command::RD, 1, nRCD, false} stored in the vector at timing[Level::Bank][Command::ACT].
     */
    std::vector<TimingEntry> timing[int(Level::MAX)][int(Command::MAX)];

  protected:  
    /************************************************
     *                  Presets
     ***********************************************/

    /// The enumeration of organization presets.
    enum class OrgPreset : int
    {
      GDDR5_512Mb_x16, GDDR5_512Mb_x32,
      GDDR5_1Gb_x16,   GDDR5_1Gb_x32,
      GDDR5_2Gb_x16,   GDDR5_2Gb_x32,
      GDDR5_4Gb_x16,   GDDR5_4Gb_x32,
      GDDR5_8Gb_x16,   GDDR5_8Gb_x32,
      MAX
    };
    inline static const std::map<std::string, enum GDDR5::OrgPreset> org_presets_map =
    {
      {"GDDR5_512Mb_x16", GDDR5::OrgPreset::GDDR5_512Mb_x16}, {"GDDR5_512Mb_x32", GDDR5::OrgPreset::GDDR5_512Mb_x32},
      {"GDDR5_1Gb_x16",   GDDR5::OrgPreset::GDDR5_1Gb_x16},   {"GDDR5_1Gb_x32",   GDDR5::OrgPreset::GDDR5_1Gb_x32},
      {"GDDR5_2Gb_x16",   GDDR5::OrgPreset::GDDR5_2Gb_x16},   {"GDDR5_2Gb_x32",   GDDR5::OrgPreset::GDDR5_2Gb_x32},
      {"GDDR5_4Gb_x16",   GDDR5::OrgPreset::GDDR5_4Gb_x16},   {"GDDR5_4Gb_x32",   GDDR5::OrgPreset::GDDR5_4Gb_x32},
      {"GDDR5_8Gb_x16",   GDDR5::OrgPreset::GDDR5_8Gb_x16},   {"GDDR5_8Gb_x32",   GDDR5::OrgPreset::GDDR5_8Gb_x32},
    };
    inline static const OrgEntry org_presets[int(OrgPreset::MAX)] =
    {
      {1<<9,  16, {0, 4, 2, 1<<12, 1<<(7+3)}}, {1<<9,  32, {0, 4, 2, 1<<12, 1<<(6+3)}},
      {1<<10, 16, {0, 4, 4, 1<<12, 1<<(7+3)}}, {1<<10, 32, {0, 4, 4, 1<<12, 1<<(6+3)}},
      {2<<10, 16, {0, 4, 4, 1<<13, 1<<(7+3)}}, {2<<10, 32, {0, 4, 4, 1<<13, 1<<(6+3)}},
      {4<<10, 16, {0, 4, 4, 1<<14, 1<<(7+3)}}, {2<<10, 32, {0, 4, 4, 1<<14, 1<<(6+3)}},
      {8<<10, 16, {0, 4, 4, 1<<14, 1<<(8+3)}}, {8<<10, 32, {0, 4, 4, 1<<14, 1<<(7+3)}}
    };

    int get_density_index(int density_Mb)
    {
      switch (density_Mb)
      {
        case 512:   return 0;
        case 1024:  return 1;
        case 2048:  return 2;
        case 4096:  return 3;
        case 8192:  return 4;
        default:    return -1;
      }
    }

    int get_dq_index(int dq)
    {
      switch (dq)
      {
        case 16: return 0;
        case 32: return 1;
        default: return -1;
      }
    }

    /// The enumeration of speedbin presets. See Table 107-113 in JESD79-4
    enum class SpeedPreset : int
    {
      GDDR5_4000,
      GDDR5_5000,
      GDDR5_6000,
      MAX
    };
    inline static const std::map<std::string, enum GDDR5::SpeedPreset> speed_presets_map =
    {
      {"GDDR5_4000", GDDR5::SpeedPreset::GDDR5_4000},
      {"GDDR5_5000", GDDR5::SpeedPreset::GDDR5_5000},
      {"GDDR5_6000", GDDR5::SpeedPreset::GDDR5_6000},
    };
    inline static const int speed_presets[int(SpeedPreset::MAX)][int(TimingCons::MAX)] =
    // From Hynix 2Gb (64Mx32) GDDR5 SGRAM H5GQ2H24AFR Datasheet
    {
    // rate   nBL  nCL  nRCDrd  nRCDwr  nRP   nRAS  nRC   nWR  nRTPS  nRTPL nCWL nPPD nCCDS nCCDL nRRDS nRRDL nWTRS nWTRL nRTW nFAW n32AW  nRFC nRFCpb nREFI nREFIpb nPD nXP nCKESR nXS tCK_ps
      {4000,   2,   6,    14,     10,   12,   28,   40,   12,    2,     2,   3,   2,    2,    3,    6,    6,    6,    6,  -1,   23,  184,   -1,   -1,    -1,    -1,  10,  10,  10,  -1,   1000},  // 5000
      {5000,   2,   6,    18,     13,   15,   35,   50,   15,    2,     2,   3,   2,    2,    3,    7,    7,    8,    8,  -1,   29,  230,   -1,   -1,    -1,    -1,  12,  12,  12,  -1,   800},   // 4000
      {6000,   2,   6,    21,     15,   18,   42,   60,   18,    2,     2,   3,   2,    2,    3,    9,    9,    9,    9,  -1,   35,  276,   -1,   -1,    -1,    -1,  16,  16,  16,  -1,   625},   // 6000
    }; 

    int get_rate_index(int rate)
    {
      switch (rate)
      {
        case 4000:  return 0;
        case 5000:  return 1;
        case 6000:  return 2;
        default:    return -1;
      }
    }

    // tRFC table (unit is nanosecond!)
    static constexpr int RFC_TABLE[5] =
    //  512Mb 1Gb   2Gb   4Gb   8Gb  
      {  -1,  -1,    65,  -1,   -1 };

    // tREFI(base) table (unit is nanosecond!)
    static constexpr int REFI_BASE_TABLE[2][5] =
    {
    //  512Mb 1Gb   2Gb   4Gb   8Gb  
      {  -1,  -1,   3900, -1,   -1 },   // 8k rows
      {  -1,  -1,   1900, -1,   -1 },   // 16k rows
    };


    const YAML::Node& config;

    void init_org(const YAML::Node&);
    void init_speed(const YAML::Node&);

    void init_lambda();
    void init_prereq();
    // SAUGATA: added function to check for row hits
    void init_rowhit();
    void init_rowopen();
    void init_timing_table();

    uint64_t get_nCK_JEDEC_int(float, int);
  
};

} /*namespace Ramulator*/

#endif /*__GDDR5_H*/
