#ifndef __DDR4_H
#define __DDR4_H

#include "DRAM.h"
#include "Request.h"
#include "Config.h"
#include <vector>
#include <functional>

namespace Ramulator
{

class DDR4
{
  public:
    DDR4(const YAML::Node& config);

    /// The name of the DRAM standard.
    inline static const std::string standard_name = "DDR4";
    /// A suffix string to distinguish between different instances of the standard.
    std::string suffix;

    int prefetch_size = 8;
    int channel_width = 64;

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
      ACT, PRE, PREab,
      RD,  WR,  RDA,  WRA,
      REF, PDE, PDX,  SRE, SRX,
      MAX
    };
    inline static const std::string command_str[int(Command::MAX)] =
    {
      "ACT", "PRE", "PREab",
      "RD",  "WR",  "RDA",  "WRA",
      "REF", "PDE", "PDX",  "SRE", "SRX"
    };

    /// A lookup table of the scope of each command.
    Level scope[int(Command::MAX)] =
    {
      Level::Row,    Level::Bank,   Level::Rank,
      Level::Column, Level::Column, Level::Column, Level::Column,
      Level::Rank,   Level::Rank,   Level::Rank,   Level::Rank,   Level::Rank
    };

    /// A lookup table of which final command each kind of memory request is translated into.
    Command translate[int(Request::Type::MAX)] =
    {
      Command::RD,  Command::WR,
      Command::REF, Command::REF, 
      Command::REF, Command::REF,
      Command::SRE,
      Command::PDE,
      Command::ACT
    };

    /// All the states in the state machine of the memory standard.
    enum class State : int
    {
      Opened, Closed, ClosedPending, PowerUp, ActPowerDown, PrePowerDown, SelfRefresh, MAX
    };
    /// A lookup table of the initial states of all the levels.
    const State start[int(Level::MAX)] =
    {
      State::MAX, State::PowerUp, State::MAX, State::Closed, State::Closed, State::MAX
    };

