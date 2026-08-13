# DAL - Derivatives Algorithms Library

[![CMake Linux CI](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/workflows/cmake-linux.yml/badge.svg?branch=master)](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/workflows/cmake-linux.yml)
[![CMake Windows CI](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/workflows/cmake-windows.yml/badge.svg?branch=master)](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/workflows/cmake-windows.yml)
[![Codacy Grade](https://app.codacy.com/project/badge/Grade/9c84afd2bb534c6c87584e5d6e4cc420)](https://app.codacy.com/app/wegamekinglc/Derivatives-Algorithms-Lib)
[![Coverage Status](https://coveralls.io/repos/github/wegamekinglc/Derivatives-Algorithms-Lib/badge.svg?branch=master)](https://coveralls.io/github/wegamekinglc/Derivatives-Algorithms-Lib?branch=master)

A C++17 quantitative finance library with built-in Automatic Adjoint Differentiation (AAD). Features include yield curve construction, cross-currency pricing and calibration, Monte Carlo simulation, finite difference PDE solvers, a scripting engine for exotic payoffs with tree-walk and compiled evaluators, and parallel model evaluation.

## CI

Every push and pull request builds and tests this compiler × AAD-backend
matrix. GitHub publishes one status badge per workflow; open a workflow run
for per-job results.

| Platform                   | Compiler | AADet | XAD | CoDiPack | Adept |
|----------------------------|----------|-------|-----|----------|-------|
| Ubuntu (`ubuntu-latest`)   | GCC 13   | ✓     | ✓   | ✓        | ✓     |
| Ubuntu (`ubuntu-latest`)   | GCC 14   | ✓     | ✓   | ✓        | ✓     |
| Ubuntu (`ubuntu-latest`)   | GCC 15   | ✓     | ✓   | ✓        | ✓     |
| Ubuntu (`ubuntu-latest`)   | Clang 18 | ✓     | ✓   | ✓        | ✓     |
| Ubuntu (`ubuntu-latest`)   | Clang 19 | ✓     | ✓   | ✓        | ✓     |
| Ubuntu (`ubuntu-latest`)   | Clang 20 | ✓     | ✓   | ✓        | ✓     |
| Windows (`windows-latest`) | MSVC     | ✓     | ✓   | —        | ✓     |

- GCC 13/14 legs additionally run gcov coverage; Coveralls tracks GCC 14 + AADet.
- Windows legs additionally build the `dal-python` bindings and the `dal-excel` add-in.
- Separate Linux jobs cover CoDiPack thread isolation, a warning-clean build,
  ASan/UBSan/TSan spot tests, Python bindings, and benchmark regression gating.

## Quick Start

```bash
git clone --recursive git@github.com:wegamekinglc/Derivatives-Algorithms-Lib.git
cd Derivatives-Algorithms-Lib
bash build_linux.sh
```

The Linux default builds/tests core and public C++ and stages the install under
`build/stage/Release-linux`; use `--full` for Python plus benchmarks. For the
supported profiles, Windows workflow, Python bindings, and troubleshooting,
see the **[installation guide](docs/installation.md)**.

## Architecture

```text
dal-cpp (DAL::cpp)
  └─ dal-public (DAL::public)
       ├─ dal-python
       └─ dal-excel
```

The native dependency graph is `dal-cpp ← dal-public ← {dal-python, dal-excel}`.
`dal-public` is a developer-facing convenience facade over core DAL types; it is
not an ABI-isolated compatibility boundary.

| Sub-project   | Purpose                                                                                |
|---------------|----------------------------------------------------------------------------------------|
| `dal-cpp/`    | Core library: math, curves, models, scripting, AAD                                     |
| `dal-public/` | Public C++ convenience facade over `DAL::cpp`                                          |
| `dal-python/` | pybind11 Python bindings                                                               |
| `dal-excel/`  | Excel `.xll` add-in (Windows-only)                                                     |

Core modules in `dal-cpp/dal/`:

- **math/** — Interpolation, optimization, PDE solvers, random numbers, matrix ops
- **math/aad/** — Automatic Adjoint Differentiation (native, XAD, Adept, CoDiPack backends)
- **curve/** — Yield curve construction, piecewise forward rates, calibration
- **script/** — Expression scripting engine for exotic payoffs, with tree-walk and compiled evaluation modes
- **model/** — Financial models (Black-Scholes, etc.)
- **concurrency/** — Thread pool for parallel Monte Carlo

## Examples

### Python

```python
from dal import *

today = Date_(2022, 9, 15)
EvaluationDate_Set(today)

spot, vol, rate, div = 100.0, 0.15, 0.0, 0.0
strike = 120.0
maturity = Date_(2025, 9, 15)

events = [f"call pays MAX(spot() - {strike}, 0.0)"]
product = Product_New([maturity], events)
model = BSModelData_New(spot, vol, rate, div)

res = MonteCarlo_Value(
    product,
    model,
    2**20,
    method="sobol",
    enable_aad=True,
    compiled=True,
)
for k, v in res.items():
    print(f"{k:<8}: {v:>10.4f}")
```

Output:
```
PV      :     4.0389
d_div   :   -85.2290
d_rate  :    73.1011
d_spot  :     0.2838
d_vol   :    58.7140
```

More examples: [Python](dal-python/examples/), [Excel](dal-excel/examples/), [C++](dal-cpp/examples/). The C++ Monte Carlo script examples show both tree-walk and compiled evaluator output where applicable.

Cross-currency examples:

- [reset-aware pricing](dal-cpp/examples/xccy_reset_pricing/)
- [staged basis calibration](dal-cpp/examples/xccy_curve_calibration/)
- [joint domestic/foreign/basis calibration](dal-cpp/examples/xccy_mtm_calibration/)
- [Python joint calibration](dal-python/examples/007.xccy_joint_calibration.py)

### Script Engine Modes

Monte Carlo script valuation defaults to the tree-walk evaluator (`compiled=false`).
Pass `compiled=True` in Python or `compiled=true` in C++ to select the flat-stream
evaluator. The compiled mode is a performance option; payoff values and AAD risks
are expected to match tree-walk results up to normal floating-point noise.

For implementation details and parity coverage, see [Script Engine methodology](docs/methodology/script_engine.md#tree-walk-and-compiled-evaluation). To compare runtime locally, build and run the `script_mc_perf` benchmark target:

```bash
bash ./build_linux.sh --benchmarks
./build/Release-linux/dal-cpp/benchmarks/script_mc_perf/script_mc_perf
```

### Excel

```
=PRODUCT.NEW("my_product", A2, B2)
=BSMODELDATA.NEW("model", 100, 0.15, 0.0, 0.0)
=MONTECARLO.VALUE(A5, C7, 2^20, "sobol", FALSE, TRUE, 0.01)
```

## Web UI

The portfolio management web UI moved to its own repository:
[wegamekinglc/dal-web](https://github.com/wegamekinglc/dal-web).

## Documentation

- **[Installation Guide](docs/installation.md)** — Canonical setup workflows
- **[Architecture Guide](docs/architecture.md)** — Components, ownership, and execution flows
- **[Public API Guide](docs/public-api.md)** — C++, Python, and Excel entry points
- **[Contributing Guide](CONTRIBUTING.md)** — Development and review workflow
- **[Documentation Index](docs/README.md)** — All methodology and component guides

Methodology notes (see the index above for the full list):

- [AAD](docs/methodology/aad.md) — Automatic adjoint differentiation: expression templates, tape, propagation
- [Yield Curve](docs/methodology/yield_curve.md) and [Yield-Curve Jacobian](docs/methodology/yield_curve_jacobian.md) — discount curves, calibration, Jacobian / inverse-Jacobian risk
- [Cross-Currency Pricing and Calibration](docs/methodology/xccy_calibration.md) — fixed, resettable, and MTM swaps; immutable fixing snapshots; staged basis and simultaneous domestic/foreign/basis calibration
- [Interpolation](docs/methodology/interpolation.md) — linear, log-linear, cubic interpolators
- [PDE](docs/methodology/pde.md) — PDE framework, grid construction, and coordinate maps
- [Script Engine](docs/methodology/script_engine.md) — expression scripting, fuzzy AAD evaluation, and compiled evaluator parity
- [Random](docs/methodology/random.md) — random number generation and path construction
- [Black / Bachelier](docs/methodology/black_scholes.md) — vanilla option pricing
- [Matrix](docs/methodology/matrix.md) — matrix and linear algebra

## License

MIT License — see [LICENSE](LICENSE)

## References

- Tom Hyer, *Derivatives Algorithms: Volume 1: Bones* ([repo](https://github.com/TomHyer/DA_Bones_Mirror))
- Antoine Savine, *Modern Computational Finance: AAD and Parallel Simulations* ([repo](https://github.com/asavine/CompFinance))
- Antoine Savine, *Modern Computational Finance: Scripting for Derivatives and xVA* ([repo](https://github.com/asavine/Scripting))
- Brian Huge and Jesper Andreasen, *Finite Difference Methods for Financial PDEs* ([repo](https://github.com/brnohu/CompFin))
