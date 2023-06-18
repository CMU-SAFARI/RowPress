#!/bin/bash -f
#*********************************************************************************************************
# Vivado (TM) v2020.2 (64-bit)
#
# Filename    : xdma.sh
# Simulator   : Synopsys Verilog Compiler Simulator
# Description : Simulation script for compiling, elaborating and verifying the project source files.
#               The script will automatically create the design libraries sub-directories in the run
#               directory, add the library logical mappings in the simulator setup file, create default
#               'do/prj' file, execute compilation, elaboration and simulation steps.
#
# Generated by Vivado on Tue Mar 08 14:25:07 CET 2022
# SW Build 3064766 on Wed Nov 18 09:12:47 MST 2020
#
# Copyright 1986-2020 Xilinx, Inc. All Rights Reserved. 
#
# usage: xdma.sh [-help]
# usage: xdma.sh [-lib_map_path]
# usage: xdma.sh [-noclean_files]
# usage: xdma.sh [-reset_run]
#
# Prerequisite:- To compile and run simulation, you must compile the Xilinx simulation libraries using the
# 'compile_simlib' TCL command. For more information about this command, run 'compile_simlib -help' in the
# Vivado Tcl Shell. Once the libraries have been compiled successfully, specify the -lib_map_path switch
# that points to these libraries and rerun export_simulation. For more information about this switch please
# type 'export_simulation -help' in the Tcl shell.
#
# You can also point to the simulation libraries by either replacing the <SPECIFY_COMPILED_LIB_PATH> in this
# script with the compiled library directory path or specify this path with the '-lib_map_path' switch when
# executing this script. Please type 'xdma.sh -help' for more information.
#
# Additional references - 'Xilinx Vivado Design Suite User Guide:Logic simulation (UG900)'
#
#*********************************************************************************************************

# Directory path for design sources and include directories (if any) wrt this path
ref_dir="."

# Override directory with 'export_sim_ref_dir' env path value if set in the shell
if [[ (! -z "$export_sim_ref_dir") && ($export_sim_ref_dir != "") ]]; then
  ref_dir="$export_sim_ref_dir"
fi

# Command line options
vlogan_opts="-full64"
vhdlan_opts="-full64"
vcs_elab_opts="-full64 -debug_pp -t ps -licqueue -l elaborate.log"
vcs_sim_opts="-ucli -licqueue -l simulate.log"

# Design libraries
design_libs=(xilinx_vip xpm gtwizard_ultrascale_v1_7_9 xil_defaultlib blk_mem_gen_v8_4_4 xdma_v4_1_8)

# Simulation root library directory
sim_lib_dir="vcs_lib"

# Script info
echo -e "xdma.sh - Script generated by export_simulation (Vivado v2020.2 (64-bit)-id)\n"

# Main steps
run()
{
  check_args $# $1
  setup $1 $2
  compile
  elaborate
  simulate
}

