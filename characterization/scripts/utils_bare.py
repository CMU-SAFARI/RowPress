import os, pathlib, subprocess
import math

exe = "./SoftMC_RowPress"
experiment_types = [
  "BER",
  "HCFIRST",
  "RETENTION_TIME",
  "RETENTION_FAILURE",
  "ROW_MAPPING",
  "FT_BER",
  "FA_BER",
  "ASYM_ROWPRESS_BER",
  "SANITY",
  "ASYM_ROWPRESS_HCFIRST",
  "RH_BER_BASELINE",
  "MIN_TAGGON",
  "MITIGATION_REF",
]



def get_pattern_files():
  patterns_root = "patterns"
  pattern_paths = []
  for dirpath, dirnames, filenames in os.walk(patterns_root):
    for filename in filenames:
      if filename.endswith(".pattern"):
        if not "mapping" in filename:
          pattern_path = os.path.join(dirpath, filename)
          pattern_paths.append(pattern_path)
  
  return pattern_paths


def get_sweep_pattern_files():
  patterns_root = "data_pattern_sweep"
  pattern_paths = []
  for dirpath, dirnames, filenames in os.walk(patterns_root):
    for filename in filenames:
      if filename.endswith(".pattern"):
        if not "mapping" in filename:
          pattern_path = os.path.join(dirpath, filename)
          pattern_paths.append(pattern_path)
  
  return pattern_paths


def get_dp_pattern_files():
  patterns_root = "patterns"
  pattern_paths = []
  for dirpath, dirnames, filenames in os.walk(patterns_root):
    for filename in filenames:
      if ".pattern" in filename:
        if (not "skew" in filename) and (not "mapping" in filename):
          pattern_path = os.path.join(dirpath, filename)
          pattern_paths.append(pattern_path)
  
  return pattern_paths


def setup_results_dir(module_name: str, experiment_id: int, pattern_file: str, temperature: int):
  data_root = "data"
  experiment_type = experiment_types[experiment_id]
  pattern_stem = pathlib.Path(pattern_file).stem
  pathlib.Path(data_root, module_name, experiment_type, pattern_stem, str(temperature) + "C").mkdir(parents=True, exist_ok=True)


def get_results_prefix(module_name: str, experiment_id: int, pattern_file: str, temperature: int):
  data_root = "data"
  experiment_type = experiment_types[experiment_id]
  pattern_stem = pathlib.Path(pattern_file).stem
  return str(pathlib.Path(data_root, module_name, experiment_type, pattern_stem, str(temperature) + "C")) + "/"

def get_prog_time_ms(num_agg_rows: int, RAS_scale: int, RP_scale: int, hc: int):
  return (5 * (RAS_scale) + 5 * (RP_scale) + 9) * num_agg_rows * hc * 6 / 1000000.0

def get_hc(num_agg_rows: int, RAS_scale: int, RP_scale: int, prog_time: int):
  if prog_time == 60 or prog_time == None:
    max_time = 60.1
  else:
    max_time = prog_time + 0.1

  if prog_time < 1.0:
    max_time = prog_time

  if (5 * (RAS_scale) + 5 * (RP_scale) + 9) * num_agg_rows * 6 / 1000000.0 > max_time:
    return 0

  return int(math.floor(max_time / ((5 * (RAS_scale) + 5 * (RP_scale) + 9) * num_agg_rows * 6 / 1000000.0)))


def get_fa_prog_time_ms(num_agg_rows: int, extra_cycles: int, hc: int):
  return (extra_cycles + 9) * num_agg_rows * 6 * hc / 1000000.0 


def get_hc_fa(num_agg_rows: int, extra_cycles: int, prog_time: int = 60):
  return int(math.ceil(prog_time / ((extra_cycles + 9) * num_agg_rows * 6 / 1000000.0)))


