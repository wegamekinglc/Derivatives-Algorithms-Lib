#!/bin/bash -e

COVERAGE=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --coverage) COVERAGE=true; shift ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

NUM_CORES=$(grep -c processor /proc/cpuinfo)
export DAL_DIR=$PWD
export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH
export BUILD_TYPE=Release
export USE_COVERAGE=$COVERAGE
export CMAKE_EXPORT_COMPILE_COMMANDS=on

echo NUM_CORES: $NUM_CORES
echo BUILD_TYPE: $BUILD_TYPE
echo USE_COVERAGE: $USE_COVERAGE
echo DAL_DIR: "$DAL_DIR"
echo CMAKE_EXPORT_COMPILE_COMMANDS: $CMAKE_EXPORT_COMPILE_COMMANDS

rm -rf ./bin
rm -rf ./lib
rm -rf ./coverage

# Build Machinist (code generator) — lives under dal-cpp/externals
(
cd dal-cpp/externals/machinist || exit
bash -e ./build_linux.sh
)

if [ $? -ne 0 ]; then
  exit 1
fi

# Run Machinist code generation
export MACHINIST_TEMPLATE_DIR=$PWD/dal-cpp/externals/machinist/template/
./dal-cpp/externals/machinist/bin/Machinist -c dal-cpp/config/dal.ifc -l dal-cpp/config/dal.mgl -d ./dal-cpp/dal
./dal-cpp/externals/machinist/bin/Machinist -c dal-cpp/config/dal.ifc -l dal-cpp/config/dal.mgl -d ./dal-excel

if [ $? -ne 0 ]; then
  exit 1
fi

rm -rf build
mkdir -p build
(
cd build || exit
cmake --preset ${BUILD_TYPE}-linux \
    -DUSE_COVERAGE=$USE_COVERAGE \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=$CMAKE_EXPORT_COMPILE_COMMANDS \
    -DDAL_BUILD_PUBLIC=ON \
    -DDAL_CPP_BUILD_EXAMPLES=ON \
    ${ADDITIONAL_CMAKE_FLAGS} ..
make -j"${NUM_CORES}"
make install
)

if [ $? -ne 0 ]; then
  exit 1
fi

# Run all tests via CTest
(
cd build || exit
ctest --output-on-failure
)

if [ $? -ne 0 ]; then
  exit 1
fi

if [ "$COVERAGE" = "true" ]; then
    echo "Generating coverage report..."

    if command -v llvm-cov &> /dev/null && [ -f default.profraw ]; then
        llvm-profdata merge -sparse default.profraw -o default.profdata
        llvm-cov report \
            --ignore-filename-regex="(externals|tests|build|/usr/)" \
            bin/dal_cpp_tests \
            -instr-profile=default.profdata
    elif command -v gcovr &> /dev/null; then
        gcovr -r . --object-directory=build \
            -e "externals" -e "tests" -e "build" \
            --print-summary --sort-percentage --decisions
    elif command -v lcov &> /dev/null; then
        lcov --capture --directory build --output-file coverage.info \
            --rc lcov_branch_coverage=1 --ignore-errors mismatch,gcov,empty,source,negative
        lcov --remove coverage.info '*/externals/*' '*/tests/*' '*/build/*' '*/auto/*' '/usr/*' \
            --output-file coverage_filtered.info --ignore-errors empty,unused
        genhtml coverage_filtered.info --output-directory coverage \
            --branch-coverage --ignore-errors empty,corrupt
        echo "HTML report saved to coverage/index.html"
    else
        echo "No coverage tool found (llvm-cov, gcovr, or lcov). Raw data may be available in build/"
    fi
fi

echo "Finished building of Derivatives Algorithms Library"
