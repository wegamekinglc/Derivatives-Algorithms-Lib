# Derivatives-Algorithms-Lib Codex Context

## Project Identity

- Name: DAL, Derivatives Algorithms Lib
- Language: C++17
- Domain: quantitative finance, derivatives pricing, risk, market curves, scripting, automatic differentiation
- Main upstream inspiration:
  - Tom Hyer, Antoine Savine, Brian Huge / Jesper Andreasen material referenced in `README.md`

## Repo Structure

- `dal/`: core implementation
- `include/dal/`: installed/public headers mirroring the core API
- `public/`: external-facing wrappers
  - `public/src/`: public C++ wrapper layer
  - `public/excel/`: Excel/XLL integration
  - `public/python/`, `public/swig/`: SWIG-based Python bindings
- `examples/`: standalone demos
- `tests/`: Google Test suite, one file per module area
- `config/`: Machinist codegen configs
- `externals/`: vendored dependencies and submodules
- `.claude/`: project methodology, rules, and workflow guidance
- `.codex/`: Codex-facing mirrors of the reusable `.claude` guidance

## Architecture Summary

- `dal/math/`: numerical kernels
  - AAD, interpolation, matrix algebra, sparse/banded solvers, PDE/FD, RNG, Sobol, distributions, root finding, optimization
- `dal/model/`: model data and implementations
  - Black-Scholes, Dupire local vol, IVS, model factory
- `dal/script/`: product DSL and simulation engine
  - parser -> AST nodes -> visitors -> preprocess/compile -> evaluation/simulation
- `dal/curve/`: yield/discount curve framework and calibration
- `dal/indice/`: reference rate/index/fixings support
- `dal/risk/`: reports and risk utilities
- `dal/storage/`: storable objects and repository helpers
- `dal/time/`: dates, calendars, schedules, day count conventions
- `dal/concurrency/`: thread pool and concurrent queue

## Important Design Notes

- The project builds `dal_library` from `dal/`.
- `public/src` builds `dal_public` as the external-facing API wrapper layer.
- `public/excel` builds `dal_excel` only when `DAL_HAS_EXCEL` is enabled by Office auto-detection on Windows.
- `public/python` and `public/swig` are present in the tree, but are not wired into the top-level CMake build.
- Code generation is part of normal builds:
  - Machinist consumes `config/dal.ifc` and `config/dal.mgl`
  - generated outputs land in `dal/auto/` and `public/auto/`
- Generated `dal/auto/*` is excluded from the core library glob filters.
- Generated `dal/auto/*` is also excluded when `public/src` statically folds core DAL sources into `dal_public`.
- `dal/storage/_repository.*` is excluded from core build globs.
- Excel COM integration is auto-enabled on Windows only if Office binaries are detected.
- The repo supports AAD with XAD, CoDiPack, and Adept and also contains older/internal AAD machinery.
- Top-level CMake defaults external AAD backends to `off`; the shipped presets currently select Adept with `DAL_USE_ADEPT_AAD=on`.

## Build And Test

- Full Linux build:
  - `bash ./build_linux.sh`
- Manual build:
  - `cmake --preset=Release-linux ..`
  - `make -j32`
  - `make install`
- Debug build:
  - `cmake --preset=Debug-linux ..`
  - `make -j32`
  - `make install`
- Tests:
  - `bin/test_suite`
  - `bin/test_suite --gtest_filter=<SuiteName>.*`
  - `bin/test_suite --gtest_filter=<SuiteName>.<TestName>`

## CMake / Build Behavior

- Top-level install prefix is the repo root.
- Installed outputs typically land in:
  - `bin/`
  - `lib/`
  - `include/`
- `build_linux.sh` does more than compile:
  - clears `bin/` and `lib/`
  - builds `externals/machinist`
  - regenerates code in `dal/` and `public/`
  - configures/builds/installs with CMake
  - runs `bin/test_suite` if tests are enabled
- Top-level `CMakeLists.txt` defaults `SKIP_TESTS` to `true`, but the shipped presets and build scripts set it to `false`.

## Public Capabilities Present In Repo

- README documents C++ examples, Excel functions, and Python bindings.
- Public wrapper modules currently cover:
  - models
  - scripts/products
  - interpolation
  - random generators
  - repository operations
  - values/global state
- Current build wiring: top-level CMake always builds `public/src`, conditionally builds `public/excel`, and leaves `public/python` / `public/swig` as repo-managed binding scaffolding outside the default CMake build.

