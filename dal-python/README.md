# dal-python

Python bindings for the Derivatives Algorithms Library (DAL) — a high-performance C++17 quantitative finance library with Automatic Adjoint Differentiation (AAD) support.

## Features

- **Black-Scholes and Dupire models** for equity derivatives pricing
- **Monte Carlo simulation** with pseudo-random and Sobol sequence generators
- **AAD Greeks** — compute pathwise sensitivities (delta, vega, rho, etc.) in a single simulation
- **Script engine** — define exotic payoffs using a domain-specific language
- **Curve calibration** — single-curve, multi-curve, and cross-currency calibration with DF-node / log-discount curves and AAD analytic Jacobians
- **Type-safe wrappers** for `Date_`, `Matrix_`, `Cell_`, and vector types

## Prerequisites

- **Python 3.10+** with development headers
- **uv** — fast Python package manager ([install guide](https://docs.astral.sh/uv/getting-started/installation/))
- **pybind11 2.11.1** — installed automatically for isolated package builds;
  repository builds fall back to the pinned `dal-cpp/externals/pybind11`
  submodule, so run `git submodule update --init --recursive` on fresh clones
- **CMake 3.21+** and a C++17 compiler (GCC 13+, Clang 18+, or MSVC 2022)
- **DAL C++ staged install** — build core/public first; the canonical workflow is
  in [docs/installation.md](../docs/installation.md#python-bindings)

### Building the C++ Library

The Python bindings depend on a compiled DAL C++ staging prefix. Build it first:

```bash
cd /path/to/Derivatives-Algorithms-Lib
./build_linux.sh
```

This produces `build/stage/Release-linux/`, containing the installed core/public
libraries, headers, and CMake package metadata.

## Installation

### Development Install (Recommended)

Clone the repository and install in editable mode:

```bash
cd Derivatives-Algorithms-Lib/dal-python

# Create a virtual environment with uv
uv venv
source .venv/bin/activate  # On Windows: .venv\Scripts\activate

# Install dependencies and build the extension
uv pip install -e ".[test]" "--config-settings=cmake.define.DAL_INSTALL_PREFIX=/absolute/path/to/Derivatives-Algorithms-Lib/build/stage/<platform-preset>"
```

Use an absolute staged-prefix path and replace `<platform-preset>` with the
preset that built DAL, such as `Release-linux` or `Release-windows`. Standalone
`dal-python` reads the installed CMake packages and automatically applies their
configuration-aware MSVC runtime contract to `_dal`.

### Workspace Build and Test

To provision Python test dependencies and run the bindings through the workspace
CTest integration:

```bash
bash ../build_linux.sh --full
```

The workspace script creates or reuses `dal-python/.venv`, builds the extension,
and runs the configured C++/public/Python tests.

## Building Distribution Packages

For production deployment, you can build pre-compiled binary wheels or source distributions.

### Building a Binary Wheel

Binary wheels contain the compiled C++ extension and can be installed without requiring compilation:

```bash
DAL_INSTALL_PREFIX=/absolute/path/to/build/stage/Release-linux ./build_wheel.sh
DAL_INSTALL_PREFIX=/absolute/path/to/build/stage/Release-linux ./build_wheel.sh --clean
```

The platform- and interpreter-tagged wheel is created under `dist/`.

Install the wheel:
```bash
uv pip install dist/dal_python-*.whl
```

**Note:** Binary wheels are platform-specific. DAL keeps native-CPU tuning off by
default so distributable builds use the compiler's portable baseline. Do not set
`DAL_ENABLE_NATIVE_ARCH=ON` for a wheel that must run on unknown machines.

### Building a Source Distribution

Source distributions allow users to build from source on any platform:

```bash
./build_sdist.sh         # Build source distribution
./build_sdist.sh --clean # Clean build artifacts before building
```

The source archive is created under `dist/`.

Install from source (requires C++ build tools):
```bash
pip install dist/dal_python-2025.12.7.tar.gz \
  "--config-settings=cmake.define.DAL_INSTALL_PREFIX=/absolute/path/to/Derivatives-Algorithms-Lib/build/stage/<platform-preset>"
# or
uv pip install dist/dal_python-2025.12.7.tar.gz \
  "--config-settings=cmake.define.DAL_INSTALL_PREFIX=/absolute/path/to/Derivatives-Algorithms-Lib/build/stage/<platform-preset>"
```

**Requirements for building from source:**
- C++17 compiler (GCC 13+, Clang 18+, or MSVC 2022)
- CMake 3.21+
- pybind11 2.11.1 (declared as an isolated build requirement and installed
  automatically; repository builds may use the pinned vendored submodule)
- Python 3.10+ development headers
- DAL staged install containing the `dal-public`/`dal-cpp` CMake packages and
  platform libraries

## Usage

### Basic Pricing Example

```python
import dal

# Set evaluation date
dal.EvaluationDate_Set(dal.Date_(2022, 9, 25))

# Define model parameters
spot, vol, rate, div = 100.0, 0.2, 0.05, 0.02
model = dal.BSModelData_New(spot=spot, vol=vol, rate=rate, div=div)

# Define a European call option
strike = 100.0
maturity = dal.Date_(2023, 9, 25)
product = dal.Product_New(
    ["STRIKE", dal.Cell_(maturity)],
    [str(strike), "call pays MAX(spot() - STRIKE, 0.0)"]
)

# Price using Monte Carlo (65,536 paths, Sobol sequences)
result = dal.MonteCarlo_Value(product, model, 2**16, "sobol")
print(f"Call PV: {result['PV']:.4f}")
# Output: Call PV: 9.2259
```

### Computing AAD Greeks

Enable AAD to compute pathwise sensitivities in a single simulation:

```python
result = dal.MonteCarlo_Value(
    product, model,
    2**14,        # num_paths
    "sobol",      # method
    False,        # use_bb
    True          # enable_aad
)

print(f"PV: {result['PV']:.6f}")
for key in sorted(result.keys()):
    if key.startswith('d_'):
        print(f"  {key}: {result[key]:.6f}")
```

Output:
```
PV: 9.223019
  d_STRIKE: -0.494542
  d_div: -58.677195
  d_rate: 49.454176
  d_spot: 0.586772
  d_vol: 37.873346
```

### Working with Dates

```python
import dal

# Create dates
d = dal.Date_(2022, 9, 25)
print(d)  # 2022-09-25

# Date arithmetic
d2 = d.AddDays(30)
print(f"Year: {dal.Year(d)}, Month: {dal.Month(d)}, Day: {dal.Day(d)}")

# Date comparisons
d3 = dal.Date_(2022, 10, 25)
print(d < d3)  # True
```

### Random Number Generation

```python
# Pseudo-random generator (MRG32k32a algorithm)
pseudo = dal.PseudoRSG_New(42, 3)  # seed=42, ndim=3
uniform_samples = dal.PseudoRSG_Get_Uniform(pseudo, 1000)  # Returns DoubleMatrix_
normal_samples = dal.PseudoRSG_Get_Normal(pseudo, 1000)

# Sobol quasi-random sequences (better convergence for MC)
sobol = dal.SobolRSG_New(0, 3)  # i_path=0, ndim=3
sobol_samples = dal.SobolRSG_Get_Uniform(sobol, 1000)
precise_sobol = dal.SobolRSG_New(0, 3, precise=True)  # opt in to precise normal draws
```

### Dupire Local Volatility Model

```python
# Define a local volatility surface with flat 20% vol
spots = [80.0, 90.0, 100.0, 110.0, 120.0]
times = [0.5, 1.0, 2.0]
vols = dal.DoubleMatrix_(len(spots), len(times), 0.2)  # Fill with 20% vol

dupire_model = dal.DupireModelData_New(
    spot=100.0,
    rate=0.05,
    repo=0.01,
    spots=spots,
    times=times,
    vols=vols
)
```

`DoubleMatrix_` also accepts rectangular nested sequences and supports mutable
`matrix[i, j]` access, so non-flat surfaces can be populated directly.

## API Reference

### Core Types

- `dal.Date_(year, month, day)` — Date object with arithmetic operations
- `dal.String_(value)` — String wrapper
- `dal.Cell_(value)` — Polymorphic value container (bool, double, Date, String)
- `dal.DoubleVector()` — Vector of doubles
- `dal.DoubleMatrix_(rows, cols, fill=0.0)` or `dal.DoubleMatrix_(nested_rows)` — mutable 2D matrix of doubles

### Models

- `dal.BSModelData_New(spot, vol, rate, div)` — Black-Scholes model
- `dal.DupireModelData_New(spot, rate, repo, spots, times, vols)` — Dupire local vol model

### Products

- `dal.Product_New(dates, events)` — Create a script product from event dates and payoff definitions
- `dal.Product_Debug(product)` — Print human-readable product structure

### Valuation

- `dal.MonteCarlo_Value(product, modelData, num_path, method="sobol", use_bb=False, enable_aad=False, smooth=0.01, compiled=None)` — Monte Carlo pricing with optional AAD Greeks

**Parameters:**
- `product` — Script product (from `Product_New`)
- `modelData` — Model data (from `BSModelData_New` or `DupireModelData_New`)
- `num_path` — Positive number of simulation paths (powers of 2 are customary for Sobol)
- `method` — Random generator: `"sobol"` (default) or `"mrg32"`
- `use_bb` — Use Brownian bridge construction (default `False`)
- `enable_aad` — Enable AAD for pathwise Greeks (default `False`)
- `smooth` — Fuzzy logic smoothing parameter for discontinuous payoffs (default `0.01`)
- `compiled` — `True` selects the compiled evaluator; `None`/`False` uses tree-walk

**Returns:** Dictionary with keys:
- `"PV"` — Present value
- `"d_spot"`, `"d_vol"`, `"d_rate"`, `"d_div"`, `"d_STRIKE"` — AAD Greeks (only if `enable_aad=True`)

### Random Generators

- `dal.PseudoRSG_New(seed, ndim=1)` — Pseudo-random generator (MRG32k32a)
- `dal.SobolRSG_New(i_path, ndim=1, precise=False, polish=False)` — Sobol quasi-random generator; set both flags to `True` for precise-CDF-polished normal draws
- `dal.PseudoRSG_Get_Uniform(rsg, num_paths)` — Uniform samples [0, 1]
- `dal.PseudoRSG_Get_Normal(rsg, num_paths)` — Standard normal samples
- `dal.SobolRSG_Get_Uniform(rsg, num_paths)` — Sobol uniform samples
- `dal.SobolRSG_Get_Normal(rsg, num_paths)` — Sobol normal samples

### Global State

- `dal.EvaluationDate_Set(date)` — Set the process-wide evaluation date; waits
  for an in-progress native valuation or scoped override
- `dal.EvaluationDate_Get()` — Read the stable process-wide evaluation date;
  remains available while valuation runs

Both bindings release the GIL before entering native synchronization.

## Testing

Build and run the full workspace suite:

```bash
bash ../build_linux.sh --full
```

After an editable install, run focused Python tests directly:

```bash
python -m pytest tests -k "test_date" -v
```

Tests are located in `tests/` and cover:
- Date arithmetic and comparisons
- Vector and matrix operations
- Model construction (BS, Dupire)
- Monte Carlo pricing accuracy vs Black-Scholes analytical formulas
- AAD Greek computation and validation
- Random number generator properties

## Project Structure

```
dal-python/
├── CMakeLists.txt          # Build configuration
├── pyproject.toml          # Python package metadata (scikit-build-core)
├── run_tests.sh            # Standalone binding test helper
├── src/
│   ├── bindings/
│   │   ├── module.cpp        # pybind11 module definition
│   │   ├── bindings.h        # shared binding helpers
│   │   ├── core.cpp          # core types (Date_, String_, Cell_, vectors, DoubleMatrix_)
│   │   ├── global.cpp         # Handle_<T> opaque types, EvaluationDate_Get/Set
│   │   ├── models.cpp         # model types (BSModelData_, etc.)
│   │   ├── random.cpp          # random number generators
│   │   ├── script.cpp          # scripting engine bindings
│   │   ├── calendar.cpp        # holiday calendars and business-day conventions
│   │   ├── curve.cpp           # curve calibration, instruments, and interpolation
│   │   └── value.cpp           # Monte Carlo valuation (MonteCarlo_Value)
│   └── dal/
│       ├── __init__.py     # Package initialization
│       └── api.py          # High-level Python API wrappers
├── tests/
│   ├── conftest.py        # Pytest fixtures
│   └── test_*.py          # Test modules
```

## Architecture

The Python bindings are generated by pybind11 from domain-organized binding files. The build process:

1. **CMake** configures the build and locates the DAL C++ libraries plus either
   the isolated pybind11 build requirement or the pinned repository fallback
2. **C++ compiler** builds `_dal.cpython-*.so` extension module from the domain-organized `src/bindings/*.cpp` files
3. **scikit-build-core** packages everything into an installable wheel

When consuming an installed DAL package under MSVC, CMake applies the package's
`DAL_CPP_MSVC_RUNTIME_LIBRARY` value to `_dal` through
`dal_cpp_apply_msvc_runtime`. The helper is a no-op on other toolchains.

The hand-written Python code in `src/dal/` provides:
- `__init__.py` — Re-exports all pybind11-generated symbols
- `api.py` — Convenience wrappers (e.g., `Product_New` with automatic type conversion, `calibrate_curve(...)` for curve calibration)

## Curve Calibration

The `curve` bindings (`dal-python/src/bindings/curve.cpp`) expose the full curve-calibration surface:

- **Instrument builders** — `Deposit_New`, `FRA_New`, `Future_New`, `Swap_New`, `OISSwap_New`, `BasisSwap_New`, `CrossCurrencySwap_New`
- **Curve factories** — `DiscountPWLF_New`, `NewDiscountLogDF`
- **Calibration entry points** — `CalibrateSingleCurve`, `CalibrateMultiCurveBundle`, `CalibrateXccyMarket`
- **Enums** — `CurveParameterization` (`PIECEWISE_LINEAR_FWD`, `PIECEWISE_CONSTANT_FWD`, `ZERO_RATE`, `LOG_DISCOUNT`), `CurveSolveMode` (`EXACT`, `APPROXIMATE`), `CurveJacobianMode` (`ANALYTIC`, `BUMPED`), `LogDfScheme` (`LOG_LINEAR`, `LOG_CUBIC_NATURAL`, `MIXED`)
- **Spec builder** — `CurveCalibrationSpecBuilder_` for assembling `CurveCalibrationSpec_` / `MultiCurveCalibrationSpec_`

The `dal.calibrate_curve(...)` helper in `api.py` wraps the common single-curve path with Python-friendly defaults. The underlying C++ methodology is documented in [docs/methodology/yield_curve.md](../docs/methodology/yield_curve.md) and [docs/methodology/yield_curve_jacobian.md](../docs/methodology/yield_curve_jacobian.md).

## Troubleshooting

### "Cannot find DAL::public" during build

Ensure `DAL_INSTALL_PREFIX` points to the correct staged DAL installation:

```text
<stage>/lib/cmake/dal-public/dal-publicConfig.cmake
<stage>/lib/cmake/dal-cpp/dal-cppConfig.cmake
<stage>/include/dal/
```

The library files beside the package metadata use the platform's native suffix,
such as `.a` on Linux or `.lib` on Windows; do not diagnose the prefix by
assuming one suffix.

### "ImportError: No module named _dal"

The extension module failed to build. Check the build logs:

```bash
uv pip install --reinstall -e . -v "--config-settings=cmake.define.DAL_INSTALL_PREFIX=/absolute/path/to/build/stage/<platform-preset>"
```

Replace `<platform-preset>` with the stage produced by the active compiler and
configuration.

### Tests fail with "ModuleNotFoundError"

Ensure you're using the virtual environment:

```bash
uv run --no-sync python -c "import dal; print(dal.__version__)"
```

## License

MIT License. See the repository [LICENSE](../LICENSE).

## Contributing

Follow the repository [contributor guide](../CONTRIBUTING.md). Binding changes
should include Python tests and updates to the
[public API guide](../docs/public-api.md) when the supported surface changes.

## See Also

- [DAL C++ Library](../README.md) — Workspace overview
- [Installation guide](../docs/installation.md) — Canonical setup commands
- [Public API guide](../docs/public-api.md) — C++, Python, and Excel entry points
- [pybind11 Documentation](https://pybind11.readthedocs.io/) — pybind11 binding syntax
