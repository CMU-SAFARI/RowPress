#ifndef __DDR5_H
#define __DDR5_H

#include "DRAM.h"
#include "Request.h"
#include "Config.h"
#include <vector>
#include <functional>

namespace Ramulator
{

class DDR5
{
  public:
    DDR5(const YAML::Node& config);

    /// The name of the DRAM standard.
    inline static const std::string standard_name = "DDR5";
    /// A suffix string to distinguish between different instances of the standard.
    std::string suffix;

    int prefetch_size = 16;
    int channel_width = 32;

    /************************************************
     *                Organization
     ***********************************************/

    /// The enumeration of different levels in the hierarchy of the memory standard.
    enum class Level : int {Channel, Rank, BankGroup, Bank, Row, Column, MAX};
    inline static const std::string level_str[int(Level::MAX)] = {"Ch", "Ra", "Bg", "Ba", "Ro", "Co"};

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

    /// The enumeration of the refresh mode of the memory.
    enum class RefreshMode : int
    {
        Refresh_1X,
        Refresh_2X,
        Refresh_4X,
        MAX
    } refresh_mode = RefreshMode::Refresh_1X;


    /************************************************
     *                  Commands
     ***********************************************/

    /// The enumeration of different commands in the memory standard.
    enum class Command : int
    {
      ACT,    PRE,    PREab,  PREsb,
      RD,     WR,     RDA,    WRA,    WRP,  WRPA,
      REFab,  REFsb,  RFMab,  RFMsb,  PDE,  PDX,  SRE, SRX,
      MAX
    };
    inline static const std::string command_str[int(Command::MAX)] =
    {
      "ACT",    "PRE",   "PREab",  "PREsb",
      "RD",     "WR",    "RDA",     "WRA",    "WRP",  "WRPA",
      "REFab",  "REFsb", "RFMab",   "RFMsb",  "PDE",  "PDX",  "SRE", "SRX"
    };

    /// A lookup table of the scope of each command.
    Level scope[int(Command::MAX)] =
    {
      Level::Row,    Level::Bank,   Level::Rank,   Level::Bank,
      Level::Column, Level::Column, Level::Column, Level::Column,     Level::Column, Level::Column,
      Level::Rank,   Level::Bank,   Level::Rank,   Level::BankGroup,  Level::Rank,   Level::Rank,   Level::Rank,   Level::Rank
    };

    /// A lookup table of which final command each kind of memory request is translated into.
    Command translate[int(Request::Type::MAX)] =
    {
      Command::RD,    Command::WR,
      Command::REFab, Command::REFsb, 
      Command::RFMab, Command::RFMsb, 
      Command::SRE,
      Command::PDE,
      Command::ACT, 
    };

    /// All the states in the state machine of the memory standard.
    enum class State : int
    {
      Opened, Closed, PowerUp, ActPowerDown, PrePowerDown, SelfRefresh, MAX
    };
    /// A lookup table of the initial states of all the levels.
    const State start[int(Level::MAX)] =
    {
      State::MAX, State::PowerUp, State::MAX, State::Closed, State::Closed, State::MAX
    };

