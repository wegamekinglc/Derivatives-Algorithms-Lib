#!/bin/sh

export num_cores=`grep -c processor /proc/cpuinfo`
export DAL_DIR=$PWD
export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$DAL_DIR ..
make clean
make -j${num_cores}
make install

cd ..
bin/test_suite
