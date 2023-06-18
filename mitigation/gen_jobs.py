import yaml
import os
import random
from argparse import ArgumentParser
import itertools
import random

BASH_HEADER = "#!/bin/bash\n"

# the command line slurm will execute
SBATCH_COMMAND_LINE = "\
    sbatch --cpus-per-task=1 --nodes=1 --ntasks=1 \
    --output={output_file_name} \
    --error={error_file_name} \
    --job-name='{job_name}' \
    {ramulator_dir}/run_cmds/{config_name}-{workload}.sh"

# the script executed by the command line slurm executes
BASE_COMMAND_LINE = "\
    {ramulator_dir}/ramulator "

# @returns SBATCH command used to invoke the ramulator script
def generateExecutionSetup(ramulator_dir, output_dir, trace_dir, config, workload_name_list):

  CMD = BASE_COMMAND_LINE.format(
    ramulator_dir = ramulator_dir,
  )

  ramulator_config=None
  with open(config) as f:    
    ramulator_config = yaml.load(f, Loader=yaml.FullLoader)
  bare_config = config.split('/')[-1]

  workload_name_list_dir = [(trace_dir + "/" + x) for x in workload_name_list]
  ramulator_config["processor"]["trace"] = workload_name_list_dir
  ramulator_config["processor"]["cache"]["L3"]["capacity"] = str(int(len(workload_name_list_dir) * 2)) + "MB"

  SBATCH_CMD = SBATCH_COMMAND_LINE.format(
    ramulator_dir = ramulator_dir,
    output_file_name = '{output_file_name}',
    error_file_name = '{error_file_name}',
    config_extension = '',
    job_name = '{job_name}',
    config_name = bare_config,
    workload = '{workload}'
  )
  
  prog_list = ""

  length = len(workload_name_list)

  for j in range(length-1):
    prog_list += workload_name_list[j] + '-'
  
  prog_list += workload_name_list[length-1]

  stats_prefix = output_dir + '/' + bare_config + '/' + prog_list + '/'
  ramulator_config["stats"]["prefix"] = stats_prefix

  # Finalize CMD
  CMD += "\"" + yaml.dump(ramulator_config) + "\""

  SBATCH_CMD = SBATCH_CMD.format(
    output_file_name = output_dir + '/' + bare_config + '/' + prog_list + '/output.txt',
    error_file_name = output_dir + '/' + bare_config + '/' + prog_list + '/error.txt',
    job_name = prog_list,
    workload = prog_list
  )            

  os.system('mkdir -p ' + output_dir + '/' + bare_config + '/' + prog_list)

  f = open(ramulator_dir + '/run_cmds/' + bare_config + '-' + prog_list + '.sh', 'w')
  f.write(BASH_HEADER)
  f.write(CMD)
  f.close()
  
  return SBATCH_CMD    

traces = [
    "401.bzip2",
    "403.gcc",
    "429.mcf",
    "433.milc",
    "434.zeusmp",
    "435.gromacs",
    "436.cactusADM",
    "437.leslie3d",
    "444.namd",
    "445.gobmk",
    "447.dealII",
    "450.soplex",
    "456.hmmer",
    "458.sjeng",
    "459.GemsFDTD",
    "462.libquantum",
    "464.h264ref",
    "470.lbm",
    "471.omnetpp",
    "473.astar",
    "481.wrf",
    "482.sphinx3",
    "483.xalancbmk",
    "500.perlbench",
    "502.gcc",
    "505.mcf",
    "507.cactuBSSN",
    "508.namd",
    "510.parest",
    "511.povray",
    "519.lbm",
    "520.omnetpp",
    "523.xalancbmk",
    "525.x264",
    "526.blender",
    "531.deepsjeng",
    "538.imagick",
    "541.leela",
    "544.nab",
    "549.fotonik3d",
    "557.xz",
    "bfs_dblp",
    "grep_map0",
    "h264_encode",
    "jp2_decode",
    "jp2_encode",
    "tpcc64",
    "tpch17",
    "tpch2",
    "tpch6",
    "wc_8443",
    "wc_map0",
    "ycsb_abgsave",
    "ycsb_aserver",
    "ycsb_bserver",
    "ycsb_cserver",
    "ycsb_dserver",
    "ycsb_eserver"
]


configs = [
  "configs/Graphene419OpenUntil636.yaml",
  "configs/Graphene555OpenUntil336.yaml",
  "configs/Graphene619OpenUntil186.yaml",
  "configs/Graphene724OpenUntil96.yaml",
  "configs/Graphene809OpenUntil66.yaml",
  "configs/Graphene1000OpenUntil36.yaml",
  "configs/Graphene.yaml",
  "configs/PARA419OpenUntil636.yaml",
  "configs/PARA555OpenUntil336.yaml",
  "configs/PARA619OpenUntil186.yaml",
  "configs/PARA724OpenUntil96.yaml",
  "configs/PARA809OpenUntil66.yaml",
  "configs/PARA1000OpenUntil36.yaml",
  "configs/PARA.yaml",
]

baseline_config = "configs/Baseline.yaml"

ramulator_dir = './'
output_dir = './results'
trace_dir = './cputraces'

# remove scripts
os.system('rm -rf ' + ramulator_dir + '/run_cmds')
os.system('mkdir -p ' + output_dir)
os.system('mkdir -p ' + ramulator_dir + '/run_cmds')

all_sbatch_commands = []
all_sbatch_commands.append(BASH_HEADER)

# Single-core Baseline
os.system('mkdir -p ' + output_dir + '/' + baseline_config.split('/')[-1])
for trace in traces:
  all_sbatch_commands.append(generateExecutionSetup(ramulator_dir, output_dir, trace_dir, baseline_config, [trace]))

# Four-core
for config in configs:
    os.system('mkdir -p ' + output_dir + '/' + config.split('/')[-1])
    four_core_traces = []
    for workload in traces:
      all_sbatch_commands.append(generateExecutionSetup(ramulator_dir, output_dir, trace_dir, config, [workload] * 4))


with open('run.sh', 'w') as f:
    f.write("rm -rf ./results/*")
    f.write('\n'.join(all_sbatch_commands))

os.system('chmod uog+x run.sh')