    /// A lookup table of functions that returns the prerequisite of each command at each level.
    std::function<Command(DRAM<DDR5>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    /// A lookup table of functions that determines whether a command can hit an opened row.
    std::function<bool(DRAM<DDR5>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];

    /// A lookup table of functions that determines whether a node is open for the command.
    std::function<bool(DRAM<DDR5>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];

    /// A lookup table of functions that models the state changes performed by the commands.
    std::function<void(DRAM<DDR5>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    #if defined(RAMULATOR_POWER)
    /// A lookup table of functions that updates the power modeler by the commands.
    std::function<void(DRAM<DDR5>*)> power_lambda[int(Level::MAX)][int(Command::MAX)];
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
        case int(Command::RD):  [[fallthrough]];
        case int(Command::RDA): [[fallthrough]];
        case int(Command::WR):  [[fallthrough]];
        case int(Command::WRA): [[fallthrough]];
        case int(Command::WRP): [[fallthrough]];
        case int(Command::WRPA):
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
        case int(Command::WRA):   [[fallthrough]];
        case int(Command::PRE):   [[fallthrough]];
        case int(Command::PREab): [[fallthrough]];
        case int(Command::PREsb): 
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
        case int(Command::REFsb): [[fallthrough]];
        case int(Command::RFMab): [[fallthrough]];
        case int(Command::RFMsb):
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
      nBL, nCL, nRCD, nRP, nRAS, nRC, nWR, nRTP, nCWL, 
      nPPD,
      nCCDS, nCCDL, nCCDLwr, nCCDLwr2, 
      nRRDS, nRRDL,
      nWTRS, nWTRL,
      nFAW,
      nRFC, nRFCsb, nREFI,
      nRFM, nREFSBRD,
      nPD, nXP, nCKESR, nXS,
      // Rank-to-Rank Switching delay, determined by both the DRAM and the memory controller.
      nRTRS,
      tCK_ps,
      MAX
    };
    inline static const std::string timingcons_str[int(TimingCons::MAX)] = 
    {
      "rate", 
      "nBL", "nCL", "nRCD", "nRP", "nRAS", "nRC", "nWR", "nRTP", "nCWL",
      "nPPD",
      "nCCDS", "nCCDL", "nCCDLwr", "nCCDLwr2", 
      "nRRDS", "nRRDL",
      "nWTRS", "nWTRL",
      "nFAW",
      "nRFC", "nRFCsb", "nREFI",
      "nRFM", "nREFSBRD",
      "nPD", "nXP", "nCKESR", "nXS",
      "nRTRS",
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
      DDR5_8Gb_x4,   DDR5_8Gb_x8,   DDR5_8Gb_x16,
      MAX
    };
    inline static const std::map<std::string, enum DDR5::OrgPreset> org_presets_map =
    {
      {"DDR5_8Gb_x4", DDR5::OrgPreset::DDR5_8Gb_x4}, {"DDR5_8Gb_x8", DDR5::OrgPreset::DDR5_8Gb_x8}, {"DDR5_8Gb_x16", DDR5::OrgPreset::DDR5_8Gb_x16},
    };
    inline static const OrgEntry org_presets[int(OrgPreset::MAX)] =
    {
      {8<<10,  4, {0, 0, 4, 4, 1<<17, 1<<10}}, {8<<10,  8, {0, 0, 4, 4, 1<<16, 1<<10}}, {8<<10, 16, {0, 0, 2, 4, 1<<16, 1<<10}}
    };

    int get_density_index(int density_Mb)
    {
      switch (density_Mb)
      {
        case 8192:  return 0;
        case 16384: return 1;
        default:    return -1;
      }
    }

    int get_dq_index(int dq)
    {
      switch (dq)
      {
        case 4:  return 0;
        case 8:  return 1;
        case 16: return 2;
        default: return -1;
      }
    }

    /// The enumeration of speedbin presets. See Table 107-113 in JESD79-4
    enum class SpeedPreset : int
    {
      DDR5_3200A,
      MAX
    };
    inline static const std::map<std::string, enum DDR5::SpeedPreset> speed_presets_map =
    {
      {"DDR5_3200A", DDR5::SpeedPreset::DDR5_3200A},
    };
    inline static const int speed_presets[int(SpeedPreset::MAX)][int(TimingCons::MAX)] =
    {
    // rate   nBL  nCL  nRCD  nRP   nRAS  nRC   nWR  nRTP nCWL nPPD nCCDS nCCDL nCCDLwr nCCDLwr2 nRRDS nRRDL nWTRS nWTRL nFAW  nRFC nRFCsb nREFI nPD nXP nCKESR nXS, nRTRS, tCK_ps
      {3200,   8,  22,  22,   22,   52,   74,   48,   12,  20,  2,    4,    8,     32,      16,     -1,   -1,    4,    16,  -1,  -1,    -1,   -1,   8,  10,   9,  -1,   -1,    625},   // 3200A
    }; 

    int get_rate_index(int rate)
    {
      switch (rate)
      {
        case 3200:  return 0;
        default:    return -1;
      }
    }


    /************************************************
     * Tables for secondary timings determined by the
     * frequency, density, and DQ width in the JEDEC
     * standard (e.g., Table 169-170, JESD79-4C).
     ***********************************************/

    static constexpr int RRDS_TABLE[3][1] =
    {
    // 3200
      { 8 },   // x4
      { 8 },   // x8
      { 8 },   // x16
    };
    static constexpr int RRDL_TABLE[3][7] =
    {
    // 3200
      { 8 },   // x4
      { 8 },   // x8
      { 8 },   // x16
    };
    static constexpr int FAW_TABLE[3][7] =
    {
    // 3200
      { 32 },   // x4
      { 32 },   // x8
      { 40 },   // x16
    };

    // tRFC table (unit is nanosecond!)
    static constexpr int RFC_TABLE[int(RefreshMode::MAX)][4] =
    {
    //  2Gb   4Gb   8Gb  16Gb
      { 160,  260,  360,  550}, // Normal refresh (tRFC1)
      { 110,  160,  260,  350}, // FGR 2x (tRFC2)
      { 90,   110,  160,  260}, // FGR 4x (tRFC4)
    };
    // tREFI(base) table (unit is nanosecond!)
    static constexpr int REFI_BASE_TABLE[4] =
    //  2Gb   4Gb   8Gb  16Gb
    {   3900, 3900, 3900, 3900 };

    // tRTRS = 4.5ns (W. Wang et al., "DraMon: Predicting memory bandwidth usage of multi-threaded programs with high accuracy and low overhead," in HPCA'14)
    static constexpr float tRTRS = 4.5f;

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

#endif /*__DDR5_H*/
