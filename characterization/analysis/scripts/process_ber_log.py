import os, sys
import pandas as pd

from script_utils import *

def main():
  try:
    data_root = sys.argv[1]
    module = sys.argv[2]
  except Exception as e:
    print("Usage: python3 parse_ber_log.py path/to/raw/data/dir module_name")
    raise

  experiment = "BER"
  results_dir = os.path.join(data_root, module, experiment)

  column_headers = ["module", "pattern", "temperature", "hc", "ac", "tAggON", "row", "bitflips", "itr", "atk_time_ms"]
  results = []

  for root, dirs, files in os.walk(results_dir):
    for file in files:
      filepath = os.path.abspath(os.path.join(root, file))
      pattern, temperature, filename = decode_filepath(filepath) 
      if temperature in [50, 80]:
        if filename.endswith(".log"):
          _module, hc, tAggON, attack_time_ms, itr = decode_ber_filename(file)
          if _module == module:
            lines = read_file_lines(filepath)
            result = []
            for line in lines:
              if "Found" in line:
                tokens = [int(s) for s in line.split() if s.isdigit()]
                row = tokens[1]
                num_bfs = tokens[2]
                ac = hc
                if "double" in pattern:
                  ac *= 2
                result.append([module, pattern, temperature, hc, ac, tAggON, row, num_bfs, itr, attack_time_ms])
            df = pd.DataFrame(result)
            results.append(df)

  df = pd.concat(results)
  df.columns = column_headers

  os.system("mkdir -p ../processed_data/ber")

  df.to_pickle("../processed_data/ber/{}_ber_log.zip".format(module))

if __name__ == "__main__":
  main()