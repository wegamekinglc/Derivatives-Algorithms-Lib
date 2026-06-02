# DAL -- Derivatives Algorithms Library

[![CMake Linux CI](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/workflows/cmake-linux.yml/badge.svg?branch=master)](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/workflows/cmake-linux.yml)
[![Codacy Grade](https://app.codacy.com/project/badge/Grade/9c84afd2bb534c6c87584e5d6e4cc420)](https://app.codacy.com/app/wegamekinglc/Derivatives-Algorithms-Lib)
[![Coverage Status](https://coveralls.io/repos/wegamekinglc/Derivatives-Algorithms-Lib/badge.svg?branch=master)](https://coveralls.io/github/wegamekinglc/Derivatives-Algorithms-Lib?branch=master)

## Overview

DAL is a C++17 quantitative finance library with built-in support for Automatic Adjoint Differentiation (AAD). It covers yield curve construction and calibration, Monte Carlo simulation, finite difference PDE solvers, a domain-specific scripting engine for exotic payoffs, and parallel model evaluation.

The project draws from the work of Tom Hyer (*Derivatives Algorithms: Bones*), Antoine Savine (*Modern Computational Finance: AAD and Parallel Simulations* and *Scripting for Derivatives and xVA*), and Brian Huge and Jesper Andreasen (*Finite Difference Methods for Financial PDEs*). Some implementation patterns trace back to those sources.

## Getting the Code

The build depends on several Git submodules (XAD, Adept, CoDiPack, Google Test, RapidJSON, and the Machinist code generator). Clone the repository and initialize the submodules before building:

```bash
git clone https://github.com/wegamekinglc/Derivatives-Algorithms-Lib.git
cd Derivatives-Algorithms-Lib
git submodule update --init --recursive
```

> **Note:** If you already cloned without `--recursive`, run `git submodule update --init --recursive` from the repository root. A missing submodule is the most common cause of configuration failures.

## Building

DAL builds with CMake (3.21+) on both Linux and Windows. The repository ships with `CMakePresets.json` and two convenience scripts (`build_linux.sh`, `build_windows.bat`) that wrap the full workflow: building the Machinist code generator, running code generation, configuring CMake, compiling, installing, and running the tests.

The build always happens in two stages:

1. **Code generation** -- the Machinist tool (in `externals/machinist`) reads the interface files in `config/` (`dal.ifc`, `dal.mgl`) and generates source into `dal/` and `public/`. This must run before the CMake build.
2. **CMake build** -- compiles the core library, the public API, the examples, and the test suite, then installs artifacts under the repository root (`lib/`, `bin/`).

Installed artifacts:
- `lib/` -- the static `dal` library (plus the XLL Excel add-in on Windows when Office is detected)
- `bin/` -- the `test_suite` binary and the example programs

### Linux

**Prerequisites**

- `git`
- `cmake` 3.21 or newer
- A C++17 compiler -- GCC 13+ or Clang 18+ (the CI matrix covers `gcc-13`, `gcc-14`, `clang-18`, `clang-19`)
- `make`
- Build essentials used by the Adept submodule: `autotools-dev`, `autoconf`, `libtool`
- OpenMP runtime (e.g. `libomp-dev` for Clang)
- Optional, for coverage reports: `lcov`, `gcovr`, or `llvm-cov`

On Debian/Ubuntu, for example:

```bash
sudo apt update
sudo apt install -y git cmake make g++ autotools-dev autoconf libtool libomp-dev
```

**One-step build (recommended)**

```bash
bash build_linux.sh
```

This script cleans previous `bin/`/`lib/` output, builds Machinist, runs code generation, configures with the `Release-linux` preset, compiles with all available cores, installs, and runs `bin/test_suite`. Add `--coverage` to additionally produce a coverage report:

```bash
bash build_linux.sh --coverage
```

**Manual build**

If you prefer to run the steps yourself (for example to use a different compiler or build type), first build Machinist and run code generation, then configure and build with CMake:

```bash
# 1. Build the Machinist code generator
(cd externals/machinist && bash -e ./build_linux.sh)

# 2. Generate sources
export MACHINIST_TEMPLATE_DIR="$PWD/externals/machinist/template/"
./externals/machinist/bin/Machinist -c config/dal.ifc -l config/dal.mgl -d ./dal
./externals/machinist/bin/Machinist -c config/dal.ifc -l config/dal.mgl -d ./public

# 3. Configure, build, and install
mkdir -p build && cd build
cmake --preset=Release-linux ..
make -j"$(nproc)"
make install
```

Use the `Debug-linux` preset instead of `Release-linux` for a debug build.

### Windows

**Prerequisites**

- `git`
- `cmake` 3.21 or newer
- Visual Studio 2022 (the Community Edition is sufficient) with the **Desktop development with C++** workload, which provides the MSVC toolset and the `VsDevCmd.bat` developer environment
- Microsoft Office (optional) -- when detected, the Excel XLL COM add-in is built automatically

**One-step build (recommended)**

From a regular Command Prompt at the repository root:

```bat
.\build_windows.bat
```

The script invokes `VsDevCmd.bat` to set up the x64 MSVC environment, builds Machinist, runs code generation, configures with the `Release-windows` preset (Visual Studio generator), builds and installs with MSBuild, and runs `bin\test_suite.exe`.

> The script expects Visual Studio at `C:\Program Files\Microsoft Visual Studio\2022\Community`. If you have a different edition or install location, edit the `VsDevCmd.bat` path at the top of `build_windows.bat`, or run the manual steps below from a **Developer Command Prompt for VS 2022**.

**Manual build**

The presets for Windows use the **Ninja** generator. From a *Developer Command Prompt for VS 2022* (so `cl` and the SDK are on the path):

```bat
:: 1. Build the Machinist code generator
cd externals\machinist
call .\build_windows.bat
cd ..\..

:: 2. Generate sources
set MACHINIST_TEMPLATE_DIR=%CD%\externals\machinist\template\
externals\machinist\bin\Machinist.exe -c config/dal.ifc -l config/dal.mgl -d dal
externals\machinist\bin\Machinist.exe -c config/dal.ifc -l config/dal.mgl -d public

:: 3. Configure, build, and install
mkdir build & cd build
cmake --preset Release-windows ..
cmake --build .
cmake --install .
```

Use the `Debug-windows` preset for a debug build.

### Selecting an AAD backend

DAL ships with a native expression-template AAD engine (called **AADET**) that is used by default. You can optionally compile against one of three external AAD backends instead by passing exactly one of the following flags at configure time (enabling more than one is an error):

| Flag                       | Backend  |
|----------------------------|----------|
| *(none -- default)*        | AADET    |
| `-DDAL_USE_XAD_AAD=on`      | XAD      |
| `-DDAL_USE_CODIPACK_AAD=on` | CoDiPack |
| `-DDAL_USE_ADEPT_AAD=on`    | Adept    |

When using the convenience scripts, pass extra CMake flags through the `ADDITIONAL_CMAKE_FLAGS` environment variable, for example:

```bash
ADDITIONAL_CMAKE_FLAGS="-DDAL_USE_XAD_AAD=on" bash build_linux.sh
```

### Useful CMake options

These options can be appended to any `cmake` configure command (or set via `ADDITIONAL_CMAKE_FLAGS`):

| Option                    | Default        | Description                                            |
|---------------------------|----------------|--------------------------------------------------------|
| `CMAKE_BUILD_TYPE`        | `Release`      | Build type for single-config generators                |
| `SKIP_TESTS`              | `false` (presets) | Set to `true` to skip building the test suite       |
| `DAL_BUILD_EXAMPLES`      | `off` (presets) | Build the programs under `examples/`                  |
| `DAL_BUILD_PUBLIC`        | `off` (presets) | Build the public API (Excel XLL / Python bindings)    |
| `USE_COVERAGE`            | `false`        | Instrument the build for coverage reporting            |

> The CMake presets disable examples, the public API, and all external AAD backends by default. Override them explicitly when you need them, e.g. `cmake --preset=Release-linux -DDAL_BUILD_EXAMPLES=on ..`.

### Troubleshooting

- **CMake cannot find a submodule / missing `externals/...` directory:** run `git submodule update --init --recursive`.
- **`Machinist` not found or generated sources missing:** the code-generation stage did not run; build Machinist and run the two `Machinist` commands shown in the manual build steps before configuring CMake.
- **Windows: `cl` is not recognized:** run the build from a *Developer Command Prompt for VS 2022* (or via `build_windows.bat`, which sets up the environment for you).
- **Only one external AAD backend may be enabled:** you passed more than one `DAL_USE_*_AAD` flag; enable at most one.

## Python Bindings

Python bindings require an Anaconda Python distribution and SWIG. After a successful C++ build:

```bash
cd public/python
python setup.py wrap
python setup.py install
```

The `dal` package exposes the full public API to Python, including AAD-aware Monte Carlo pricing.

## Running Tests

```bash
# All tests
bin/test_suite

# A single suite
bin/test_suite --gtest_filter=CurveTest.*

# A single test
bin/test_suite --gtest_filter=CurveTest.TestDiscountPWLFConstruction
```

## Architecture

| Directory    | Purpose                                                                                            |
|--------------|----------------------------------------------------------------------------------------------------|
| `dal/`       | Core library: math, curve construction, models, scripting engine, AAD, concurrency                 |
| `public/`    | Public API wrapping the core library (Excel XLL, Python bindings)                                  |
| `tests/`     | Google Test suites, one subdirectory per module                                                    |
| `examples/`  | Standalone programs demonstrating AAD, Monte Carlo, PDE solvers, scripting, Sobol, and calibration |
| `config/`    | Machinist code-generation interface files                                                          |
| `externals/` | Git submodules: XAD, Adept, CoDiPack, Google Test, RapidJSON, Machinist                            |
| `miscs/`     | Excel workbooks and Python scripts showcasing exotic product pricing                               |

### Core Modules

| Module             | Description                                                                                                                  |
|--------------------|------------------------------------------------------------------------------------------------------------------------------|
| `dal/math/`        | Interpolation, optimization (underdetermined search), PDE solvers, random number generation, matrix operations, root finding |
| `dal/math/aad/`    | Automatic Adjoint Differentiation with native expression-template, XAD, Adept, and CoDiPack backends                         |
| `dal/curve/`       | Yield curve construction, piecewise forward-rate discount curves, multi-curve framework, calibration                         |
| `dal/script/`      | Expression scripting engine (parser, AST, simulation/evaluation) for exotic payoff definitions                               |
| `dal/model/`       | Financial models (Black-Scholes, etc.)                                                                                       |
| `dal/concurrency/` | Thread pool and concurrent queue for parallel Monte Carlo                                                                    |
| `dal/storage/`     | Data persistence and archiving                                                                                               |
| `dal/indice/`      | Reference rate index management                                                                                              |

## Excel Interface

The Excel XLL add-in exposes DAL functionality through worksheet functions.

### Linear Interpolation

Given a data table:

| x | y  |
|---|----|
| 1 | 10 |
| 3 | 8  |
| 5 | 6  |
| 7 | 4  |
| 9 | 2  |

Create an interpolator:

```
=INTERP1.NEW.LINEAR(E1, A2:A6, B2:B6)
```

This returns a string handle (e.g. `~Interp1~my.interp~2F18E558`) that can be used later:

```
=INTERP1.GET("~Interp1~my.interp~2F18E558", 6.5)   ' returns 4.5
```

### Scripted Exotic Option Pricing

Define a product with a schedule of events:

| Date      | Event                            |
|-----------|----------------------------------|
| 2022/9/25 | call pays MAX(spot() - 120, 0.0) |

Create the product:

```
=PRODUCT.NEW("my_product", A2, B2)
```

Define the model parameters:

| Field    | Value |
|----------|-------|
| spot     | 100   |
| vol      | 0.15  |
| rate     | 0.0   |
| dividend | 0.0   |

```
=BSMODELDATA.NEW("model", D2, D3, D4, D5)
```

Price it with Monte Carlo:

```
=MONTECARLO.VALUE(A5, C7, 2^20, "sobol", FALSE)
```

| Result | 4.0389 |
|--------|--------|

Additional examples:
- [Up-and-out call](miscs/excel/004.up%20and%20out%20call.xlsx)
- [Snowball](miscs/excel/005.snowball.xlsx)

## Python Interface

The same European option example in Python:

```python
from dal import *

today = Date_(2022, 9, 15)
EvaluationDate_Set(today)

spot = 100.0
vol = 0.15
rate = 0.0
div = 0.0
strike = 120.0
maturity = Date_(2025, 9, 15)

n_paths = 2 ** 20
rsg = "sobol"

event_dates = [maturity]
events = [f"call pays MAX(spot() - {strike}, 0.0)"]

product = Product_New(event_dates, events)
model = BSModelData_New(spot, vol, rate, div)

res = MonteCarlo_Value(product, model, n_paths, rsg, False, True)

for k, v in res.items():
    print(f"{k:<8}: {v:>10.4f}")
```

Output:

```
d_div   :   -85.2290
d_rate  :    73.1011
d_spot  :     0.2838
d_vol   :    58.7140
value   :     4.0389
```

Additional Python examples:
- [Up-and-out call](miscs/python/002.uoc.py)
- [Snowball](miscs/python/003.snowball.py)

## C++ Examples

Runnable examples are in the [`examples/`](examples/) directory:

| Example            | Description                                               |
|--------------------|-----------------------------------------------------------|
| `aad/`             | AAD in isolation (recording, propagation, multi-output)   |
| `european_mc/`     | European option pricing with Monte Carlo and AAD Greeks   |
| `european_fd/`     | European option pricing with finite difference PDE solver |
| `script/`          | Payoff scripting engine usage                             |
| `snowball/`        | Snowball autocallable pricing                             |
| `uoc/`             | Up-and-out call pricing                                   |
| `sobol/`           | Sobol sequence generation                                 |
| `underdetermined/` | Yield curve calibration with underdetermined search       |
| `concurrency/`     | Parallel Monte Carlo with thread pool                     |
| `vanilla/`         | Vanilla option pricing with multiple models               |

## License

This project is licensed under the MIT License -- see the [LICENSE](LICENSE) file for details.

## References

- Tom Hyer, *Derivatives Algorithms: Volume 1: Bones* ([repository](https://github.com/TomHyer/DA_Bones_Mirror))
- Antoine Savine, *Modern Computational Finance: AAD and Parallel Simulations* ([repository](https://github.com/asavine/CompFinance))
- Antoine Savine, *Modern Computational Finance: Scripting for Derivatives and xVA* ([repository](https://github.com/asavine/Scripting))
- Brian Huge and Jesper Andreasen, *Finite Difference Methods for Financial PDEs* ([repository](https://github.com/brnohu/CompFin))
- Brian Huge, *WBS_FD* ([repository](https://github.com/brnohu/WBS_FD))
