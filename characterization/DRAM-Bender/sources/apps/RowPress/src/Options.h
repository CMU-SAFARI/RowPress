#pragma once

#include <type_traits>
#include <boost/program_options.hpp>

#include "Tester.h"

// Needed for boost::program_options to accept enums
namespace RowPress
{
  template<typename T>
  typename std::enable_if<std::is_enum<T>::value, std::istream& >::type
  operator>>(std::istream& in, T& e);
};

struct Options
{
  public:
    // DRAM Organization
    std::string module_id;
    RowPress::RowMapping row_mapping;

    uint num_banks;
    uint num_rows ;
    uint num_cols ;


    // Experiment Settings
    RowPress::ExperimentType experiment = RowPress::ExperimentType::MAX;
    RowPress::AccessPat access_pattern = RowPress::AccessPat::MAX;
    RowPress::DataPat data_pattern = RowPress::DataPat::MAX;
    std::string pattern_filename = "";

    std::vector<uint> bank_list;
    uint num_banks_to_test;
    uint num_rows_to_test;  
    uint blast_radius;
    RowPress::RowCoverage row_coverage = RowPress::RowCoverage::MAX;
      
    uint hammer_count;
    uint head_hammer_count;
    uint tail_hammer_count;
    uint head_RAS_scale;
    uint tail_RAS_scale;
    
    uint hc_resolution;
    uint RAS_scale;
    uint RP_scale;

    uint extra_cycles;
    float RAS_ratio;

    /**
     * Applies to experiments that takes a long period of time
     * If enabled, the ACT/PRE commands in hammer_rows() will be
     * replaced with NOPs, thus essentially verifies whether the 
     * bitflips are retention errors or not for the same experiment
     */
    bool is_retention;

    uint ret_time_ms;

    // For logs and result files
    std::string prefix;
    std::string suffix;


  public:
    Options(int argc, char* argv[]);
    
    RowPress::Parameters generate_param_pack();
};

