#!/bin/bash
#
# Build and test the DAL Python binding using a fresh uv-managed environment.
#
# Usage:
#   cd dal-python && bash run_tests.sh              # run all tests
#   cd dal-python && bash run_tests.sh -v            # verbose pytest output
#   cd dal-python && bash run_tests.sh -k test_date  # run specific tests
#
# Prerequisites:
#   - uv (https://docs.astral.sh/uv/)
#   - The C++ library must be built first (run ../build_linux.sh from repo root)
#   - pybind11 (fetched via CMake FetchContent) and Python 3.10+ development headers
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

DAL_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "================================================"
echo " DAL Python Binding — Build & Test (uv-managed)"
echo "================================================"
echo "  DAL root:    $DAL_DIR"
echo "  Working dir: $SCRIPT_DIR"
echo ""

# ---- Step 1: Verify C++ library is built ------------------------------------

if [[ ! -f "$DAL_DIR/lib/libdal_public.a" ]] || [[ ! -f "$DAL_DIR/lib/libdal_cpp.a" ]]; then
    echo "ERROR: DAL C++ libraries not found in $DAL_DIR/lib/"
    echo "       Run the main build first:"
    echo ""
    echo "         cd $DAL_DIR && bash build_linux.sh"
    echo ""
    exit 1
fi
echo "[OK] C++ libraries found"

# ---- Step 2: Create or reuse uv virtual environment -------------------------

VENV_DIR="$SCRIPT_DIR/.venv"
if [[ ! -d "$VENV_DIR" ]]; then
    echo "Creating fresh uv virtual environment..."
    uv venv "$VENV_DIR" --python ">=3.10"
else
    echo "Reusing existing virtual environment at .venv/"
fi

# Activate the venv for subsequent commands
# shellcheck disable=SC1091
source "$VENV_DIR/bin/activate"
echo "  Python: $(python --version)"
echo "  Path:   $(which python)"

# ---- Step 3: Install build + test dependencies ------------------------------

echo ""
echo "Installing dependencies (scikit-build-core, pytest, numpy)..."
uv pip install scikit-build-core pytest numpy

# ---- Step 4: Install the package in editable mode ---------------------------

echo ""
echo "Installing dal-python in editable mode..."
echo "  DAL_DIR=$DAL_DIR"

export DAL_DIR

uv pip install \
    --no-build-isolation \
    --config-settings=cmake.define.DAL_DIR="$DAL_DIR" \
    -e ".[test]" 2>&1

echo "[OK] Package installed"

# ---- Step 5: Verify import works --------------------------------------------

echo ""
echo "Verifying import..."
if python -c "import dal; print(f'  dal {dal.__version__} loaded successfully')"; then
    echo "[OK] Import successful"
else
    echo "ERROR: Failed to import dal module"
    exit 1
fi

# ---- Step 6: Run tests ------------------------------------------------------

echo ""
echo "================================================"
echo " Running tests"
echo "================================================"
echo ""

# Pass through any extra arguments to pytest (e.g., -v, -k, --tb=short)
python -m pytest tests/ -v "$@"
