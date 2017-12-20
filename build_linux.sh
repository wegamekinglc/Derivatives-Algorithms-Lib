#!/bin/sh

export num_cores=`grep -c processor /proc/cpuinfo`

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make clean
make -j${num_cores}

cd ..
bin/test_suite
