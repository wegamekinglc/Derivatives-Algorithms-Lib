#!/bin/bash -e

num_cores=$(grep -c processor /proc/cpuinfo)
export DAL_DIR=$PWD
export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH
export BUILD_TYPE=Release

(
cd machinist2 || exit
bash -e ./build_linux.sh
)

if [ $? -ne 0 ]; then
  exit 1
fi

export MACHINIST_TEMPLATE_DIR=$PWD/machinist2/template/
./machinist2/bin/Machinist -c config/dal.ifc -l config/dal.mgl -d ./dal

if [ $? -ne 0 ]; then
  exit 1
fi

mkdir -p build
(
cd build || exit
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_INSTALL_PREFIX="$DAL_DIR" ..
make clean
make -j"${num_cores}"
make install
)

if [ $? -ne 0 ]; then
  exit 1
fi

bin/test_suite

echo "Finished building of Derivatives Algorithms Library"
