# dal-python

Python bindings for the Derivatives Algorithms Library (DAL) — a high-performance C++17 quantitative finance library with Automatic Adjoint Differentiation (AAD) support.

## Features

- **Black-Scholes and Dupire models** for equity derivatives pricing
- **Monte Carlo simulation** with pseudo-random and Sobol sequence generators
- **AAD Greeks** — compute pathwise sensitivities (delta, vega, rho, etc.) in a single simulation
- **Script engine** — define exotic payoffs using a domain-specific language
- **Curve calibration** — single-curve, multi-curve, staged XCCY, and joint domestic/foreign/basis calibration with resettable and MTM instruments plus AAD analytic Jacobians
- **Rate cashflow pricing** — typed planning, batch PV, and AAD node sensitivities for deposit, FRA, future, OIS, IRS, basis-swap, and cross-currency trades
- **Type-safe wrappers** for `Date_`, `Matrix_`, `Cell_`, and vector types

## Prerequisites

- **CPython 3.9-3.13** with development headers (`Requires-Python: >=3.9,<3.14`)
- **uv** — fast Python package manager ([install guide](https://docs.astral.sh/uv/getting-started/installation/))
- **pybind11 2.11.1** — installed automatically for isolated package builds;
  repository builds fall back to the pinned `dal-cpp/externals/pybind11`
  submodule, so run `git submodule update --init --recursive` on fresh clones
- **CMake 3.21+** and a C++17 compiler (GCC 13+, Clang 18+, or MSVC 2022)
- **DAL C++ staged install** — build core/public first; the canonical workflow is
  in the [installation guide](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/master/docs/installation.md#python-bindings)

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
uv venv --python ">=3.9,<3.14"
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

### Selecting a CPython Version

The local helpers accept an exact supported minor. On POSIX entry points use
`--python`; on PowerShell entry points use `-Python`:

```bash
# From the repository root; --python also enables the Python bindings.
bash ./build_linux.sh --python 3.9

# From dal-python/.
./build_sdist.sh --python 3.9
./build_wheel.sh --python 3.9
./run_tests.sh --python 3.9
```

```powershell
# From dal-python/.
.\build_wheel.ps1 -Python 3.9
.\run_tests.ps1 -Python 3.9
```

The accepted values are 3.9, 3.10, 3.11, 3.12, and 3.13. When the selector is
omitted, the helpers resolve a CPython in `>=3.9,<3.14`. A reused `.venv` must
already use the selected CPython minor; mismatches fail without replacing the
environment.

## Building Distribution Packages

For production deployment, you can build pre-compiled binary wheels or source distributions.
Official PyPI releases contain precompiled wheels only.

### Building a Binary Wheel

Binary wheels contain the compiled C++ extension and can be installed without requiring compilation:

```bash
DAL_INSTALL_PREFIX=/absolute/path/to/build/stage/Release-linux ./build_wheel.sh
DAL_INSTALL_PREFIX=/absolute/path/to/build/stage/Release-linux ./build_wheel.sh --python 3.9
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
./build_sdist.sh --python 3.9 # Select an exact supported CPython
./build_sdist.sh --clean # Clean build artifacts before building
```

The source archive is created under `dist/`.

Install from source (requires C++ build tools):
```bash
pip install dist/dal_python-2026.8.14.tar.gz \
  "--config-settings=cmake.define.DAL_INSTALL_PREFIX=/absolute/path/to/Derivatives-Algorithms-Lib/build/stage/<platform-preset>"
# or
uv pip install dist/dal_python-2026.8.14.tar.gz \
  "--config-settings=cmake.define.DAL_INSTALL_PREFIX=/absolute/path/to/Derivatives-Algorithms-Lib/build/stage/<platform-preset>"
```

**Requirements for building from source:**
- C++17 compiler (GCC 13+, Clang 18+, or MSVC 2022)
- CMake 3.21+
- pybind11 2.11.1 (declared as an isolated build requirement and installed
  automatically; repository builds may use the pinned vendored submodule)
- CPython 3.9-3.13 development headers
- DAL staged install containing the `dal-public`/`dal-cpp` CMake packages and
  platform libraries

## PyPI Binary Release

The repository release workflow builds and tests this wheel matrix:

| Operating system | Architecture | Wheel platform tag      | CPython versions |
|------------------|--------------|-------------------------|------------------|
| Linux            | x86-64       | `manylinux_2_28_x86_64` | 3.9-3.13         |
| Windows          | x86-64       | `win_amd64`             | 3.9-3.13         |

Every release artifact is a CPython-specific native wheel. Python/ABI tags run
from `cp39-cp39` through `cp313-cp313`; DAL does not publish `abi3` or universal
wheels. Path-matched pull requests build the floor and ceiling (`cp39` and
`cp313`) on both platforms, for four wheels; documentation-only component
changes remain on the lightweight documentation CI path. Manual and release-tag
runs build all five supported interpreters on both platforms, for ten wheels.
Every wheel runs the complete installed-wheel Python suite, and the two `cp39`
wheels also run a fresh, source-independent installed-wheel smoke test.

Linux filenames always include `manylinux_2_28_x86_64` and may also contain
unique compatible PEP 600 x86-64 components for glibc baselines no newer than
2.28. Mixed platform families, other architectures, raw Linux, legacy manylinux,
and musllinux tags are rejected. macOS, Linux ARM, PyPy, free-threaded CPython,
source distributions, and CPython 3.14 are not part of the current PyPI release
contract.

The SHA-256 release manifest records the exact verified bytes of every wheel,
and the publish job re-checks the downloaded artifacts against it before
upload. The toolchain is pinned (full action SHAs, exact build dependency
versions, named manylinux/runner images), but byte-for-byte reproducibility
across independent rebuilds is not currently enforced.

### One-time PyPI setup

Configure a Trusted Publisher on the existing `dal-python` PyPI project with:

| Field                | Value                              |
|----------------------|------------------------------------|
| PyPI project         | `dal-python`                       |
| GitHub owner         | `wegamekinglc`                     |
| GitHub repository    | `Derivatives-Algorithms-Lib`       |
| Workflow filename    | `dal-python-release.yml`           |
| GitHub environment   | `pypi`                             |

Create the matching `pypi` environment in the GitHub repository and require a
manual deployment approval if the repository plan supports it. The workflow uses
OIDC short-lived credentials; do not add a long-lived PyPI API token.

### Release procedure

1. Choose a new PEP 440 version that does not exist on PyPI. Update both
   `pyproject.toml` and `src/dal/__init__.py`.
2. Build and test the workspace with `bash ./build_linux.sh --full` from the
   repository root. Review and merge the version and release-note changes to
   `master` only after the exact PR head is green.
3. Run the `dal-python wheels and PyPI release` workflow manually from `master`.
   This is a build-only rehearsal. Confirm that ten wheels and the SHA-256
   release manifest are present.
4. Tag that reviewed `master` commit and push only the tag:

   ```bash
   git tag -a dal-python-v<version> -m "Release dal-python <version>"
   git push origin dal-python-v<version>
   ```

5. The tag run rebuilds and tests every wheel, validates the combined manifest,
   checks that the version is unused on PyPI, then publishes the exact artifacts
   from the build jobs through the `pypi` environment.
6. Verify the PyPI file list contains all ten wheels. In fresh Windows and Linux
   environments, install `dal-python==<version>`, import `dal`, and confirm
   `dal.__version__` equals `<version>`.

PyPI versions and files are immutable. Never use a skip-existing option to repair
an incomplete release; correct the issue, increment the version, and run the full
process again. Local `build_wheel.*` scripts are for diagnostics and private
deployment only; their output is not a PyPI release artifact.

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
precise_sobol = dal.SobolRSG_New(
    0, 3, precise=True, polish=True
)  # opt in to the precise-CDF Newton correction
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
- `dal.Product_Debug(product)` — Print the legacy human-readable product structure
- `dal.Product_DebugJson(product)` — Versioned JSON dump of the product AST (schema `dal.script-product/1`)
- `dal.Product_DebugTree(product, ascii=False, width=125)` — Width-aware Unicode (or ASCII) product tree

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
- `dal.SobolRSG_New(i_path, ndim=1, precise=False, polish=False)` — Sobol
  quasi-random generator; `polish` enables the Newton correction and `precise`
  selects its CDF, so the precise-CDF correction requires both flags to be `True`
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
- Curve construction plus single and staged multi-curve calibration
- Rate cashflow planning, pricing, and node sensitivities
- Staged XCCY basis calibration, sensitivity matrices, axes, and availability metadata
- Resettable/MTM XCCY construction with immutable fixing snapshots
- Joint domestic/foreign/basis XCCY calibration, including matrix and named-range contracts

## Project Structure

```
dal-python/
├── CMakeLists.txt          # Build configuration
├── pyproject.toml          # Python package metadata (scikit-build-core)
├── build_sdist.sh          # Source distribution helper
├── build_wheel.sh          # Wheel build helpers (POSIX and PowerShell)
├── build_wheel.ps1
├── run_tests.sh            # Standalone binding test helpers (POSIX and PowerShell)
├── run_tests.ps1
├── examples/               # Numbered end-to-end Python examples
├── scripts/                # Release verification and installed-wheel smoke helpers
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

The `curve` bindings (`dal-python/src/bindings/curve.cpp`) expose the supported Python
curve-construction and calibration workflows:

- **Instrument builders** — `Deposit_New`, `FRA_New`, `Future_New`, `Swap_New`, `OISSwap_New`, `BasisSwap_New`, `CrossCurrencySwap_New`
- **Curve factories** — `DiscountPWLF_New`, `DiscountZeroRate_New`
- **Calibration entry points** — `CalibrateSingleCurve`, `CalibrateMultiCurveBundle`, `CalibrateXccyMarket`, `CalibrateJointXccyMarket`
- **Enums** — `CurveParameterization` (`PIECEWISE_LINEAR_FWD`, `PIECEWISE_CONSTANT_FWD`, `ZERO_RATE`, `LOG_DISCOUNT`), `CurveSolveMode` (`EXACT`, `APPROXIMATE`), `CurveJacobianMode` (`ANALYTIC`, `BUMPED`), `LogDfScheme` (`LOG_LINEAR`, `LOG_CUBIC_NATURAL`, `MIXED`), `XccyNotionalMode` (`FIXED`, `RESETTABLE`, `MARK_TO_MARKET`)
- **Spec builders** — `CurveCalibrationSpecBuilder_`, `CrossCurrencyCalibrationSpecBuilder_`, and `JointXccyCalibrationSpecBuilder_`

The `dal.calibrate_curve(...)` helper in `api.py` wraps the common single-curve path with Python-friendly defaults. The underlying C++ methodology is documented in the [yield-curve guide](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/master/docs/methodology/yield_curve.md) and [Jacobian guide](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/master/docs/methodology/yield_curve_jacobian.md).

### Continuously Compounded Zero-Rate Curves

Build a persistent zero-rate curve directly with future-only nodes:

```python
today = dal.Date_(2026, 1, 2)
node_dates = [dal.Date_(2027, 1, 2), dal.Date_(2028, 1, 2)]

curve = dal.DiscountZeroRate_New(
    "usd_zero",
    "USD",
    today,
    node_dates,
    [0.02, 0.025],
    day_count=dal.DayBasis_("ACT_365F"),
    log_df_scheme=dal.LogDfScheme.LOG_LINEAR,
)
```

Each continuously compounded decimal rate $z_i$ is mapped to
`logDF_i = -z_i * YearFrac(today, node_date_i)`. The anchor log DF is fixed at zero
and has no zero-rate parameter. `LOG_LINEAR`, `LOG_CUBIC_NATURAL`, and `MIXED` all
interpolate the mapped log DFs. Before the anchor, `LOG_LINEAR` and `MIXED` clamp the
log DF to zero, while `LOG_CUBIC_NATURAL` extends its first cubic segment. Beyond the
last node, every scheme uses the last two mapped log-DF nodes as a secant. The returned
`DiscountZeroRate_` exposes read-only `anchor_date`, `node_dates`, `zero_rates`,
`day_count`, and `log_df_scheme` properties.

For calibration, select `CurveParameterization.ZERO_RATE` and supply strictly-future
knots. `initialGuess_` is a decimal continuously compounded zero rate copied to every
node. Both low-level `CalibrateSingleCurve` and the convenience helper use the analytic
AAD Jacobian when the normal single-discount-curve eligibility gates are met:

```python
result = dal.calibrate_curve(
    today,
    "USD",
    instruments,
    node_dates,
    settings={
        "parameterization": dal.CurveParameterization.ZERO_RATE,
        "log_df_scheme": dal.LogDfScheme.LOG_CUBIC_NATURAL,
        "initial_guess": 0.02,
    },
    jacobian_mode=dal.CurveJacobianMode.ANALYTIC,
    base_curve=base_curve,  # optional: zero rates are spread coordinates over this base
)
```

Python exposes single, staged multi-curve, staged XCCY basis, and simultaneous
joint XCCY calibration. A base curve is multiplied into the calibrated component;
it is not a replacement for the pricing discount curve required by a forward-curve stage.
Staged XCCY supports both the backward-compatible
`CalibrateXccyMarket(spec)` call and `CalibrateXccyMarket(spec, options)`.
`CrossCurrencyCalibrationOptions_` defaults to `ANALYTIC` with
`compute_forward_jacobian = True` and
`compute_eff_jacobian_inverse = True`; trailing-underscore property names are
available alongside the snake-case names.

The matrices remain on `result.diagnostics`. `diagnostics.jacobian` has
instrument rows and basis-parameter columns;
`diagnostics.eff_jacobian_inverse` has the reversed axes.
`instrument_names` follows input order and may contain duplicate labels.
`parameter_knot_dates` follows the spec's knot order and labels the
piecewise-constant basis curve's right-forward parameters. The diagnostics also
publish `residual_tolerance`, `jacobian_scaling == "unscaled"`,
`eff_jacobian_inverse_scaling == "solver_scaled"`, and independent
`jacobian_availability` / `eff_jacobian_inverse_availability` values:
`available`, `not_requested`, or `not_available_for_mode`.

For a raw decimal quote-bump vector `dq`, the solver-scaled effective inverse
`E` maps parameters as `dx = E * dq / residual_tolerance`. An unavailable
matrix is empty; inspect its availability property to distinguish an explicit
opt-out from a mode limitation.

### Resettable and Joint XCCY Calibration

Use `CrossCurrencySwapConfigBuilder_` to set the currency pair, notionals, leg
conventions, `notional_mode`, `fx_reset`, and explicit `domestic_rate_fixing` /
`foreign_rate_fixing` identities. `MarketFixingSnapshot_New` takes a nested
dictionary whose keys are index names and whose values map `DateTime_` objects to
observations. One immutable snapshot can hold domestic rate, foreign rate, and FX
fixings for an already-started swap:

```python
snapshot = dal.MarketFixingSnapshot_New({
    "USD-JOINT-3M": {historical_fixing: 0.040},
    "EUR-JOINT-3M": {historical_fixing: 0.030},
    "FX[EUR/USD]": {historical_fixing: 1.20},
})
```

`JointCurrencyCurveSpec_` holds the ordered domestic or foreign
`JointCurveDeclaration_` objects. `XccyBasisCurveDeclaration_` holds configured
XCCY instruments and basis knots. Assemble those groups with
`JointXccyCalibrationSpecBuilder_`, then call
`CalibrateJointXccyMarket(builder.build())`. The result exposes the domestic and
foreign curve blocks, `fx_forward_curve`, basis curve, retained snapshot, group
diagnostics, full market/model/residual vectors, analytic Jacobian, effective
inverse, and named `parameter_ranges` / `residual_ranges`. Pass
`JointXccyCalibrationOptions_` to select `ANALYTIC` or `BUMPED` and to disable
either diagnostic matrix. The `eff_jacobian_inverse` matrix has shape
`totalParameters x totalResiduals` and is the weighted inverse of the solver's
tolerance-scaled Jacobian. Transforming a raw decimal quote bump therefore
requires division by the spec's `tolerance_`; see the
[Jacobian methodology](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/master/docs/methodology/yield_curve_jacobian.md#joint-xccy-jacobian-layout).

The runnable [joint XCCY calibration example](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/master/dal-python/examples/007.xccy_joint_calibration.py)
uses an explicit fixing snapshot for a started MTM trade. It prints convergence,
the maximum absolute residual, Jacobian dimensions, named parameter and residual
half-open ranges, and every FX-forward date and value. With the `dal` package
installed in the active environment, run it from the repository root:

```bash
python dal-python/examples/007.xccy_joint_calibration.py
```

## Rate Cashflow Pricing and Node Risk

Typed rate trades price and produce AAD node sensitivities against a
component-keyed market. Every pricing and sensitivity function is keyword-only,
returns read-only results, and releases the GIL around native work. A complete
deposit example, mirroring the fixtures in `tests/test_curve_pricing.py`:

```python
import dal

today, maturity = dal.Date_(2026, 1, 15), dal.Date_(2027, 1, 15)

# 1) Curve and market: curves are registered by component key; trade terms
#    address them through their *_component_key fields.
curve = dal.DiscountPWC_New("usd", "USD", [maturity], [0.04])
market = dal.RatePricingMarket_(
    valuation_time=dal.DateTime_(today, 10, 30),
    result_currency="USD",
    curve_components={"discount": curve, "forecast": curve},
    fixings=dal.MarketFixingSnapshot_New({}),
)

# 2) Index convention, terms, and trade
index = dal.RateIndexConvention_New(
    dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_365F"), dal.CollateralType_OIS())
terms = dal.DepositTradeTerms_(
    notional=100.0, contract_rate=0.05, lend=True,
    index=index, discount_component_key="discount")
trade = dal.RateTradeDefinition_(
    instrument_id="deposit-1", instrument_type=dal.RateInstrumentType.DEPOSIT,
    trade_date=today, start_date=today, maturity_date=maturity,
    currency="USD", terms=terms)

# 3) Single-trade sensitivity (a deposit depends only on the discount component,
#    so a "forecast" request returns reason="TRADE_DOES_NOT_DEPEND_ON_COMPONENT")
r = dal.RateTradeNodeSensitivities(trade=trade, market=market, component_key="discount")
assert r.eligible and len(r.gradient) == 1 and r.reason == ""

# 4) Batch: component_keys must be a list — a tuple raises TypeError before any
#    native work starts. Deterministic trade-major then key order.
cells = dal.RateTradeNodeSensitivitiesBatch(
    trades=[trade], market=market, component_keys=["discount", "forecast"])
for c in cells:
    print(c.instrument_id, c.component_key, c.result.eligible, c.result.reason)

# 5) Portfolio aggregation
agg = dal.AggregateRatePortfolioNodeRisk(
    trades=[trade], market=market, component_keys=["discount"])
print(agg.policy)                    # UnconvertedByActualPvCcy
comp = agg.components[0]             # .component_key / .node_count / .node_dates /
                                     # .node_components / .values
print(dict(agg.pv_by_actual_pv_ccy)) # {'USD': ...}
print(agg.meta[0].reason, agg.meta[0].actual_pv_ccy)
```

Failures never raise. A trade that fails passive pricing — for example
`notional=float("nan")` — keeps `PriceRateTrades` field-level detail in
`result[0].error`, while every sensitivity call returns the canonical read-only
four-field result: `eligible=False`, `pv=0.0`, `gradient=[]`, and a stable
`reason` token (`"TRADE_VALIDATION_FAILED"` here). In a batch, failed entries
are isolated per (trade, component) cell; the remaining entries are unaffected.

All seven families — deposit, FRA, future, OIS, IRS, basis swap, and XCCY —
share this call pattern and differ only in their terms class. The per-family
terms fields, the addressable components, and the C++ and Excel equivalents are
in the [public API guide](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/master/docs/public-api.md#c-rate-cashflow-pricing).

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

MIT License. See the repository [LICENSE](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/master/LICENSE).

## Contributing

Follow the repository [contributor guide](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/master/CONTRIBUTING.md). Binding changes
should include Python tests and updates to the
[public API guide](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/master/docs/public-api.md) when the supported surface changes.

## See Also

- [DAL C++ Library](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib) — Workspace overview
- [Installation guide](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/master/docs/installation.md) — Canonical setup commands
- [Public API guide](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/master/docs/public-api.md) — C++, Python, and Excel entry points
- [pybind11 Documentation](https://pybind11.readthedocs.io/) — pybind11 binding syntax
