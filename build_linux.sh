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
export SKIP_TESTS=false
export USE_COVERAGE=$COVERAGE  # make it `false` when you need a full performance lib
export CMAKE_EXPORT_COMPILE_COMMANDS=on

echo NUM_CORES: $NUM_CORES
echo BUILD_TYPE: $BUILD_TYPE
echo SKIP_TESTS: $SKIP_TESTS
echo USE_COVERAGE: $USE_COVERAGE
echo DAL_DIR: "$DAL_DIR"
echo CMAKE_EXPORT_COMPILE_COMMANDS: $CMAKE_EXPORT_COMPILE_COMMANDS

rm -rf ./bin
rm -rf ./lib
rm -rf ./coverage

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

rm -rf build
mkdir -p build
(
cd build || exit
cmake --preset ${BUILD_TYPE}-linux -DUSE_COVERAGE=$USE_COVERAGE -DCMAKE_EXPORT_COMPILE_COMMANDS=$CMAKE_EXPORT_COMPILE_COMMANDS ${ADDITIONAL_CMAKE_FLAGS} ..
make -j"${NUM_CORES}"
make install
)

if [ $? -ne 0 ]; then
  exit 1
fi

if [ "$SKIP_TESTS" = "false" ]; then
  bin/test_suite
fi

if [ "$COVERAGE" = "true" ]; then
    echo "Generating coverage report..."

    if command -v llvm-cov &> /dev/null && [ -f default.profraw ]; then
        llvm-profdata merge -sparse default.profraw -o default.profdata
        llvm-cov report \
            --ignore-filename-regex="(externals|tests|build|/usr/)" \
            bin/test_suite \
            -instr-profile=default.profdata
    elif command -v gcovr &> /dev/null; then
        gcovr -r . --object-directory=build \
            -e "externals" -e "tests" -e "build" \
            --print-summary --sort-percentage --decisions
    elif command -v lcov &> /dev/null; then
        lcov --capture --directory build --output-file coverage.info \
            --rc lcov_branch_coverage=1 --ignore-errors mismatch,gcov,empty,source,negative
        lcov --remove coverage.info '*/externals/*' '*/tests/*' '*/build/*' '/usr/*' \
            --output-file coverage_filtered.info --ignore-errors empty,unused
        genhtml coverage_filtered.info --output-directory coverage \
            --branch-coverage --ignore-errors empty,corrupt
        echo "HTML report saved to coverage/index.html"
    else
        echo "No coverage tool found (llvm-cov, gcovr, or lcov). Raw data may be available in build/"
    fi
fi

echo "Finished building of Derivatives Algorithms Library"
