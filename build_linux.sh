#!/bin/sh

export num_cores=`grep -c processor /proc/cpuinfo`
export dal_dir=$PWD

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$dal_dir ..
make clean
make -j${num_cores}
make install

cd ..
bin/test_suite