    /// A lookup table of functions that returns the prerequisite of each command at each level.
    std::function<Command(DRAM<DDR4>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    /// A lookup table of functions that determines whether a command can hit an opened row.
    std::function<bool(DRAM<DDR4>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];

    /// A lookup table of functions that determines whether a node is open for the command.
    std::function<bool(DRAM<DDR4>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];

    /// A lookup table of functions that models the state changes performed by the commands.
    std::function<void(DRAM<DDR4>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    #if defined(RAMULATOR_POWER)
    enum class Voltage : int
    {
      VDD, VPP, MAX
    };
    inline static const std::string voltage_str[int(Voltage::MAX)] =
    {
      "VDD", "VPP"
    };
    double voltage_entry[int(Voltage::MAX)] =
    {
      1.2, 2.5
    };

    enum class Current : int
    {
      IDD0,  IPP0,
      IDD2N, IPP2N,   
      IDD3N, IPP3N,
      IDD4W, IPP4W,
      IDD4R, IPP4R,
      IDD5B, IPP5B,
      MAX
    };
    inline static const std::string current_str[int(Current::MAX)] =
    {
      "IDD0" , "IPP0" ,
      "IDD2N", "IPP2N",   
      "IDD3N", "IPP3N",
      "IDD4W", "IPP4W",
      "IDD4R", "IPP4R",
      "IDD5B", "IPP5B"
    };
    double current_entry[int(Current::MAX)];

    /// A lookup table of functions that updates the power modeler by the commands.
    std::function<void(DRAM<DDR4>*)> power_lambda[int(Level::MAX)][int(Command::MAX)];
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
        case int(Command::WR):  [[fallthrough]];
        case int(Command::RDA): [[fallthrough]];
        case int(Command::WRA):
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
        case int(Command::PRE): [[fallthrough]];
        case int(Command::PREab):
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
        case int(Command::REF):
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
      nCCDS, nCCDL,
      nRRDS, nRRDL,
      nWTRS, nWTRL,
      nFAW,
      nRFC, nREFI,
      nPD, nXP, nCKESR, nXS,
      // Rank-to-Rank Switching delay, determined by both the DRAM and the memory controller.
      nRTRS,
      tCK_ps,
      MAX
    };
    inline static const std::string timingcons_str[int(TimingCons::MAX)] = 
    {
      "rate", 
      "nBL", "nCL", "nRCD", "nRP", "nRAS", "nRC", "nWR", "nRTP", "nCWL"
      "nCCDS", "nCCDL",
      "nRRDS", "nRRDL",
      "nWTRS", "nWTRL",
      "nFAW",
      "nRFC", "nREFI",
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
     * For example, the timing constraint nRCD should be modeled by a TimingEntry {Command::RCD, 1, nRCD, false} stored in the vector at timing[Level::Bank][Command::ACT].
     */
    std::vector<TimingEntry> timing[int(Level::MAX)][int(Command::MAX)];

  protected:  
    /************************************************
     *                  Presets
     ***********************************************/

    /// The enumeration of organization presets.
    enum class OrgPreset : int
    {
      DDR4_2Gb_x4,   DDR4_2Gb_x8,   DDR4_2Gb_x16,
      DDR4_4Gb_x4,   DDR4_4Gb_x8,   DDR4_4Gb_x16,
      DDR4_8Gb_x4,   DDR4_8Gb_x8,   DDR4_8Gb_x16,
      DDR4_16Gb_x4,  DDR4_16Gb_x8,  DDR4_16Gb_x16,
      MAX
    };
    inline static const std::map<std::string, enum DDR4::OrgPreset> org_presets_map =
    {
      {"DDR4_2Gb_x4", DDR4::OrgPreset::DDR4_2Gb_x4},    {"DDR4_2Gb_x8", DDR4::OrgPreset::DDR4_2Gb_x8},    {"DDR4_2Gb_x16", DDR4::OrgPreset::DDR4_2Gb_x16},
      {"DDR4_4Gb_x4", DDR4::OrgPreset::DDR4_4Gb_x4},    {"DDR4_4Gb_x8", DDR4::OrgPreset::DDR4_4Gb_x8},    {"DDR4_4Gb_x16", DDR4::OrgPreset::DDR4_4Gb_x16},
      {"DDR4_8Gb_x4", DDR4::OrgPreset::DDR4_8Gb_x4},    {"DDR4_8Gb_x8", DDR4::OrgPreset::DDR4_8Gb_x8},    {"DDR4_8Gb_x16", DDR4::OrgPreset::DDR4_8Gb_x16},
      {"DDR4_16Gb_x4", DDR4::OrgPreset::DDR4_16Gb_x4},  {"DDR4_16Gb_x8", DDR4::OrgPreset::DDR4_16Gb_x8},  {"DDR4_16Gb_x16", DDR4::OrgPreset::DDR4_16Gb_x16},
    };
    inline static const OrgEntry org_presets[int(OrgPreset::MAX)] =
    {
      {2<<10,   4, {0, 0, 4, 4, 1<<15, 1<<10}}, {2<<10,   8, {0, 0, 4, 4, 1<<14, 1<<10}},   {2<<10,   16, {0, 0, 2, 4, 1<<14, 1<<10}},
      {4<<10,   4, {0, 0, 4, 4, 1<<16, 1<<10}}, {4<<10,   8, {0, 0, 4, 4, 1<<15, 1<<10}},   {4<<10,   16, {0, 0, 2, 4, 1<<15, 1<<10}},
      {8<<10,   4, {0, 0, 4, 4, 1<<17, 1<<10}}, {8<<10,   8, {0, 0, 4, 4, 1<<16, 1<<10}},   {8<<10,   16, {0, 0, 2, 4, 1<<16, 1<<10}},
      {16<<10,  4, {0, 0, 4, 4, 1<<18, 1<<10}}, {16<<10,  8, {0, 0, 4, 4, 1<<17, 1<<10}},   {16<<10,  16, {0, 0, 2, 4, 1<<17, 1<<10}}
    };

    int get_density_index(int density_Mb)
    {
      switch (density_Mb)
      {
        case 2048:  return 0;
        case 4096:  return 1;
        case 8192:  return 2;
        case 16384: return 3;
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
      DDR4_1600J,   DDR4_1600K,   DDR4_1600L,
      DDR4_1866L,   DDR4_1866M,   DDR4_1866N,
      DDR4_2133N,   DDR4_2133P,   DDR4_2133R,
      DDR4_2400P,   DDR4_2400R,   DDR4_2400U,   DDR4_2400T,
      DDR4_2666T,   DDR4_2666U,   DDR4_2666V,   DDR4_2666W,
      DDR4_2933V,   DDR4_2933W,   DDR4_2933Y,   DDR4_2933AA,
      DDR4_3200W,   DDR4_3200AA,  DDR4_3200AC,
      MAX
    };
    inline static const std::map<std::string, enum DDR4::SpeedPreset> speed_presets_map =
    {
      {"DDR4_1600J", DDR4::SpeedPreset::DDR4_1600J},  {"DDR4_1600K",  DDR4::SpeedPreset::DDR4_1600K},   {"DDR4_1600L",  DDR4::SpeedPreset::DDR4_1600L},
      {"DDR4_1866L", DDR4::SpeedPreset::DDR4_1866L},  {"DDR4_1866M",  DDR4::SpeedPreset::DDR4_1866M},   {"DDR4_1866N",  DDR4::SpeedPreset::DDR4_1866N},
      {"DDR4_2133N", DDR4::SpeedPreset::DDR4_2133N},  {"DDR4_2133P",  DDR4::SpeedPreset::DDR4_2133P},   {"DDR4_2133R",  DDR4::SpeedPreset::DDR4_2133R},
      {"DDR4_2400P", DDR4::SpeedPreset::DDR4_2400P},  {"DDR4_2400R",  DDR4::SpeedPreset::DDR4_2400R},   {"DDR4_2400U",  DDR4::SpeedPreset::DDR4_2400U},   {"DDR4_2400T",  DDR4::SpeedPreset::DDR4_2400T},
      {"DDR4_2666T", DDR4::SpeedPreset::DDR4_2666T},  {"DDR4_2666U",  DDR4::SpeedPreset::DDR4_2666U},   {"DDR4_2666V",  DDR4::SpeedPreset::DDR4_2666V},   {"DDR4_2666W",  DDR4::SpeedPreset::DDR4_2666W},
      {"DDR4_2933V", DDR4::SpeedPreset::DDR4_2933V},  {"DDR4_2933W",  DDR4::SpeedPreset::DDR4_2933W},   {"DDR4_2933Y",  DDR4::SpeedPreset::DDR4_2933Y},   {"DDR4_2933AA", DDR4::SpeedPreset::DDR4_2933AA},
      {"DDR4_3200W", DDR4::SpeedPreset::DDR4_3200W},  {"DDR4_3200AA", DDR4::SpeedPreset::DDR4_3200AA},  {"DDR4_3200AC", DDR4::SpeedPreset::DDR4_3200AC},  
    };
    inline static const int speed_presets[int(SpeedPreset::MAX)][int(TimingCons::MAX)] =
    {
    // rate   nBL  nCL  nRCD  nRP   nRAS  nRC   nWR  nRTP nCWL nCCDS nCCDL nRRDS nRRDL nWTRS nWTRL nFAW  nRFC nREFI nPD nXP nCKESR nXS, nRTRS,  tCK_ps
      {1600,   4,  10,  10,   10,   28,   38,   12,   6,   9,    4,    5,   -1,   -1,    2,    6,   -1,  -1,   -1,   4,  5,    5,  -1,   -1,    1250},  // 1600J
      {1600,   4,  11,  11,   11,   28,   39,   12,   6,   9,    4,    5,   -1,   -1,    2,    6,   -1,  -1,   -1,   4,  5,    5,  -1,   -1,    1250},  // 1600K
      {1600,   4,  12,  12,   12,   28,   40,   12,   6,   9,    4,    5,   -1,   -1,    2,    6,   -1,  -1,   -1,   4,  5,    5,  -1,   -1,    1250},  // 1600L

      {1866,   4,  12,  12,   12,   32,   44,   14,   7,   10,   4,    5,   -1,   -1,    3,    7,   -1,  -1,   -1,   5,  6,    6,  -1,   -1,    1071},  // 1866L
      {1866,   4,  13,  13,   13,   32,   45,   14,   7,   10,   4,    5,   -1,   -1,    3,    7,   -1,  -1,   -1,   5,  6,    6,  -1,   -1,    1071},  // 1866M
      {1866,   4,  14,  14,   14,   32,   46,   14,   7,   10,   4,    5,   -1,   -1,    3,    7,   -1,  -1,   -1,   5,  6,    6,  -1,   -1,    1071},  // 1866N

      {2133,   4,  14,  14,   14,   36,   50,   16,   8,   11,   4,    6,   -1,   -1,    3,    8,   -1,  -1,   -1,   6,  7,    7,  -1,   -1,    937},   // 2133N
      {2133,   4,  15,  15,   15,   36,   51,   16,   8,   11,   4,    6,   -1,   -1,    3,    8,   -1,  -1,   -1,   6,  7,    7,  -1,   -1,    937},   // 2133P
      {2133,   4,  16,  16,   16,   36,   52,   16,   8,   11,   4,    6,   -1,   -1,    3,    8,   -1,  -1,   -1,   6,  7,    7,  -1,   -1,    937},   // 2133R

      {2400,   4,  15,  15,   15,   39,   54,   18,   9,   12,   4,    6,   -1,   -1,    3,    9,   -1,  -1,   -1,   6,  8,    7,  -1,   -1,    833},   // 2400P
      {2400,   4,  16,  16,   16,   39,   55,   18,   9,   12,   4,    6,   -1,   -1,    3,    9,   -1,  -1,   -1,   6,  8,    7,  -1,   -1,    833},   // 2400R
      {2400,   4,  17,  17,   17,   39,   56,   18,   9,   12,   4,    6,   -1,   -1,    3,    9,   -1,  -1,   -1,   6,  8,    7,  -1,   -1,    833},   // 2400U
      {2400,   4,  18,  18,   18,   39,   57,   18,   9,   12,   4,    6,   -1,   -1,    3,    9,   -1,  -1,   -1,   6,  8,    7,  -1,   -1,    833},   // 2400T

      {2666,   4,  17,  17,   17,   43,   60,   20,   10,  14,   4,    7,   -1,   -1,    4,    10,  -1,  -1,   -1,   7,  8,    8,  -1,   -1,    750},   // 2666T
      {2666,   4,  18,  18,   18,   43,   61,   20,   10,  14,   4,    7,   -1,   -1,    4,    10,  -1,  -1,   -1,   7,  8,    8,  -1,   -1,    750},   // 2666U
      {2666,   4,  19,  19,   19,   43,   62,   20,   10,  14,   4,    7,   -1,   -1,    4,    10,  -1,  -1,   -1,   7,  8,    8,  -1,   -1,    750},   // 2666V
      {2666,   4,  20,  20,   20,   43,   63,   20,   10,  14,   4,    7,   -1,   -1,    4,    10,  -1,  -1,   -1,   7,  8,    8,  -1,   -1,    750},   // 2666W

      {2933,   4,  19,  19,   19,   47,   66,   22,   11,  16,   4,    8,   -1,   -1,    4,    11,  -1,  -1,   -1,   8,  9,    9,  -1,   -1,    682},   // 2933V
      {2933,   4,  20,  20,   20,   47,   67,   22,   11,  16,   4,    8,   -1,   -1,    4,    11,  -1,  -1,   -1,   8,  9,    9,  -1,   -1,    682},   // 2933W
      {2933,   4,  21,  21,   21,   47,   68,   22,   11,  16,   4,    8,   -1,   -1,    4,    11,  -1,  -1,   -1,   8,  9,    9,  -1,   -1,    682},   // 2933Y
      {2933,   4,  22,  22,   22,   47,   69,   22,   11,  16,   4,    8,   -1,   -1,    4,    11,  -1,  -1,   -1,   8,  9,    9,  -1,   -1,    682},   // 2933AA

      {3200,   4,  20,  20,   20,   52,   72,   24,   12,  16,   4,    8,   -1,   -1,    4,    12,  -1,  -1,   -1,   8,  10,   9,  -1,   -1,    625},   // 3200W
      {3200,   4,  22,  22,   22,   52,   74,   24,   12,  16,   4,    8,   -1,   -1,    4,    12,  -1,  -1,   -1,   8,  10,   9,  -1,   -1,    625},   // 3200AA
      {3200,   4,  24,  24,   24,   52,   76,   24,   12,  16,   4,    8,   -1,   -1,    4,    12,  -1,  -1,   -1,   8,  10,   9,  -1,   -1,    625},   // 3200AC
    }; 

    int get_rate_index(int rate)
    {
      switch (rate)
      {
        case 1600:  return 0;
        case 1866:  return 1;
        case 2133:  return 2;
        case 2400:  return 3;
        case 2666:  return 4;
        case 2933:  return 5;
        case 3200:  return 6;
        default:    return -1;
      }
    }


    /************************************************
     * Tables for secondary timings determined by the
     * frequency, density, and DQ width in the JEDEC
     * standard (e.g., Table 169-170, JESD79-4C).
     ***********************************************/

    static constexpr int RRDS_TABLE[3][7] =
    {
    // 1600  1866  2133  2400  2666  2933  3200
      { 4,    4,    4,    4,    4,    4,    4},   // x4
      { 4,    4,    4,    4,    4,    4,    4},   // x8
      { 5,    5,    6,    7,    8,    8,    9},   // x16
    };
    static constexpr int RRDL_TABLE[3][7] =
    {
    // 1600  1866  2133  2400  2666  2933  3200
      { 5,    5,    6,    6,    7,    8,    8 },  // x4
      { 5,    5,    6,    6,    7,    8,    8 },  // x8
      { 6,    6,    7,    8,    9,    10,   11},  // x16
    };
    static constexpr int FAW_TABLE[3][7] =
    {
    // 1600  1866  2133  2400  2666  2933  3200
      { 16,   16,   16,   16,   16,   16,   16},  // x4
      { 20,   22,   23,   26,   28,   31,   34},  // x8
      { 28,   28,   32,   36,   40,   44,   48},  // x16
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
    {   7800, 7800, 7800, 7800 };

    // tRTRS = 4.5ns (W. Wang et al., "DraMon: Predicting memory bandwidth usage of multi-threaded programs with high accuracy and low overhead," in HPCA'14)
    static constexpr float tRTRS = 4.5f;

    const YAML::Node& config;

    void init_org(const YAML::Node&);
    void init_speed(const YAML::Node&);
    void init_lambda();
    void init_prereq();
    void init_rowhit();
    void init_rowopen();
    void init_timing_table();

    #if defined(RAMULATOR_POWER)
    void init_power_parameters(const YAML::Node& power_config);
    void init_power_lambda();
    #endif

    uint64_t get_nCK_JEDEC_int(float, int);
  
};

} /*namespace Ramulator*/

#endif /*__DDR4_H*/
