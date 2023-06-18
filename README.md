# RowPress: Amplifying Read Disturbance in Modern DRAM Chips
This repository provides all the necessary files and instructions to reproduce the results of our ISCA'23 paper.

> Haocong Luo, Ataberk Olgun, Abdullah Giray Yaglikci, Yahya Can Tugrul, Steve Rhyner, M. Banu Cavlak, Joel Lindegger, Mohammad Sadrosadati, and Onur Mutlu, "RowPress: Amplifying Read Disturbance in Modern DRAM Chips", ISCA'23.

Please use the following citation to cite RowPress if the repository is useful for you.
```
@inproceedings{luo2023rowpress,
      title={{RowPress: Amplifying Read Disturbance in Modern DRAM Chips}}, 
      author={Haocong Luo, Ataberk Olgun, Abdullah Giray Ya{\u{g}}l{\i}k{{c}}{\i}, Yahya Can Tu{\u{g}}rul, Steve Rhyner, M. Banu Cavlak, Joel Lindegger, Mohammad Sadrosadati, and Onur Mutlu},
      year={2023},
      booktitle={ISCA}
}
```
This repository consists of three parts: 
- Codebase and scripts to conduct RowPress `characterization` tests on real DRAM chips and analyze the raw data
- C++ program for real-system `demonstration` of RowPress and scripts to analyze the raw data
- Ramulator implementation and evaluation of the proposed `mitigation` of RowPress

# Characterization
We provide the source code of our characterization program, a python script to drive the characterization experiments, and a set of scripts and a Jupyter notebook to analyze and plot the results.