def run_sanity(module_name: str, num_banks: int, num_rows: int, num_cols: int, mapping: int,
               pattern_filepath: str, bank_list: list, rows_to_test: int, row_coverage: int,
               hammer_count: int, RAS_scale: int,
               temperature: int,
               iteration: int):
  setup_results_dir(module_name, 8, pattern_filepath, temperature)
  subprocess.run([exe, 
                  "--module", module_name,
                  "--num_banks", str(num_banks),
                  "--num_rows", str(num_rows),
                  "--num_cols", str(num_cols),
                  "--mapping", str(mapping),

                  "--experiment", str(0),
                  "--pattern_file", pattern_filepath,
                  "--bank_list",  ",".join([str(b) for b in bank_list]),
                  "--rows_to_test", str(rows_to_test),
                  "--row_coverage", str(row_coverage),

                  "--hammer_count", str(hammer_count),
                  "--RAS_scale", str(RAS_scale),
                  "--RP_scale", "0",
                  "--is_retention", "1",

                  "--prefix", get_results_prefix(module_name, 8, pattern_filepath, temperature),
                  "--suffix", "_" + str(hammer_count) + "_" + str(RAS_scale) + "_" + "sanity_" + "itr" + str(iteration)
                 ])



def run_rh_bitflips(module_name: str, num_banks: int, num_rows: int, num_cols: int, mapping: int,
                    pattern_filepath: str, bank_list: list, rows_to_test: int, row_coverage: int,
                    hammer_count: int, RAS_scale: int, RP_scale: int, is_retention: bool,
                    temperature: int, atk_time: int,
                    iteration: int):
  setup_results_dir(module_name, 0, pattern_filepath, temperature)
  subprocess.run([exe, 
                  "--module", module_name,
                  "--num_banks", str(num_banks),
                  "--num_rows", str(num_rows),
                  "--num_cols", str(num_cols),
                  "--mapping", str(mapping),

                  "--experiment", str(0),
                  "--pattern_file", pattern_filepath,
                  "--bank_list",  ",".join([str(b) for b in bank_list]),
                  "--rows_to_test", str(rows_to_test),
                  "--row_coverage", str(row_coverage),

                  "--hammer_count", str(hammer_count),
                  "--RAS_scale", str(RAS_scale),
                  "--RP_scale", str(RP_scale),
                  "--is_retention", str(is_retention),

                  "--prefix", get_results_prefix(module_name, 0, pattern_filepath, temperature),
                  "--suffix", "_" + str(hammer_count) + "_" + str(RAS_scale) + "_" + str(atk_time) + "ms_" + "itr" + str(iteration)
                 ])
  
def run_rh_bitflips_baseline(module_name: str, num_banks: int, num_rows: int, num_cols: int, mapping: int,
                             pattern_filepath: str, bank_list: list, rows_to_test: int, row_coverage: int,
                             hammer_count: int, is_retention: bool,
                             temperature: int, atk_time: int,
                             iteration: int):
  setup_results_dir(module_name, 10, pattern_filepath, temperature)
  subprocess.run([exe, 
                  "--module", module_name,
                  "--num_banks", str(num_banks),
                  "--num_rows", str(num_rows),
                  "--num_cols", str(num_cols),
                  "--mapping", str(mapping),

                  "--experiment", str(0),
                  "--pattern_file", pattern_filepath,
                  "--bank_list",  ",".join([str(b) for b in bank_list]),
                  "--rows_to_test", str(rows_to_test),
                  "--row_coverage", str(row_coverage),

                  "--hammer_count", str(hammer_count),
                  "--RAS_scale", str(0),
                  "--RP_scale", str(0),
                  "--is_retention", str(is_retention),

                  "--prefix", get_results_prefix(module_name, 10, pattern_filepath, temperature),
                  "--suffix", "_" + str(hammer_count) + "_" + str(atk_time) + "ms_" + "itr" + str(iteration)
                 ])
  

