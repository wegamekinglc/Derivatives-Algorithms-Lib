#!/bin/sh

export num_cores=`grep -c processor /proc/cpuinfo`
export dal_dir=$PWD
epxort LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$dal_dir ..
make clean
make -j${num_cores}
make install

cd ..
bin/test_suite
