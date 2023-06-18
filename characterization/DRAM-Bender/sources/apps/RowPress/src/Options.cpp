#include <string>
#include <filesystem>

#include "Options.h"

namespace po = boost::program_options;

Options::Options(int argc, char* argv[])
{
  po::options_description help("General options");
  help.add_options()("help,h", "Display the help message\n");

  po::options_description dram_org("DRAM organization options");
  dram_org.add_options()
  ("module",    po::value<std::string>(&module_id)->required(),             "Specify the module id")
  ("mapping",   po::value<RowPress::RowMapping>(&row_mapping)->required(), "Specify the row address scheme\n"
                                                                            "Supported schemes:\n"
                                                                            "  0 = Sequential\n"
                                                                            "  1 = Samsung 16\n"
                                                                            "  2 = MI0\n")

  ("num_banks", po::value<uint>(&num_banks)->default_value(16),             "Specify the number of banks in the chip")
  ("num_rows",  po::value<uint>(&num_rows )->default_value(65536),          "Specify the number of rows per bank")
  ("num_cols",  po::value<uint>(&num_cols )->default_value(128),            "Specify the number of columns (cachelines) per row\n")
  ;

  po::options_description exp_settings("Experiment settings options");
  exp_settings.add_options()
  ("experiment",    po::value<RowPress::ExperimentType>(&experiment)->required(),
                                                                            "What kind of experiment to perform.\n"
                                                                            "Supported experiments:\n"
                                                                            "  0 = RowHammer Bitflips\n"
                                                                            "  1 = RowHammer HCFirst\n"
                                                                            "  2 = Retention Time\n"
                                                                            "  3 = Retention Failure\n"
                                                                            "  4 = Row Mapping\n"
                                                                            "  5 = RowHammer Bitflips (RAS Ratio)\n"
                                                                            "  6 = Asymmetric RowPress BER\n"
                                                                            "  7 = Asymmetric RowPress HCFirst\n")

  ("pattern_file",  po::value<std::string>(&pattern_filename),              "Whether to use a customized RowHammer pattern specified by a file.\n"
                                                                            "Conflicts with <act_pattern> and <data_pattern>.\n")

  ("acc_pattern",   po::value<RowPress::AccessPat>(&access_pattern),       "Specify the access pattern\n" 
                                                                            "Supported patterns:\n"
                                                                            "  0 = Single-sided\n"
                                                                            "  1 = Double-sided\n")

  ("data_pattern",  po::value<RowPress::DataPat>(&data_pattern),           "Specify the data pattern\n" 
                                                                            "Supported patterns:\n"
                                                                            "  0 = All zeros\n"
                                                                            "  1 = All ones\n"
                                                                            "  2 = Checkerboard\n"
                                                                            "  3 = Checkerboard Inv.\n"
                                                                            "  4 = Col. Stripe\n"
                                                                            "  5 = Col. Stripe Inv.\n"
                                                                            "  6 = Row Stripe\n"
                                                                            "  7 = Row Stripe Inv.\n")
  ("blast_radius",  po::value<uint>(&blast_radius)->default_value(3),       "Specify the number of victim rows to check around the pivot rows")


  ("bank_list",     po::value<std::vector<uint>>(&bank_list)->required()->multitoken(),   
                                                                            "Specify the number of banks to test (starting from bank 0)")
  ("rows_to_test",  po::value<uint>(&num_rows_to_test)->required(),         "Specify the number of rows to test per bank\n" 
                                                                            "(will test the first, the middle, and the last X rows of the bank)\n")
  ("row_coverage",  po::value<RowPress::RowCoverage>(&row_coverage)->required(),
                                                                            "Specify the coverage of the rows\n"
                                                                            "Supported coverage:\n"
                                                                            "  0 = Full (will test <rows_to_test> rows from the beginning of the bank)\n"
                                                                            "  1 = HMT Sampling (will test <rows_to_test> rows each from the Head, Middle and Tail of the bnak)\n")


  ("hammer_count",       po::value<uint>(&hammer_count)->default_value(100000),  "Specify the number of activations per aggressor row")
  ("hc_resolution",      po::value<uint>(&hc_resolution)->default_value(500),    "Specify the resolution of HC First.")
  ("RAS_scale",          po::value<uint>(&RAS_scale)->default_value(0),          "Specify the aggressor row on time (1 unit = 30ns)")
  ("RP_scale",           po::value<uint>(&RP_scale)->default_value(0),           "Specify the aggressor row off time (1 unit = 18ns)")

  ("head_hammer_count",  po::value<uint>(&head_hammer_count)->default_value(2),  "Specify the number of head activations per aggressor row (Asymmetric RowPress)")
  ("tail_hammer_count",  po::value<uint>(&tail_hammer_count)->default_value(5),  "Specify the number of tail activations per aggressor row (Asymmetric RowPress)")
  ("head_RAS_scale",     po::value<uint>(&head_RAS_scale)->default_value(0),     "Specify the aggressor row on time (1 unit = 30ns) for head activations (Asymmetric RowPress)")
  ("tail_RAS_scale",     po::value<uint>(&tail_RAS_scale)->default_value(0),     "Specify the aggressor row on time (1 unit = 30ns) for tail activations (Asymmetric RowPress)")


  ("extra_cycles",  po::value<uint>(&extra_cycles)->default_value(0),       "Specify the extra fabric cycles on top of tRAS + tRP between two ACT cmds")
  ("RAS_ratio",     po::value<float>(&RAS_ratio)->default_value(0.5f),      "Specify the what portion of the extra abric cycles is added to tAggOn")

  ("is_retention",  po::value<bool>(&is_retention)->default_value(false),   "Whether to perform retention error sanity checks")


  ("ret_time_ms",   po::value<uint>(&ret_time_ms)->default_value(512),      "Specify the retention time in ms for retention-related tests.")


  ("prefix",        po::value<std::string>(&prefix),                        "Prefix for the log and results file.")
  ("suffix",        po::value<std::string>(&suffix),                        "Suffix for the log and results file.")
  ;

  po::options_description all;
  all.add(help).add(dram_org).add(exp_settings);

  po::variables_map vm;
  try
  {
    po::store(po::parse_command_line(argc, argv, all), vm); 
    po::notify(vm);   

    // Check for pattern conflicts
    if (vm.count("pattern_file"))
    {
      if (vm.count("acc_pattern") || vm.count("data_pattern"))
        spdlog::warn("Pattern file is given. Ignore acc_pattern and data_pattern.");
    }
    else
    {
      if (!(vm.count("acc_pattern") && vm.count("data_pattern")))
      {
        spdlog::critical("Pattern file is not given. Both acc_pattern and data_pattern must be set!");
        throw std::logic_error("Pattern file is not given. Both acc_pattern and data_pattern must be set!");
      }
    }

    // Check whether bank_list is valid
    for (uint bank: bank_list)
    {
      if (bank >= num_banks)
      {
        spdlog::critical("Bank list contains a bank id ({}) exceeding the number of banks ({})!", bank, num_banks);
        throw std::logic_error("Invalid bank list!");
      }
    }
  }
  catch(const po::error& e)
  {
    if (vm.count("help") || argc == 1) 
    {
      std::cout << all << "\n";
      std::exit(1);
    }
    else
    {
      std::cerr << "CRITICAL: " << e.what() << std::endl;
      std::cerr << "  Use \"--help\" to view all options" << std::endl;
      std::exit(1);
    }
  }

  if (pattern_filename == "")
    spdlog::info("No pattern file given");
}

RowPress::Parameters Options::generate_param_pack()
{
  if (pattern_filename != "")
    return RowPress::Parameters(pattern_filename,
                                 hammer_count, hc_resolution, RAS_scale, RP_scale, is_retention,
                                 ret_time_ms, extra_cycles, RAS_ratio, 
                                 head_hammer_count, tail_hammer_count, head_RAS_scale, tail_RAS_scale);
  else
    return RowPress::Parameters(access_pattern, data_pattern, blast_radius,
                                 hammer_count, hc_resolution, RAS_scale, RP_scale, is_retention,
                                 ret_time_ms, extra_cycles, RAS_ratio,
                                 head_hammer_count, tail_hammer_count, head_RAS_scale, tail_RAS_scale);
}


namespace RowPress
{
  template<typename T>
  typename std::enable_if<std::is_enum<T>::value, std::istream& >::type
  operator>>(std::istream& in, T& e)
  {
    uint i;
    in >> i;

    if (i > (uint) T::MAX)
      e = T::MAX;
    else
      e = T(i);

    return in;
  }
};