def run_rh_bitflips_fixed_time(module_name: str, num_banks: int, num_rows: int, num_cols: int, mapping: int,
                               pattern_filepath: str, bank_list: list, rows_to_test: int, row_coverage: int,
                               hammer_count: int, extra_cycles: int, RAS_ratio: float, is_retention: bool,
                               temperature: int, atk_time: int,
                               iteration: int):
  setup_results_dir(module_name, 5, pattern_filepath, temperature)
  subprocess.run([exe, 
                  "--module", module_name,
                  "--num_banks", str(num_banks),
                  "--num_rows", str(num_rows),
                  "--num_cols", str(num_cols),
                  "--mapping", str(mapping),

                  "--experiment", str(5),
                  "--pattern_file", pattern_filepath,
                  "--bank_list",  ",".join([str(b) for b in bank_list]),
                  "--rows_to_test", str(rows_to_test),
                  "--row_coverage", str(row_coverage),

                  "--hammer_count", str(hammer_count),
                  "--extra_cycles", str(extra_cycles),
                  "--RAS_ratio", str(RAS_ratio),
                  "--is_retention", str(is_retention),

                  "--prefix", get_results_prefix(module_name, 5, pattern_filepath, temperature),
                  "--suffix", "_" + str(hammer_count) + "_" + str(extra_cycles) + "_" + str(RAS_ratio) + "_" + str(atk_time) + "ms_" + "itr" + str(iteration)
                 ])


def run_rh_bitflips_fixed_act(module_name: str, num_banks: int, num_rows: int, num_cols: int, mapping: int,
                              pattern_filepath: str, bank_list: list, rows_to_test: int, row_coverage: int,
                              hammer_count: int, extra_cycles: int, RAS_ratio: float, is_retention: bool,
                              temperature: int,
                              iteration: int):
  setup_results_dir(module_name, 6, pattern_filepath, temperature)
  subprocess.run([exe, 
                  "--module", module_name,
                  "--num_banks", str(num_banks),
                  "--num_rows", str(num_rows),
                  "--num_cols", str(num_cols),
                  "--mapping", str(mapping),

                  "--experiment", str(5),
                  "--pattern_file", pattern_filepath,
                  "--bank_list",  ",".join([str(b) for b in bank_list]),
                  "--rows_to_test", str(rows_to_test),
                  "--row_coverage", str(row_coverage),

                  "--hammer_count", str(hammer_count),
                  "--extra_cycles", str(extra_cycles),
                  "--RAS_ratio", str(RAS_ratio),
                  "--is_retention", str(is_retention),

                  "--prefix", get_results_prefix(module_name, 6, pattern_filepath, temperature),
                  "--suffix", "_" + str(hammer_count) + "_" + str(extra_cycles) + "_" + str(RAS_ratio) + "_" + "itr" + str(iteration)
                 ])



def run_rh_hcfirst(module_name: str, num_banks: int, num_rows: int, num_cols: int, mapping: int,
                   pattern_filepath: str, bank_list: list, rows_to_test: int, row_coverage: int,
                   init_hammer_count: int, hc_resolution: int, RAS_scale: int, RP_scale: int, is_retention: bool,
                   temperature: int,
                   iteration: int):
  setup_results_dir(module_name, 1, pattern_filepath, temperature)
  subprocess.run([exe, 
                  "--module", module_name,
                  "--num_banks", str(num_banks),
                  "--num_rows", str(num_rows),
                  "--num_cols", str(num_cols),
                  "--mapping", str(mapping),

                  "--experiment", str(1),
                  "--pattern_file", pattern_filepath,
                  "--bank_list",  ",".join([str(b) for b in bank_list]),
                  "--rows_to_test", str(rows_to_test),
                  "--row_coverage", str(row_coverage),

                  "--hammer_count", str(init_hammer_count),
                  "--hc_resolution", str(hc_resolution),
                  "--RAS_scale", str(RAS_scale),
                  "--RP_scale", str(RP_scale),
                  "--is_retention", str(is_retention),

                  "--prefix", get_results_prefix(module_name, 1, pattern_filepath, temperature),
                  "--suffix", "_" + str(RAS_scale) + "_" + "itr" + str(iteration)
                 ])


