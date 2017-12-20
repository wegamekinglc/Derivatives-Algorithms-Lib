#!/bin/sh

export num_cores=`grep -c processor /proc/cpuinfo`

cd build
cmake ..
make clean
make -j${num_cores}

cd ..
bin/test_suite
