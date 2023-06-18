#ifndef __DDR3_H
#define __DDR3_H

#include "DRAM.h"
#include "Request.h"
#include "Config.h"
#include <vector>
#include <functional>

namespace Ramulator
{

class DDR3
{
  public:
    DDR3(const YAML::Node& config);

    /// The name of the DRAM standard.
    inline static const std::string standard_name = "DDR3";
    /// A suffix string to distinguish between different instances of the standard.
    std::string suffix;

    int prefetch_size = 8;
    int channel_width = 64;

    /************************************************
     *                Organization
     ***********************************************/

    /// The enumeration of different levels in the hierarchy of the memory standard.
    enum class Level : int {Channel, Rank, Bank, Row, Column, MAX};
    inline static const std::string level_str[int(Level::MAX)] = {"Ch", "Ra", "Ba", "Ro", "Co"};

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
      Command::REF, Command::PDE, Command::SRE
    };

    /// All the states in the state machine of the memory standard.
    enum class State : int
    {
      Opened, Closed, ClosedPending, PowerUp, ActPowerDown, PrePowerDown, SelfRefresh, MAX
    };
    /// A lookup table of the initial states of all the levels.
    const State start[int(Level::MAX)] =
    {
      State::MAX, State::PowerUp, State::Closed, State::Closed, State::MAX
    };

    /// A lookup table of functions that returns the prerequisite of each command at each level.
    std::function<Command(DRAM<DDR3>*, Command cmd, int)> prereq[int(Level::MAX)][int(Command::MAX)];

    /// A lookup table of functions that determines whether a command can hit an opened row.
    std::function<bool(DRAM<DDR3>*, Command cmd, int)> rowhit[int(Level::MAX)][int(Command::MAX)];

    /// A lookup table of functions that determines whether a node is open for the command.
    std::function<bool(DRAM<DDR3>*, Command cmd, int)> rowopen[int(Level::MAX)][int(Command::MAX)];

    /// A lookup table of functions that models the state changes performed by the commands.
    std::function<void(DRAM<DDR3>*, int)> lambda[int(Level::MAX)][int(Command::MAX)];

    #if defined(RAMULATOR_POWER)
    enum class Voltage : int
    {
      VDD, MAX
    };
    inline static const std::string voltage_str[int(Voltage::MAX)] =
    {
      "VDD"
    };
    double voltage_entry[int(Voltage::MAX)] =
    {
      1.5
    };

    enum class Current : int
    {
      IDD0, 
      IDD2N,   
      IDD3N,
      IDD4W,
      IDD4R,
      IDD5B,
      MAX
    };
    inline static const std::string current_str[int(Current::MAX)] =
    {
      "IDD0" ,
      "IDD2N",   
      "IDD3N",
      "IDD4W",
      "IDD4R",
      "IDD5B"
    };
    double current_entry[int(Current::MAX)];