def run_mitigation_ref(module_name: str, num_banks: int, num_rows: int, num_cols: int, mapping: int,
                   pattern_filepath: str, bank_list: list, rows_to_test: int, row_coverage: int,
                   init_hammer_count: int, hc_resolution: int, RAS_scale: int, RP_scale: int, is_retention: bool,
                   temperature: int,
                   iteration: int):
  setup_results_dir(module_name, 12, pattern_filepath, temperature)
  subprocess.run([exe, 
                  "--module", module_name,
                  "--num_banks", str(num_banks),
                  "--num_rows", str(num_rows),
                  "--num_cols", str(num_cols),
                  "--mapping", str(mapping),

                  "--experiment", str(9),
                  "--pattern_file", pattern_filepath,
                  "--bank_list",  ",".join([str(b) for b in bank_list]),
                  "--rows_to_test", str(rows_to_test),
                  "--row_coverage", str(row_coverage),

                  "--hammer_count", str(init_hammer_count),
                  "--hc_resolution", str(hc_resolution),
                  "--RAS_scale", str(RAS_scale),
                  "--RP_scale", str(RP_scale),
                  "--is_retention", str(is_retention),

                  "--prefix", get_results_prefix(module_name, 12, pattern_filepath, temperature),
                  "--suffix", "_" + str(RAS_scale) + "_" + "itr" + str(iteration)
                 ])

def run_min_tAggON(module_name: str, num_banks: int, num_rows: int, num_cols: int, mapping: int,
                   pattern_filepath: str, bank_list: list, rows_to_test: int, row_coverage: int,
                   temperature: int,
                   hammer_count: int,
                   iteration: int):
  setup_results_dir(module_name, 11, pattern_filepath, temperature)
  subprocess.run([exe, 
                  "--module", module_name,
                  "--num_banks", str(num_banks),
                  "--num_rows", str(num_rows),
                  "--num_cols", str(num_cols),
                  "--mapping", str(mapping),

                  "--experiment", str(8),
                  "--pattern_file", pattern_filepath,
                  "--bank_list",  ",".join([str(b) for b in bank_list]),
                  "--rows_to_test", str(rows_to_test),
                  "--row_coverage", str(row_coverage),

                  "--hammer_count", str(hammer_count),

                  "--prefix", get_results_prefix(module_name, 11, pattern_filepath, temperature),
                  "--suffix", "_" + str(hammer_count) + "_" + "itr" + str(iteration)
                 ])


def run_asym_rowpress_ber(module_name: str, num_banks: int, num_rows: int, num_cols: int, mapping: int,
                          pattern_filepath: str, bank_list: list, rows_to_test: int, row_coverage: int,
                          head_hammer_count: int, tail_hammer_count: int, head_RAS_scale: int, tail_RAS_scale: int, is_retention: bool,
                          temperature: int, atk_time: int,
                          iteration: int):
  setup_results_dir(module_name, 7, pattern_filepath, temperature)
  subprocess.run([exe, 
                  "--module", module_name,
                  "--num_banks", str(num_banks),
                  "--num_rows", str(num_rows),
                  "--num_cols", str(num_cols),
                  "--mapping", str(mapping),

                  "--experiment", str(6),
                  "--pattern_file", pattern_filepath,
                  "--bank_list",  ",".join([str(b) for b in bank_list]),
                  "--rows_to_test", str(rows_to_test),
                  "--row_coverage", str(row_coverage),

                  "--head_hammer_count", str(head_hammer_count),
                  "--tail_hammer_count", str(tail_hammer_count),
                  "--head_RAS_scale", str(head_RAS_scale),
                  "--tail_RAS_scale", str(tail_RAS_scale),
                  "--is_retention", str(is_retention),

                  "--prefix", get_results_prefix(module_name, 7, pattern_filepath, temperature),
                  "--suffix", "_" + str(head_hammer_count) + "_" + str(tail_hammer_count) + "_" + str(head_RAS_scale) + "_" + str(atk_time) + "ms_" + "itr" + str(iteration)
                 ])


