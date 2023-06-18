#include <stdexcept>
#include <cmath>

#include "Tester.h"

namespace RowPress
{
  Inst all_nops() { return  __pack_mininsts(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_NOP()); };

  Tester::Tester(const std::string& module_id, RowMapping row_mapping, 
                 uint num_banks, uint num_rows, uint num_cols,
                 const std::string& prefix, const std::string& suffix):
  module_id(module_id), row_mapping(row_mapping), 
  num_banks(num_banks), num_rows(num_rows), num_cols(num_cols),
  prefix(prefix), suffix(suffix)
  {
    // Set up spdlog loggers
    set_loggers();

    if (row_mapping == RowMapping::MAX)
    {
      spdlog::critical("Row address mapping invalid!");
      throw std::invalid_argument("Invalid row address mapping.");
    }

    _row_buffer = new uint8_t[8 * 1024];

    logger->info(fmt::format("{:<15}: {}", "Module",      module_id));
    logger->info(fmt::format("{:<15}: {}", "Row Mapping", enum_str(row_mapping)));
    logger->info(fmt::format("{:<15}: {}", "Num Banks",   num_banks));
    logger->info(fmt::format("{:<15}: {}", "Num Rows",    num_rows));
    logger->info(fmt::format("{:<15}: {}", "Num Cols",    num_cols));
  }

  void Tester::set_loggers()
  {
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_st>());

