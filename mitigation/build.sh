mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
cp ramulator ..
cd ..
