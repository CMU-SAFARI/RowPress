#pragma once

#include <array>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <type_traits>

#include <spdlog/spdlog.h>

namespace RowPress
{
  enum class ExperimentType: uint
  {
    RH_BITFLIPS,
    RH_HCFIRST,
    RETENTION_TIME,
    RETENTION_FAILURE,
    ROW_MAPPING,
    RH_BITFLIPS_RAS_RATIO,
    ASYMMETRIC_ROWPRESS_BER,
    ASYMMETRIC_ROWPRESS_HCFIRST,
    TAGGON_MIN,
    MITIGATION_REF,
    MAX
  };
  static constexpr std::array<std::string_view, uint(ExperimentType::MAX)>
  experiment_type_names = 
  {
    "RowHammer Bitflips",
    "RowHammer HCFirst",
    "Retention Time",
    "Retention Failure",
    "Row Mapping",
    "RowHammer Bitflips (RAS Ratio)",
    "Asymmetric RowPress BER",
    "Asymmetric RowPress HCFIRST",
    "Min tAggON",
    "Mitigation Ref",
  };
  inline std::string_view enum_str(ExperimentType v)
  {
    return experiment_type_names[uint(v)];
  };

  enum class RowMapping: uint
  {
    SEQUENTIAL, 
    SAMSUNG_16,
    MI0,
    MAX
  };
  static constexpr std::array<std::string_view, uint(RowMapping::MAX)>
  row_mapping_names = 
  {
    "Sequential",
    "Samsung_16",
    "MI0",
  };
  inline std::string_view enum_str(RowMapping v)
  {
    return row_mapping_names[uint(v)];
  };

  enum class AccessPat: uint
  {
    SINGLE_SIDED, DOUBLE_SIDED,
    CUSTOM,
    MAX
  };
  static constexpr std::array<std::string_view, uint(AccessPat::MAX)>
  access_pat_names = 
  {
    "Single-Sided",
    "Double-Sided",
    "Custom",
  };
  inline std::string_view enum_str(AccessPat v)
  {
    return access_pat_names[uint(v)];
  };

  enum class RowType: uint
  {
    VICTIM, AGGRESSOR,
    PIVOT_VICTIM, PIVOT_AGGRESSOR,
    MAX
  };
  static constexpr std::array<std::string_view, uint(RowType::MAX)>
  row_type_names = 
  {
    "V",  "A",
    "PV", "PA",
  };
  inline std::string_view enum_str(RowType v)
  {
    return row_type_names[uint(v)];
  };

  template<typename T>
  inline std::string_view enum_str(T v)
  {
    if constexpr (std::is_enum_v<T>)
      static_assert("Enum string not defined for T!");
    else
      static_assert("T must be an enum in enum_name(T enum_v)!");

    return "INVALID";
  };

  enum class DataPat: uint
  {
    ZEROS, ONES, 
    CHECKERBOARD, CHECKERBOARD_I,
    COLUMNSTRIPE, COLUMNSTRIPE_I,
    ROWSTRIPE,    ROWSTRIPE_I,
    MAX
  };

  static const uint32_t data_patterns[uint(DataPat::MAX)][2] = 
  {
    {0x00000000, 0x00000000}, {0xFFFFFFFF, 0xFFFFFFFF},
    {0xAAAAAAAA, 0x55555555}, {0x55555555, 0xAAAAAAAA},
    {0x55555555, 0x55555555}, {0xAAAAAAAA, 0xAAAAAAAA},
    {0x00000000, 0xFFFFFFFF}, {0xFFFFFFFF, 0x00000000},
  };

  enum class RowCoverage: uint
  {
    FULL,
    SAMPLE_HMT,
    MAX
  };

  using CacheLine_t = std::array<uint32_t, 16>;
  struct Row_t
  {
    RowType row_type;
    uint row_address;
    int  row_offset;
    CacheLine_t cacheline;
  };

  class Pattern
  {
    public:
      std::vector<Row_t> rows;

      int pivot_id;
      int head_offset;
      int tail_offset;


    public:
      Pattern(const std::vector<Row_t>& rows);
      Pattern(AccessPat act_pattern, DataPat data_pattern, uint blast_radius);
      
      size_t size() const;
      size_t get_num_agg_rows() const;
      const Row_t& operator[](std::size_t idx) const;

      std::string to_string() const;
  };

  class Parameters
  {
    public:
      // Data pattern
      Pattern pattern;

      // RowHammer parameters
      uint hammer_count;
      uint hc_resolution;
      uint RAS_scale;
      uint RP_scale;
      bool is_retention_RH_sanity_check;

      // Retention parameters
      uint ret_time_ms;

      // Fixed attack time parameters
      uint extra_cycles;
      float RAS_ratio;

      // Asymmetric RowPress parameters
      uint head_hammer_count;
      uint tail_hammer_count;
      uint head_RAS_scale;
      uint tail_RAS_scale;

    public:
      Parameters(const std::string& pattern_filename, 
                 uint hammer_count, uint hc_resolution, uint RAS_scale, uint RP_scale, bool is_retention_RH_sanity_check,
                 uint ret_time_ms, uint extra_cycles, float RAS_ratio, 
                 uint head_hammer_count, uint tail_hammer_count, uint head_RAS_scale, uint tail_RAS_scale);
      Parameters(AccessPat act_pattern, DataPat data_pattern, uint blast_radius, 
                 uint hammer_count, uint hc_resolution, uint RAS_scale, uint RP_scale, bool is_retention_RH_sanity_check,
                 uint ret_time_ms, uint extra_cycles, float RAS_ratio, 
                 uint head_hammer_count, uint tail_hammer_count, uint head_RAS_scale, uint tail_RAS_scale);
      Parameters() = delete;

    private:
      Pattern parse_pattern_file(const std::string pattern_filename);
  };

  class BitFlip
  {
    public:
      int hammer_count;
      int bank;
      int pivot_row;
      int row_offset;
      int cacheline;
      int word;
      int byte;
      int bit;
      int dir;  

    public:
      BitFlip(int hammer_count, int bank, int pivot_row, int row_offset,
              int cacheline, int word, int byte, int bit, int dir);
      
      std::string get_column_header();
  };
} // namespace RowPress

template<typename T>
void join_vectors(std::vector<T>& base, const std::vector<T>& ext)
{
  base.reserve(base.size() + std::distance(ext.begin(), ext.end()));
  base.insert(base.end(), ext.begin(), ext.end());
}

std::ostream& operator<<(std::ostream& os, const RowPress::BitFlip& bf);