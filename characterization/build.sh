#!/bin/bash
set -e 
set -o pipefail

cd ./DRAM-Bender/sources/apps/RowPress
rm -rf build
mkdir -p build
cd build
cmake ..
make -j

cp ./SoftMC_RowPress ../../../../../SoftMC_RowPress
cd ../../../../..