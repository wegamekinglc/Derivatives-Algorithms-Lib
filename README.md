# DAL -- Derivatives Algorithms Library

[![CMake Linux CI](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/workflows/cmake-linux.yml/badge.svg?branch=master)](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/workflows/cmake-linux.yml)
[![Codacy Grade](https://app.codacy.com/project/badge/Grade/9c84afd2bb534c6c87584e5d6e4cc420)](https://app.codacy.com/app/wegamekinglc/Derivatives-Algorithms-Lib)
[![Coverage Status](https://coveralls.io/repos/wegamekinglc/Derivatives-Algorithms-Lib/badge.svg?branch=master)](https://coveralls.io/github/wegamekinglc/Derivatives-Algorithms-Lib?branch=master)

## Overview

DAL is a C++17 quantitative finance library with built-in support for Automatic Adjoint Differentiation (AAD). It covers yield curve construction and calibration, Monte Carlo simulation, finite difference PDE solvers, a domain-specific scripting engine for exotic payoffs, and parallel model evaluation.

The project draws from the work of Tom Hyer (*Derivatives Algorithms: Bones*), Antoine Savine (*Modern Computational Finance: AAD and Parallel Simulations* and *Scripting for Derivatives and xVA*), and Brian Huge and Jesper Andreasen (*Finite Difference Methods for Financial PDEs*). Some implementation patterns trace back to those sources.

## Getting the Code

```bash
git clone git@github.com:wegamekinglc/Derivatives-Algorithms-Lib.git
cd Derivatives-Algorithms-Lib
git submodule update --init --recursive
```

## Building

### Linux

**Prerequisites:** git, cmake (3.16+), g++ (supporting C++17), zip

```bash
bash build_linux.sh
```

The script runs code generation (Machinist), a Release build, installs artifacts under the repo root, and executes the test suite.

Build artifacts:
- `lib/` -- static library
- `bin/` -- test suite and example binaries

For a manual build:

```bash
mkdir -p build && cd build
cmake --preset=Release-linux .. && make -j$(nproc) && make install
```

### Windows

**Prerequisites:** git, cmake, Visual Studio 2022 Community Edition

```bash
.\build_windows.bat
```

Build artifacts:
- `lib/` -- static library and XLL Excel extension
- `bin/` -- example binaries

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
