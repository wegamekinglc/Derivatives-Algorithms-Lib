# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Full Linux build (runs Machinist code generation, configures the workspace,
# builds all enabled sub-projects, installs into the repo root, and runs CTest)
bash ./build_linux.sh

# Manual build (top-level workspace)
mkdir build && cd build
cmake --preset=Release-linux .. && make -j32 && make install

# Debug build
mkdir build && cd build
cmake --preset=Debug-linux .. && make -j32 && make install
```

The top-level `CMakeLists.txt` is a thin workspace that selects sub-projects via options:

- `DAL_BUILD_PUBLIC` (default `ON`) — build `dal-public`
- `DAL_BUILD_PYTHON` (default `OFF`) — build `dal-python` (pybind11 + Python package)
- `DAL_BUILD_EXCEL` (default `OFF`) — build `dal-excel` (Windows-only)
- `DAL_CPP_BUILD_TESTS` (default `ON`) — build the `dal-cpp` test suite
- `DAL_CPP_BUILD_EXAMPLES` (default `ON`) — build the `dal-cpp` example programs
- `DAL_CPP_BUILD_BENCHMARKS` (default `ON`) — build the `dal-cpp` benchmark programs
- `DAL_USE_ADEPT_AAD` / `DAL_USE_XAD_AAD` / `DAL_USE_CODIPACK_AAD` — pick the AAD backend (source default: Adept; CMake presets override all three to OFF unless explicitly enabled)

CMake installs to the repo root (`CMAKE_INSTALL_PREFIX=${sourceDir}`), placing binaries in `bin/`, libraries in `lib/`, and headers in `include/`.

## Running Tests

Tests are driven through CTest at the workspace level. Each sub-project registers its own GoogleTest binary:

- `dal_cpp_tests` — core library tests (built from `dal-cpp/tests/`)
- `dal_public_tests` — public-API tests (built from `dal-public/tests/`)

```bash
# Run all registered tests
(cd build && ctest --output-on-failure)

# Run a single binary directly
bin/dal_cpp_tests
bin/dal_public_tests

# Run a single test suite
bin/dal_cpp_tests --gtest_filter=<SuiteName>.*