# RUN_STEP: <compile>
compile()
{
  # Compile design files
  vlogan -work xilinx_vip $vlogan_opts -sverilog +incdir+"$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source" +incdir+"$ref_dir/../../../ipstatic/hdl/verilog" +incdir+"/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/include" \
    "/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/hdl/axi4stream_vip_axi4streampc.sv" \
    "/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/hdl/axi_vip_axi4pc.sv" \
    "/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/hdl/xil_common_vip_pkg.sv" \
    "/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/hdl/axi4stream_vip_pkg.sv" \
    "/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/hdl/axi_vip_pkg.sv" \
    "/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/hdl/axi4stream_vip_if.sv" \
    "/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/hdl/axi_vip_if.sv" \
    "/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/hdl/clk_vip_if.sv" \
    "/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/hdl/rst_vip_if.sv" \
  2>&1 | tee -a vlogan.log

  vlogan -work xpm $vlogan_opts -sverilog +incdir+"$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source" +incdir+"$ref_dir/../../../ipstatic/hdl/verilog" +incdir+"/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/include" \
    "/tools/Xilinx/Vivado/2020.2/data/ip/xpm/xpm_cdc/hdl/xpm_cdc.sv" \
    "/tools/Xilinx/Vivado/2020.2/data/ip/xpm/xpm_fifo/hdl/xpm_fifo.sv" \
    "/tools/Xilinx/Vivado/2020.2/data/ip/xpm/xpm_memory/hdl/xpm_memory.sv" \
  2>&1 | tee -a vlogan.log

  vhdlan -work xpm $vhdlan_opts \
    "/tools/Xilinx/Vivado/2020.2/data/ip/xpm/xpm_VCOMP.vhd" \
  2>&1 | tee -a vhdlan.log

  vlogan -work gtwizard_ultrascale_v1_7_9 $vlogan_opts +v2k +incdir+"$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source" +incdir+"$ref_dir/../../../ipstatic/hdl/verilog" +incdir+"/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/include" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_bit_sync.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gte4_drp_arb.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gthe4_delay_powergood.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gtye4_delay_powergood.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gthe3_cpll_cal.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gthe3_cal_freqcnt.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gthe4_cpll_cal.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gthe4_cpll_cal_rx.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gthe4_cpll_cal_tx.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gthe4_cal_freqcnt.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gtye4_cpll_cal.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gtye4_cpll_cal_rx.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gtye4_cpll_cal_tx.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gtye4_cal_freqcnt.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gtwiz_buffbypass_rx.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gtwiz_buffbypass_tx.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gtwiz_reset.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gtwiz_userclk_rx.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gtwiz_userclk_tx.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gtwiz_userdata_rx.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_gtwiz_userdata_tx.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_reset_sync.v" \
    "$ref_dir/../../../ipstatic/hdl/gtwizard_ultrascale_v1_7_reset_inv_sync.v" \
  2>&1 | tee -a vlogan.log

  vlogan -work xil_defaultlib $vlogan_opts +v2k +incdir+"$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source" +incdir+"$ref_dir/../../../ipstatic/hdl/verilog" +incdir+"/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/include" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/ip_0/sim/gtwizard_ultrascale_v1_7_gtye4_channel.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/ip_0/sim/xdma_pcie4_ip_gt_gtye4_channel_wrapper.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/ip_0/sim/gtwizard_ultrascale_v1_7_gtye4_common.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/ip_0/sim/xdma_pcie4_ip_gt_gtye4_common_wrapper.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/ip_0/sim/xdma_pcie4_ip_gt_gtwizard_gtye4.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/ip_0/sim/xdma_pcie4_ip_gt_gtwizard_top.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/ip_0/sim/xdma_pcie4_ip_gt.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_gtwizard_top.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_phy_ff_chain.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_phy_pipeline.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_bram_16k_int.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_bram_16k.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_bram_32k.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_bram_4k_int.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_bram_msix.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_bram_rep_int.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_bram_rep.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_bram_tph.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_bram.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_gt_gt_channel.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_gt_gt_common.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_gt_phy_clk.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_gt_phy_rst.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_gt_phy_rxeq.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_gt_phy_txeq.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_sync_cell.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_sync.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_gt_cdr_ctrl_on_eidle.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_gt_receiver_detect_rxterm.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_gt_phy_wrapper.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_init_ctrl.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_pl_eq.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_vf_decode.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_pipe.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_phy_top.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_seqnum_fifo.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_sys_clk_gen_ps.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source/xdma_pcie4_ip_pcie4_uscale_core_top.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/sim/xdma_pcie4_ip.v" \
  2>&1 | tee -a vlogan.log

  vlogan -work blk_mem_gen_v8_4_4 $vlogan_opts +v2k +incdir+"$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source" +incdir+"$ref_dir/../../../ipstatic/hdl/verilog" +incdir+"/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/include" \
    "$ref_dir/../../../ipstatic/simulation/blk_mem_gen_v8_4.v" \
  2>&1 | tee -a vlogan.log

  vlogan -work xil_defaultlib $vlogan_opts +v2k +incdir+"$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source" +incdir+"$ref_dir/../../../ipstatic/hdl/verilog" +incdir+"/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/include" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_1/sim/xdma_v4_1_8_blk_mem_64_reg_be.v" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_2/sim/xdma_v4_1_8_blk_mem_64_noreg_be.v" \
  2>&1 | tee -a vlogan.log

  vlogan -work xdma_v4_1_8 $vlogan_opts -sverilog +incdir+"$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source" +incdir+"$ref_dir/../../../ipstatic/hdl/verilog" +incdir+"/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/include" \
    "$ref_dir/../../../ipstatic/hdl/xdma_v4_1_vl_rfs.sv" \
  2>&1 | tee -a vlogan.log

  vlogan -work xil_defaultlib $vlogan_opts -sverilog +incdir+"$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/ip_0/source" +incdir+"$ref_dir/../../../ipstatic/hdl/verilog" +incdir+"/tools/Xilinx/Vivado/2020.2/data/xilinx_vip/include" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/xdma_v4_1/hdl/verilog/xdma_dma_bram_wrap.sv" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/xdma_v4_1/hdl/verilog/xdma_dma_bram_wrap_1024.sv" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/xdma_v4_1/hdl/verilog/xdma_dma_bram_wrap_2048.sv" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/xdma_v4_1/hdl/verilog/xdma_core_top.sv" \
    "$ref_dir/../../../../U200.srcs/sources_1/ip/xdma/sim/xdma.sv" \
  2>&1 | tee -a vlogan.log


  vlogan -work xil_defaultlib $vlogan_opts +v2k \
    glbl.v \
  2>&1 | tee -a vlogan.log

}