    /// A lookup table of functions that updates the power modeler by the commands.
    std::function<void(DRAM<DDR3>*)> power_lambda[int(Level::MAX)][int(Command::MAX)];
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
      nCCD, 
      nRRD,
      nWTR,
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
      "nBL", "nCL", "nRCD", "nRP", "nRAS", "nRC", "nWR", "nRTP", "nCWL",
      "nCCD",
      "nRRD",
      "nWTR",
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
      DDR3_512Mb_x4,   DDR3_512Mb_x8,   DDR3_512Mb_x16,
      DDR3_1Gb_x4,     DDR3_1Gb_x8,     DDR3_1Gb_x16,
      DDR3_2Gb_x4,     DDR3_2Gb_x8,     DDR3_2Gb_x16,
      DDR3_4Gb_x4,     DDR3_4Gb_x8,     DDR3_4Gb_x16,
      DDR3_8Gb_x4,     DDR3_8Gb_x8,     DDR3_8Gb_x16,
      MAX
    };
    inline static const std::map<std::string, enum DDR3::OrgPreset> org_presets_map =
    {
      {"DDR3_512Mb_x4", DDR3::OrgPreset::DDR3_512Mb_x4}, {"DDR3_512Mb_x8", DDR3::OrgPreset::DDR3_512Mb_x8}, {"DDR3_512Mb_x16", DDR3::OrgPreset::DDR3_512Mb_x16},
      {"DDR3_1Gb_x4",   DDR3::OrgPreset::DDR3_1Gb_x4},   {"DDR3_1Gb_x8",   DDR3::OrgPreset::DDR3_1Gb_x8},   {"DDR3_2Gb_x16",   DDR3::OrgPreset::DDR3_1Gb_x16},
      {"DDR3_2Gb_x4",   DDR3::OrgPreset::DDR3_2Gb_x4},   {"DDR3_2Gb_x8",   DDR3::OrgPreset::DDR3_2Gb_x8},   {"DDR3_2Gb_x16",   DDR3::OrgPreset::DDR3_2Gb_x16},
      {"DDR3_4Gb_x4",   DDR3::OrgPreset::DDR3_4Gb_x4},   {"DDR3_4Gb_x8",   DDR3::OrgPreset::DDR3_4Gb_x8},   {"DDR3_4Gb_x16",   DDR3::OrgPreset::DDR3_4Gb_x16},
      {"DDR3_8Gb_x4",   DDR3::OrgPreset::DDR3_8Gb_x4},   {"DDR3_8Gb_x8",   DDR3::OrgPreset::DDR3_8Gb_x8},   {"DDR3_8Gb_x16",   DDR3::OrgPreset::DDR3_8Gb_x16},
    };
    inline static const OrgEntry org_presets[int(OrgPreset::MAX)] =
    {
      {1<<9 ,  4, {0, 0, 8, 1<<13, 1<<11}}, {1<<9 ,  8, {0, 0, 8, 1<<13, 1<<10}}, {1<<9 , 16, {0, 0, 8, 1<<12, 1<<10}},
      {1<<10,  4, {0, 0, 8, 1<<14, 1<<11}}, {1<<10,  8, {0, 0, 8, 1<<14, 1<<10}}, {1<<10, 16, {0, 0, 8, 1<<13, 1<<10}},
      {2<<10,  4, {0, 0, 8, 1<<15, 1<<11}}, {2<<10,  8, {0, 0, 8, 1<<15, 1<<10}}, {2<<10, 16, {0, 0, 8, 1<<14, 1<<10}},
      {4<<10,  4, {0, 0, 8, 1<<16, 1<<11}}, {4<<10,  8, {0, 0, 8, 1<<16, 1<<10}}, {4<<10, 16, {0, 0, 8, 1<<15, 1<<10}},
      {8<<10,  4, {0, 0, 8, 1<<16, 1<<12}}, {8<<10,  8, {0, 0, 8, 1<<16, 1<<11}}, {8<<10, 16, {0, 0, 8, 1<<16, 1<<10}}
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
        case 4:  return 0;
        case 8:  return 1;
        case 16: return 2;
        default: return -1;
      }
    }

    /// The enumeration of speedbin presets. See Table 107-113 in JESD79-4
    enum class SpeedPreset : int
    {
      DDR3_800D,  DDR3_800E,
      DDR3_1066E, DDR3_1066F, DDR3_1066G,
      DDR3_1333G, DDR3_1333H,
      DDR3_1600H, DDR3_1600J, DDR3_1600K,
      DDR3_1866K, DDR3_1866L,
      DDR3_2133L, DDR3_2133M,
      MAX
    };
    inline static const std::map<std::string, enum DDR3::SpeedPreset> speed_presets_map =
    {
      {"DDR3_800D",  DDR3::SpeedPreset::DDR3_800D},  {"DDR3_800E",  DDR3::SpeedPreset::DDR3_800E},
      {"DDR3_1066E", DDR3::SpeedPreset::DDR3_1066E}, {"DDR3_1066F", DDR3::SpeedPreset::DDR3_1066F}, {"DDR3_1066G", DDR3::SpeedPreset::DDR3_1066G},
      {"DDR3_1333G", DDR3::SpeedPreset::DDR3_1333G}, {"DDR3_1333H", DDR3::SpeedPreset::DDR3_1333H},
      {"DDR3_1600H", DDR3::SpeedPreset::DDR3_1600H}, {"DDR3_1600J", DDR3::SpeedPreset::DDR3_1600J}, {"DDR3_1600K", DDR3::SpeedPreset::DDR3_1600K},
      {"DDR3_1866K", DDR3::SpeedPreset::DDR3_1866K}, {"DDR3_1866L", DDR3::SpeedPreset::DDR3_1866L},
      {"DDR3_2133L", DDR3::SpeedPreset::DDR3_2133L}, {"DDR3_2133M", DDR3::SpeedPreset::DDR3_2133M}
    };
    inline static const int speed_presets[int(SpeedPreset::MAX)][int(TimingCons::MAX)] =
    {
    // rate   nBL  nCL  nRCD  nRP   nRAS  nRC   nWR  nRTP nCWL nCCD   nRRD  nWTR  nFAW  nRFC nREFI nPD nXP nCKESR nXS, nRTRS,  tCK_ps
      {800,    4,   5,   5,    5,    15,  20,    6,   4,   9,    4,    -1,    4,   -1,  -1,   -1,   3,  3,    4,  -1,   -1,    2500},  // 800D
      {800,    4,   6,   6,    6,    15,  21,    6,   4,   9,    4,    -1,    4,   -1,  -1,   -1,   3,  3,    4,  -1,   -1,    2500},  // 800E

      {1066,   4,   6,   6,    6,    20,  26,    8,   4,   9,    4,    -1,    4,   -1,  -1,   -1,   3,  4,    4,  -1,   -1,    1875},  // 1066E
      {1066,   4,   7,   7,    7,    20,  27,    8,   4,   9,    4,    -1,    4,   -1,  -1,   -1,   3,  4,    4,  -1,   -1,    1875},  // 1066F
      {1066,   4,   8,   8,    8,    20,  28,    8,   4,   9,    4,    -1,    4,   -1,  -1,   -1,   3,  4,    4,  -1,   -1,    1875},  // 1066G

      {1333,   4,   8,   8,    8,    24,  32,   10,   5,   9,    4,    -1,    5,   -1,  -1,   -1,   4,  4,    5,  -1,   -1,    1500},  // 1333G
      {1333,   4,   9,   9,    9,    24,  33,   10,   5,   9,    4,    -1,    5,   -1,  -1,   -1,   4,  4,    5,  -1,   -1,    1500},  // 1333H
      
      {1600,   4,   9,   9,    9,    28,  37,   12,   6,   9,    4,    -1,    6,   -1,  -1,   -1,   4,  5,    5,  -1,   -1,    1250},  // 1600H
      {1600,   4,  10,  10,   10,    28,  38,   12,   6,   9,    4,    -1,    6,   -1,  -1,   -1,   4,  5,    5,  -1,   -1,    1250},  // 1600J
      {1600,   4,  11,  11,   11,    28,  39,   12,   6,   9,    4,    -1,    6,   -1,  -1,   -1,   4,  5,    5,  -1,   -1,    1250},  // 1600K

      {1866,   4,  11,  11,   11,    32,  43,   14,   7,   9,    4,    -1,    7,   -1,  -1,   -1,   5,  6,    6,  -1,   -1,    1071},  // 1866K
      {1866,   4,  12,  12,   12,    32,  44,   14,   7,   9,    4,    -1,    7,   -1,  -1,   -1,   5,  6,    6,  -1,   -1,    1071},  // 1866L

      {2133,   4,  12,  12,   12,    36,  48,   16,   8,   9,    4,    -1,    8,   -1,  -1,   -1,   6,  7,    7,  -1,   -1,    937},   // 2133L
      {2133,   4,  13,  13,   13,    36,  49,   16,   8,   9,    4,    -1,    8,   -1,  -1,   -1,   6,  7,    7,  -1,   -1,    937},   // 2133M
    }; 

    int get_rate_index(int rate)
    {
      switch (rate)
      {
        case  800:  return 0;
        case 1066:  return 1;
        case 1333:  return 2;
        case 1600:  return 3;
        case 1866:  return 4;
        case 2133:  return 5;
        default:    return -1;
      }
    }


    /************************************************
     * Tables for secondary timings determined by the
     * frequency, density, and DQ width in the JEDEC
     * standard (e.g., Table 169-170, JESD79-4C).
     ***********************************************/

    static constexpr int RRD_TABLE[3][6] =
    {
    // 800   1066  1333  1600  1866  2133 
      { 4,    4,    4,    5,    5,    6},   // x4
      { 4,    4,    4,    5,    5,    6},   // x8
      { 4,    6,    5,    6,    6,    7},   // x16
    };

    static constexpr int FAW_TABLE[3][6] =
    {
    // 800   1066  1333  1600  1866  2133 
      { 16,   20,   20,   24,   26,   27},  // x4
      { 16,   20,   20,   24,   26,   27},  // x8
      { 20,   27,   30,   32,   33,   34},  // x16
    };

    // tRFC table (unit is nanosecond!)
    static constexpr int RFC_TABLE[5] =
    // 512Mb   1Gb   2Gb   4Gb   8Gb
    {   90,    110,  160,  260,  350};
    // tREFI(base) table (unit is nanosecond!)
    static constexpr int REFI_BASE_TABLE[5] =
    // 512Mb   1Gb   2Gb   4Gb   8Gb
    {  7800,   7800, 7800, 7800, 7800 };

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

#endif /*__DDR3_H*/
