#!/bin/bash
#
# Build and test the DAL Python binding using a fresh uv-managed environment.
#
# Usage:
#   cd dal-python && bash run_tests.sh              # run all tests
#   cd dal-python && bash run_tests.sh --python 3.9  # select an exact CPython
#   cd dal-python && bash run_tests.sh -v            # verbose pytest output
#   cd dal-python && bash run_tests.sh -k test_date  # run specific tests
#
# Prerequisites:
#   - uv (https://docs.astral.sh/uv/)
#   - The staged C++ install must exist (run ../build_linux.sh from repo root)
#   - pybind11 (vendored as a git submodule at dal-cpp/externals/pybind11, v2.11.1) and CPython 3.9-3.13 development headers
#

set -eu

PYTHON_REQUESTED=false
PYTHON_VERSION=""
SUPPORTED_PYTHONS="3.9, 3.10, 3.11, 3.12, 3.13"
PYTEST_ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --python)
            PYTHON_REQUESTED=true
            if [[ $# -lt 2 ]]; then
                echo "--python requires one of $SUPPORTED_PYTHONS" >&2
                exit 1
            fi
            PYTHON_VERSION=$2
            shift
            ;;
        *) PYTEST_ARGS+=("$1") ;;
    esac
    shift
done

if [[ $PYTHON_REQUESTED == true ]]; then
    case "$PYTHON_VERSION" in
        3.9|3.10|3.11|3.12|3.13) ;;
        *)
            echo "--python: unsupported value '$PYTHON_VERSION'; expected one of $SUPPORTED_PYTHONS" >&2
            exit 1
            ;;
    esac
fi

if ! command -v uv >/dev/null 2>&1; then
    echo "Error: uv is required by dal-python/run_tests.sh" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DAL_INSTALL_PREFIX="${DAL_INSTALL_PREFIX:-${DAL_DIR:-$REPO_ROOT/build/stage/Release-linux}}"
DAL_DIR="$DAL_INSTALL_PREFIX"

echo "================================================"
echo " DAL Python Binding — Build & Test (uv-managed)"
echo "================================================"
echo "  DAL install: $DAL_INSTALL_PREFIX"
echo "  Working dir: $SCRIPT_DIR"
echo ""

# ---- Step 1: Verify C++ library is built ------------------------------------

DAL_PUBLIC_CONFIG=$(find "$DAL_INSTALL_PREFIX" -type f -path "*/cmake/dal-public/dal-publicConfig.cmake" -print -quit 2>/dev/null || true)
if [[ -z "$DAL_PUBLIC_CONFIG" ]]; then
    echo "ERROR: dal-publicConfig.cmake not found under $DAL_INSTALL_PREFIX"
    echo "       Run the main build first:"
    echo ""
    echo "         cd $REPO_ROOT && bash build_linux.sh"
    echo ""
    exit 1
fi
echo "[OK] DAL CMake package found: $DAL_PUBLIC_CONFIG"

# ---- Step 2: Create or reuse uv virtual environment -------------------------

VENV_DIR="$SCRIPT_DIR/.venv"
VENV_PYTHON="$VENV_DIR/bin/python"
if [[ ! -x "$VENV_PYTHON" ]]; then
    if [[ -e "$VENV_DIR" ]]; then
        echo "run_tests.sh: environment '$VENV_DIR' has no executable Python; remove or replace $VENV_DIR, then rerun" >&2
        exit 1
    fi
    echo "Creating fresh uv virtual environment..."
    if [[ $PYTHON_REQUESTED == true ]]; then
        uv venv "$VENV_DIR" --python "$PYTHON_VERSION"
    else
        uv venv "$VENV_DIR" --python ">=3.9,<3.14"
    fi
else
    echo "Reusing existing virtual environment at .venv/"
fi

compat_args=(
    --entry-point dal-python/run_tests.sh
    --environment "$VENV_DIR"
    --remediation "remove or replace $VENV_DIR, then rerun"
)
if [[ $PYTHON_REQUESTED == true ]]; then
    compat_args+=(--requested "$PYTHON_VERSION")
fi
"$VENV_PYTHON" "$SCRIPT_DIR/scripts/python_compat.py" "${compat_args[@]}"

# Activate the venv for subsequent commands
# shellcheck disable=SC1091
source "$VENV_DIR/bin/activate"
echo "  Python: $(python --version)"
echo "  Path:   $(which python)"

# ---- Step 3: Install build + test dependencies ------------------------------

echo ""
echo "Installing dependencies (scikit-build-core, pytest, numpy)..."
uv pip install "scikit-build-core==1.0.3" pytest numpy

# ---- Step 4: Install the package in editable mode ---------------------------

echo ""
echo "Installing dal-python in editable mode..."
echo "  DAL_INSTALL_PREFIX=$DAL_INSTALL_PREFIX"

export DAL_DIR DAL_INSTALL_PREFIX

uv pip install \
    --no-build-isolation \
    --config-settings=cmake.define.DAL_INSTALL_PREFIX="$DAL_INSTALL_PREFIX" \
    --config-settings=cmake.define.CMAKE_PREFIX_PATH="$DAL_INSTALL_PREFIX" \
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
python -m pytest tests/ -v "${PYTEST_ARGS[@]}"
