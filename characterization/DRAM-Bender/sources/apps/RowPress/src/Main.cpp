#include "Options.h"
#include "Tester.h"

int main(int argc, char* argv[])
{
  Options opts = Options(argc, argv);
  RowPress::Tester rh_tester(opts.module_id, opts.row_mapping, 
                              opts.num_banks, opts.num_rows, opts.num_cols,
                              opts.prefix, opts.suffix);
  rh_tester.initialize_platform();

  RowPress::Parameters params = opts.generate_param_pack();
  std::vector<RowPress::BitFlip> results;

  rh_tester.test_and_save_results(results, opts.experiment,
                                  opts.row_coverage, opts.bank_list, opts.num_rows_to_test,
                                  params);
}
