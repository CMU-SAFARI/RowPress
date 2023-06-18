#!/bin/bash
# Simple script to submit multiple jobs to slurm
data_root=$1
modules=$(ls $1)

mkdir -p ../processed_data/hcf
mkdir -p ../processed_data/ber
mkdir -p ../processed_data/ftber
mkdir -p ../processed_data/ecc
mkdir -p ../processed_data/hcf_bitflipdirection
mkdir -p ../processed_data/relation

for module in $modules;
do
  srun python3 process_hcf_log.py       $data_root $module &
  srun python3 process_ber_log.py       $data_root $module &
  srun python3 process_ftber_log.py     $data_root $module &
  srun python3 process_ecc.py           $data_root $module &
  srun python3 process_relation.py      $data_root $module &
  
  srun python3 process_hcf_bitflipdirection.py  $data_root $module &
done
