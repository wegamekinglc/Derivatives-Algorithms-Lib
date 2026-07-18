# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository. Keep this as operational guidance for agents; keep public project orientation in [README.md](README.md), and put method-level explanations under `docs/methodology/`.

## Build Commands

```bash
# Default Linux build (configures core + public C++ + examples via the
# Release-linux preset, installs into build/stage/Release-linux, and runs CTest).
# Pass --full for Python plus benchmarks, --benchmarks for benchmarks only, or
# --generate to regenerate Machinist sources.
bash ./build_linux.sh

# Manual build (top-level workspace) -- Release-linux/Debug-linux do not declare a
# binaryDir, so pass -S/-B explicitly (this is what build_linux.sh does) and use
# cmake --build / cmake --install instead of raw make.
cmake --preset=Release-linux -S . -B build/Release-linux
cmake --build build/Release-linux -j32
cmake --install build/Release-linux

# Debug build
cmake --preset=Debug-linux -S . -B build/Debug-linux
cmake --build build/Debug-linux -j32
cmake --install build/Debug-linux
```

The top-level `CMakeLists.txt` is a thin workspace that selects sub-projects via options:

- `DAL_BUILD_PUBLIC` (default `ON`) — build `dal-public`
- `DAL_BUILD_PYTHON` (default `OFF`) — build `dal-python` (pybind11 + Python package)
- `DAL_BUILD_EXCEL` (default `OFF`) — build `dal-excel` (Windows-only)
- `DAL_CPP_BUILD_TESTS` (default `ON`) — build the `dal-cpp` test suite
- `DAL_CPP_BUILD_EXAMPLES` (default `ON`) — build the `dal-cpp` example programs
- `DAL_CPP_BUILD_BENCHMARKS` (CMake option default `ON`, but the `base` preset in `CMakePresets.json` overrides it to `off`, so preset-driven builds — including `build_linux.sh` without `--benchmarks`/`--full` — disable benchmarks) — build the `dal-cpp` benchmark programs
- `DAL_USE_ADEPT_AAD` / `DAL_USE_XAD_AAD` / `DAL_USE_CODIPACK_AAD` — pick the AAD backend (source default: all three OFF, i.e. the native backend; CMake presets also override all three to OFF unless explicitly enabled)
- `DAL_ENABLE_SANITIZERS` (default empty, i.e. instrumentation off) — semicolon-separated sanitizer list (e.g. `"address;undefined"` or `"thread"`); GCC/Clang only, applies `-fsanitize=<list>` to the compile and link of every target in the workspace, plus compile-only `-fno-omit-frame-pointer`

CMake installs into a per-preset stage directory (`CMAKE_INSTALL_PREFIX=${sourceDir}/build/stage/${presetName}` in `CMakePresets.json`); `build_linux.sh` installs into `build/stage/Release-linux/`, placing binaries in `bin/`, libraries in `lib/`, and headers in `include/` under that stage directory.

## Running Tests

Tests are driven through CTest at the workspace level. Each sub-project registers its own GoogleTest binary:

- `dal_cpp_tests` — core library tests (built from `dal-cpp/tests/`)
- `dal_public_tests` — public-API tests (built from `dal-public/tests/`)

```bash
# Run all registered tests (build_linux.sh puts the build tree at build/Release-linux)
ctest --test-dir build/Release-linux --output-on-failure

# Run a single binary directly (build tree)
./build/Release-linux/dal-cpp/dal_cpp_tests
./build/Release-linux/dal-public/dal_public_tests

# Run a single test suite
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter=<SuiteName>.*

# Run a single test
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter=<SuiteName>.<TestName>

# Focused script tree-walk/compiled evaluator parity checks
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*'
```

For script engine performance changes, build and smoke-run the benchmark (benchmarks are off in the `base` preset, so enable them explicitly):

```bash
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_BENCHMARKS=ON
cmake --build build/Release-linux --target script_mc_perf -j 4
./build/Release-linux/dal-cpp/benchmarks/script_mc_perf/script_mc_perf
```