## Yield Curve Methodology

- See `.claude/methodology/yield_curve.md`.
- See `.claude/methodology/underdetermined_search.md` for the solver itself.
- Core curve types:
  - `YieldCurve_`
  - `DiscountCurve_`
  - `DiscountPWLF_`
  - `PiecewiseConstant_`
  - `PiecewiseLinear_`
  - `YCInstrument_` plus `Deposit_`, `Swap_`, and `STIR_` calibration instruments
- Discount curves are built from piecewise-linear instantaneous forwards integrated into discount factors.
- Calibration is framed as an underdetermined optimization problem using `dal/math/optimization/underdetermined.*`.
- Curve framework supports dependency tracking, base-curve layering, cloning, and substitution for bump-and-reprice style risk.

## Coding Conventions

- Formatting is enforced by `.clang-format`.
- Key style points from `.claude/rules/code-style.md`:
  - 4-space indentation
  - 150 column limit
  - attach braces
  - `T*` not `T *`
  - `nullptr` not `NULL`
  - all files end with newline
- Naming:
  - classes/types: `PascalCase_`
  - template params: `T_`, `E_`, etc.
  - functions/methods: `PascalCase`
  - members: `camelCase_`
  - locals: `camelCase`
  - constants/macros: `UPPER_SNAKE_CASE`
- Headers:
  - always `#pragma once`
  - standard three-line file header:
    - `//`
    - `// Created by <author> on <date>.`
    - `//`
- Include order:
  - standard/system headers
  - DAL/project headers
  - local headers
- Most `.cpp` files include `<dal/platform/platform.hpp>`.
- Prefer `using` over `typedef`.
- Use `explicit` for single-argument constructors.
- Use project exception/assertion macros:
  - `THROW`
  - `ASSERT`
  - `REQUIRE`
- Comments should be sparse and explain why, not what.
- Namespace closing braces should be commented, e.g. `} // namespace Dal`

## Test Conventions

- Google Test only.
- All tests compile into one executable: `test_suite`.
- Prefer plain `TEST(...)`, not fixtures.
- Prefer `ASSERT_*` over `EXPECT_*`.
- Float checks typically use `ASSERT_NEAR(..., tol)` with tight tolerances such as `1e-8` to `1e-10`.
- Test file naming:
  - `tests/<module>/test_<name>.cpp`
- Suite/test naming:
  - suite in PascalCase, e.g. `InterpTest`
  - test names prefixed with `Test...`
- Common AAD test flow:
  - clear tape
  - setup variables
  - start recording
  - compute
  - propagate adjoints
  - assert values and sensitivities

## Git / PR Conventions

- Base branch: `master`
- Branch naming:
  - `feature/<short-description>`
  - `fix/<short-description>`
- Commit messages:
  - imperative summary under 72 chars
  - blank line
  - body explaining why
  - if AI-assisted, append a co-author trailer in the format expected by the team's workflow
- Keep one logical change per commit when possible.
- PR body format from `.claude/rules/git-commit-pr.md`:
  - `## Summary`
  - `## Test plan`

## Examples / Reality Check

- There are currently many examples under `examples/`, including:
  - AAD
  - vanilla MC
  - finite difference
  - sobol
  - snowball
  - script products
  - barriers / UOC
  - concurrency
- There are about 90 `test_*.cpp` files in `tests/`.
- `tests/math/optimization/test_underdetermined.cpp` now has concrete solver coverage:
  - weighted `Underdetermined::Find` on a linear one-constraint system
  - stable convergence/termination behavior for `Underdetermined::Approximate`
  - custom `Jacobian_` path on a multi-residual system
  - failure path when `UnderdeterminedControls_` exhausts allowed evaluations/restarts
- The repo is large because `externals/` vendors substantial third-party code. Focus review/search work on project-owned paths first.

## Working Guidance For Future Sessions

- Read `CLAUDE.md` and this `AGENTS.md` first, then the relevant rules/methodology under `.claude/` or `.codex/`.
- Treat build scripts and current source tree as more authoritative than stale prose in docs when they conflict.
- When changing public APIs, check both:
  - core implementation in `dal/`
  - wrapper exposure in `public/src`, `public/swig`, and possibly Excel glue
- When changing generated interfaces, inspect `config/dal.ifc` / `config/dal.mgl` and the Machinist workflow before editing generated files directly.
- For curve work, align with the documented `DiscountPWLF_` / underdetermined calibration design rather than introducing unrelated abstractions.
