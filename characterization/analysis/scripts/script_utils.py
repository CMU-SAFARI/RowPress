def ras_scale_to_tAggON_ns(ras_scale: int) -> int:
  # Convert the RAS scale number to tAggON in nanoseconds
  return 36 + (ras_scale) * 5 * 6


def extra_delay_to_ns(extra_delay: int) -> int:
  # Convert the extra_delay number to extra delay in nanoseconds
  return 6 * extra_delay


def extra_delay_ratio_to_tAggON_OFF(extra_delay: int, ratio: float):
  # Convert the extra_delay number and the ratio into tAggON and tAggOFF in nanoseconds
  extra_delay_ns = 6 * extra_delay
  tAggON = extra_delay_ns * ratio + 36                      # 36ns tRAS
  tAggOFF = (extra_delay_ns - extra_delay_ns * ratio) + 15  # 15ns tRP

  return tAggON, tAggOFF


def ac_to_attack_time_ns(ras_scale: int, ac: int) -> int:
  # Convert the hcfirst to attack time
  return (9 + (ras_scale) * 5) * 6 * ac


def decode_filepath(path: str):
  # Get experiment info from the raw data directory structure: module/experiment/access_pattern/temperature/filename
  import os
  try:
    path_components = os.path.normpath(path).split(os.path.sep)

    access_pattern    = path_components[-3]
    temperatue        = int(path_components[-2].replace("C", ""))
    filename          = path_components[-1]
  except Exception as e:
    print(f"Failed to parse \"{path}\"!")
    print(e)
    raise
  
  return access_pattern, temperatue, filename

def decode_filepath_bitflipdirection(path: str):
  # Get experiment info from the raw data directory structure: module/experiment/access_pattern/temperature/filename
  import os
  path_components = os.path.normpath(path).split(os.path.sep)

  if "fix_data" in path_components:
    path_components = path_components[path_components.index("fix_data"):]

  if "new_data" in path_components:
    path_components = path_components[path_components.index("new_data"):]

  if "data" in path_components:
    path_components = path_components[path_components.index("data"):]
  
  module            = path_components[1]
  experiment        = path_components[2]
  access_pattern    = path_components[3]
  temperatue        = int(path_components[4].replace("C", ""))
  filename          = path_components[5]
  return module, experiment, access_pattern, temperatue, filename

def decode_retention_filename(filename: str):
  # Get experiment parameters for RETENTION_FAILURE experiment results
  import os
  try:
    file = os.path.splitext(filename)[0]
    tokens       = file.split("_")

    module       = tokens[0]
    wait_time_ms = int(tokens[1])
    itr          = int(tokens[2].replace("itr", ""))
  except Exception as e:
    print(f"Failed to parse \"{filename}\"!")
    print(e)
    raise

  return module, wait_time_ms, itr


def decode_hcf_filename(filename: str):
  # Get experiment parameters for RH_HCFirst experiment results
  import os
  try:
    file = os.path.splitext(filename)[0]
    tokens = file.split("_")

    module    = tokens[0]
    ras_scale = int(tokens[1])
    tAggON    = ras_scale_to_tAggON_ns(ras_scale)
    itr       = int(tokens[2].replace("itr", ""))
  except Exception as e:
    print(f"Failed to parse \"{filename}\"!")
    print(e)
    raise

  return module, tAggON, itr, ras_scale


def decode_min_tAggON_filename(filename: str):
  # Get experiment parameters for RH_HCFirst experiment results
  import os
  try:
    file = os.path.splitext(filename)[0]
    tokens = file.split("_")

    module    = tokens[0]
    act_count = int(tokens[1])
    itr       = int(tokens[2].replace("itr", ""))
  except Exception as e:
    print(f"Failed to parse \"{filename}\"!")
    print(e)
    raise

  return module, act_count, itr


def decode_ber_filename(filename: str):
  # Get experiment parameters for RH_BITFLIPS experiment results
  import os
  try:
    file = os.path.splitext(filename)[0]
    tokens = file.split("_")

    module         = tokens[0]
    hc             = int(tokens[1])
    tAggON         = ras_scale_to_tAggON_ns(int(tokens[2]))
    attack_time_ms = int(tokens[3].replace("ms", ""))
    itr            = int(tokens[4].replace("itr", ""))
  except Exception as e:
    print(f"Failed to parse \"{filename}\"!")
    print(e)
    raise

  return module, hc, tAggON, attack_time_ms, itr


def decode_faber_filename(filename: str):
  # Get experiment parameters for FA_RH_BITFLIPS experiment results
  import os
  try:
    file = os.path.splitext(filename)[0]
    tokens         = file.split("_")

    module         = tokens[0]
    hc             = int(tokens[1])
    extra_delay_ns = extra_delay_to_ns(int(tokens[2]))
    tAggON_ratio   = float(tokens[3])
    itr            = int(tokens[4].replace("itr", ""))
  except Exception as e:
    print(f"Failed to parse \"{filename}\"!")
    print(e)
    raise

  return module, hc, extra_delay_ns, tAggON_ratio, itr


def decode_ftber_filename(filename: str):
  # Get experiment parameters for FT_RH_BITFLIPS experiment results
  import os
  try:
    file = os.path.splitext(filename)[0]
    tokens         = file.split("_")

    module         = tokens[0]
    hc             = int(tokens[1])
    extra_delay_ns = extra_delay_to_ns(int(tokens[2]))
    tAggON_ratio   = float(tokens[3])
    attack_time_ms = int(tokens[4].replace("ms", ""))
    itr            = int(tokens[5].replace("itr", ""))
  except Exception as e:
    print(f"Failed to parse \"{filename}\"!")
    print(e)
    raise
  return module, hc, extra_delay_ns, tAggON_ratio, attack_time_ms, itr


def read_file_lines(path):
  # Read all lines of a file into a list
  with open(path) as f:
    lines = f.readlines()
    lines = [line.rstrip() for line in lines]
    return lines