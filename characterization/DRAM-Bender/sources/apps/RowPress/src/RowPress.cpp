#include <fstream>
#include <filesystem>
#include <boost/algorithm/hex.hpp>

#include "RowPress.h"

namespace RowPress
{
  Pattern::Pattern(const std::vector<Row_t>& rows)
  {
    this->rows = rows;

    for (uint i = 0; i < rows.size(); i++)
    {
      if (rows[i].row_type == RowType::PIVOT_AGGRESSOR || rows[i].row_type == RowType::PIVOT_VICTIM)
      {
        pivot_id = i;
        break;
      }
    }

    head_offset = -pivot_id;
    tail_offset = rows.size() - 1 - pivot_id;
  };

  Pattern::Pattern(AccessPat act_pattern, DataPat data_pattern, uint blast_radius)
  {
    if      (act_pattern == AccessPat::SINGLE_SIDED)
    {
      if (blast_radius < 1 || blast_radius > 4)
      {
        spdlog::critical("Blast radius of {} for single-sided RowHammer is unreasonable. Recommended range is [1, 4]", blast_radius);
        throw std::runtime_error("Unreasonable experiment settings.");
      }

      for (uint i = 0; i < blast_radius; i++)
      {
        Row_t row;
        row.row_type = RowType::VICTIM;
        row.cacheline.fill(data_patterns[uint(data_pattern)][uint(RowType::VICTIM)]);
        rows.push_back(row);
      }

      Row_t row;
      row.row_type = RowType::PIVOT_AGGRESSOR;
      row.cacheline.fill(data_patterns[uint(data_pattern)][uint(RowType::AGGRESSOR)]);
      rows.push_back(row);

      for (uint i = 0; i < blast_radius; i++)
      {
        Row_t row;
        row.row_type = RowType::VICTIM;
        row.cacheline.fill(data_patterns[uint(data_pattern)][uint(RowType::VICTIM)]);
        rows.push_back(row);
      }
    }
    else if (act_pattern == AccessPat::DOUBLE_SIDED)
    {
      if (blast_radius < 2 || blast_radius > 8)
      {
        spdlog::critical("Blast radius of {} for single-sided RowHammer is unreasonable. Recommended range is [2, 8]", blast_radius);
        throw std::runtime_error("Unreasonable experiment settings.");
      }

      for (uint i = 0; i < blast_radius - 1; i++)
      {
        Row_t row;
        row.row_type = RowType::VICTIM;
        row.cacheline.fill(data_patterns[uint(data_pattern)][uint(RowType::VICTIM)]);
        rows.push_back(row);
      }

      Row_t aggressor_row;
      aggressor_row.row_type = RowType::AGGRESSOR;
      aggressor_row.cacheline.fill(data_patterns[uint(data_pattern)][uint(RowType::AGGRESSOR)]);
      rows.push_back(aggressor_row);

      Row_t victim_row;
      victim_row.row_type = RowType::PIVOT_VICTIM;
      victim_row.cacheline.fill(data_patterns[uint(data_pattern)][uint(RowType::VICTIM)]);
      rows.push_back(victim_row);

      rows.push_back(aggressor_row);

      for (uint i = 0; i < blast_radius - 1; i++)
      {
        Row_t row;
        row.row_type = RowType::VICTIM;
        row.cacheline.fill(data_patterns[uint(data_pattern)][uint(RowType::VICTIM)]);
        rows.push_back(row);
      }
    }
    else
    {
      spdlog::critical("Access pattern of {} invalid!", (uint) act_pattern);
      throw std::runtime_error("Invalid access pattern.");
    }

    for (uint i = 0; i < rows.size(); i++)
    {
      if (rows[i].row_type == RowType::PIVOT_AGGRESSOR || rows[i].row_type == RowType::PIVOT_VICTIM)
      {
        pivot_id = i;
        break;
      }
    }

    head_offset = -pivot_id;
    tail_offset = rows.size() - 1 - pivot_id;
  };


  size_t Pattern::size() const
  {
    return rows.size();
  }

  size_t Pattern::get_num_agg_rows() const
  {
    size_t num_agg_rows = 0;
    for (const Row_t& row : rows)
    {
      if (row.row_type == RowType::PIVOT_AGGRESSOR || row.row_type == RowType::AGGRESSOR)
        num_agg_rows++;
    }
    return num_agg_rows;
  }

  const Row_t& Pattern::operator[](std::size_t idx) const
  {
    return rows[idx];
  }

  std::string Pattern::to_string() const
  {
    std::stringstream ss;
    ss << std::endl;
    for (const Row_t& row : rows)
    {
      std::stringstream cacheline_ss;
      for (const uint32_t& word : row.cacheline)
        cacheline_ss << std::hex << std::setfill('0') << std::setw(8) << std::uppercase << word;
      ss << fmt::format("{:<2} : {}\n", enum_str(row.row_type), cacheline_ss.str());
    }
    return ss.str();
  }


  Parameters::Parameters(const std::string& pattern_filename, 
                         uint hammer_count, uint hc_resolution, uint RAS_scale, uint RP_scale, bool is_retention_RH_sanity_check,
                         uint ret_time_ms, uint extra_cycles, float RAS_ratio, 
                         uint head_hammer_count, uint tail_hammer_count, uint head_RAS_scale, uint tail_RAS_scale):
  pattern(parse_pattern_file(pattern_filename)),
  hammer_count(hammer_count), hc_resolution(hc_resolution), RAS_scale(RAS_scale), RP_scale(RP_scale), is_retention_RH_sanity_check(is_retention_RH_sanity_check),
  ret_time_ms(ret_time_ms),
  extra_cycles(extra_cycles), RAS_ratio(RAS_ratio),
  head_hammer_count(head_hammer_count), tail_hammer_count(tail_hammer_count), head_RAS_scale(head_RAS_scale), tail_RAS_scale(tail_RAS_scale)
  {};


