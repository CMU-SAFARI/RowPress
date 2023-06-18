import sys
import time
import subprocess
import scripts.utils_bare as utils

def main():
  # Always build the executable
  build_exe = subprocess.run(["./build.sh"], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  try:
    build_exe.check_returncode()
  except subprocess.CalledProcessError:
    print("Error building SMC exe. Abort!")
    exit(-1)

  # DRAM Module Info
  # Change this to match your module under test
  module_name = sys.argv[1]
  mapping = 0
  num_banks = 16
  num_rows = 65536
  num_cols = 128

  try:
    print(f"{module_name}, {num_banks} banks, {num_rows} rows, {num_cols} cols, mapping {mapping}.")
    time.sleep(5)

    bank_list = [1] 
    rows_to_test = 1024 # 1024 rows per chunk
    row_coverage = 1    # 3 chunks from the head, middle, and tail of the bank
    num_itr = 5

    pattern_paths = utils.get_pattern_files()


    ############################################################################
    ############################################################################
    #
    # All our internal infrastructure code has been removed. 
    # You need to add your own code for e.g. controlling the temperature
    #
    ############################################################################
    ############################################################################

    ############################# HC First (AC_min) ##############################
    temperature_list = [50, 80]
    # MAKE SURE TO CHANGE THE TEMPERATURE OF THE MODULE!!!
    for temperature in temperature_list:
      ras_scale_list = [0, 1, 2, 5, 10, 20, 50, 100, 258, 500, 1000, 2338, 5000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000]
      for ras_scale in ras_scale_list:
        for pattern_file in pattern_paths:
            hammer_count = utils.get_hc(1, ras_scale, 0, 60)
            if "double-" in pattern_file:
              hammer_count = int(hammer_count / 2)

            if hammer_count >= 2:
              for itr in range(num_itr):
                  utils.run_rh_hcfirst(module_name, num_banks, num_rows, num_cols, mapping, 
                                      pattern_file, bank_list, rows_to_test, row_coverage, 
                                      hammer_count, 1, ras_scale, 0, False, 
                                      temperature,
                                      itr)
    #############################################################################

    #################### HC First (Data Pattern Sensitivity) ####################
    temperature_list = [50, 80]
    # MAKE SURE TO CHANGE THE TEMPERATURE OF THE MODULE!!!

    data_pattern_sweep_pattern_files = utils.get_sweep_pattern_files()

    for temperature in temperature_list:
      ras_scale_list = [0, 1, 20, 258, 2338, 10000, 100000, 200000, 500000, 1000000, 2000000]
      for ras_scale in ras_scale_list:
        for pattern_file in data_pattern_sweep_pattern_files:
            hammer_count = utils.get_hc(1, ras_scale, 0, 60)
            if "double-" in pattern_file:
              hammer_count = int(hammer_count / 2)

            if hammer_count >= 2:
              for itr in range(num_itr):
                  utils.run_rh_hcfirst(module_name, num_banks, num_rows, num_cols, mapping, 
                                      pattern_file, bank_list, rows_to_test, row_coverage, 
                                      hammer_count, 1, ras_scale, 0, False, 
                                      temperature,
                                      itr)
    #############################################################################


    #################################### BER ####################################
    temperature_list = [50, 80]
    # MAKE SURE TO CHANGE THE TEMPERATURE OF THE MODULE!!!
    for temperature in temperature_list:
      ################# Sanity ##################
      for pattern_file in pattern_paths:
        hammer_count = 1
        if "single-" in pattern_file:
          hammer_count = 2

        for itr in range(num_itr):
          utils.run_sanity(module_name, num_banks, num_rows, num_cols, mapping, 
                          pattern_file, bank_list, rows_to_test, row_coverage, 
                          hammer_count, 1000000,  
                          temperature, 
                          itr)
      ##########################################

      ras_scale_list = [0, 1, 2, 5, 10, 20, 50, 100, 258, 500, 1000, 2338, 5000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000, 2000000]
      atk_time_list = [60]
      for atk_time in atk_time_list:
        for ras_scale in ras_scale_list:
          hammer_count = utils.get_hc(1, ras_scale, 0, atk_time)
          single_time = utils.get_prog_time_ms(1, ras_scale, 0, hammer_count)
          double_time = utils.get_prog_time_ms(2, ras_scale, 0, int(hammer_count / 2))
          if abs(single_time - atk_time) <= 0.1 and abs(double_time - atk_time) <= 0.1:
            if hammer_count > 0 and int(hammer_count / 2) > 0:
              for pattern_file in pattern_paths:
                if "double-" in pattern_file:
                  hammer_count = int(hammer_count / 2)
                elif "single-" in pattern_file:
                  hammer_count = utils.get_hc(1, ras_scale, 0, atk_time)

                for itr in range(num_itr):
                  utils.run_rh_bitflips(module_name, num_banks, num_rows, num_cols, mapping, 
                                        pattern_file, bank_list, rows_to_test, row_coverage, 
                                        hammer_count, ras_scale, 0, False, 
                                        temperature, atk_time,
                                        itr)

      ras_scale_list = [2000000]
      atk_time_list = [60]
      for atk_time in atk_time_list:
        for ras_scale in ras_scale_list:
          for pattern_file in pattern_paths:
            if "single-" in pattern_file:
              hammer_count = 1
              for itr in range(num_itr):
                utils.run_rh_bitflips(module_name, num_banks, num_rows, num_cols, mapping, 
                                      pattern_file, bank_list, rows_to_test, row_coverage, 
                                      hammer_count, ras_scale, 0, False, 
                                      temperature, atk_time,
                                      itr)
                utils.run_sanity(module_name, num_banks, num_rows, num_cols, mapping, 
                                  pattern_file, bank_list, rows_to_test, row_coverage, 
                                  hammer_count, ras_scale,  
                                  temperature, 
                                  itr)
    #############################################################################


    ###################### Min tAggON (Temperature Sweep) #######################
    temperature_list = [80, 75, 70, 65, 60, 55, 50]
    hc_list = [1, 5, 10, 50, 100, 500, 1000, 5000, 10000]
    for temperature in temperature_list:
      # MAKE SURE TO CHANGE THE TEMPERATURE OF THE MODULE!!!
      for hc in hc_list:
        for pattern_file in pattern_paths:
          if "single" in pattern_file:
            for itr in range(num_itr):
              utils.run_min_tAggON(module_name, num_banks, num_rows, num_cols, mapping, 
                                   pattern_file, bank_list, rows_to_test, row_coverage, 
                                   temperature,
                                   hc,
                                   itr)
    ###########################################################################


    ####################### FTBER (RowPress-ONOFF Pattern) #######################
    temperature_list = [50, 80]
    # MAKE SURE TO CHANGE THE TEMPERATURE OF THE MODULE!!!
    for temperature in temperature_list:
      atk_time_list = [60]
      ras_ratio_list = [0.00, 0.25, 0.50, 0.75, 1.00]
      extra_cycles_list = [20, 40, 100, 200, 400, 1000, 10000, 100000]
      for atk_time in atk_time_list:
        for ras_ratio in ras_ratio_list:
          for extra_cycles in extra_cycles_list:
            for pattern_file in pattern_paths:
              hammer_count = utils.get_hc_fa(1, extra_cycles, atk_time)
              if "double-" in pattern_file:
                hammer_count = int(hammer_count / 2)
              if hammer_count != 0:
                for itr in range(num_itr):
                  utils.run_rh_bitflips_fixed_time(module_name, num_banks, num_rows, num_cols, mapping, 
                                                    pattern_file, bank_list, rows_to_test, row_coverage, 
                                                    hammer_count, extra_cycles, ras_ratio, False, 
                                                    temperature, atk_time,
                                                    itr)
    #############################################################################

    ################################ Retention ##################################
    temperature = 80 # MAKE SURE TO CHANGE THE TEMPERATURE OF THE MODULE!!!
    for pattern_file in pattern_paths:
      for itr in range(num_itr):
          utils.run_retention_failure(module_name, num_banks, num_rows, num_cols, mapping, 
                                      pattern_file, bank_list, rows_to_test, row_coverage, 
                                      4096, 
                                      temperature,
                                      itr)
    #############################################################################

  except Exception as e:
    print(str(e))
    exit(-1)


if __name__ == "__main__":
  main()