# Run a single test
bin/dal_cpp_tests --gtest_filter=<SuiteName>.<TestName>
```

## Code Style

Formatting is enforced via `.clang-format` (LLVM-based):
- 4-space indentation, 150 column limit
- `PointerBindsToType: true` (use `T*` not `T *`)
- Braces attach style (`BreakBeforeBraces: Attach`)

## Architecture

This is a C++17 quantitative finance library with AAD (Automatic Adjoint Differentiation) support, organized as a multi-project workspace.

```
Derivatives-Algorithms-Lib/
├── CMakeLists.txt          (thin workspace selecting sub-projects)
├── dal-cpp/                DAL::cpp — core library (always built)
├── dal-public/             DAL::public — public API, depends on DAL::cpp
├── dal-python/             DAL::python — pybind11 + Python package, depends on DAL::public
├── dal-excel/              DAL::excel — Excel add-in, depends on DAL::public (Windows-only)
└── dal-web/                Portfolio management web app (FastAPI + React)
```

The dependency graph is `dal-cpp ← dal-public ← {dal-python, dal-excel}`. Each sub-project owns its own `CMakeLists.txt` and stands alone as a buildable target.

**Core library (`dal-cpp/`)** — built as the `dal_cpp` target (alias `DAL::cpp`):
- `dal-cpp/dal/math/` — numerical algorithms: interpolation, optimization, PDE solvers, random number generation, matrix ops, root finding, AAD
- `dal-cpp/dal/script/` — expression scripting engine using visitor pattern (parser → AST nodes → simulation/evaluation)
- `dal-cpp/dal/model/` — financial models
- `dal-cpp/dal/curve/` — yield/discount curve handling
- `dal-cpp/dal/indice/` — reference rate index management
- `dal-cpp/dal/risk/` — risk calculations
- `dal-cpp/dal/concurrency/` — thread pool and concurrent queue
- `dal-cpp/dal/storage/` — data persistence (files matching `_repository.*` are excluded from the build)
- `dal-cpp/dal/auto/` — auto-generated code (Machinist output, glob-included into the library)
- `dal-cpp/tests/` — Google Test files compiled into the `dal_cpp_tests` binary
- `dal-cpp/examples/` — standalone example programs (AAD, Monte Carlo, finite difference, scripting, concurrency, Sobol, underdetermined optimization)
- `dal-cpp/benchmarks/` — standalone performance benchmark programs (matrix, script engine)
- `dal-cpp/externals/` — git submodules for AAD frameworks, gtest, rapidjson, machinist
- `dal-cpp/config/` — Machinist input (`dal.ifc`, `dal.mgl`)
- `dal-cpp/cmake/` — `Platform.cmake` and helpers

**Public API (`dal-public/`)** — built as the `dal_public` target (alias `DAL::public`):
- `dal-public/src/` — C++ wrapper layer that depends only on `DAL::cpp`'s public headers
- `dal-public/tests/` — public-API tests compiled into `dal_public_tests`

**Python bindings (`dal-python/`, off by default)** — depends on `DAL::public`:
- `dal-python/src/bindings/` — pybind11 binding module
- `dal-python/src/dal/` — Python package (`__init__.py`, `api.py`)
- `dal-python/tests/` — pytest-based tests

**Excel add-in (`dal-excel/`, Windows-only, off by default)** — depends on `DAL::public`:
- `dal-excel/src/` — Excel binding sources
- `dal-excel/auto/` — Machinist output for Excel `xl_*` wrappers
- `dal-excel/tests/` — Excel-specific tests

**Web UI (`dal-web/`, not built by CMake)** — FastAPI backend + React frontend:
- `dal-web/backend/` — Python FastAPI application, uses `dal-python` bindings or stub
- `dal-web/frontend/` — React + TypeScript SPA, uses Vite
- `dal-web/scripts/` — `start.sh`, `stop.sh`, and `setup-playwright.sh`

Start the web UI with `./dal-web/scripts/start.sh` (requires Python 3.13+, uv, Node.js 20+, npm). Frontend at http://localhost:5173, backend API docs at http://127.0.0.1:8001/docs. For frontend e2e, run `./dal-web/scripts/setup-playwright.sh` once, then `cd dal-web/frontend && npm run test:e2e`.

**Code generation** — `dal-cpp/config/dal.ifc` is processed by the Machinist tool. `build_linux.sh` runs Machinist twice:
- once with `-d ./dal-cpp/dal` to produce core enum and serialization files under `dal-cpp/dal/auto/`
- once with `-d ./dal-excel` to produce Excel public-function stubs under `dal-excel/auto/`

**External dependencies** (`dal-cpp/externals/`, git submodules):
- `xad/` — XAD AAD framework for automatic differentiation
- `adept/` — Adept AAD framework for automatic differentiation
- `googletest/` — Google Test framework
- `rapidjson/` — JSON parsing
- `machinist/` — code generation tool
- `CodiPack/` — CoDiPack AD framework
- `pybind11/` — Python bindings (v2.11.1, used by dal-python)

## Methodology

Detailed documentation of the quantitative methods implemented in this library:

- **Automatic Adjoint Differentiation (AAD)** — [AAD methodology](docs/methodology/aad.md)
- **Yield Curve Construction** — [Yield curve construction](docs/methodology/yield_curve.md)
- **Underdetermined Search** — [Underdetermined search](docs/methodology/underdetermined_search.md)
- **Cross-Currency Calibration** — [Cross-currency calibration](docs/methodology/xccy_calibration.md)
- **Interpolation** — [Interpolation](docs/methodology/interpolation.md)
- **Log-Discount Curve** — [Log-discount curve](docs/methodology/log_discount_curve.md)
- **PDE Finite-Difference Meshers and Coordinate Maps** — [PDE meshers](docs/methodology/pde.md)
- **Yield-Curve Jacobian and Inverse-Jacobian Risk** — [Yield-curve Jacobian](docs/methodology/yield_curve_jacobian.md)
- **Script Engine** — [Script engine](docs/methodology/script_engine.md)
- **Dupire Local Volatility** — [Dupire local volatility](docs/methodology/dupire.md)
- **Black / Bachelier Vanilla Pricing** — [Black / Bachelier vanilla pricing](docs/methodology/black_scholes.md)

Notable fundamental changes are recorded in [CHANGELOG.md](CHANGELOG.md).

## Rules to follow

- **coding style**: [Code style guide](.claude/rules/code-style.md)
- **unit test style**: [Unit test style guide](.claude/rules/unit-test-style.md)
- **web UI design**: [Web UI design standards](.claude/rules/dal-web-design.md)

## Specialist agents

Specialist agents for the spec -> design -> critique -> implement -> review -> document
pipeline are defined in [.claude/agents/](.claude/agents/README.md). In particular,
`dal-doc-writer` owns the freshness of everything under `docs/` and curates
[CHANGELOG.md](CHANGELOG.md); invoke it when docs need reconciling against current code or a
change may warrant a changelog entry.

