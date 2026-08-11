#!/bin/bash
# Build Python source distribution (sdist) for dal-python
#
# This script builds a source distribution (.tar.gz) that can be used to build
# the package from source on any platform with the required C++ dependencies.
#
# Prerequisites:
# - uv must be installed
# - CPython 3.9-3.13
#
# Usage:
#   ./build_sdist.sh         # Build source distribution
#   ./build_sdist.sh --python 3.9 # Select an exact supported CPython
#   ./build_sdist.sh --clean      # Clean build artifacts before building

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
PYTHON_REQUESTED=false
PYTHON_VERSION=""
SUPPORTED_PYTHONS="3.9, 3.10, 3.11, 3.12, 3.13"
while [[ $# -gt 0 ]]; do
    case "$1" in
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
            echo "Build Python source distribution for dal-python"
            echo ""
            echo "Options:"
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
        echo "build_sdist.sh: environment '$VENV_DIR' has no executable Python; rerun with --clean to recreate it" >&2
        exit 1
    fi
    if [[ $PYTHON_REQUESTED == true ]]; then
        uv venv "$VENV_DIR" --python "$PYTHON_VERSION"
    else
        uv venv "$VENV_DIR" --python ">=3.9,<3.14"
    fi
fi
compat_args=(
    --entry-point dal-python/build_sdist.sh
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
uv pip install -q "scikit-build-core==1.0.3" build "pybind11==2.11.1"
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
echo "    \"--config-settings=cmake.define.DAL_INSTALL_PREFIX=/absolute/path/to/Derivatives-Algorithms-Lib/build/stage/<platform-preset>\""
echo ""
echo "Or with uv:"
echo "  uv pip install $SDIST_FILE \\"
echo "    \"--config-settings=cmake.define.DAL_INSTALL_PREFIX=/absolute/path/to/Derivatives-Algorithms-Lib/build/stage/<platform-preset>\""
echo ""
echo "Note: Building from source requires:"
echo "  - C++17 compiler (GCC 13+, Clang 18+, or MSVC 2022)"
echo "  - CMake 3.21+"
echo "  - pybind11 2.11.1 (installed automatically from the sdist build requirements)"
echo "  - CPython 3.9-3.13 development headers"
echo "  - DAL staged install containing lib/cmake/dal-public/dal-publicConfig.cmake"
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