## Prerequisite
The real DRAM chip characterization of RowPress is based on the open-source FPGA-based DRAM characterization infrastructure [DRAM Bender](https://github.com/CMU-SAFARI/DRAM-Bender). Please check out and follow the installation instructions of [DRAM Bender](https://github.com/CMU-SAFARI/DRAM-Bender).

The software dependencies for the characterization are:
- GNU Make, CMake 3.10+
- `c++-17` build toolchain (tested with `gcc-9`)
- Python 3.9+
- `pip` packages `pandas`, `scipy`, `matplotlib`, and `seaborn`

## Hardware Setup
Our real DRAM chip characterization infrastructure consists of the following components:
- A host x86 machine with a PCIe 3.0 x16 slot
- An FPGA board with a DIMM/SODIMM slot supported by [DRAM Bender](https://github.com/CMU-SAFARI/DRAM-Bender) (e.g., Xilinx Alveo U200)
- Heater pads attached to the DRAM module under test
- A temperature controller (e.g., MaxWell FT200) programmable by the host machine connected to the heater pads

## Directory Structure
```
characterization
└ DRAM-Bender           # A fork of DRAM Bender that contains the characterization program
  └ sources           
    └ apps           
      └ RowPress        # Source code of the characterization program    
└ analysis              # Scripts to aggregate, analyze, and plot the characterization data
  └ plots               # Jupyter notebooks to plot the data
  └ scripts             # Shell scripts to aggregate and analyze the data
└ data_pattern_sweep    # Access and data pattern files used by the data pattern sensitivity analysis
└ patterns              # Access and data pattern files used by the other parts of the characterization
└ scripts               # Various utility scripts used in the characterization
└ build.sh              # Builds the characterization program
└ run_bare.py           # The python script to drive all characterization experiments
```
## Running the Characterization Experiments
We provide a python script `characterization/run_bare.py` to drive all characterization experiments. This script contains all parameters we used in our experiments (including all temperature values). This script does NOT include the instructions to actually set the temperature of the DRAM chip, because they differ from different temperature controllers. The user is responsible for handling the communication between the host machine and the temperature controller.
### Step 0: Get a persistent shell session
The real DRAM chip characterization takes a long period of time. To run all our characterization experiments, a completion time of 3-4 weeks is expected. Therefore, it is recommended to run the characterization experiment script in a persistent shell session (e.g., using a terminal multiplexer like screen).
```
  $ cd characterization
```
### Step 1: Build the characterization program
```
  $ ./build.sh
```
This generates the executable `SoftMC_RowPress` in `characterization/`.
### [Optional] Step 2: Remove old results (if any)
The characterization program will generate bitflip records and experiment logs under `characterization/data`. If there are stale results (e.g., as a result of an interrupted experiment), it is recommended to remove them before starting new experiments.
### Step 3: Start the experiment
```
  $ python3 run_bare.py ${MODULE}
```
Executing the python script will start all characterization experiments. The results are saved to `characterization/data`.

## Aggregate the Raw Characterization Data
```
  $ cd analysis/scripts
  $ ./process_data_local.sh [path-to-characterization-data] [DRAM Module ID]
```
Executing the shell script `process_data_local.sh` calls a set of python scripts (`characterization/analysis/process_*.py`) to process the characterization data into Pandas dataframes (serialized using `pickle`, compressed with `zip`) stored at `characterization/analysis/processed_results/${DRAM Module ID}`. 

For submitting jobs to the `slurm` workload manager, we also provide `characterization/analysis/scripts/process_data_slurm.sh`.

## Analyzing and Plotting the Characterization Results
```
  $ cd analysis/plots
```
Open the Jupyter notebook `paper_plots.ipynb` and execute all cells to analyze and plot the characterization results.

# Demonstration
We provide the source code of our real-system demonstration program and Jupyter notebooks to analyze and plot the results. 

## Prerequisite
We performed the real-system demonstration on the following hardware:
- Intel Core i5 10400 (Comet Lake-S)
- Samsung M378A2K43CB1-CTD DDR4 DRAM module (DRAM Part K4A8G085WC-BCTD, with TRR)

We hardcoded the DRAM bank functions of Intel Core i5 10400 and a baseline access pattern to bypass the TRR implementation of K4A8G085WC-BCTD. Users with a different system (i.e., different processor and/or DRAM module) are expected to adapt the demonstration program to the DRAM bank functions/TRR implementations of their systems.

Our machine has Ubuntu 18.04 (Linux kernel 5.4.0-13) with 1GB hugepage enabled. We use the hugepage only to simplify the process of finding adjacent DRAM rows from the physical addresses.



## Disclaimer
We have reused/repurposed/referenced code from [Blacksmith](https://github.com/comsec-group/blacksmith), [TRResspass](https://github.com/vusec/trrespass/tree/master/drama), and [DRAMA](https://github.com/IAIK/drama).

## Directory Structure
```
demonstration
└ main.cpp                    # Entry point of the source code of the real-system demonstration program
└ Mappiong.h                  # Hardcoded bank functions of Comet Lake-S
└ mount_hugepage.sh           # Helper script to mount a 1GB hugepage
└ real_system_bitflips.ipynb  # Analyze the results of inducing RowPress bitflips in a real system
└ real_system_access.ipynb    # Analyze the results of verifying the increase in DRAM row open time
```

## Demonstrating RowPress in a Real System
### Step 0: Build the demonstration program
```
  $ cd demonstration
  $ make
```
This builds the executable `demon` in `demonstration/`.
### Step 1: Mount the 1GB hugepage
```
  $ sudo ./mount_hugepage.sh      # Should print 1 if successful
```
Note that the root privilege is only used for the hugepage.
### Step 2: Run the demonstration program for bitflips
```
  $ sudo ./demo --num_victims 1500 > bitflips.txt
```
Bitflip records will be saved to `bitflips.txt`.
### Step 3: Process the results
```
  $ python3 analyze.py bitflips.txt > parsed_results.txt
```
### Step 4: Analyze and plot the results
Open the `real_system_bitflips.ipynb` Jupyter notebook and execute all cells to plot the number of bitflips and the number of rows with bitflips.

## Verifying the Increase in Row Open Time
### Step 0: Disable the prefetcher
```
  $ sudo ./disable_prefetching.sh
```
### Step 1: Run the demonstration program for verifying the increase in row open time
```
  $ sudo ./demo --verify
```
### Step 2: Analyze and plot the results
Open the `real_system_analyze.ipynb` Jupyter notebook and execute all cells.

# Mitigation
## Prerequisite
- GNU Make, CMake 3.10+
- `c++-17` build toolchain (tested with `gcc-9`)
- Python 3.9+
- `pip` packages `pandas`, `scipy`, `matplotlib`, and `seaborn`

## Directory Structure
```
mitigation
└ configs               # Ramulator configurations used in the evaluation of the mitigation
└ src                   # Source code of the Ramulator implementation of the mitigation
└ build.sh              # Builds the Ramulator executable
└ gen_jobs.py           # Generate the simulation configurations
└ analyze.ipynb         # Analyze the simulation results
```
## Running the Simulation
### Step 0: Build the executable
```
  $ cd mitigation
  $ ./build.sh
```
This builds the `ramulator` executable under `mitigation/`.
### Step 1: Generate the simulation configurations
```
  $ python3 gen_jobs.py
```
This generates all the simulation configurations as `run_cmds/<config>-<workload>.sh` and a shell script `run.sh` to submit all of them as `slurm` jobs. 
### Step 2: Submit the simulation jobs
```
  $ ./run.sh
```
Users can also utilize the individual simulation configurations (`run_cmds/<config>-<workload>.sh`) to adapt to their own workload scheduler. The simulation results are saved in `results/`.
### Step 3: Analyze the results
Open the Jupyter notebook `analyze.ipynb` and execute all cells to analyze the simulation results and get the additional performance overhead of Graphene-RP (PARA-RP) over Graphene (PARA).