def run_asym_rowpress_hcf(module_name: str, num_banks: int, num_rows: int, num_cols: int, mapping: int,
                          pattern_filepath: str, bank_list: list, rows_to_test: int, row_coverage: int,
                          head_hammer_count: int, tail_hammer_count: int, hc_resolution: int, head_RAS_scale: int, tail_RAS_scale, is_retention: bool,
                          temperature: int, time_cap_ms: int,
                          iteration: int):
  setup_results_dir(module_name, 9, pattern_filepath, temperature)
  subprocess.run([exe, 
                  "--module", module_name,
                  "--num_banks", str(num_banks),
                  "--num_rows", str(num_rows),
                  "--num_cols", str(num_cols),
                  "--mapping", str(mapping),

                  "--experiment", str(7),
                  "--pattern_file", pattern_filepath,
                  "--bank_list",  ",".join([str(b) for b in bank_list]),
                  "--rows_to_test", str(rows_to_test),
                  "--row_coverage", str(row_coverage),

                  "--head_hammer_count", str(head_hammer_count),
                  "--tail_hammer_count", str(tail_hammer_count),
                  "--hc_resolution", str(hc_resolution),
                  "--head_RAS_scale", str(head_RAS_scale),
                  "--tail_RAS_scale", str(tail_RAS_scale),

                  "--ret_time_ms", str(time_cap_ms),

                  "--is_retention", str(is_retention),

                  "--prefix", get_results_prefix(module_name, 9, pattern_filepath, temperature),
                  "--suffix", "_" + str(head_hammer_count) + str(head_RAS_scale) + "_" + str(tail_hammer_count) + "_" + str(tail_RAS_scale) + "_" + str(time_cap_ms) + "ms_" + "itr" + str(iteration)
                 ])


def run_retention_time(module_name: str, num_banks: int, num_rows: int, num_cols: int, mapping: int,
                       pattern_filepath: str, bank_list: list, rows_to_test: int, row_coverage: int,
                       init_retention_time_ms: int, 
                       temperature: int,
                       iteration: int):
  setup_results_dir(module_name, 2, pattern_filepath, temperature)
  subprocess.run([exe, 
                  "--module", module_name,
                  "--num_banks", str(num_banks),
                  "--num_rows", str(num_rows),
                  "--num_cols", str(num_cols),
                  "--mapping", str(mapping),

                  "--experiment", str(2),
                  "--pattern_file", pattern_filepath,
                  "--bank_list",  ",".join([str(b) for b in bank_list]),
                  "--rows_to_test", str(rows_to_test),
                  "--row_coverage", str(row_coverage),

                  "--ret_time_ms", str(init_retention_time_ms),

                  "--prefix", get_results_prefix(module_name, 2, pattern_filepath, temperature),
                  "--suffix", "_" + "itr" + str(iteration)
                ])


def run_retention_failure(module_name: str, num_banks: int, num_rows: int, num_cols: int, mapping: int,
                           pattern_filepath: str, bank_list: list, rows_to_test: int, row_coverage: int,
                           retention_time_ms: int, 
                           temperature: int,
                           iteration: int):
  setup_results_dir(module_name, 3, pattern_filepath, temperature)
  subprocess.run([exe, 
                  "--module", module_name,
                  "--num_banks", str(num_banks),
                  "--num_rows", str(num_rows),
                  "--num_cols", str(num_cols),
                  "--mapping", str(mapping),

                  "--experiment", str(3),
                  "--pattern_file", pattern_filepath,
                  "--bank_list",  ",".join([str(b) for b in bank_list]),
                  "--rows_to_test", str(rows_to_test),
                  "--row_coverage", str(row_coverage),

                  "--ret_time_ms", str(retention_time_ms),

                  "--prefix", get_results_prefix(module_name, 3, pattern_filepath, temperature),
                  "--suffix", "_" + str(retention_time_ms) + "_" + "itr" + str(iteration)
                ])