#!/bin/bash -e

NUM_CORES=$(grep -c processor /proc/cpuinfo)
export DAL_DIR=$PWD
export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH
export BUILD_TYPE=Release
export SKIP_TESTS=false

echo NUM_CORES: NUM_CORES
echo BUILD_TYPE: $BUILD_TYPE
echo DAL_DIR: $DAL_DIR
echo SKIP_TESTS: $SKIP_TESTS

(
cd machinist2 || exit
bash -e ./build_linux.sh
)

if [ $? -ne 0 ]; then
  exit 1
fi

export MACHINIST_TEMPLATE_DIR=$PWD/machinist2/template/
./machinist2/bin/Machinist -c config/dal.ifc -l config/dal.mgl -d ./dal
./machinist2/bin/Machinist -c config/dal.ifc -l config/dal.mgl -d ./public

if [ $? -ne 0 ]; then
  exit 1
fi

mkdir -p build
(
cd build || exit
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_INSTALL_PREFIX="$DAL_DIR" -DSKIP_TESTS=$SKIP_TESTS ..
make clean
make -j"${NUM_CORES}"
make install
)

if [ $? -ne 0 ]; then
  exit 1
fi

if [ "$SKIP_TESTS" = "false" ]; then
  bin/test_suite
fi

echo "Finished building of Derivatives Algorithms Library"
