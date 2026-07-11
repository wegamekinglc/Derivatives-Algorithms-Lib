#!/usr/bin/env bash

set -euo pipefail

COVERAGE=false
BUILD_PYTHON=false
BUILD_BENCHMARKS=false
GENERATE=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --coverage) COVERAGE=true ;;
        --full) BUILD_PYTHON=true; BUILD_BENCHMARKS=true ;;
        --benchmarks) BUILD_BENCHMARKS=true ;;
        --generate) GENERATE=true ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
    shift
done

DAL_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BUILD_TYPE=${BUILD_TYPE:-Release}
PRESET="${BUILD_TYPE}-linux"
BUILD_DIR=${DAL_BUILD_DIR:-"$DAL_DIR/build/$PRESET"}
INSTALL_DIR=${DAL_INSTALL_DIR:-"$DAL_DIR/build/stage/$PRESET"}
NUM_CORES=${NUM_CORES:-$(nproc)}

if [[ "${ADDITIONAL_CMAKE_FLAGS:-}" =~ DAL_BUILD_PYTHON=(ON|on|TRUE|true|1) ]]; then
    BUILD_PYTHON=true
fi
if [[ "${ADDITIONAL_CMAKE_FLAGS:-}" =~ DAL_CPP_BUILD_BENCHMARKS=(ON|on|TRUE|true|1) ]]; then
    BUILD_BENCHMARKS=true
fi

cmake_flags=(
    "-DUSE_COVERAGE=$COVERAGE"
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    "-DDAL_BUILD_PUBLIC=ON"
    "-DDAL_CPP_BUILD_EXAMPLES=ON"
    "-DDAL_CPP_BUILD_BENCHMARKS=$([[ $BUILD_BENCHMARKS == true ]] && echo ON || echo OFF)"
    "-DDAL_BUILD_PYTHON=$([[ $BUILD_PYTHON == true ]] && echo ON || echo OFF)"
)

if [[ -n "${ADDITIONAL_CMAKE_FLAGS:-}" ]]; then
    # Preserve the existing CI/user override contract. Values containing spaces
    # should be supplied through a CMake cache or a preset instead.
    read -r -a additional_flags <<< "$ADDITIONAL_CMAKE_FLAGS"
    cmake_flags+=("${additional_flags[@]}")
fi

if [[ $BUILD_PYTHON == true ]]; then
    venv_dir="$DAL_DIR/dal-python/.venv"
    python_bin="$venv_dir/bin/python"
    if [[ ! -x "$python_bin" ]]; then
        if command -v uv >/dev/null 2>&1; then
            uv venv "$venv_dir" --python ">=3.10"
        elif command -v python3 >/dev/null 2>&1; then
            python3 -m venv "$venv_dir"
        else
            echo "Python bindings requested, but neither uv nor python3 is available." >&2
            exit 1
        fi
    fi

    if ! "$python_bin" -c "import numpy, pytest" >/dev/null 2>&1; then
        if command -v uv >/dev/null 2>&1; then
            uv pip install --python "$python_bin" "numpy>=1.24" "pytest>=7.0"
        else
            "$python_bin" -m pip install "numpy>=1.24" "pytest>=7.0"
        fi
    fi
    export PATH="$venv_dir/bin:$PATH"
    cmake_flags+=("-DPython3_EXECUTABLE=$python_bin")
fi

echo "Build type:       $BUILD_TYPE"
echo "Build directory:  $BUILD_DIR"
echo "Install prefix:   $INSTALL_DIR"
echo "Parallel jobs:    $NUM_CORES"
echo "Coverage:         $COVERAGE"
echo "Python:           $BUILD_PYTHON"
echo "Benchmarks:       $BUILD_BENCHMARKS"

cmake --preset "$PRESET" -S "$DAL_DIR" -B "$BUILD_DIR" "${cmake_flags[@]}"

if [[ $GENERATE == true ]]; then
    cmake --build "$BUILD_DIR" --target dal_generate --parallel "$NUM_CORES"
fi

cmake --build "$BUILD_DIR" --parallel "$NUM_CORES"

canonical_root=$(realpath -m "$DAL_DIR")
canonical_install=$(realpath -m "$INSTALL_DIR")
case "$canonical_install" in
    "$canonical_root"/build/*) rm -rf -- "$canonical_install" ;;
    *) echo "Leaving custom install prefix in place: $canonical_install" ;;
esac
cmake --install "$BUILD_DIR" --prefix "$INSTALL_DIR"

ctest_flags=(--test-dir "$BUILD_DIR" --output-on-failure)
if [[ "${VERBOSE:-0}" == "1" ]]; then
    ctest_flags+=(--verbose)
fi
ctest "${ctest_flags[@]}"

if [[ $COVERAGE == true ]]; then
    echo "Generating coverage report..."
    if command -v llvm-cov >/dev/null 2>&1 && [[ -f "$BUILD_DIR/default.profraw" ]]; then
        llvm-profdata merge -sparse "$BUILD_DIR/default.profraw" -o "$BUILD_DIR/default.profdata"
        test_binary=$(find "$BUILD_DIR" -type f -name dal_cpp_tests -print -quit)
        llvm-cov report \
            --ignore-filename-regex="(externals|tests|build|/usr/)" \
            "$test_binary" -instr-profile="$BUILD_DIR/default.profdata"
    elif command -v gcovr >/dev/null 2>&1; then
        gcovr -r "$DAL_DIR" --object-directory="$BUILD_DIR" \
            -e "externals" -e "tests" -e "build" \
            --print-summary --sort-percentage --decisions
    elif command -v lcov >/dev/null 2>&1; then
        lcov --capture --directory "$BUILD_DIR" --output-file "$DAL_DIR/coverage.info" \
            --rc lcov_branch_coverage=1 --ignore-errors mismatch,gcov,empty,source,negative
        lcov --remove "$DAL_DIR/coverage.info" '*/externals/*' '*/tests/*' '*/build/*' '*/auto/*' '/usr/*' \
            --output-file "$DAL_DIR/coverage_filtered.info" --ignore-errors empty,unused
        genhtml "$DAL_DIR/coverage_filtered.info" --output-directory "$DAL_DIR/coverage" \
            --branch-coverage --ignore-errors empty,corrupt
        echo "HTML report saved to $DAL_DIR/coverage/index.html"
    else
        echo "No coverage tool found (llvm-cov, gcovr, or lcov)."
    fi
fi

echo "Finished building Derivatives Algorithms Library"