    std::string log_filename = prefix + module_id + suffix + ".log";

    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_st>(log_filename));
    logger = std::make_shared<spdlog::logger>("combined logger", begin(sinks), end(sinks));
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %^[%l]%$ %v");
  };

  void Tester::log_experiment_params(ExperimentType experiment, const Parameters& params)
  {
    logger->info("{:<15}: {}", "Experiment",   enum_str(experiment));

    switch (experiment)
    {
      case ExperimentType::RH_BITFLIPS:
      {
        logger->info("{:<15}: {}",    "Hammer Count", params.hammer_count);
        logger->info("{:<15}: {}",    "RAS Scale",    params.RAS_scale);
        logger->info("{:<15}: {}",    "RP Scale",     params.RP_scale);
        logger->info("{:<15}: {}",    "Sanity Check", params.is_retention_RH_sanity_check);
        logger->info("{:<15}: {} ms", "Prog Time",    exp_time_ns(params.hammer_count,
                                                                  params.RAS_scale, params.RP_scale,
                                                                  params.pattern.get_num_agg_rows()) / 
                                                                  1E6);
        break;
      }

      case ExperimentType::RH_BITFLIPS_RAS_RATIO:
      {
        logger->info("{:<15}: {}",    "Hammer Count", params.hammer_count);
        logger->info("{:<15}: {} ns", "Extra delay",  params.extra_cycles * 6);
        logger->info("{:<15}: {} ns", "tAggON",       params.extra_cycles * 6 * params.RAS_ratio + 36);
        logger->info("{:<15}: {} ns", "tAggOFF",      params.extra_cycles * 6 - params.extra_cycles * 6 * params.RAS_ratio + 15);
        logger->info("{:<15}: {}",    "RAS Ratio",    params.RAS_ratio);
        logger->info("{:<15}: {}",    "Sanity Check", params.is_retention_RH_sanity_check);
        logger->info("{:<15}: {} ms", "Prog Time",    exp_time_ns(params.hammer_count,
                                                                  params.RAS_scale, params.RP_scale,
                                                                  params.pattern.get_num_agg_rows(),
                                                                  params.extra_cycles) / 
                                                                  1E6);
        break;
      }

      case ExperimentType::RH_HCFIRST:
      {
        logger->info("{:<15}: {}",    "Hammer Count",  params.hammer_count);
        logger->info("{:<15}: {}",    "HC Resolution", params.hc_resolution);
        logger->info("{:<15}: {}",    "RAS Scale",     params.RAS_scale);
        logger->info("{:<15}: {}",    "RP Scale",      params.RP_scale);
        logger->info("{:<15}: {}",    "Sanity Check",  params.is_retention_RH_sanity_check);
        logger->info("{:<15}: {} ms", "Prog Time",     exp_time_ns(params.hammer_count,
                                                                   params.RAS_scale, params.RP_scale,
                                                                   params.pattern.get_num_agg_rows()) / 
                                                                   1E6);
        break;
      }

      case ExperimentType::RETENTION_TIME:  [[fallthrough]];
      case ExperimentType::RETENTION_FAILURE:
      {
        logger->info("{:<15}: {}", "Retention (ms)", params.ret_time_ms);
        break;
      }

      case ExperimentType::ROW_MAPPING:
      {
        logger->info("{:<15}: {}", "Hammer Count", params.hammer_count);
        break;
      }

      case ExperimentType::ASYMMETRIC_ROWPRESS_BER:
      {
        logger->info("{:<15}: {}",    "Head Hammer Count",  params.head_hammer_count);
        logger->info("{:<15}: {}",    "Head RAS Scale",     params.head_RAS_scale);
        logger->info("{:<15}: {}",    "Tail Hammer Count",  params.tail_hammer_count);
        logger->info("{:<15}: {}",    "Tail RAS Scale",     params.tail_RAS_scale);
        logger->info("{:<15}: {}",    "Sanity Check",       params.is_retention_RH_sanity_check);
        logger->info("{:<15}: {} ms", "Prog Time",          asym_exp_time_ns(params.head_hammer_count,
                                                                             params.head_RAS_scale,
                                                                             params.tail_hammer_count,
                                                                             params.tail_RAS_scale,
                                                                             params.pattern.get_num_agg_rows()) / 1E6);
        break;
      }

      case ExperimentType::ASYMMETRIC_ROWPRESS_HCFIRST:
      {
        logger->info("{:<15}: {}",    "Head Hammer Count",  params.head_hammer_count);
        logger->info("{:<15}: {}",    "Head RAS Scale",     params.head_RAS_scale);
        logger->info("{:<15}: {}",    "Initial Tail Hammer Count",  params.tail_hammer_count);
        logger->info("{:<15}: {}",    "Tail RAS Scale",     params.tail_RAS_scale);
        logger->info("{:<15}: {}",    "HC Resolution",      params.hc_resolution);
        logger->info("{:<15}: {}",    "Sanity Check",       params.is_retention_RH_sanity_check);
        logger->info("{:<15}: {} ms", "Prog Time Cap",      params.ret_time_ms);
        break;
      }

      case ExperimentType::TAGGON_MIN:
      {
        logger->info("{:<15}: {}",    "Sanity Check",  params.is_retention_RH_sanity_check);
        break;
      }

      case ExperimentType::MITIGATION_REF:
      {
        logger->info("{:<15}: {}",    "Hammer Count",  params.hammer_count);
        logger->info("{:<15}: {}",    "HC Resolution", params.hc_resolution);
        logger->info("{:<15}: {}",    "RAS Scale",     params.RAS_scale);
        logger->info("{:<15}: {}",    "RP Scale",      params.RP_scale);
        logger->info("{:<15}: {}",    "Sanity Check",  params.is_retention_RH_sanity_check);
        logger->info("{:<15}: {} ms", "Prog Time",     exp_time_ns(params.hammer_count,
                                                                   params.RAS_scale, params.RP_scale,
                                                                   params.pattern.get_num_agg_rows()) / 
                                                                   1E6);
        break;
      }


      case ExperimentType::MAX:
      {
        spdlog::critical("Experiment type invalid!");
        throw std::invalid_argument("Invalid experiment type.");
      }
    }

    logger->info("{:<15}: \n{}", "Pattern",    params.pattern.to_string());
  }

  void Tester::reset(const std::string& new_prefix, const std::string& new_suffix)
  {
    platform.reset_fpga();
    logger->flush();

    if (new_prefix != "")
      prefix = new_prefix;
    if (new_suffix != "")
      suffix = new_suffix;
    
    spdlog::drop("combined logger");
    set_loggers();

    logger->info(fmt::format("{:<15}: {}", "Module",      module_id));
    logger->info(fmt::format("{:<15}: {}", "Row Mapping", enum_str(row_mapping)));
    logger->info(fmt::format("{:<15}: {}", "Num Banks",   num_banks));
    logger->info(fmt::format("{:<15}: {}", "Num Rows",    num_rows));
    logger->info(fmt::format("{:<15}: {}", "Num Cols",    num_cols));
  };

  void Tester::initialize_platform()
  {
    int err = platform.init();
    if(err != SOFTMC_SUCCESS)
    {
      logger->critical(fmt::format("SoftMC platform initialization failed! Error code: {}", err));
      throw std::logic_error("SoftMC platform initialization failed.");
    }
    else
      logger->info(fmt::format("SoftMC platform initialized."));

    platform.reset_fpga();
  }

  uint Tester::map_row(uint row)
  {
    switch (row_mapping)
    {
      case RowMapping::SEQUENTIAL:
        return row;

      case RowMapping::SAMSUNG_16:
      {
        if(row & 0x8) 
        {
          uint p_row = row & 0xFFFFFFF9;
          p_row |= (~row & 0x00000006); // set bit pos 3 and 2
          return p_row;
        } 
        else
          return row;
      }

      case RowMapping::MI0:
      // Bit 1 and Bit 2 are flipped
      {
        if (row & 0x8)
        {
          uint mask = 0x6;
          return row ^ mask;
        }
        else
          return row;
      }

      default:
      {
        logger->critical("Row address mapping invalid!");
        throw std::logic_error("Invalid row address mapping.");
      }
    }
  };

  void Tester::test(std::vector<BitFlip>& results, ExperimentType experiment,
                    RowCoverage row_coverage, std::vector<uint> bank_list, uint num_rows_to_test, 
                    const Parameters& params)
  {
    log_experiment_params(experiment, params);

    for (uint bank : bank_list)
    {
      if (bank > num_banks - 1)
      {
        spdlog::warn("Bank {:<3} exceeding the total number of banks {:<3}. Skipping.", bank, num_banks);
        continue;
      }
      else
      {
        test_bank(results, experiment, 
                  row_coverage, bank, num_rows_to_test,
                  params);
      }
    }
  }

  void Tester::test_and_save_results(std::vector<BitFlip>& results, ExperimentType experiment,
                                    RowCoverage row_coverage, std::vector<uint> bank_list, uint num_rows_to_test, 
                                    const Parameters& params)
  {
    test(results, experiment,
         row_coverage, bank_list, num_rows_to_test,
         params);
    
    results_file.open(prefix + module_id + suffix + ".csv");
    results_file << results[0].get_column_header();
    for (const BitFlip& bf : results)
      results_file << bf;
    results_file.close();
  };

  void Tester::test_bank(std::vector<BitFlip>& results, ExperimentType experiment,
                         RowCoverage row_coverage, uint bank, uint num_rows_to_test, 
                         const Parameters& params)
  {
    if      (row_coverage == RowCoverage::FULL)
    {
      uint start_row = 0 - params.pattern.head_offset;
      test_chunk(results, experiment, 
                 bank, start_row, num_rows_to_test, 
                 params);
    }
    else if (row_coverage == RowCoverage::SAMPLE_HMT)
    {
      uint start_row = 0 - params.pattern.head_offset;
      test_chunk(results, experiment, 
                 bank, start_row, num_rows_to_test, 
                 params);

      start_row = num_rows / 2 - params.pattern.head_offset;
      test_chunk(results, experiment, 
                 bank, start_row, num_rows_to_test, 
                 params);

      start_row = num_rows - num_rows_to_test - params.pattern.tail_offset - 1;
      test_chunk(results, experiment, 
                 bank, start_row, num_rows_to_test, 
                 params);    
    }
    else
    {
      logger->critical("Invalid row coverage type {}!", (uint) row_coverage);
      throw std::runtime_error("Invalid row coverage type.");
    }
  };

  void Tester::test_chunk(std::vector<BitFlip>& results, ExperimentType experiment,
                          uint bank, uint start_row, uint num_rows_to_test, 
                          const Parameters& params)
  {
    for (uint i = 0; i < num_rows_to_test; i++)
    {
      int curr_pivot_row = (int) start_row + i;
      int head_row = (int) curr_pivot_row + params.pattern.head_offset;

      if (verify_mapping(params.pattern, (int) start_row + i))
      {
        // Instaniate aggressor and victim rows
        std::vector<Row_t> aggressor_rows;
        std::vector<Row_t> victim_rows;
        generate_aggressor_and_victim_rows(params.pattern, head_row, aggressor_rows, victim_rows);

        if (experiment == ExperimentType::RH_BITFLIPS)
        {
          test_bitflips(results, params.pattern, bank, head_row, aggressor_rows, victim_rows, 
                        params.hammer_count, params.RAS_scale, params.RP_scale, params.is_retention_RH_sanity_check);
        }
        if (experiment == ExperimentType::RH_BITFLIPS_RAS_RATIO)
        {
          test_bitflips_RAS_ratio(results, params.pattern, bank, head_row, aggressor_rows, victim_rows, 
                                  params.hammer_count, params.extra_cycles, params.RAS_ratio, params.is_retention_RH_sanity_check);
        }
        else if (experiment == ExperimentType::RH_HCFIRST)
        {
          test_hcfirst(results, params.pattern, bank, head_row, aggressor_rows, victim_rows, 
                       params.hammer_count, params.hc_resolution, params.RAS_scale, params.RP_scale, params.is_retention_RH_sanity_check);
        }
        else if (experiment == ExperimentType::RETENTION_TIME)
        {
          test_retention_time(results, params.pattern, bank, head_row, params.ret_time_ms);
        }
        else if (experiment == ExperimentType::RETENTION_FAILURE)
        {
          test_retention_failures(results, params.pattern, victim_rows, bank, head_row, params.ret_time_ms);
        }
        else if (experiment == ExperimentType::ROW_MAPPING)
        {
          test_rowmapping(results, params.pattern, bank, head_row, aggressor_rows, victim_rows, params.hammer_count);
        }
        else if (experiment == ExperimentType::ASYMMETRIC_ROWPRESS_BER)
        {
          test_asymmetric_rowpress_ber(results, params.pattern, bank, head_row, aggressor_rows, victim_rows,
                                       params.head_hammer_count, params.tail_hammer_count, params.head_RAS_scale, params.tail_RAS_scale, params.is_retention_RH_sanity_check);
        }        
        else if (experiment == ExperimentType::ASYMMETRIC_ROWPRESS_HCFIRST)
        {
          test_asymmetric_rowpress_hcfirst(results, params.pattern, bank, head_row, aggressor_rows, victim_rows,
                                           params.head_hammer_count, params.tail_hammer_count, params.hc_resolution, params.head_RAS_scale, params.tail_RAS_scale,
                                           params.ret_time_ms * 1E6,
                                           params.is_retention_RH_sanity_check);
        }
        else if (experiment == ExperimentType::TAGGON_MIN)
        {
          test_taggon_min(results, params.pattern, bank, head_row, aggressor_rows, victim_rows,
                          params.hammer_count,
                          params.is_retention_RH_sanity_check);
        }
        else if (experiment == ExperimentType::MITIGATION_REF) {
          test_mitigation_ref(results, params.pattern, bank, head_row, aggressor_rows, victim_rows, 
                              params.hammer_count, params.hc_resolution, params.RAS_scale, params.RP_scale, params.is_retention_RH_sanity_check);
        }
      }
      else
      {
        logger->warn(fmt::format("Could not map pattern to row {:<7}. Skipping.", curr_pivot_row));
        continue;
      }
    }
  }

  bool Tester::verify_mapping(const Pattern& pattern, int curr_pivot_row)
  {
    int head_row = curr_pivot_row + pattern.head_offset;
    int tail_row = curr_pivot_row + pattern.tail_offset;

    if (head_row < 0 || tail_row > (int) this->num_rows - 1)
    {
      logger->warn(fmt::format("Could not map pattern to row {:<7}. Skipping.", curr_pivot_row));
      return false;
    }
    else
      return true;
  }

  void Tester::generate_aggressor_and_victim_rows(const Pattern& pattern, uint head_row,
                                                  std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows)
  {
    for (uint r = 0; r < pattern.size(); r++)
    {
      if (pattern[r].row_type == RowType::AGGRESSOR || pattern[r].row_type == RowType::PIVOT_AGGRESSOR)
      {
        Row_t row = {pattern[r].row_type, map_row(head_row + r), (int) r - pattern.pivot_id, pattern[r].cacheline};
        aggressor_rows.push_back(row);
      }
      else if (pattern[r].row_type == RowType::VICTIM || pattern[r].row_type == RowType::PIVOT_VICTIM)
      {
        Row_t row = {pattern[r].row_type, map_row(head_row + r), (int) r - pattern.pivot_id, pattern[r].cacheline};
        victim_rows.push_back(row);
      }
    }
  }


  void Tester::test_bitflips(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                             std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                             uint hammer_count, uint RAS_scale, uint RP_scale,
                             bool is_retention)
  {
    // Initialize the pattern in DRAM
    for (uint r = 0; r < pattern.size(); r++)
      SMC_initialize_row(pattern[r].cacheline, bank, map_row(head_row + r));
    SMC_hammer_rows(bank, aggressor_rows, hammer_count, RAS_scale, RP_scale, is_retention);
    SMC_read_rows(bank, victim_rows);

    size_t prev_results_size = results.size();

    check_pattern_bitflips(results, victim_rows, hammer_count, bank, head_row - pattern.head_offset);
    logger->info("Bank {:<2} Row {:<7}: Found {} bitflips",
                  bank,
                  head_row - pattern.head_offset,
                  results.size() - prev_results_size);
  };

  void Tester::test_bitflips_RAS_ratio(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                                               std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                                               uint hammer_count, uint extra_cycles, float RAS_ratio, 
                                               bool is_retention)
  {
    if (extra_cycles % 4 != 0)
      logger->warn("Extra fabric cycles not a multiple of 4!");
    if (RAS_ratio != 0.0f && RAS_ratio != 0.25f && RAS_ratio != 0.5f && RAS_ratio != 0.75f && RAS_ratio != 1.0f)
      logger->warn("RAS ratio should be in {0.00, 0.25, 0.50, 0.75, 1.00}!");
  
    // Initialize the pattern in DRAM
    for (uint r = 0; r < pattern.size(); r++)
      SMC_initialize_row(pattern[r].cacheline, bank, map_row(head_row + r));
    SMC_hammer_rows_RAS_ratio(bank, aggressor_rows, hammer_count, extra_cycles, RAS_ratio, is_retention);
    SMC_read_rows(bank, victim_rows);

    size_t prev_results_size = results.size();

    check_pattern_bitflips(results, victim_rows, hammer_count, bank, head_row - pattern.head_offset);
    logger->info("Bank {:<2} Row {:<7}: Found {} bitflips",
                  bank,
                  head_row - pattern.head_offset,
                  results.size() - prev_results_size);
  };

  void Tester::test_hcfirst(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                            std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                            uint initial_hammer_count, uint resolution, uint RAS_scale, uint RP_scale,
                            bool is_retention)
  {
    uint hammer_count = initial_hammer_count;
    uint hc_first = 0;
    uint hc_first_resolution = resolution;
    std::vector<BitFlip> previous_bitflips;

    bool is_lastround = false;

    while (true)
    {
      logger->trace("Bank {:<2} Row {:<7}: Trying HC = {:<7}, HC_First Guess = {:<7}, Resolution = {}.", bank, head_row - pattern.head_offset, hammer_count, hc_first, hc_first_resolution);

      for (uint r = 0; r < pattern.size(); r++)
        SMC_initialize_row(pattern[r].cacheline, bank, map_row(head_row + r));
      SMC_hammer_rows(bank, aggressor_rows, hammer_count, RAS_scale, RP_scale, is_retention);
      SMC_read_rows(bank, victim_rows);

      std::vector<BitFlip> pattern_bitflips;
      check_pattern_bitflips(pattern_bitflips, victim_rows, hammer_count, bank, head_row - pattern.head_offset);
      
      if (pattern_bitflips.size() > 0)
      {
        hc_first = hammer_count;
        hammer_count = hammer_count / 2;
        hc_first_resolution = std::max(int(hammer_count / 100.0), 1);
        previous_bitflips = pattern_bitflips;

        if (hammer_count == 0)
        {
          join_vectors(results, previous_bitflips);
          break;
        }
      }
      else
      {
        if (is_lastround)
        {
          logger->info("Bank {:<2} Row {:<7}: HC_First = Inf.",
                        bank,
                        head_row - pattern.head_offset);
          return;
        }

        if (std::abs((int)hc_first - (int)hammer_count) <= hc_first_resolution)
        // End search as we reached the resolution
        {
          join_vectors(results, previous_bitflips);
          break;
        }

        if (hc_first > 0)
        {
          uint prev_hammer_count = hammer_count;
          hammer_count = (hammer_count + hc_first) / 2;
          hc_first_resolution = std::max(int(hammer_count / 100.0), 1);

          if (hammer_count == prev_hammer_count)
          {
            join_vectors(results, previous_bitflips);
            break;
          }
        }
        else
        {
          logger->trace("Initial hc_first guess {} for Bank {:<2} Row {:<7} is too low. Trying from {}", 
                         hammer_count, bank, head_row - pattern.head_offset, 2 * hammer_count);
          hammer_count *= 2;

          if (exp_time_ns(hammer_count, RAS_scale, RP_scale, aggressor_rows.size()) >= 60000000)
          // We try a last time with max possible hammer_count
          {
            hammer_count = max_hammer_count(60000000, RAS_scale, RP_scale, aggressor_rows.size());
            hc_first_resolution = std::max(int(hammer_count / 100.0), 1);
            is_lastround = true;
          }
        }
      }
    }

    logger->info("Bank {:<2} Row {:<7}: HC_First = {}",
                  bank,
                  head_row - pattern.head_offset,
                  hc_first);
  };

  void Tester::test_retention_failures(std::vector<BitFlip>& results, const Pattern& pattern, std::vector<Row_t>& victim_rows, uint bank, uint head_row, 
                                       uint ret_time_ms)
  {
    uint64_t sleep_cycles = ret_time_ms * 1E6 / 6;

    for (uint r = 0; r < pattern.size(); r++)
      SMC_initialize_row(pattern[r].cacheline, bank, map_row(head_row + r));

    SMC_retention_rows(victim_rows, bank, sleep_cycles);

    size_t original_results_size = results.size();
    check_pattern_bitflips(results, victim_rows, 0, bank, head_row - pattern.head_offset);
    
    logger->info("Bank {:<2} Row {:<7}: Found {} retention failure bitflips",
                  bank,
                  head_row - pattern.head_offset,
                  results.size() - original_results_size);
  }

  void Tester::test_retention_time(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                                   uint initial_retention_time_ms)
  {
    uint ret_time_ms = 0;
    uint retention_time_ms_resolution = 16;
    uint retention_time_ms_cap = 65536;

    uint sleep_time_ms = initial_retention_time_ms;

    std::vector<BitFlip> previous_bitflips;
    while (true)
    {
      if (sleep_time_ms >= retention_time_ms_cap)
      {
        logger->warn("Bank {:<2} Row {:<7}: Retention time larger than 64 ms? Skipping.",
                      bank,
                      head_row - pattern.head_offset);
        return;
      }

      for (uint r = 0; r < pattern.size(); r++)
        SMC_initialize_row(pattern[r].cacheline, bank, map_row(head_row + r));

      uint64_t sleep_cycles = sleep_time_ms * 1E6 / 6;
      SMC_retention_row(pattern[pattern.pivot_id].cacheline, bank, map_row(head_row + pattern.pivot_id), sleep_cycles);

      std::vector<BitFlip> pattern_bitflips;
      check_pattern_bitflips(pattern_bitflips, {pattern[pattern.pivot_id]}, 0, bank, head_row - pattern.head_offset);
      
      if (pattern_bitflips.size() > 0)
      {
        ret_time_ms = sleep_time_ms;
        sleep_time_ms = sleep_time_ms / 2;
        previous_bitflips = pattern_bitflips;

        logger->trace("Retention time {} ms for Bank {:<2} Row {:<7} caused {} bitflips.", 
                      ret_time_ms, bank, head_row - pattern.head_offset, pattern_bitflips.size());
      }
      else
      {
        if (ret_time_ms - sleep_time_ms < retention_time_ms_resolution)
        // End search as we reached the resolution
        {
          join_vectors(results, previous_bitflips);
          break;
        }

        if (ret_time_ms > 0)
          sleep_time_ms = (ret_time_ms + sleep_time_ms) / 2;
        else
        {
          sleep_time_ms *= 2;
        }

        logger->trace("Retention time {} ms for Bank {:<2} Row {:<7} did NOT cause bitflips.", 
                      ret_time_ms, bank, head_row - pattern.head_offset);
      }
    }

    logger->info("Bank {:<2} Row {:<7}: Retention = {} ms",
                  bank,
                  head_row - pattern.head_offset,
                  ret_time_ms);
  }


  void Tester::test_rowmapping(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                               std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                               uint hammer_count)
  {
    // Initialize the pattern in DRAM
    for (uint r = 0; r < pattern.size(); r++)
      SMC_initialize_row(pattern[r].cacheline, bank, map_row(head_row + r));
    SMC_hammer_rows(bank, aggressor_rows, hammer_count, 1, 1, false);
    SMC_read_rows(bank, victim_rows);

    check_pattern_bitflips(results, victim_rows, hammer_count, bank, head_row - pattern.head_offset, true);
  }


  void Tester::test_asymmetric_rowpress_ber(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                                            std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                                            uint head_hammer_count, uint tail_hammer_count, uint head_RAS_scale, uint tail_RAS_scale, bool is_retention)
  {
    // Initialize the pattern in DRAM
    for (uint r = 0; r < pattern.size(); r++)
      SMC_initialize_row(pattern[r].cacheline, bank, map_row(head_row + r));
    SMC_asymmetric_rowpress(bank, aggressor_rows, head_RAS_scale, head_hammer_count, tail_RAS_scale, tail_hammer_count, is_retention);
    SMC_read_rows(bank, victim_rows);

    size_t prev_results_size = results.size();

    check_pattern_bitflips(results, victim_rows, head_hammer_count + tail_hammer_count, bank, head_row - pattern.head_offset);
    logger->info("Bank {:<2} Row {:<7}: Found {} bitflips",
                  bank,
                  head_row - pattern.head_offset,
                  results.size() - prev_results_size);
  };


  void Tester::test_asymmetric_rowpress_hcfirst(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                                                std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                                                uint head_hammer_count, uint initial_tail_hammer_count, uint resolution, uint head_RAS_scale, uint tail_RAS_scale, uint total_time_cap_ns, 
                                                bool is_retention)
  {
    uint tail_hammer_count = initial_tail_hammer_count;
    uint hc_first = 0;
    uint hc_first_resolution = resolution;
    std::vector<BitFlip> previous_bitflips;

    bool is_lastround = false;

    while (true)
    {
      logger->trace("Bank {:<2} Row {:<7}: Trying HC = {:<7}, HC_First Guess = {:<7}, Resolution = {}.", bank, head_row - pattern.head_offset, tail_hammer_count, hc_first, hc_first_resolution);

      for (uint r = 0; r < pattern.size(); r++)
        SMC_initialize_row(pattern[r].cacheline, bank, map_row(head_row + r));
      SMC_asymmetric_rowpress(bank, aggressor_rows, head_RAS_scale, head_hammer_count, tail_RAS_scale, tail_hammer_count, is_retention);
      SMC_read_rows(bank, victim_rows);

      std::vector<BitFlip> pattern_bitflips;
      check_pattern_bitflips(pattern_bitflips, victim_rows, tail_hammer_count, bank, head_row - pattern.head_offset);
      
      if (pattern_bitflips.size() > 0)
      {
        hc_first = tail_hammer_count;
        tail_hammer_count = tail_hammer_count / 2;
        hc_first_resolution = std::max(int(tail_hammer_count / 100.0), 1);
        previous_bitflips = pattern_bitflips;

        if (tail_hammer_count == 0)
        {
          join_vectors(results, previous_bitflips);
          break;
        }
      }
      else
      {
        if (is_lastround)
        {
          logger->info("Bank {:<2} Row {:<7}: HC_First = Inf.",
                        bank,
                        head_row - pattern.head_offset);
          return;
        }

        if (std::abs((int)hc_first - (int)tail_hammer_count) <= hc_first_resolution)
        // End search as we reached the resolution
        {
          if (hc_first == 0)
          {
            logger->info("Bank {:<2} Row {:<7}: HC_First = Inf.",
              bank,
              head_row - pattern.head_offset);
            return;
          }

          join_vectors(results, previous_bitflips);
          break;
        }

        if (hc_first > 0)
        {
          uint prev_tail_hammer_count = tail_hammer_count;
          tail_hammer_count = (tail_hammer_count + hc_first) / 2;
          hc_first_resolution = std::max(int(tail_hammer_count / 100.0), 1);

          if (tail_hammer_count == prev_tail_hammer_count)
          {
            join_vectors(results, previous_bitflips);
            break;
          }
        }
        else
        {
          logger->trace("Initial hc_first guess {} for Bank {:<2} Row {:<7} is too low. Trying from {}", 
                         tail_hammer_count, bank, head_row - pattern.head_offset, 2 * tail_hammer_count);
          tail_hammer_count *= 2;

          uint head_time_ns = exp_time_ns(head_hammer_count, head_RAS_scale, 0, aggressor_rows.size());
          uint tail_time_cap = total_time_cap_ns - head_time_ns;

          if (exp_time_ns(tail_hammer_count, tail_RAS_scale, 0, aggressor_rows.size()) >= tail_time_cap)
          // We try a last time with max possible hammer_count
          {
            // logger->info("Head time {}, tail time cap {}, curr tail time {}", head_time_ns, tail_time_cap, exp_time_ns(tail_hammer_count, tail_RAS_scale, 0, aggressor_rows.size()));
            tail_hammer_count = max_hammer_count(tail_time_cap, tail_RAS_scale, 0, aggressor_rows.size());
            // logger->info("Now trying last time with tail time {}", exp_time_ns(tail_hammer_count, tail_RAS_scale, 0, aggressor_rows.size()));
            hc_first_resolution = std::max(int(tail_hammer_count / 100.0), 1);
            is_lastround = true;
          }
        }
      }
    }

    logger->info("Bank {:<2} Row {:<7}: HC_First = {}",
                  bank,
                  head_row - pattern.head_offset,
                  hc_first);

    // uint head_time_ns = exp_time_ns(head_hammer_count, head_RAS_scale, 0, aggressor_rows.size());
    // logger->info("Time {} + {} = {} ns",
    //       head_time_ns,
    //       exp_time_ns(hc_first, tail_RAS_scale, 0, aggressor_rows.size()),
    //       head_time_ns + exp_time_ns(hc_first, tail_RAS_scale, 0, aggressor_rows.size())); 
  };

  void Tester::test_taggon_min(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                               std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                               uint hammer_count,
                               bool is_retention) 
  {
    uint RAS_scale = max_RAS_scale(60000060, hammer_count, 0, aggressor_rows.size());

    uint min_RAS_scale = 0;
    uint RAS_scale_resolution = 10;
    std::vector<BitFlip> previous_bitflips;

    bool is_lastround = false;

    while (true)
    {
      logger->trace("Bank {:<2} Row {:<7}: Trying RAS Scale = {:<7}, RAS Scale Guess = {:<7}, Resolution = {}.", bank, head_row - pattern.head_offset, RAS_scale, min_RAS_scale, RAS_scale_resolution);

      for (uint r = 0; r < pattern.size(); r++)
        SMC_initialize_row(pattern[r].cacheline, bank, map_row(head_row + r));
      SMC_hammer_rows(bank, aggressor_rows, hammer_count, RAS_scale, 0, is_retention);
      SMC_read_rows(bank, victim_rows);

      std::vector<BitFlip> pattern_bitflips;
      check_pattern_bitflips(pattern_bitflips, victim_rows, hammer_count, bank, head_row - pattern.head_offset);
      
      if (pattern_bitflips.size() > 0)
      {
        min_RAS_scale = RAS_scale;
        RAS_scale = RAS_scale / 2;
        RAS_scale_resolution = std::max(int(RAS_scale / 100.0), 1);
        previous_bitflips = pattern_bitflips;

        if (RAS_scale == 0)
        {
          join_vectors(results, previous_bitflips);
          break;
        }
      }
      else
      {
        if (is_lastround)
        {
          logger->info("Bank {:<2} Row {:<7}: HC_First = Inf.",
                        bank,
                        head_row - pattern.head_offset);
          return;
        }

        if (RAS_scale == max_RAS_scale(60000060, hammer_count, 0, aggressor_rows.size()))
        // End search as we reached the max RAS_scale
        {
          logger->info("Bank {:<2} Row {:<7}: RAS Scale = Inf.",
                        bank,
                        head_row - pattern.head_offset);
          return;
        }

        if (std::abs((int)min_RAS_scale - (int)RAS_scale) <= RAS_scale_resolution)
        // End search as we reached the resolution
        {
          join_vectors(results, previous_bitflips);
          break;
        }

        if (RAS_scale > 0)
        {
          uint prev_RAS_scale = RAS_scale;
          RAS_scale = (RAS_scale + min_RAS_scale) / 2;
          RAS_scale_resolution = std::max(int(RAS_scale / 100.0), 1);

          if (RAS_scale == prev_RAS_scale)
          {
            join_vectors(results, previous_bitflips);
            break;
          }
        }
        else
        {
          logger->trace("Initial RAS Scale guess {} for Bank {:<2} Row {:<7} is too low. Trying from {}", 
                         RAS_scale, bank, head_row - pattern.head_offset, 2 * RAS_scale);
          RAS_scale *= 2;

          if (exp_time_ns(hammer_count, RAS_scale, 0, aggressor_rows.size()) >= 60000000)
          // We try a last time with max possible hammer_count
          {
            logger->info("Bank {:<2} Row {:<7}: RAS Scale = Inf.",
                          bank,
                          head_row - pattern.head_offset);
            return;
          }
        }
      }
    }

    logger->info("Bank {:<2} Row {:<7}: Min RAS Scale = {}",
                  bank,
                  head_row - pattern.head_offset,
                  min_RAS_scale);
  }


  void Tester::test_mitigation_ref(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                                   std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                                   uint initial_hammer_count, uint resolution, uint RAS_scale, uint RP_scale,
                                   bool is_retention)
  {
    uint hammer_count = initial_hammer_count;
    uint hc_first = 0;
    uint hc_first_resolution = resolution;
    std::vector<BitFlip> previous_bitflips;

    bool is_lastround = false;

    while (true)
    {
      logger->trace("Bank {:<2} Row {:<7}: Trying HC = {:<7}, HC_First Guess = {:<7}, Resolution = {}.", bank, head_row - pattern.head_offset, hammer_count, hc_first, hc_first_resolution);

      for (uint r = 0; r < pattern.size(); r++)
        SMC_initialize_row(pattern[r].cacheline, bank, map_row(head_row + r));
      SMC_hammer_rows(bank, aggressor_rows, hammer_count, RAS_scale, RP_scale, is_retention);
      SMC_read_rows(bank, victim_rows);

      std::vector<BitFlip> pattern_bitflips;
      check_pattern_bitflips(pattern_bitflips, victim_rows, hammer_count, bank, head_row - pattern.head_offset);
      
      if (pattern_bitflips.size() > 0)
      {
        hc_first = hammer_count;
        hammer_count = hammer_count / 2;
        hc_first_resolution = std::max(int(hammer_count / 100.0), 1);
        previous_bitflips = pattern_bitflips;

        if (hammer_count == 0)
        {
          join_vectors(results, previous_bitflips);
          break;
        }
      }
      else
      {
        if (is_lastround)
        {
          logger->info("Bank {:<2} Row {:<7}: HC_First = Inf.",
                        bank,
                        head_row - pattern.head_offset);
          return;
        }

        if (std::abs((int)hc_first - (int)hammer_count) <= hc_first_resolution)
        // End search as we reached the resolution
        {
          join_vectors(results, previous_bitflips);
          break;
        }

        if (hc_first > 0)
        {
          uint prev_hammer_count = hammer_count;
          hammer_count = (hammer_count + hc_first) / 2;
          hc_first_resolution = std::max(int(hammer_count / 100.0), 1);

          if (hammer_count == prev_hammer_count)
          {
            join_vectors(results, previous_bitflips);
            break;
          }
        }
        else
        {
          logger->trace("Initial hc_first guess {} for Bank {:<2} Row {:<7} is too low. Trying from {}", 
                         hammer_count, bank, head_row - pattern.head_offset, 2 * hammer_count);
          hammer_count *= 2;

          if (exp_time_ns(hammer_count, RAS_scale, RP_scale, aggressor_rows.size()) >= 60000000)
          // We try a last time with max possible hammer_count
          {
            hammer_count = max_hammer_count(60000000, RAS_scale, RP_scale, aggressor_rows.size());
            hc_first_resolution = std::max(int(hammer_count / 100.0), 1);
            is_lastround = true;
          }
        }
      }
    }

    int verify_hc = hc_first * 0.9;
    for (uint r = 0; r < pattern.size(); r++)
      SMC_initialize_row(pattern[r].cacheline, bank, map_row(head_row + r));
    SMC_hammer_rows(bank, aggressor_rows, verify_hc, RAS_scale, RP_scale, is_retention);
    SMC_act_rows(bank, victim_rows);

    SMC_read_rows(bank, victim_rows);

    std::vector<BitFlip> verify_bitflips;
    check_pattern_bitflips(verify_bitflips, victim_rows, verify_hc, bank, head_row - pattern.head_offset);

    if (verify_bitflips.size() == 0) {
      logger->info("Bank {:<2} Row {:<7}: HC_First = {}. Refresh OK!",
                    bank,
                    head_row - pattern.head_offset,
                    hc_first);
    } else {
      logger->info("Bank {:<2} Row {:<7}: HC_First = {}. Refresh mitigation FAILED!",
                    bank,
                    head_row - pattern.head_offset,
                    hc_first);
    }
  };


  void Tester::check_pattern_bitflips(std::vector<BitFlip>& results, const std::vector<Row_t>& victim_rows, uint hammer_count, uint bank, uint pivot_row, bool print_each_row)
  {
    for (const Row_t& row : victim_rows)
    {
      bool has_bitflip = check_row_bitflips(row.cacheline);
      if (has_bitflip)
      {
        int row_offset = row.row_offset;
        if (print_each_row)
          print_row_bitflip_counts(row.cacheline, bank, pivot_row, row_offset);
        generate_row_bitflip_record(results, row.cacheline, hammer_count, bank, pivot_row, row_offset);
      }
    }
  };

  bool Tester::check_row_bitflips(const CacheLine_t& cacheline)
  {
    platform.receiveData(_row_buffer, 8*1024);
    uint64_t* row_buffer = (uint64_t*) _row_buffer;
    uint64_t* cacheline_p64 = (uint64_t*) cacheline.data();
    for(uint cl = 0 ; cl < 128 ; cl++)
    {
      for (uint word = 0 ; word < 8 ; word++)
      {
        uint word_id = cl * 8 + word;
        uint64_t word_read = row_buffer[word_id];
        uint64_t flips_mask = cacheline_p64[word] ^ word_read;

        if (flips_mask != 0)
          return true;
      }
    }

    return false;
  };

  void Tester::generate_row_bitflip_record(std::vector<BitFlip>& results, const CacheLine_t& cacheline, uint hammer_count, uint bank, uint row, int row_offset)
  {
    uint64_t* row_buffer = (uint64_t*) _row_buffer;
    uint64_t* cacheline_p64 = (uint64_t*) cacheline.data();
    for(uint cl = 0 ; cl < 128 ; cl++)
    {
      for (uint word = 0 ; word < 8 ; word++)
      {
        uint word_id = cl * 8 + word;
        uint64_t word_read = row_buffer[word_id];
        uint64_t flips_mask = cacheline_p64[word] ^ word_read;

        for (uint byte = 0; byte < 8; byte++)
        {
          for (uint bit = 0; bit < 8; bit++)
          {
            uint bit_id = byte * 8 + bit;
            if ((flips_mask >> bit_id) & 0x1)
            {
              uint dir = (cacheline_p64[word] >> bit_id) & 0x1;

              BitFlip bf(hammer_count, bank, row, row_offset,
                         cl, word, byte, bit, dir);

              results.push_back(bf);
            }
          }
        }
      }
    }
  }

  void Tester::print_row_bitflip_counts(const CacheLine_t& cacheline, uint bank, uint row, int row_offset)
  {
    uint64_t* row_buffer = (uint64_t*) _row_buffer;
    uint64_t* cacheline_p64 = (uint64_t*) cacheline.data();

    size_t bf_count = 0;

    for(uint cl = 0 ; cl < 128 ; cl++)
    {
      for (uint word = 0 ; word < 8 ; word++)
      {
        uint word_id = cl * 8 + word;
        uint64_t word_read = row_buffer[word_id];
        uint64_t flips_mask = cacheline_p64[word] ^ word_read;

        for (uint byte = 0; byte < 8; byte++)
        {
          for (uint bit = 0; bit < 8; bit++)
          {
            uint bit_id = byte * 8 + bit;
            if ((flips_mask >> bit_id) & 0x1)
              bf_count++;
          }
        }
      }
    }

    logger->info("Bank {:<2} Row {:<7} Offset {:<4}: Found {} bitflips",
                  bank,
                  row, row_offset,
                  bf_count);
  }
};
