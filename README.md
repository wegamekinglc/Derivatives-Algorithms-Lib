# DAL -- Derivatives Algorithms Library

[![CMake Linux CI](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/workflows/cmake-linux.yml/badge.svg?branch=master)](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/workflows/cmake-linux.yml)
[![Codacy Grade](https://app.codacy.com/project/badge/Grade/9c84afd2bb534c6c87584e5d6e4cc420)](https://app.codacy.com/app/wegamekinglc/Derivatives-Algorithms-Lib)
[![Coverage Status](https://coveralls.io/repos/wegamekinglc/Derivatives-Algorithms-Lib/badge.svg?branch=master)](https://coveralls.io/github/wegamekinglc/Derivatives-Algorithms-Lib?branch=master)

A C++17 quantitative finance library with built-in Automatic Adjoint Differentiation (AAD). Features include yield curve construction, Monte Carlo simulation, finite difference PDE solvers, a scripting engine for exotic payoffs, and parallel model evaluation.

## Quick Start

```bash
git clone --recursive git@github.com:wegamekinglc/Derivatives-Algorithms-Lib.git
cd Derivatives-Algorithms-Lib
bash build_linux.sh          # or build_windows.bat on Windows
```

For detailed installation instructions (Python bindings, Web UI, troubleshooting), see **[docs/installation.md](docs/installation.md)**.

## Architecture

```
dal-cpp     → Core quant library (DAL::cpp)
  ↑
dal-public  → Stable public C++ API (DAL::public)
  ↑        ↑
dal-python  dal-excel
  ↑
dal-web     → FastAPI + React portfolio management UI
```

| Sub-project   | Purpose                                                  |
|---------------|----------------------------------------------------------|
| `dal-cpp/`    | Core library: math, curves, models, scripting, AAD       |
| `dal-public/` | Stable public API wrapping `DAL::cpp`                    |
| `dal-python/` | SWIG Python bindings                                     |
| `dal-excel/`  | Excel `.xll` add-in (Windows-only)                       |
| `dal-web/`      | Portfolio management web app                             |

Core modules in `dal-cpp/dal/`:
- **math/** — Interpolation, optimization, PDE solvers, random numbers, matrix ops
- **math/aad/** — Automatic Adjoint Differentiation (native, XAD, Adept, CoDiPack backends)
- **curve/** — Yield curve construction, piecewise forward rates, calibration
- **script/** — Expression scripting engine for exotic payoffs
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

res = MonteCarlo_Value(product, model, 2**20, "sobol", False, True)
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

More examples: [Python](miscs/python/), [Excel](miscs/excel/), [C++](dal-cpp/examples/)

### Excel

```
=PRODUCT.NEW("my_product", A2, B2)
=BSMODELDATA.NEW("model", 100, 0.15, 0.0, 0.0)
=MONTECARLO.VALUE(A5, C7, 2^20, "sobol", FALSE)
```

## Web UI

Portfolio management web app in `dal-web/`:

```bash
./dal-web/scripts/start.sh     # Start backend + frontend
./dal-web/scripts/stop.sh      # Stop services
```

- Frontend: http://localhost:5173
- API docs: http://127.0.0.1:8001/docs

## Documentation

- **[Installation Guide](docs/installation.md)** — Complete setup instructions
- **[Methodology](docs/methodology/)** — Technical deep dives:
  - [AAD](docs/methodology/aad.md) — Expression templates, tape, propagation
  - [Yield Curves](docs/methodology/yield_curve.md) — Discount curves, calibration
  - [Underdetermined Search](docs/methodology/underdetermined_search.md) — Optimization
- **[Designs](docs/designs/)** — Architectural decisions

## License

MIT License — see [LICENSE](LICENSE)

## References

- Tom Hyer, *Derivatives Algorithms: Volume 1: Bones* ([repo](https://github.com/TomHyer/DA_Bones_Mirror))
- Antoine Savine, *Modern Computational Finance: AAD and Parallel Simulations* ([repo](https://github.com/asavine/CompFinance))
- Antoine Savine, *Modern Computational Finance: Scripting for Derivatives and xVA* ([repo](https://github.com/asavine/Scripting))
- Brian Huge and Jesper Andreasen, *Finite Difference Methods for Financial PDEs* ([repo](https://github.com/brnohu/CompFin))
