#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <random>
#include <bitset>
#include <chrono>
#include <thread>

#include <boost/program_options.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "platform.h"
#include "instruction.h"
#include "prog.h"

#include "RowPress.h"
#include "SMC_Registers.h"

namespace RowPress
{
  Inst all_nops();

  class Tester
  {
    private:
      /// SoftMC infrastructure
      SoftMCPlatform platform;
      uint8_t* _row_buffer = nullptr;

      /// DRAM organization
      const std::string module_id;
      const RowMapping row_mapping = RowMapping::MAX;
      uint num_banks;
      uint num_rows;
      uint num_cols;

      /// Output
      std::string prefix;
      std::string suffix;
      std::ofstream results_file;

      std::shared_ptr<spdlog::logger> logger;


    public:
      Tester(const std::string& module_id, RowMapping row_mapping, 
             uint num_banks, uint num_rows, uint num_cols,
             const std::string& prefix, const std::string& suffix); 
      void initialize_platform();

      void reset(const std::string& new_prefix, const std::string& new_suffix);

      void test_chunk(std::vector<BitFlip>& results, ExperimentType experiment,
                      uint bank, uint start_row, uint num_rows_to_test, 
                      const Parameters& params);

      void test_bank(std::vector<BitFlip>& results, ExperimentType experiment,
                     RowCoverage row_coverage, uint bank, uint num_rows_to_test, 
                     const Parameters& params);

      void test(std::vector<BitFlip>& results, ExperimentType experiment,
                RowCoverage row_coverage, std::vector<uint> bank_list, uint num_rows_to_test, 
                const Parameters& params);

      void test_and_save_results(std::vector<BitFlip>& results, ExperimentType experiment,
                                 RowCoverage row_coverage, std::vector<uint> bank_list, uint num_rows_to_test, 
                                 const Parameters& params);

      void test_bitflips(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                         std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                         uint hammer_count, uint RAS_scale, uint RP_scale, 
                         bool is_retention = false);

      void test_bitflips_RAS_ratio(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                                   std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                                    uint hammer_count, uint extra_cycles, float RAS_ratio, 
                                    bool is_retention = false);
                         
      void test_hcfirst(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                        std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                        uint initial_hammer_count, uint resolution, uint RAS_scale, uint RP_scale,
                        bool is_retention = false);

      void test_mitigation_ref(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                               std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                               uint initial_hammer_count, uint resolution, uint RAS_scale, uint RP_scale,
                               bool is_retention = false);
 

      void test_retention_failures(std::vector<BitFlip>& results, const Pattern& pattern, std::vector<Row_t>& victim_rows, uint bank, uint head_row, 
                                   uint ret_time_ms = 128);
      void test_retention_time(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                               uint initial_retention_time_ms = 128);

      void test_rowmapping(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                           std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                           uint hammer_count);

      void test_asymmetric_rowpress_ber(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                                        std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                                        uint head_hammer_count, uint tail_hammer_count, uint head_RAS_scale, uint tail_RAS_scale, bool is_retention = false);

      void test_asymmetric_rowpress_hcfirst(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                                            std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                                            uint head_hammer_count, uint initial_tail_hammer_count, uint resolution, uint head_RAS_scale, uint tail_RAS_scale, uint total_time_cap_ns = 60000000,
                                            bool is_retention = false);

      void test_taggon_min(std::vector<BitFlip>& results, const Pattern& pattern, uint bank, uint head_row, 
                           std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows,
                           uint hammer_count,
                           bool is_retention = false);

      bool verify_mapping(const Pattern& pattern, int curr_pivot_row);
      void generate_aggressor_and_victim_rows(const Pattern& pattern, uint head_row,
                                              std::vector<Row_t>& aggressor_rows, std::vector<Row_t>& victim_rows);

    private:
      void SMC_initialize_row(const CacheLine_t& cacheline, uint bank, uint row);
      void SMC_initialize_rows(const CacheLine_t& cacheline, uint bank, std::vector<uint>& rows);

      void SMC_hammer_row(uint bank, uint aggressor_row, uint hammer_count, uint RAS_scale, uint RP_scale);
      void SMC_hammer_row_and_read_victim(uint bank, uint aggressor_row, uint hammer_count, uint RAS_scale, uint RP_scale, uint victim_row);

      void SMC_hammer_row_double(uint bank, uint aggressor_row_1, uint aggressor_row_2, uint hammer_count, uint RAS_scale, uint RP_scale);
      void SMC_hammer_row_double_and_read_victim(uint bank, uint aggressor_row_1, uint aggressor_row_2, uint hammer_count, uint RAS_scale, uint RP_scale, uint victim_row);

      void SMC_hammer_rows(uint bank, const std::vector<Row_t>& aggressor_rows, uint hammer_count, uint RAS_scale, uint RP_scale, bool is_retention = false);
      void SMC_hammer_rows_RAS_ratio(uint bank, const std::vector<Row_t>& aggressor_rows, uint hammer_count, uint act_interval_cycles, float RAS_ratio, bool is_retention = false);
      void SMC_asymmetric_rowpress(uint bank, const std::vector<Row_t>& aggressor_rows, uint head_RAS_scale, uint head_hammer_count, uint tail_RAS_scale, uint tail_hammer_count, bool is_retention = false);

      uint exp_time_ns(uint hammer_count, uint RAS_scale, uint RP_scale, uint num_agg_rows, uint extra_cycles = 0);
      uint asym_exp_time_ns(uint head_hammer_count, uint head_RAS_scale, uint tail_hammer_count, uint tail_RAS_scale, uint num_agg_rows);
      uint max_hammer_count(uint prog_time_ns, uint RAS_scale, uint RP_scale, uint num_agg_rows);
      uint max_RAS_scale(uint prog_time_ns, uint hammer_count, uint RP_scale, uint num_agg_rows);
      // void SMC_hammer_rows_and_read_victims(uint bank, const std::vector<Row_t>& aggressor_rows, uint hammer_count, uint RAS_scale, uint RP_scale);

      void SMC_read_row(uint bank, uint row);
      void SMC_read_rows(uint bank, const std::vector<Row_t>& victim_rows);

      void SMC_act_rows(uint bank, const std::vector<Row_t>& victim_rows);

      void SMC_retention_row(const CacheLine_t& cacheline, uint bank, uint row, uint64_t sleep_cycles);
      void SMC_retention_rows(const std::vector<Row_t>& victim_rows, uint bank, uint64_t sleep_cycles);

      void SMC_add_sleep(Program& p, uint64_t sleep_cycles);


    private:
      uint map_row(uint row);

      void check_pattern_bitflips(std::vector<BitFlip>& results, const std::vector<Row_t>& victim_rows, uint hammer_count, uint bank, uint pivot_row, bool print_each_row = false);
      bool check_row_bitflips(const CacheLine_t& cacheline);
      void generate_row_bitflip_record(std::vector<BitFlip>& results, const CacheLine_t& cacheline, uint hammer_count, uint bank, uint row, int row_offset);
      void print_row_bitflip_counts(const CacheLine_t& cacheline, uint bank, uint row, int row_offset);

    private:
      void set_loggers();
      void log_experiment_params(ExperimentType experiment, const Parameters& params);
  };
};
