#!/bin/bash
# Build Python wheel package for dal-python
#
# This script builds a binary wheel (.whl) that contains the compiled _dal extension.
# The wheel can be installed without requiring compilation or the C++ source code.
#
# Prerequisites:
# - Staged C++ install must contain the exported dal-public CMake package
# - uv must be installed
# - CPython 3.9-3.13 with development headers
#
# Usage:
#   ./build_wheel.sh              # Build wheel for current platform
#   ./build_wheel.sh --python 3.9 # Select an exact supported CPython
#   ./build_wheel.sh --manylinux  # Build manylinux-compatible wheel (Linux only)
#   ./build_wheel.sh --clean      # Clean build artifacts before building

set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Parse arguments
MANYLINUX=false
CLEAN=false
PYTHON_REQUESTED=false
PYTHON_VERSION=""
SUPPORTED_PYTHONS="3.9, 3.10, 3.11, 3.12, 3.13"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --manylinux)
            MANYLINUX=true
            ;;
        --clean)
            CLEAN=true
            ;;
        --python)
            PYTHON_REQUESTED=true
            if [[ $# -lt 2 ]]; then
                echo "--python requires one of $SUPPORTED_PYTHONS" >&2
                exit 1
            fi
            PYTHON_VERSION=$2
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Build Python wheel package for dal-python"
            echo ""
            echo "Options:"
            echo "  --manylinux    Build manylinux-compatible wheel (Linux only)"
            echo "  --clean        Clean build artifacts before building"
            echo "  --python MINOR Select CPython 3.9, 3.10, 3.11, 3.12, or 3.13"
            echo "  --help, -h     Show this help message"
            exit 0
            ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
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

# Check prerequisites
echo -e "${YELLOW}Checking prerequisites...${NC}"

if ! command -v uv &> /dev/null; then
    echo -e "${RED}Error: uv is not installed${NC}"
    echo "Install uv: curl -LsSf https://astral.sh/uv/install.sh | sh"
    exit 1
fi

REPO_ROOT="$(dirname "$SCRIPT_DIR")"
DAL_INSTALL_PREFIX="${DAL_INSTALL_PREFIX:-${DAL_DIR:-$REPO_ROOT/build/stage/Release-linux}}"
DAL_DIR="$DAL_INSTALL_PREFIX"
DAL_PUBLIC_CONFIG=$(find "$DAL_INSTALL_PREFIX" -type f -path "*/cmake/dal-public/dal-publicConfig.cmake" -print -quit 2>/dev/null || true)
if [ -z "$DAL_PUBLIC_CONFIG" ]; then
    echo -e "${RED}Error: dal-publicConfig.cmake not found under $DAL_INSTALL_PREFIX${NC}"
    echo "Build the staged C++ install first: cd $REPO_ROOT && ./build_linux.sh"
    exit 1
fi

echo -e "${GREEN}✓ Prerequisites satisfied${NC}"

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning build artifacts...${NC}"
    rm -rf build/ dist/ .venv/ *.egg-info .pytest_cache
    find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
    echo -e "${GREEN}✓ Clean complete${NC}"
fi

# Create build environment
echo -e "${YELLOW}Creating build environment...${NC}"
VENV_DIR="$SCRIPT_DIR/.venv"
VENV_PYTHON="$VENV_DIR/bin/python"
if [[ ! -x "$VENV_PYTHON" ]]; then
    if [[ -e "$VENV_DIR" ]]; then
        echo "build_wheel.sh: environment '$VENV_DIR' has no executable Python; rerun with --clean to recreate it" >&2
        exit 1
    fi
    if [[ $PYTHON_REQUESTED == true ]]; then
        uv venv "$VENV_DIR" --python "$PYTHON_VERSION"
    else
        uv venv "$VENV_DIR" --python ">=3.9,<3.14"
    fi
fi
compat_args=(
    --entry-point dal-python/build_wheel.sh
    --environment "$VENV_DIR"
    --remediation "rerun with --clean to recreate $VENV_DIR"
)
if [[ $PYTHON_REQUESTED == true ]]; then
    compat_args+=(--requested "$PYTHON_VERSION")
fi
"$VENV_PYTHON" "$SCRIPT_DIR/scripts/python_compat.py" "${compat_args[@]}"
source "$VENV_DIR/bin/activate"
echo -e "${GREEN}✓ Build environment ready${NC}"

# Install build dependencies
echo -e "${YELLOW}Installing build dependencies...${NC}"
uv pip install -q "scikit-build-core==1.0.3" cmake ninja build
if [ "$MANYLINUX" = true ]; then
    uv pip install -q auditwheel
fi
echo -e "${GREEN}✓ Build dependencies installed${NC}"

# Build wheel
echo -e "${YELLOW}Building wheel...${NC}"
export DAL_DIR DAL_INSTALL_PREFIX
# Use --no-build-isolation to use the current venv's dependencies
# and pass the installed DAL package prefix through config-settings
uv build --wheel --no-build-isolation \
    --config-settings=cmake.define.DAL_INSTALL_PREFIX="$DAL_INSTALL_PREFIX" \
    --config-settings=cmake.define.CMAKE_PREFIX_PATH="$DAL_INSTALL_PREFIX"

# Find the built wheel
WHEEL_FILE=$(ls -1 dist/*.whl 2>/dev/null | head -n 1)
if [ -z "$WHEEL_FILE" ]; then
    echo -e "${RED}Error: No wheel file found in dist/${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Wheel built: $WHEEL_FILE${NC}"

# Repair wheel for manylinux compatibility (Linux only)
if [ "$MANYLINUX" = true ]; then
    if [[ "$(uname)" == "Linux" ]]; then
        echo -e "${YELLOW}Repairing wheel for manylinux compatibility...${NC}"

        # Create wheelhouse directory
        mkdir -p wheelhouse

        # Repair the wheel
        auditwheel repair "$WHEEL_FILE" -w wheelhouse

        # Find the repaired wheel
        REPAIRED_WHEEL=$(ls -1 wheelhouse/*.whl 2>/dev/null | head -n 1)
        if [ -n "$REPAIRED_WHEEL" ]; then
            echo -e "${GREEN}✓ Manylinux wheel created: $REPAIRED_WHEEL${NC}"
            WHEEL_FILE="$REPAIRED_WHEEL"
        else
            echo -e "${YELLOW}Warning: auditwheel repair did not produce output${NC}"
            echo "The original wheel may still work on your system"
        fi
    else
        echo -e "${YELLOW}Warning: --manylinux flag ignored (not on Linux)${NC}"
    fi
fi

# Display wheel info
echo ""
echo -e "${GREEN}Build complete!${NC}"
echo "Wheel location: $WHEEL_FILE"
echo "Wheel size: $(du -h "$WHEEL_FILE" | cut -f1)"
echo ""

# Show installation instructions
echo "To install the wheel:"
echo "  pip install $WHEEL_FILE"
echo ""
echo "Or with uv:"
echo "  uv pip install $WHEEL_FILE"
echo ""

# Show wheel contents
if command -v unzip &> /dev/null; then
    echo "Wheel contents:"
    unzip -l "$WHEEL_FILE" | grep -E "\.py$|\.so$|\.pyd$" | awk '{print "  " $4}' | head -20
    TOTAL=$(unzip -l "$WHEEL_FILE" | grep -cE "\.py$|\.so$|\.pyd$")
    if [ "$TOTAL" -gt 20 ]; then
        echo "  ... and $((TOTAL - 20)) more files"
    fi
fi
