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

  experiment = "MIN_TAGGON"
  results_dir = os.path.join(data_root, module, experiment)

  column_headers = ["module", "pattern", "temperature", "row", "act_count", "min_tAggON", "itr"]
  results = []

  for root, dirs, files in os.walk(results_dir):
    for file in files:
      filepath = os.path.abspath(os.path.join(root, file))
      pattern, temperature, filename = decode_filepath(filepath) 
      if filename.endswith(".log"):
        try:

          _module, act_count, itr = decode_min_tAggON_filename(file)
          if _module == module:
            lines = read_file_lines(filepath)
            result = []
            for line in lines:
              if "Min RAS Scale =" in line and "Inf." not in line:
                tokens = [int(s) for s in line.split() if s.isdigit()]
                row = tokens[1]
                min_RAS_scale = tokens[2]
                if min_RAS_scale != 0:
                  result.append([module, pattern, temperature, row, act_count, ras_scale_to_tAggON_ns(min_RAS_scale), itr])

            df = pd.DataFrame(result)
            if not df.empty:
              results.append(df)
        except Exception as e:
          print(f"{filepath}")
  try:
    df = pd.concat(results)
    df.columns = column_headers
    os.system("mkdir -p ../processed_data/min_taggon")
    df.to_pickle("../processed_data/min_taggon/{}_mintAggON_log.zip".format(module))
  except Exception as e:
    print(f"{module}")
    raise

if __name__ == "__main__":
  main()