The paired regression gate that CI runs on pull requests lives in `.github/scripts/check_benchmark_regressions.py`; see `.claude/agents/dal-performancer.md` for reproducing it locally.

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
- `dal-cpp/dal/script/` — expression scripting engine using visitor pattern (parser → AST nodes → simulation/evaluation), with tree-walk evaluation as the default and a compiled flat-stream evaluator behind the `compiled` flag
- `dal-cpp/dal/model/` — financial models
- `dal-cpp/dal/curve/` — yield/discount curve handling
- `dal-cpp/dal/indice/` — reference rate index management
- `dal-cpp/dal/risk/` — risk calculations
- `dal-cpp/dal/concurrency/` — thread pool and concurrent queue
- `dal-cpp/dal/storage/` — data persistence (files matching `_repository.*` are excluded from the build)
- `dal-cpp/dal/auto/` — auto-generated code (Machinist output, glob-included into the library)
- `dal-cpp/tests/` — Google Test files compiled into the `dal_cpp_tests` binary
- `dal-cpp/examples/` — standalone example programs (AAD, Monte Carlo, finite difference, scripting, concurrency, Sobol, underdetermined optimization)
- `dal-cpp/benchmarks/` — standalone performance benchmark programs, one executable per target (19 total: matrix, script, tape, jacobian, pde, rng, interp, krylov, banded, cholesky, specialfunctions, black, iv_brent, script_mc, curve_calibration, xccy, ycinstrument, threadpool, stacks); each registers as a CTest test under the `benchmark` label (CI discovers them with `ctest -N -L benchmark`), and `build_linux.sh` never runs them in its ctest pass (`-LE benchmark`); an 8-target subset is gated by the paired regression script. `script_mc_perf` compares tree-walk and compiled script evaluation
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
- `dal-web/backend/` — Python FastAPI application, uses the `dal-python` bindings directly
- `dal-web/frontend/` — React + TypeScript SPA, uses Vite
- `dal-web/scripts/` — `start.sh`/`stop.sh` (Linux/macOS), `start.ps1`/`stop.ps1` (Windows/PowerShell 7), and `setup-playwright.sh`

The backend persists all entities through a SQLAlchemy 2.x store (`app/services/db/`) behind the `Store` seam. Default backend is a local SQLite file under `dal-web/backend/.data/` (gitignored); set `DAL_WEB_DB_URL` to point at any SQLAlchemy URL, `DAL_WEB_STORE=memory` to use the legacy in-memory store, and `DAL_WEB_AUTO_MIGRATE=1` to apply Alembic migrations on startup instead of `create_all()`.

Start the web UI with `./dal-web/scripts/start.sh` on Linux/macOS or `dal-web/scripts/start.ps1` on Windows (requires Python 3.13+, uv, Node.js 20+, npm). Frontend at http://localhost:5173, backend API docs at http://127.0.0.1:8001/docs. For frontend e2e, run `./dal-web/scripts/setup-playwright.sh` once, then `cd dal-web/frontend && npm run test:e2e`.

**Code generation** — `dal-cpp/config/dal.ifc` is processed by the Machinist tool. Regeneration is opt-in: `build_linux.sh --generate` (or `cmake --build build/Release-linux --target dal_generate` on a configured tree) runs Machinist twice:
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
- **Matrix and Linear Algebra** — [Matrix and linear algebra](docs/methodology/matrix.md)
- **Log-Discount Curve** — [Log-discount curve](docs/methodology/log_discount_curve.md)
- **PDE Finite-Difference Meshers and Coordinate Maps** — [PDE meshers](docs/methodology/pde.md)
- **Yield-Curve Jacobian and Inverse-Jacobian Risk** — [Yield-curve Jacobian](docs/methodology/yield_curve_jacobian.md)
- **Script Engine** — [Script engine](docs/methodology/script_engine.md), including tree-walk, fuzzy AAD, compiled evaluation, parity coverage, and benchmarks
- **Dupire Local Volatility** — [Dupire local volatility](docs/methodology/dupire.md)
- **Black / Bachelier Vanilla Pricing** — [Black / Bachelier vanilla pricing](docs/methodology/black_scholes.md)
- **Numerical Quadrature** — [Numerical quadrature](docs/methodology/quadrature.md)
- **Random Number Generation and Path Construction** — [Random and path generation](docs/methodology/random.md)

Notable fundamental changes are recorded in [CHANGELOG.md](CHANGELOG.md).

## Rules to follow

- **coding style**: [Code style guide](.claude/rules/code-style.md)
- **unit test style**: [Unit test style guide](.claude/rules/unit-test-style.md)
- **web UI design**: [Web UI design standards](.claude/rules/dal-web-design.md)
- **dal-web backend style**: [dal-web backend code style](.claude/rules/dal-web-code-style.md)

Documentation guidance:
- Keep `README.md` concise and user-facing; link to detailed methodology instead of duplicating it.
- Put script engine evaluator details in [docs/methodology/script_engine.md](docs/methodology/script_engine.md), especially tree-walk vs compiled behavior, AAD/fuzzy behavior, parity tests, and benchmark scope.
- Update [CHANGELOG.md](CHANGELOG.md) only for notable behavior, API, methodology, or performance-facing changes.

## Specialist agents

Specialist agents for the spec -> design -> critique -> implement -> review -> document
pipeline are defined in [.claude/agents/](.claude/agents/README.md). In particular,
`dal-doc-writer` owns the freshness of everything under `docs/` and curates
[CHANGELOG.md](CHANGELOG.md); invoke it when docs need reconciling against current code or a
change may warrant a changelog entry.
