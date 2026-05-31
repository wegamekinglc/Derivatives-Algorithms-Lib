#!/bin/bash
# Build Python source distribution (sdist) for dal-python
#
# This script builds a source distribution (.tar.gz) that can be used to build
# the package from source on any platform with the required C++ dependencies.
#
# Prerequisites:
# - uv must be installed
# - Python 3.10+
#
# Usage:
#   ./build_sdist.sh         # Build source distribution
#   ./build_sdist.sh --clean # Clean build artifacts before building

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Parse arguments
CLEAN=false
for arg in "$@"; do
    case $arg in
        --clean)
            CLEAN=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Build Python source distribution for dal-python"
            echo ""
            echo "Options:"
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
uv pip install -q scikit-build-core build
echo -e "${GREEN}✓ Build dependencies installed${NC}"

# Build sdist
echo -e "${YELLOW}Building source distribution...${NC}"
uv build --sdist --no-build-isolation

# Find the built sdist
SDIST_FILE=$(ls -1 dist/*.tar.gz 2>/dev/null | head -n 1)
if [ -z "$SDIST_FILE" ]; then
    echo -e "${RED}Error: No source distribution found in dist/${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Source distribution built: $SDIST_FILE${NC}"

# Display sdist info
echo ""
echo -e "${GREEN}Build complete!${NC}"
echo "Source distribution: $SDIST_FILE"
echo "Size: $(du -h "$SDIST_FILE" | cut -f1)"
echo ""

# Show installation instructions
echo "To build and install from source:"
echo "  pip install $SDIST_FILE \\"
echo "    --config-settings=cmake.define.DAL_DIR=/path/to/Derivatives-Algorithms-Lib"
echo ""
echo "Or with uv:"
echo "  uv pip install $SDIST_FILE \\"
echo "    --config-settings=cmake.define.DAL_DIR=/path/to/Derivatives-Algorithms-Lib"
echo ""
echo "Note: Building from source requires:"
echo "  - C++17 compiler (GCC 13+, Clang 18+, or MSVC 2022)"
echo "  - CMake 3.21+"
echo "  - SWIG 4.x"
echo "  - Python 3.10+ development headers"
echo "  - DAL C++ library (libdal_public.a and libdal_cpp.a)"
echo ""

# Show sdist contents
if command -v tar &> /dev/null; then
    echo "Source distribution contents:"
    tar -tzf "$SDIST_FILE" | grep -E "\.py$|\.i$|pyproject\.toml|README\.md|CMakeLists\.txt" | sed 's/^/  /' | head -20
    TOTAL=$(tar -tzf "$SDIST_FILE" | wc -l)
    if [ "$TOTAL" -gt 20 ]; then
        echo "  ... and $((TOTAL - 20)) more files"
    fi
fi
