#!/bin/bash -e

NUM_CORES=$(grep -c processor /proc/cpuinfo)
export DAL_DIR=$PWD
export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH
export BUILD_TYPE=Release
export SKIP_TESTS=false
export USE_COVERAGE=false  # make it `false` when you need a full performance lib
export CMAKE_EXPORT_COMPILE_COMMANDS=on
export BUILD_SHARED_LIBS=off
export CMAKE_TOOLCHAIN_FILE=$PWD/externals/vcpkg/scripts/buildsystems/vcpkg.cmake

echo NUM_CORES: $NUM_CORES
echo BUILD_TYPE: $BUILD_TYPE
echo DAL_DIR: "$DAL_DIR"
echo SKIP_TESTS: $SKIP_TESTS
echo USE_COVERAGE: $USE_COVERAGE
echo BUILD_SHARED_LIBS: $BUILD_SHARED_LIBS
echo CMAKE_EXPORT_COMPILE_COMMANDS: $CMAKE_EXPORT_COMPILE_COMMANDS
echo CMAKE_TOOLCHAIN_FILE: "$CMAKE_TOOLCHAIN_FILE"

(
cd externals/vcpkg
if [ -f "./vcpkg" ]; then
  echo "vcpkg executable already exists"
else
  bash bootstrap-vcpkg.sh
fi

./vcpkg install gtest
./vcpkg install rapidjson
)

if [ $? -ne 0 ]; then
  exit 1
fi

(
cd externals/machinist || exit
bash -e ./build_linux.sh
)

if [ $? -ne 0 ]; then
  exit 1
fi

export MACHINIST_TEMPLATE_DIR=$PWD/externals/machinist/template/
./externals/machinist/bin/Machinist -c config/dal.ifc -l config/dal.mgl -d ./dal
./externals/machinist/bin/Machinist -c config/dal.ifc -l config/dal.mgl -d ./public

if [ $? -ne 0 ]; then
  exit 1
fi

mkdir -p build
(
cd build || exit
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_INSTALL_PREFIX="$DAL_DIR" -DUSE_COVERAGE=$USE_COVERAGE -DSKIP_TESTS=$SKIP_TESTS -DCMAKE_EXPORT_COMPILE_COMMANDS=$CMAKE_EXPORT_COMPILE_COMMANDS -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS -DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE -DUSE_AADET=true -DXAD_STATIC_MSVC_RUNTIME=ON -DXAD_SIMD_OPTION=AVX2 ..
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
