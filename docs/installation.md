# Installation Guide

This guide covers the complete installation process for DAL, including the C++ library, Python bindings, and Web UI.

## Table of Contents

- [System Requirements](#system-requirements)
- [Getting the Source Code](#getting-the-source-code)
- [C++ Library Installation](#c-library-installation)
  - [Linux](#linux)
  - [Windows](#windows)
- [Python Bindings](#python-bindings)
- [Web UI](#web-ui)
- [Verifying Installation](#verifying-installation)
- [Troubleshooting](#troubleshooting)

---

## System Requirements

### Core C++ Library

**Common Requirements:**
- Git (with submodule support)
- CMake 3.21 or later
- C++17-compatible compiler

**Linux:**
- GCC 13+ or Clang 18+
- GNU Make or Ninja
- Python 3.10+ (for Python bindings)

**Windows:**
- Visual Studio 2022 Community Edition (or later)
- MSVC compiler with C++17 support
- Python 3.10+ (for Python bindings)

### Python Bindings

- Python 3.10 or later
- [uv](https://docs.astral.sh/uv/) package manager (recommended) or pip
- pybind11 (vendored as a git submodule at `dal-cpp/externals/pybind11`)

### Web UI

- Python 3.13 or later
- Node.js 20+ and npm
- [uv](https://docs.astral.sh/uv/) package manager

---

## Getting the Source Code

Clone the repository with all submodules:

```bash
# SSH (requires SSH key configured)
git clone --recursive git@github.com:wegamekinglc/Derivatives-Algorithms-Lib.git
cd Derivatives-Algorithms-Lib
```

**Important:** The `--recursive` flag is required to fetch all git submodules (XAD, Adept, CoDiPack, Google Test, RapidJSON, Machinist).

If you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

---

## C++ Library Installation

### Linux

#### Automated Build (Recommended)

The `build_linux.sh` script handles the entire build process:

```bash
bash build_linux.sh
```

This script:
1. Builds the Machinist code generator
2. Runs code generation (creates `dal-cpp/dal/auto/` and `dal-excel/auto/`)
3. Configures CMake with default options
4. Compiles all enabled sub-projects
5. Installs artifacts to the repository root
6. Runs the test suite via CTest

**Build Options:**

To customize the build, pass CMake cache overrides through `ADDITIONAL_CMAKE_FLAGS`:

```bash
# Disable Python bindings
ADDITIONAL_CMAKE_FLAGS="-DDAL_BUILD_PYTHON=OFF" bash build_linux.sh

# Disable benchmarks
ADDITIONAL_CMAKE_FLAGS="-DDAL_CPP_BUILD_BENCHMARKS=OFF" bash build_linux.sh

# Use the XAD backend instead of the preset default (native)
ADDITIONAL_CMAKE_FLAGS="-DDAL_USE_XAD_AAD=ON" bash build_linux.sh
```

#### Manual Build

For more control over the build process:

```bash
mkdir -p build && cd build

# Configure (Release mode)
cmake --preset=Release-linux \
  -DDAL_BUILD_PUBLIC=ON \
  -DDAL_CPP_BUILD_EXAMPLES=ON \
  -DDAL_CPP_BUILD_BENCHMARKS=ON \
  -DDAL_USE_ADEPT_AAD=ON \
  ..

# Build
make -j$(nproc)

# Install (to repository root)
make install
```

**Available CMake Presets:**
- `Release-linux` — Optimized build with debugging symbols
- `Debug-linux` — Debug build with full symbols

**CMake Options:**

The table below shows source-level defaults from `CMakeLists.txt` files. Note that
`CMakePresets.json` (`Release-linux`/`Debug-linux`) overrides several values (for
example: all external AAD backends OFF, examples/benchmarks/public/Python ON, Excel
OFF on Linux and ON on Windows) unless you override them with `-D...` flags.

| Option                     | Default | Description                                    |
|----------------------------|---------|------------------------------------------------|
| `DAL_BUILD_PUBLIC`         | `ON`    | Build `dal-public` (stable public API)         |
| `DAL_BUILD_PYTHON`         | `OFF`   | Build `dal-python` (pybind11 + Python package) |
| `DAL_BUILD_EXCEL`          | `OFF`   | Build `dal-excel` (Windows-only)               |
| `DAL_CPP_BUILD_TESTS`      | `ON`    | Build test suite                               |
| `DAL_CPP_BUILD_EXAMPLES`   | `ON`    | Build example programs                         |
| `DAL_CPP_BUILD_BENCHMARKS` | `ON`    | Build performance benchmarks                   |
| `DAL_USE_ADEPT_AAD`        | `ON`    | Use Adept AAD backend (preset default: OFF)    |
| `DAL_USE_XAD_AAD`          | `OFF`   | Use XAD AAD backend                            |
| `DAL_USE_CODIPACK_AAD`     | `OFF`   | Use CoDiPack AAD backend                       |

Example with custom options:

```bash
cmake --preset=Release-linux \
  -DDAL_BUILD_PYTHON=ON \
  -DDAL_CPP_BUILD_BENCHMARKS=ON \
  -DDAL_USE_XAD_AAD=ON \
  -DDAL_USE_ADEPT_AAD=OFF \
  ..
```

#### Installed Artifacts

After installation, the repository root contains:

```
bin/        — Executables (dal_cpp_tests, dal_public_tests, examples)
lib/        — Static libraries (libdal_cpp.a, libdal_public.a)
include/    — Public headers
```

### Windows

#### Automated Build (Recommended)

Use the batch script:

```cmd
build_windows.bat
```

This performs the same steps as `build_linux.sh`:
1. Builds Machinist
2. Runs code generation
3. Configures CMake (Visual Studio 2022 generator)
4. Compiles all sub-projects
5. Installs artifacts
6. Runs tests via CTest

#### Manual Build

```cmd
mkdir build
cd build

:: Configure
cmake --preset=Release-windows ..

:: Build
cmake --build . --config Release

:: Install
cmake --install . --config Release
```

**Note:** The Excel add-in (`dal-excel`) is only built on Windows when `DAL_BUILD_EXCEL=ON`.

---

## Python Bindings

### Prerequisites

Ensure the C++ library is built and installed (see above), so that `lib/` and `include/` are populated.

### Installation with uv (Recommended)

[uv](https://docs.astral.sh/uv/) is a fast Python package manager that handles virtual environments and dependencies automatically.

From the `dal-python/` directory:

```bash
cd dal-python

# Create virtual environment
uv venv

# Activate it
source .venv/bin/activate        # Linux/macOS
# or: .venv\Scripts\activate     # Windows

# Install in editable mode with test dependencies
uv pip install -e ".[test]" --no-build-isolation \
  --config-settings=cmake.define.DAL_DIR=$(pwd)/..
```

The `DAL_DIR` parameter should point to the repository root (where `lib/` and `include/` are located).

### Quick Verification

Test the installation with the provided test runner:

```bash
cd dal-python
bash run_tests.sh
```

This runs the full pytest suite and verifies that the Python bindings work correctly.

### Manual Installation with pip

If you prefer using pip directly:

```bash
cd dal-python

# Create and activate virtual environment
python -m venv .venv
source .venv/bin/activate        # Linux/macOS
# or: .venv\Scripts\activate     # Windows

# Install build dependencies
pip install scikit-build-core pytest numpy

# Install DAL in editable mode
pip install -e . --no-build-isolation \
  --config-settings=cmake.define.DAL_DIR=$(pwd)/..
```

### What Gets Installed

The `dal` Python package exposes:
- Public C++ API (products, models, valuations)
- Monte Carlo pricing engine
- AAD-aware Greeks computation
- Scripted exotic product support

---

## Web UI

The Web UI is a FastAPI + React application for portfolio management. It is **not** part of the CMake workspace and runs as a separate service.

### Prerequisites

1. Build the C++ library (see [C++ Library Installation](#c-library-installation))
2. Install Python bindings (see [Python Bindings](#python-bindings))
3. Ensure Node.js 20+ and npm are installed

### Installation

The Web UI uses [uv](https://docs.astral.sh/uv/) for Python dependency management.

From the repository root:

```bash
cd dal-web/backend

# Install Python dependencies
uv sync

cd ../frontend

# Install Node.js dependencies
npm install
```

### Running the Web UI

The easiest way to start both services is with the provided scripts:

```bash
# From repository root
./dal-web/scripts/start.sh
```

This script:
1. Checks prerequisites (Python ≥ 3.13, uv, node, npm)
2. Verifies ports 8001 (backend) and 5173 (frontend) are free
3. Installs dependencies if needed
4. Starts the backend (uvicorn on `:8001`)
5. Starts the frontend (vite on `:5173`)
6. Waits for both services to be ready
7. Smoke-tests the proxy

**Access the UI:**
- Frontend: http://localhost:5173
- Backend API docs: http://127.0.0.1:8001/docs

### Stopping the Web UI

```bash
./dal-web/scripts/stop.sh
```

Use `--force` if services don't stop gracefully:

```bash
./dal-web/scripts/stop.sh --force
```

### Using the Native DAL Backend

By default, the Web UI uses a pure-Python stub backend for development. To use the compiled DAL bindings:

1. Build and install Python bindings (see [Python Bindings](#python-bindings))
2. Install into the backend's environment:

```bash
cd dal-web/backend
uv pip install ../../dal-python
```

3. Start the UI with the native backend flag:

```bash
DAL_REQUIRE_NATIVE=1 ./dal-web/scripts/start.sh
```

### Manual Startup (Without Scripts)

If you need to start services individually:

**Backend:**

```bash
cd dal-web/backend
uv sync
uv run python -m uvicorn app.main:app --reload --host 127.0.0.1 --port 8001
```

**Frontend:**

```bash
cd dal-web/frontend
npm install
./node_modules/.bin/vite
```

**Note:** Run vite directly (not `npm run dev`) to avoid parent process issues with signal handling.

---

## Verifying Installation

### C++ Library

Run the test suite:

```bash
# From repository root (after build)
(cd build && ctest --output-on-failure)

# Or run installed binaries directly
bin/dal_cpp_tests
bin/dal_public_tests

# Run specific test suites
bin/dal_cpp_tests --gtest_filter=CurveTest.*
bin/dal_cpp_tests --gtest_filter=AADTest.*
```

### Python Bindings

Run the Python test suite:

```bash
cd dal-python
bash run_tests.sh
```

Or manually:

```bash
cd dal-python
source .venv/bin/activate
pytest tests/ -v
```

### Web UI

After starting the Web UI, verify:

1. Backend health check:

```bash
curl http://127.0.0.1:8001/api/health
# Expected: {"status":"ok","backend":"dal_stub","is_native":false,"evaluation_date":"2022-09-15"}
```

2. Frontend accessibility:

Open http://localhost:5173 in a browser — the dashboard should load.

3. Run backend tests:

```bash
cd dal-web/backend
uv run pytest
```

4. Run frontend e2e smoke tests:

```bash
./dal-web/scripts/setup-playwright.sh
cd dal-web/frontend
npm run test:e2e
```

---

## Troubleshooting

### Common Issues

#### Submodules not initialized

**Error:** CMake configuration fails with missing headers

**Solution:**

```bash
git submodule update --init --recursive
```

#### Code generation fails

**Error:** `Machinist: command not found` or missing generated files

**Solution:** Ensure `build_linux.sh` (or `build_windows.bat`) completed successfully. The script builds Machinist and runs code generation automatically.

To manually regenerate:

```bash
cd dal-cpp/externals/machinist
bash build_linux.sh

cd ../../..
export MACHINIST_TEMPLATE_DIR=$PWD/dal-cpp/externals/machinist/template/
./dal-cpp/externals/machinist/bin/Machinist -c dal-cpp/config/dal.ifc -l dal-cpp/config/dal.mgl -d ./dal-cpp/dal
./dal-cpp/externals/machinist/bin/Machinist -c dal-cpp/config/dal.ifc -l dal-cpp/config/dal.mgl -d ./dal-excel
```

#### Python bindings import error

**Error:** `ModuleNotFoundError: No module named 'dal'`

**Solution:**
1. Ensure virtual environment is activated: `source .venv/bin/activate`
2. Verify installation: `pip list | grep dal`
3. Reinstall if needed: `uv pip install -e . --no-build-isolation --config-settings=cmake.define.DAL_DIR=/path/to/repo`

#### Web UI port already in use

**Error:** `Address already in use` for ports 8001 or 5173

**Solution:**

```bash
# Stop any running instances
./dal-web/scripts/stop.sh

# Or manually kill processes
sudo fuser -k 8001/tcp
sudo fuser -k 5173/tcp
```

#### CMake build fails with C++17 errors

**Error:** Compiler complains about missing C++17 features

**Solution:**
- **Linux:** Ensure GCC 13+ or Clang 18+ is installed
- **Windows:** Ensure Visual Studio 2022 is installed and up to date

Check compiler version:

```bash
g++ --version    # Linux
cl               # Windows (from Developer Command Prompt)
```

#### Python bindings fail to compile

**Error:** pybind11 compilation errors

**Solution:**
1. Ensure pybind11 is available: it is vendored as a git submodule at `dal-cpp/externals/pybind11` (run `git submodule update --init` if the directory is empty), or install `pybind11` system-wide and set `pybind11_DIR` or `CMAKE_PREFIX_PATH` so CMake's `find_package` locates it.
2. Clean and rebuild:

```bash
cd dal-python
rm -rf build/ dist/ *.egg-info
uv pip install -e . --no-build-isolation --force-reinstall
```

#### Web UI backend fails to start

**Error:** Backend crashes on startup

**Solution:**
1. Check logs: `cat dal-web/backend/.server.log`
2. Verify Python dependencies: `cd dal-web/backend && uv sync`
3. Test backend manually:

```bash
cd dal-web/backend
uv run python -m uvicorn app.main:app --host 127.0.0.1 --port 8001
```

### Getting Help

If you encounter issues not covered here:

1. Check the main [README.md](../README.md) for additional context
2. Review [methodology docs](methodology/) for algorithm details
3. Open an issue on [GitHub](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/issues)

---

## Next Steps

After successful installation:

- **Try the examples:** Run programs in `dal-cpp/examples/` to see DAL in action
- **Explore the Web UI:** Visit http://localhost:5173 to manage portfolios
- **Read the methodology:** Check [docs/methodology/](methodology/) for deep dives into algorithms
- **Run benchmarks:** Build with `DAL_CPP_BUILD_BENCHMARKS=ON` and run programs in `dal-cpp/benchmarks/`
