import os, sys
import pandas as pd

from script_utils import *

def main():
  try:
    data_root = sys.argv[1]
    module = sys.argv[2]
  except Exception as e:
    print("Usage: python3 parse_hcf_log.py path/to/raw/data/dir module_name")
    raise

  experiment = "HCFIRST"
  results_dir = os.path.join(data_root, module, experiment)

  column_headers = ["module", "pattern", "temperature", "tAggON", "row", "hcf", "ac_min", "itr", "atk_time_ns"]
  results = []

  for root, dirs, files in os.walk(results_dir):
    for file in files:
      filepath = os.path.abspath(os.path.join(root, file))
      pattern, temperature, filename = decode_filepath(filepath) 
      if temperature in [50, 80]:
        if filename.endswith(".log"):
          try:
            _module, tAggON, itr, ras_scale = decode_hcf_filename(file)
            if _module == module:
              lines = read_file_lines(filepath)
              result = []
              for line in lines:
                if "HC_First =" in line and "Inf." not in line:
                  tokens = [int(s) for s in line.split() if s.isdigit()]
                  row = tokens[1]
                  hcf = tokens[2]
                  ac_min = hcf
                  if "double" in pattern:
                    ac_min *= 2
                  result.append([module, pattern, temperature, tAggON, row, hcf, ac_min, itr, ac_to_attack_time_ns(ras_scale, ac_min)])

              df = pd.DataFrame(result)
              results.append(df)
          except Exception as e:
            print(e)
            print(f"{filepath}")
            raise

  df = pd.concat(results)
  df.columns = column_headers
  os.system("mkdir -p ../processed_data/hcf")

  df.to_pickle("../processed_data/hcf/{}_hcf_log.zip".format(module))

if __name__ == "__main__":
  main()