  Parameters::Parameters(AccessPat act_pattern, DataPat data_pattern, uint blast_radius,
                         uint hammer_count, uint hc_resolution, uint RAS_scale, uint RP_scale, bool is_retention_RH_sanity_check,
                         uint ret_time_ms, uint extra_cycles, float RAS_ratio, 
                         uint head_hammer_count, uint tail_hammer_count, uint head_RAS_scale, uint tail_RAS_scale):
  pattern(Pattern(act_pattern, data_pattern, blast_radius)),
  hammer_count(hammer_count), hc_resolution(hc_resolution), RAS_scale(RAS_scale), RP_scale(RP_scale), is_retention_RH_sanity_check(is_retention_RH_sanity_check),
  ret_time_ms(ret_time_ms),
  extra_cycles(extra_cycles), RAS_ratio(RAS_ratio),
  head_hammer_count(head_hammer_count), tail_hammer_count(tail_hammer_count), head_RAS_scale(head_RAS_scale), tail_RAS_scale(tail_RAS_scale)
  {};

  Pattern Parameters::parse_pattern_file(const std::string pattern_filename)
  {
    if (std::filesystem::exists(pattern_filename))
    {
      std::vector<RowPress::Row_t> rows;
      bool has_pivot_aggressor = false;
      bool has_pivot_victim = false;

      std::ifstream fin(pattern_filename);
      std::string line;
      while (std::getline(fin, line))
      {
        // Remove comments
        line.erase(std::find(line.begin(), line.end(), '#'), line.end());
        std::istringstream iss(line);
        std::string row_type_str;
        std::string data_pattern_str;
        if (iss >> row_type_str >> data_pattern_str)
        {
          RowPress::Row_t row;

          // Parse the row type
          if      (row_type_str == RowPress::row_type_names[uint(RowPress::RowType::AGGRESSOR)])
            row.row_type = RowPress::RowType::AGGRESSOR;
          else if (row_type_str == RowPress::row_type_names[uint(RowPress::RowType::VICTIM)])
            row.row_type = RowPress::RowType::VICTIM;
          else if (row_type_str == RowPress::row_type_names[uint(RowPress::RowType::PIVOT_AGGRESSOR)])
          {
            row.row_type = RowPress::RowType::PIVOT_AGGRESSOR;
            has_pivot_aggressor = true;
            if (has_pivot_victim && has_pivot_aggressor)
            {
              spdlog::critical(fmt::format("Only one pivot row is allowed!"));
              throw std::runtime_error("Encountered multiple pivot rows!");
            }
          }
          else if (row_type_str == RowPress::row_type_names[uint(RowPress::RowType::PIVOT_VICTIM)])
          {
            row.row_type = RowPress::RowType::PIVOT_VICTIM;
            has_pivot_victim = true;
            if (has_pivot_aggressor && has_pivot_victim)
            {
              spdlog::critical(fmt::format("Only one pivot row is allowed!"));
              throw std::runtime_error("Encountered multiple pivot rows!");
            }
          }
          else
          {
            spdlog::critical(fmt::format("Unrecognized row type \"{}\"!", row_type_str));
            throw std::runtime_error("Invalid custom pattern file format!");
          }

          // Parse the hex string as the cacheline data
          for (uint i = 0; i < 16; i++)
            row.cacheline[i] = 0;
          uint8_t* cl_byte_ptr = (uint8_t*)(row.cacheline.data());

          if (data_pattern_str.size() < 128)
            spdlog::warn(fmt::format("Data pattern: \"{}\" is smaller than 64 Bytes. The rest are zero padded.", data_pattern_str));
          else if (data_pattern_str.size() > 128)
            spdlog::warn(fmt::format("Data pattern: \"{}\" is larger than 64 Bytes long. The exceeding parts are ignored.", data_pattern_str));

          std::string bytes = boost::algorithm::unhex(data_pattern_str); 
          std::copy(bytes.begin(), bytes.end(), cl_byte_ptr);
          rows.push_back(row);
        }
        else
        {
          spdlog::warn(fmt::format("Invalid row \"{}\"!", line));
          break;
        }
      }

      return RowPress::Pattern(rows);
    }
    else
    {
      spdlog::critical(fmt::format("Custom pattern file \"{}\" invalid!", pattern_filename));
      throw std::runtime_error("Invalid custom pattern file!");
    }
  }



  BitFlip::BitFlip(int hammer_count, int bank, int pivot_row, int row_offset,
                   int cacheline, int word, int byte, int bit, int dir):
  hammer_count(hammer_count), bank(bank), pivot_row(pivot_row), row_offset(row_offset),
  cacheline(cacheline), word(word), byte(byte), bit(bit), dir(dir)
  { };

  std::string BitFlip::get_column_header()
  {
    return "Hammer Count,"
           "Bank,"
           "Pivot Row,"
           "Row Offset,"
           "Cacheline,"
           "Word,"
           "Byte,"
           "Bit,"
           "Dir\n";
  };
};

std::ostream& operator<<(std::ostream& os, const RowPress::BitFlip& bf)
{
  os << fmt::format("{},{},{},{},{},{},{},{},{}\n",
                     bf.hammer_count, bf.bank, bf.pivot_row, bf.row_offset,
                     bf.cacheline, bf.word, bf.byte, bf.bit, bf.dir);
  return os;
}