# dal-python

Python bindings for the Derivatives Algorithms Library (DAL) — a high-performance C++17 quantitative finance library with Automatic Adjoint Differentiation (AAD) support.

## Features

- **Black-Scholes and Dupire models** for equity derivatives pricing
- **Monte Carlo simulation** with pseudo-random and Sobol sequence generators
- **AAD Greeks** — compute pathwise sensitivities (delta, vega, rho, etc.) in a single simulation
- **Script engine** — define exotic payoffs using a domain-specific language
- **Type-safe wrappers** for `Date_`, `Matrix_`, `Cell_`, and vector types

## Prerequisites

- **Python 3.10+** with development headers
- **uv** — fast Python package manager ([install guide](https://docs.astral.sh/uv/getting-started/installation/))
- **pybind11** — fetched automatically via CMake FetchContent during build
- **CMake 3.21+** and a C++17 compiler (GCC 13+, Clang 18+, or MSVC 2022)
- **DAL C++ library** — must be built first (see [Building the C++ Library](#building-the-c-library))

### Building the C++ Library

The Python bindings depend on the compiled DAL C++ library. Build it first:

```bash
cd /path/to/Derivatives-Algorithms-Lib
./build_linux.sh
```

This produces:
- `lib/libdal_public.a` — public API library
- `lib/libdal_cpp.a` — core library
- `include/` — public headers

## Installation

### Development Install (Recommended)

Clone the repository and install in editable mode:

```bash
cd Derivatives-Algorithms-Lib/dal-python

# Create a virtual environment with uv
uv venv
source .venv/bin/activate  # On Windows: .venv\Scripts\activate

# Install dependencies and build the extension
uv pip install -e ".[test]" --no-build-isolation \
    --config-settings=cmake.define.DAL_DIR=/path/to/Derivatives-Algorithms-Lib
```

Replace `/path/to/Derivatives-Algorithms-Lib` with the absolute path to the repository root.

### Quick Start Script

Use the provided test runner to automatically set up the environment and verify the installation:

```bash
bash run_tests.sh
```

This script:
1. Creates a uv virtual environment
2. Installs all dependencies (scikit-build-core, pytest, numpy)
3. Builds the Python extension in development mode
4. Runs the full test suite (112 tests)

## Building Distribution Packages

For production deployment, you can build pre-compiled binary wheels or source distributions.

### Building a Binary Wheel

Binary wheels contain the compiled C++ extension and can be installed without requiring compilation:

```bash
./build_wheel.sh              # Build wheel for current platform
./build_wheel.sh --manylinux  # Build manylinux-compatible wheel (Linux only)
./build_wheel.sh --clean      # Clean build artifacts before building
```

The wheel will be created in `dist/` directory:
- `dal_python-2025.12.7-cp313-cp313-linux_x86_64.whl` (2.7 MB)

Install the wheel:
```bash
pip install dist/dal_python-2025.12.7-cp313-cp313-linux_x86_64.whl
# or
uv pip install dist/dal_python-2025.12.7-cp313-cp313-linux_x86_64.whl
```

**Note:** Binary wheels are platform-specific. A wheel built on Linux x86_64 will only work on similar systems.

### Building a Source Distribution

Source distributions allow users to build from source on any platform:

```bash
./build_sdist.sh         # Build source distribution
./build_sdist.sh --clean # Clean build artifacts before building
```

The source distribution will be created in `dist/` directory:
- `dal_python-2025.12.7.tar.gz` (20 KB)

Install from source (requires C++ build tools):
```bash
pip install dist/dal_python-2025.12.7.tar.gz \
  --config-settings=cmake.define.DAL_DIR=/path/to/Derivatives-Algorithms-Lib
# or
uv pip install dist/dal_python-2025.12.7.tar.gz \
  --config-settings=cmake.define.DAL_DIR=/path/to/Derivatives-Algorithms-Lib
```

**Requirements for building from source:**
- C++17 compiler (GCC 13+, Clang 18+, or MSVC 2022)
- CMake 3.21+
- pybind11 (fetched via CMake FetchContent)
- Python 3.10+ development headers
- DAL C++ library (libdal_public.a and libdal_cpp.a)

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

**Note:** The current pybind11 binding only supports read access to matrix elements via `matrix(i, j)`. Use the constructor's fill value parameter to initialize all elements.

## API Reference

### Core Types

- `dal.Date_(year, month, day)` — Date object with arithmetic operations
- `dal.String_(value)` — String wrapper
- `dal.Cell_(value)` — Polymorphic value container (bool, double, Date, String)
- `dal.DoubleVector()` — Vector of doubles
- `dal.DoubleMatrix_(rows, cols)` — 2D matrix of doubles

### Models

- `dal.BSModelData_New(spot, vol, rate, div)` — Black-Scholes model
- `dal.DupireModelData_New(spot, rate, repo, spots, times, vols)` — Dupire local vol model

### Products

- `dal.Product_New(dates, events)` — Create a script product from event dates and payoff definitions
- `dal.Product_Debug(product)` — Print human-readable product structure

### Valuation

- `dal.MonteCarlo_Value(product, model, num_paths, method="sobol", use_bb=False, enable_aad=False, smooth=0.01)` — Monte Carlo pricing with optional AAD Greeks

**Parameters:**
- `product` — Script product (from `Product_New`)
- `model` — Model data (from `BSModelData_New` or `DupireModelData_New`)
- `num_paths` — Number of simulation paths (use powers of 2 for Sobol)
- `method` — Random generator: `"sobol"` (default) or `"mrg32"`
- `use_bb` — Use Brownian bridge construction (default `False`)
- `enable_aad` — Enable AAD for pathwise Greeks (default `False`)
- `smooth` — Fuzzy logic smoothing parameter for discontinuous payoffs (default `0.01`)

**Returns:** Dictionary with keys:
- `"PV"` — Present value
- `"d_spot"`, `"d_vol"`, `"d_rate"`, `"d_div"`, `"d_STRIKE"` — AAD Greeks (only if `enable_aad=True`)

### Random Generators

- `dal.PseudoRSG_New(seed, ndim=1)` — Pseudo-random generator (MRG32k32a)
- `dal.SobolRSG_New(i_path, ndim=1)` — Sobol quasi-random generator
- `dal.PseudoRSG_Get_Uniform(rsg, num_paths)` — Uniform samples [0, 1]
- `dal.PseudoRSG_Get_Normal(rsg, num_paths)` — Standard normal samples
- `dal.SobolRSG_Get_Uniform(rsg, num_paths)` — Sobol uniform samples
- `dal.SobolRSG_Get_Normal(rsg, num_paths)` — Sobol normal samples

### Global State

- `dal.EvaluationDate_Set(date)` — Set global evaluation date
- `dal.EvaluationDate_Get()` — Get global evaluation date

## Testing

Run the full test suite:

```bash
bash run_tests.sh
```

Run specific tests:

```bash
bash run_tests.sh -k "test_date"  # Run date-related tests
bash run_tests.sh -v              # Verbose output
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
├── run_tests.sh            # Development workflow script
├── src/
│   ├── bindings/
│   │   ├── module.cpp        # pybind11 module definition
│   │   ├── bindings.h        # shared binding helpers
│   │   ├── core.cpp          # core types (Date_, String_, Vector_, Matrix_, Handle_)
│   │   ├── global.cpp         # global functions (BlackScholes, Interp1, Solutions)
│   │   ├── models.cpp         # model types (BSModelData_, etc.)
│   │   ├── random.cpp          # random number generators
│   │   ├── script.cpp          # scripting engine bindings
│   │   └── value.cpp           # value type conversions
│   └── dal/
│       ├── __init__.py     # Package initialization
│       └── api.py          # High-level Python API wrappers
├── tests/
│   ├── conftest.py        # Pytest fixtures
│   └── test_*.py          # Test modules
```

## Architecture

The Python bindings are generated by pybind11 from domain-organized binding files. The build process:

1. **CMake** configures the build, locates DAL C++ libraries, and fetches pybind11
2. **C++ compiler** builds `_dal.cpython-*.so` extension module from the domain-organized `src/bindings/*.cpp` files
3. **scikit-build-core** packages everything into an installable wheel

The hand-written Python code in `src/dal/` provides:
- `__init__.py` — Re-exports all pybind11-generated symbols
- `api.py` — Convenience wrappers (e.g., `Product_New` with automatic type conversion)

## Troubleshooting

### "Cannot find DAL::public" during build

Ensure `DAL_DIR` points to the correct DAL installation:

```bash
ls $DAL_DIR/lib/libdal_public.a  # Should exist
ls $DAL_DIR/include/dal          # Should exist
```

### "ImportError: No module named _dal"

The extension module failed to build. Check the build logs:

```bash
# Rebuild with verbose output
rm -rf build/ *.egg-info
uv pip install -e . --no-build-isolation \
    --config-settings=cmake.define.DAL_DIR=/path/to/dal \
    -v
```

### Tests fail with "ModuleNotFoundError"

Ensure you're using the virtual environment:

```bash
source .venv/bin/activate
python -c "import dal; print(dal.__version__)"
```

## License

BSD 3-Clause License. See the main DAL repository for details.

## Contributing

1. Run tests before submitting changes: `bash run_tests.sh`
2. Follow the existing code style in `src/dal/` and `tests/`
3. Add tests for new functionality in `tests/`
4. Update this README if you change the public API

## See Also

- [DAL C++ Library](../README.md) — Core library documentation
- [CLAUDE.md](../CLAUDE.md) — AI-assisted development context (coding conventions, build commands, test workflow for Claude Code)
- [pybind11 Documentation](https://pybind11.readthedocs.io/) — pybind11 binding syntax