# RUN_STEP: <elaborate>
elaborate()
{
  vcs $vcs_elab_opts xil_defaultlib.xdma xil_defaultlib.glbl -o xdma_simv
}

# RUN_STEP: <simulate>
simulate()
{
  ./xdma_simv $vcs_sim_opts -do simulate.do
}

# STEP: setup
setup()
{
  case $1 in
    "-lib_map_path" )
      if [[ ($2 == "") ]]; then
        echo -e "ERROR: Simulation library directory path not specified (type \"./xdma.sh -help\" for more information)\n"
        exit 1
      fi
      create_lib_mappings $2
    ;;
    "-reset_run" )
      reset_run
      echo -e "INFO: Simulation run files deleted.\n"
      exit 0
    ;;
    "-noclean_files" )
      # do not remove previous data
    ;;
    * )
      create_lib_mappings $2
  esac

  create_lib_dir

  # Add any setup/initialization commands here:-

  # <user specific commands>

}

# Define design library mappings
create_lib_mappings()
{
  file="synopsys_sim.setup"
  if [[ -e $file ]]; then
    if [[ ($1 == "") ]]; then
      return
    else
      rm -rf $file
    fi
  fi

  touch $file

  if [[ ($1 != "") ]]; then
    lib_map_path="$1"
  else
    lib_map_path="/home/ataberk/SoftMC_DDR4/projects/U200/U200.cache/compile_simlib/vcs"
  fi

  for (( i=0; i<${#design_libs[*]}; i++ )); do
    lib="${design_libs[i]}"
    mapping="$lib:$sim_lib_dir/$lib"
    echo $mapping >> $file
  done

  if [[ ($lib_map_path != "") ]]; then
    incl_ref="OTHERS=$lib_map_path/synopsys_sim.setup"
    echo $incl_ref >> $file
  fi
}

# Create design library directory paths
create_lib_dir()
{
  if [[ -e $sim_lib_dir ]]; then
    rm -rf $sim_lib_dir
  fi

  for (( i=0; i<${#design_libs[*]}; i++ )); do
    lib="${design_libs[i]}"
    lib_dir="$sim_lib_dir/$lib"
    if [[ ! -e $lib_dir ]]; then
      mkdir -p $lib_dir
    fi
  done
}

# Delete generated data from the previous run
reset_run()
{
  files_to_remove=(ucli.key xdma_simv vlogan.log vhdlan.log compile.log elaborate.log simulate.log .vlogansetup.env .vlogansetup.args .vcs_lib_lock scirocco_command.log 64 AN.DB csrc xdma_simv.daidir)
  for (( i=0; i<${#files_to_remove[*]}; i++ )); do
    file="${files_to_remove[i]}"
    if [[ -e $file ]]; then
      rm -rf $file
    fi
  done

  create_lib_dir
}

# Check command line arguments
check_args()
{
  if [[ ($1 == 1 ) && ($2 != "-lib_map_path" && $2 != "-noclean_files" && $2 != "-reset_run" && $2 != "-help" && $2 != "-h") ]]; then
    echo -e "ERROR: Unknown option specified '$2' (type \"./xdma.sh -help\" for more information)\n"
    exit 1
  fi

  if [[ ($2 == "-help" || $2 == "-h") ]]; then
    usage
  fi
}

# Script usage
usage()
{
  msg="Usage: xdma.sh [-help]\n\
Usage: xdma.sh [-lib_map_path]\n\
Usage: xdma.sh [-reset_run]\n\
Usage: xdma.sh [-noclean_files]\n\n\
[-help] -- Print help information for this script\n\n\
[-lib_map_path <path>] -- Compiled simulation library directory path. The simulation library is compiled\n\
using the compile_simlib tcl command. Please see 'compile_simlib -help' for more information.\n\n\
[-reset_run] -- Recreate simulator setup files and library mappings for a clean run. The generated files\n\
from the previous run will be removed. If you don't want to remove the simulator generated files, use the\n\
-noclean_files switch.\n\n\
[-noclean_files] -- Reset previous run, but do not remove simulator generated files from the previous run.\n\n"
  echo -e $msg
  exit 1
}

# Launch script
run $1 $2
