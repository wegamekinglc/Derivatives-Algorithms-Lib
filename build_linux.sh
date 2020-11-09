#!/bin/sh

export num_cores=`grep -c processor /proc/cpuinfo`
export DAL_DIR=$PWD
export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH
export BUILD_TYPE=Debug

mkdir build
(
cd build || exit
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_INSTALL_PREFIX=$DAL_DIR ..
make clean
make -j${num_cores}
make install
) || ret=$?

if [ $ret -ne 0 ]; then
    echo "Error in building"
    exit 1
fi

bin/test_suite || ret=$?

if [ $ret -ne 0 ]; then
    echo "Error in testing"
    exit 1
fi

echo "Finished building of Derivatives Algorithms Library"