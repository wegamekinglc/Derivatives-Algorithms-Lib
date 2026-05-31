#!/bin/bash
# Build Python wheel package for dal-python
#
# This script builds a binary wheel (.whl) that contains the compiled _dal extension.
# The wheel can be installed without requiring compilation or the C++ source code.
#
# Prerequisites:
# - C++ library must be built (lib/libdal_public.a and lib/libdal_cpp.a)
# - uv must be installed
# - Python 3.10+ with development headers
#
# Usage:
#   ./build_wheel.sh              # Build wheel for current platform
#   ./build_wheel.sh --manylinux  # Build manylinux-compatible wheel (Linux only)
#   ./build_wheel.sh --clean      # Clean build artifacts before building

set -e

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
for arg in "$@"; do
    case $arg in
        --manylinux)
            MANYLINUX=true
            shift
            ;;
        --clean)
            CLEAN=true
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
            echo "  --help, -h     Show this help message"
            exit 0
            ;;
    esac
done

# Check prerequisites
echo -e "${YELLOW}Checking prerequisites...${NC}"

if ! command -v uv &> /dev/null; then
    echo -e "${RED}Error: uv is not installed${NC}"
    echo "Install uv: curl -LsSf https://astral.sh/uv/install.sh | sh"
    exit 1
fi

DAL_DIR="${DAL_DIR:-$(dirname "$SCRIPT_DIR")}"
if [ ! -f "$DAL_DIR/lib/libdal_public.a" ]; then
    echo -e "${RED}Error: libdal_public.a not found in $DAL_DIR/lib/${NC}"
    echo "Build the C++ library first: cd $DAL_DIR && ./build_linux.sh"
    exit 1
fi

if [ ! -f "$DAL_DIR/lib/libdal_cpp.a" ]; then
    echo -e "${RED}Error: libdal_cpp.a not found in $DAL_DIR/lib/${NC}"
    echo "Build the C++ library first: cd $DAL_DIR && ./build_linux.sh"
    exit 1
fi

echo -e "${GREEN}✓ Prerequisites satisfied${NC}"

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning build artifacts...${NC}"
    rm -rf build/ dist/ *.egg-info .pytest_cache
    find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
    echo -e "${GREEN}✓ Clean complete${NC}"
fi

# Create build environment
echo -e "${YELLOW}Creating build environment...${NC}"
if [ ! -d ".venv" ]; then
    uv venv
fi
source .venv/bin/activate
echo -e "${GREEN}✓ Build environment ready${NC}"

# Install build dependencies
echo -e "${YELLOW}Installing build dependencies...${NC}"
uv pip install -q scikit-build-core cmake ninja swig build
if [ "$MANYLINUX" = true ]; then
    uv pip install -q auditwheel
fi
echo -e "${GREEN}✓ Build dependencies installed${NC}"

# Build wheel
echo -e "${YELLOW}Building wheel...${NC}"
export DAL_DIR
# Use --no-build-isolation to use the current venv's dependencies
# and pass DAL_DIR through config-settings
uv build --wheel --no-build-isolation --config-settings=cmake.define.DAL_DIR="$DAL_DIR"

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
