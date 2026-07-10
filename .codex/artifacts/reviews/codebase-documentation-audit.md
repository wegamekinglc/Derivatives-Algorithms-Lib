# DAL Codebase and Documentation Audit

- Date: 2026-07-10
- Branch reviewed: `master`
- Commit reviewed: `7b959542`

Mode: read-only team review of architecture, quantitative methodology, public APIs,
build/install behavior, tests, CI, and user-facing documentation.

## Executive Summary

DAL has a strong quantitative core, unusually broad compiler/AAD-backend CI coverage,
and detailed methodology documentation. The script and curve-calibration systems are
organized into recognizable phases, the Python and Excel layers follow the intended
dependency direction, and the web backend centralizes native-library access in one
gateway.

The main risk is that several documents faithfully describe behavior that is itself
mathematically inconsistent. Passing tests therefore do not establish correctness for
all supported inputs. The highest-priority discrepancies are the rate-aware Dupire
formula, the underdetermined solver's backtracking minimizer, Bachelier behavior for
negative forwards/strikes, and unchecked Monte Carlo path counts. These should be
resolved with independent numerical oracle tests before treating the methodology docs
as authoritative.

The second risk is productization. A clean web checkout cannot start as documented,
native pricing holds the Python GIL, importing DAL eagerly creates one thread per CPU,
the advertised public CMake boundary is not installable as a package, and web/install
paths are largely absent from CI.

## Architecture Overview

The intended and actual high-level dependency graph is:

```text
dal-cpp (DAL::cpp)
  -> dal-public (DAL::public)
       -> dal-python (_dal pybind11 module)
            -> dal-web backend
       -> dal-excel (.xll, Windows only)

dal-web frontend -> FastAPI REST API -> DalGateway -> dal Python module
```

The arrows above mean "is consumed by". The native build dependency direction is:

```text
dal-cpp <- dal-public <- {dal-python, dal-excel}
```

### Component Responsibilities

| Component           | Current responsibility                                                                                                                |
|---------------------|---------------------------------------------------------------------------------------------------------------------------------------|
| `dal-cpp/`          | Core types, calendars, conventions, storage, math, AAD, curves, models, scripting, Monte Carlo, PDE, random generation, and concurrency |
| `dal-public/`       | Thin C++ construction, calibration, and valuation facade over core types                                                              |
| `dal-python/`       | pybind11 bindings plus a small handwritten Python convenience layer                                                                   |
| `dal-excel/`        | Excel conversion/repository layer plus Machinist-generated registration glue                                                          |
| `dal-web/backend/`  | FastAPI routers, persistence, valuation orchestration, and the single DAL gateway                                                     |
| `dal-web/frontend/` | React/Vite portfolio, trade, model, product, and valuation UI                                                                         |

### Representative Execution Flows

#### Scripted Monte Carlo Valuation

```text
React/API/Python caller
  -> DalGateway.value / MonteCarlo_Value
  -> ValueByMonteCarlo
  -> script preprocessing and parsing
  -> model factory
  -> MCSimulation<double> or MCSimulation<AAD::Number_>
  -> thread-pool batches
  -> tree-walk or compiled evaluator
  -> PV and optional AAD risks
```

Primary implementation sites:

- `dal-web/backend/app/services/dal_gateway.py`
- `dal-python/src/bindings/value.cpp`
- `dal-public/src/value.cpp`
- `dal-cpp/dal/script/simulation.hpp`

#### Curve Calibration

```text
public calibration builder
  -> validation and knot construction
  -> exact or approximate underdetermined solve
  -> bumped or eligible AAD analytic Jacobian
  -> calibrated curve
  -> residual, forward-Jacobian, and inverse-Jacobian diagnostics
```

Primary implementation sites:

- `dal-public/src/curvespec.cpp`
- `dal-cpp/dal/curve/calibration.hpp`
- `dal-cpp/dal/curve/calibration.cpp`

## Findings

### P0 - Quantitative And Process Correctness

#### 1. Rate-aware Dupire inversion is inconsistent with the stated formula

**Evidence**

- `docs/methodology/dupire.md` states the standard numerator term
  `(r - q) * K * C_K`.
- `docs/methodology/dupire.md` drops the strike multiplier in its
  implementation formula.
- `dal-cpp/dal/model/ivs.hpp` implements the formula without `K`.
- `IVS_::Call` in `dal-cpp/dal/model/ivs.hpp` does not use the stored `r_`
  or `q_` when producing the call surface.
- Existing Dupire calibration coverage uses an `IVS_` with default zero rates, so
  the rate-aware path is not tested.

**Impact**

For nonzero carry, the local-volatility surface can be inconsistent with the input
implied-volatility surface and may fail the defining vanilla-repricing property.

**Recommendation**

1. Define whether `IVS_::Call` represents a discounted spot call, an undiscounted
   forward call, or another measure-specific quantity.
2. Derive the matching Dupire formula from that contract.
3. Add an oracle test using a flat Black-Scholes volatility surface with nonzero
   `r` and `q`; recovered local volatility should be flat and vanilla prices should
   reprice within a declared tolerance.
4. Correct source and documentation together.

#### 2. The exact-solver backtracking minimizer does not match its quadratic model

**Evidence**

`docs/methodology/underdetermined_search.md` defines

```text
Q(k) = a*k^2 + 2*b*k*(1-k) + c*(1-k)^2
```

Differentiating that expression gives:

```text
k_min = (c - b) / (a - 2*b + c)
```

The document and `dal-cpp/dal/math/optimization/underdetermined.cpp`
instead use:

```text
(c - 0.5*b) / (c - b + a)
```

**Impact**

The solver can choose an incorrect backtrack fraction, causing unnecessary restarts,
poor convergence, or failure on nonlinear residuals. Current mostly linear/exact tests
do not isolate the quadratic minimizer.

**Recommendation**

Add a focused test for a synthetic nonlinear residual whose full step overshoots and
whose one-dimensional minimum is known analytically. Confirm the intended definition
of `k`, then correct either the model or the implementation.

#### 3. Bachelier implied volatility and sign handling contradict the normal model

**Evidence**

- `docs/methodology/black_scholes.md` says Bachelier solves directly in
  de-annualized volatility and supports real forwards/strikes.
- The shared solver in `dal-cpp/dal/math/distribution/black.cpp` exponentiates
  its coordinate for both Black and Bachelier.
- The Bachelier guess transform in `black.cpp` supplies a direct volatility
  value to a solver that then exponentiates it.
- `dal-cpp/dal/math/distribution/black.hpp` returns intrinsic value whenever
  `fwd * strike` is not positive, even though the normal model is valid for negative
  and opposite-sign forwards/strikes.

**Impact**

Negative-rate use cases can be silently mispriced, and positive explicit guesses can
be mapped to inappropriate starting volatilities.

**Recommendation**

Separate the Black and Bachelier solver-coordinate policies. Add price/implied-vol
round-trip tests for positive, negative, zero, and opposite-sign forward/strike pairs,
including explicit positive guesses.

#### 4. Invalid Monte Carlo path counts can return NaN or terminate the process

**Evidence**

- `dal-public/src/value.cpp` accepts signed `int nPaths` without validation.
- It passes the value to the `size_t` interface in
  `dal-cpp/dal/script/simulation.hpp`.
- Batching narrows path arithmetic back to `int` in `simulation.hpp` and
  `simulation.hpp`.
- A direct Python probe returned `{"PV": NaN}` for zero paths.
- A direct Python probe with `-1` terminated with `SIGFPE` during batching arithmetic.

**Impact**

This is a public C++/Python/Excel process-safety defect. Invalid user input can bypass
the exception model and crash the host process.

**Recommendation**

Validate `nPaths > 0` at the public C++ boundary before any signed-to-unsigned
conversion. Use one unsigned count type consistently inside the batching loop and add
C++, Python, and Excel regression tests for zero, negative, one, and boundary values.

### P1 - Runtime And Product Reliability

#### 5. Clean web startup cannot satisfy its native DAL dependency

**Evidence**

- `dal-web/backend/app/services/dal_gateway.py` imports `dal` unconditionally.
- `dal-web/backend/pyproject.toml` does not declare or install `dal-python`.
- `dal-web/scripts/start.sh` runs `uv sync` and starts Uvicorn without
  installing or validating DAL.
- A backend-environment probe reproduced `ModuleNotFoundError: No module named 'dal'`.
- `README.md` and `docs/installation.md` describe a `dal_stub.py` default
  and `DAL_REQUIRE_NATIVE`, but neither exists in runtime source.
- `dal-web/README.md` correctly describes the current native-only behavior,
  contradicting the other two documents.

**Impact**

The documented quick start fails for a clean checkout, and the frontend Playwright
launcher depends on that same broken path.

**Recommendation**

Choose one supported contract:

1. Native-only: declare/install the local DAL package and add a startup preflight with
   an actionable error; or
2. Explicit development stub: restore it as a real gateway implementation with a
   clearly visible non-production state.

Update all startup docs and smoke tests to the chosen contract.

#### 6. `asyncio.to_thread` does not keep the event loop responsive during pricing

**Evidence**

- `dal-web/backend/app/services/valuation.py` claims that `to_thread` keeps
  the event loop responsive.
- `dal-python/src/bindings/value.cpp` does not release the Python GIL around
  `ValueByMonteCarlo`.
- `DalGateway.value` holds a process-wide lock at
  `dal-web/backend/app/services/dal_gateway.py` because evaluation date is
  global state.

**Impact**

A long native valuation can prevent other Python work from running. Even after GIL
release, valuations remain serialized by the gateway lock.

**Recommendation**

Release the GIL only around the pure C++ valuation call. Add a responsiveness test with
a deliberately slow native call. Separately decide whether the supported concurrency
model is a serialized in-process queue, process workers, or request-scoped evaluation
state that permits concurrent valuations.

#### 7. DAL eagerly creates one thread per hardware CPU and uses an unsynchronized stop flag

**Evidence**

- `dal-cpp/dal/concurrency/threadpool.hpp` constructs and starts a static
  `ThreadPool_` using `hardware_concurrency()`.
- `dal-cpp/dal/concurrency/threadpool.cpp` starts those workers during library
  initialization.
- Importing `dal` in the review environment changed the process from 1 to 32 threads.
- `interrupt_` is a plain `bool` read by workers and written by `Stop()` across threads
  (`threadpool.cpp`).

**Impact**

Notebook, web-worker, and embedded processes consume substantial resources merely by
loading the module. The stop flag also creates a C++ data race.

**Recommendation**

Create the pool lazily, make the thread limit configurable through a public initializer
or environment variable, and synchronize worker termination using an atomic or the
queue's synchronization primitive. Add lifecycle and repeated-start/stop tests.

#### 8. The web Dupire editor accepts surfaces the Python binding cannot represent

**Evidence**

- `dal-web/frontend/src/pages/Models.tsx` accepts non-flat surfaces.
- `dal-web/backend/app/services/dal_gateway.py` rejects non-flat surfaces at
  valuation time because the Python `DoubleMatrix_` binding lacks mutation support.

**Impact**

Users can save apparently valid models that fail only when valued.

**Recommendation**

Add nested-list conversion or matrix mutation to the Python binding. Until then,
validate the flat-only restriction when the model is created and reflect the constraint
in the UI control.

### P1 - Public API, Packaging, And Build Contracts

#### 9. `dal-public` is a convenience facade rather than an enforced stable boundary

**Evidence**

- `dal-public/CMakeLists.txt` exports the repository root and links `DAL::cpp`
  publicly.
- Public headers such as `dal-public/src/curvespec.hpp` expose core concrete
  types directly.
- Python includes core calibration headers directly in
  `dal-python/src/bindings/curve.cpp`.
- Excel reaches internal implementation headers from `dal-excel/src/__platform.hpp`.
- Public consumers include headers through a path containing `src`, for example
  `<dal-public/src/value.hpp>`.

**Impact**

Core source and ABI changes propagate to all bindings despite the "stable public API"
description. Include-path enforcement cannot detect accidental boundary violations.

**Recommendation**

Decide whether `dal-public` promises source/ABI stability or is only a convenience
layer. For a stable boundary, introduce dedicated installed headers and public value
types, hide implementation details, stop exporting the workspace root, and add an
include-boundary check. At minimum, rename/document the current contract accurately.

#### 10. Installed and standalone CMake consumption is incomplete

**Evidence**

- `dal-public`, Python, and Excel contain `find_package(dal-cpp)` or
  `find_package(dal-public)` paths.
- The project installs no DAL `*Config.cmake`, version file, or exported DAL targets.
- A review configure of `dal-public` against the installed repository tree failed
  because `dal-cppConfig.cmake` was absent.
- No CI job builds a small out-of-tree consumer against an install prefix.

**Impact**

The repository can build as one workspace, but the advertised standalone component
paths are not reliable downstream-consumer contracts.

**Recommendation**

Export relocatable `DAL::cpp` and `DAL::public` packages with proper
`BUILD_INTERFACE`/`INSTALL_INTERFACE` include paths and dependency declarations. Add a
minimal external consumer that configures, links, and runs on Linux and Windows CI.

#### 11. Compiler flags make Debug builds optimized and binary wheels CPU-specific

**Evidence**

- `dal-cpp/cmake/Platform.cmake` overwrites `CMAKE_CXX_FLAGS` with `-O3
  -march=native` for GCC and Clang regardless of build type.
- A configured Debug compile command contained both `-O3` and `-g`.
- `docs/installation.md` describes `Debug-linux` as a normal debug build.
- The same core libraries are linked into Python wheels, making Linux artifacts depend
  on the build machine's instruction set.

**Impact**

Debugging is difficult, caller-supplied flags can be lost, and a distributed wheel can
execute unsupported CPU instructions on another machine.

**Recommendation**

Use target-scoped compile options and configuration expressions. Reserve `-O3` for
Release, avoid `-march=native` in distributable builds, and define an explicit portable
CPU baseline for wheels and CI artifacts.

#### 12. The root build path has hidden Python prerequisites and source-tree side effects

**Evidence**

- `CMakePresets.json` enables Python and benchmarks in the base preset.
- `README.md` and `docs/installation.md` present `build_linux.sh` as a
  sufficient clean-checkout command.
- `build_linux.sh` does not create a Python environment or install pytest/numpy; the
  extended Linux CI job performs these missing steps explicitly.
- Presets install into the repository root.
- `build_linux.sh` deletes `build`, `bin`, and `lib`, regenerates tracked source-tree
  artifacts, but does not clear `include`, allowing stale installed headers.

**Impact**

The quick start depends on undeclared local state and can leave a dirty or internally
inconsistent source tree.

**Recommendation**

Create separate `core-dev`, `full-dev`, and `distribution` presets. Install to a build
staging prefix, make code generation an explicit target, and provision Python test
requirements whenever Python tests are enabled.

### P2 - Documentation Accuracy And Governance

#### 13. Sobol precision settings do not behave as documented

**Evidence**

- `docs/methodology/random.md` says `precise=true, polish=false` gives a
  full-precision default.
- `dal-cpp/dal/math/specialfunctions.cpp` consults `precise` only inside the
  `polish` branch, so `precise` has no effect when `polish=false`.
- `SobolSet_::Clone` in `dal-cpp/dal/math/random/sobol.cpp` drops both
  precision flags.

**Recommendation**

Define the two flags as independent, testable policies or replace them with one named
inverse-CDF mode. Add accuracy and clone-equivalence tests before updating the document.

#### 14. Mixed log-discount interpolation is described in the opposite direction

**Evidence**

- `docs/methodology/interpolation.md` and
  `docs/methodology/log_discount_curve.md` describe a cubic head and linear
  long-end tail.
- `dal-cpp/dal/math/interp/interpmixed.cpp` implements a linear head and cubic
  tail.
- `docs/methodology/log_discount_curve.md` later describes the implemented
  direction, contradicting the earlier section.
- Existing tests verify knots and continuity but not which scheme applies on each side.

**Recommendation**

Decide which orientation is financially intended, add off-knot shape tests on both
sides of the cutoff, then align the implementation, enumeration description, and both
methodology documents.

#### 15. Quadrature documentation contains exactness and convergence-order errors

**Evidence**

- `docs/methodology/quadrature.md` says a two-node Gauss-Hermite rule exactly
  integrates the fourth moment; degree four requires at least three nodes because the
  exactness limit is `2*n - 1`.
- `quadrature.md` alternates between treating `n` as subintervals and points;
  `dal-cpp/dal/math/integral/quadrature.cpp` treats it as grid points.
- `quadrature.md` calls composite Simpson globally second order while also
  mentioning the correct fourth-order truncation behavior.

**Recommendation**

Correct the terminology and convergence statements. Add an explicit three-node fourth-
moment test and a mesh-refinement test demonstrating fourth-order Simpson convergence.

#### 16. License metadata conflicts across the repository

**Evidence**

- Root `LICENSE:1` is MIT.
- `README.md` says MIT.
- `dal-python/pyproject.toml` declares `BSD-3-Clause`.
- `dal-python/README.md` also says BSD 3-Clause.

**Impact**

Published Python package metadata presents a different license from the repository.

**Recommendation**

Choose the intended distribution license and align the root license, package metadata,
classifiers, wheel metadata, and component README. Review third-party source headers
separately rather than assuming the root license overrides their notices.

#### 17. Documentation status and command examples have drifted

**Evidence**

- `docs/experimental/aad-analytic-jacobian-curve-calibration.md` labels an on-by-
  default shipped capability "experimental" even though normative methodology already
  covers it.
- `docs/installation.md` tells users to run `bin/dal_public_tests`, but that
  executable is not installed by `dal-public/CMakeLists.txt`.
- The documented `CurveTest.*` filter has no matching suite.
- `docs/README.md` calls itself the canonical index but omits component READMEs.
- Setup commands are duplicated across root, installation, Python, and web READMEs;
  the web-stub contradiction demonstrates the maintenance cost.

**Recommendation**

Move historical rollout information to `CHANGELOG.md`, retire or redirect the duplicate
experimental note, correct test commands, and designate one canonical owner for each
setup workflow. Component READMEs should link to that owner rather than restating it.

#### 18. Architecture and contributor documentation are missing

**Evidence**

Methodology coverage is detailed, but there is no public `docs/architecture.md` or
`CONTRIBUTING.md`, and `dal-cpp`, `dal-public`, and `dal-excel` lack component READMEs.

**Recommendation**

Add:

- `docs/architecture.md`: dependency graph, global state, thread/tape ownership,
  generated-code workflow, valuation flow, and calibration flow.
- `CONTRIBUTING.md`: clean build, targeted tests, code generation, formatting, docs
  ownership, and review expectations.
- A public API guide showing supported C++, Python, and Excel entry points.

### P2 - CI And Maintenance Coverage

#### 19. CI does not exercise important product and documentation surfaces

**Evidence**

- Current workflows cover C++, public C++, and Python across multiple compilers and
  AAD backends.
- They do not run backend pytest/Ruff, frontend typechecking/build, Playwright, a native
  gateway integration test, an installed CMake consumer, or documentation checks.
- Backend tests inject a fake `dal` module, so passing backend tests do not verify the
  native contract.
- Benchmark jobs report failures but deliberately remain green.
- Code generation is run but generated-file drift is not fail-gated.

**Recommendation**

Add separate, focused CI jobs for:

1. Web backend pytest and Ruff.
2. Frontend TypeScript/Vite build.
3. Browser smoke tests using a deliberate test backend.
4. One native DAL web integration after the extended Python build.
5. Installed-tree CMake consumer tests on Linux and Windows.
6. Markdown links, anchors, published snippets, and option-table checks.
7. Generated-code regeneration followed by `git diff --exit-code`.
8. Warning-clean and ASan/UBSan coverage; add TSan once the thread-pool lifecycle is
   deterministic.

#### 20. Several false-green and legacy maintenance paths remain

**Evidence**

- Recursive source globs omit `CONFIGURE_DEPENDS`.
- `dal-public/src/CMakeLists.txt` is unused legacy build logic that conflicts with the
  active parent definition.
- `dal-public/src/random.hpp` includes itself.
- `DiscountPWC_::Write` always throws an explicit TODO at
  `dal-cpp/dal/curve/ycconst.cpp`.
- Several tests have no meaningful assertion, including the large-path Sobol and
  lower-band accumulator cases.
- The dynamic `Stack_::TopAndPop` in `dal-cpp/dal/math/stacks.hpp` reads a different
  index convention from `Top`, while the fixed script stack has no capacity checks.

**Recommendation**

Remove dead build definitions, make source discovery deterministic, turn placeholder
tests into assertions, define the PWC serialization contract, and consolidate the two
stack implementations behind one bounds-tested abstraction.

## Strengths

1. The native dependency direction is clear and consistently wired at the target level.
2. Script parsing, preprocessing, domain analysis, tree evaluation, compiled evaluation,
   and fuzzy AAD are separated into understandable phases.
3. Double and AAD Monte Carlo share a templated flow while maintaining per-thread RNG,
   path, evaluator, and tape state.
4. Curve calibration exposes useful diagnostics rather than only returning a curve.
5. The web integration with DAL is centralized in one gateway, and persistence is behind
   a store protocol.
6. Linux CI spans four compilers/backends, with representative Windows coverage.
7. Methodology documents are detailed enough to expose mathematical inconsistencies;
   PDE and script-engine documents tracked the source particularly well.
8. Local relative-link inspection found no genuine missing documentation targets.
9. Root and Python pricing examples reproduced their documented numerical results.

## Recommended Delivery Plan

### Phase 0: Correctness Baseline - 0 to 2 weeks

1. Add independent oracle tests for Dupire with nonzero carry.
2. Add a direct backtracking-minimizer test and resolve the line-search formula.
3. Add Bachelier negative-forward/strike and implied-vol round-trip tests.
4. Reject invalid Monte Carlo path counts at the public boundary.
5. Add Sobol precision and clone-equivalence tests.
6. Decide and test the intended mixed-interpolation orientation.

Acceptance criterion: each corrected methodology equation is exercised by a test whose
expected value is derived independently from the implementation.

### Phase 1: Reliable User Workflows - 2 to 4 weeks

1. Make the web native/stub contract explicit and clean-checkout reproducible.
2. Release the GIL around native pricing and add an event-loop responsiveness test.
3. Make the native thread pool lazy, configurable, and race-free.
4. Validate Dupire model constraints at creation time.
5. Add web backend, frontend, and native-integration CI.

Acceptance criterion: a clean clone can run every documented quick start in CI.

### Phase 2: Packaging And Build Contracts - 1 to 2 months

1. Export relocatable CMake packages for `DAL::cpp` and `DAL::public`.
2. Add out-of-tree consumer tests.
3. Decide and enforce the `dal-public` compatibility promise.
4. Split development and distribution presets.
5. Replace global native optimization flags with target/configuration options.
6. Move installation and generation side effects out of the source tree.

Acceptance criterion: installed C++ and Python artifacts work from a clean staging
prefix on a machine with the documented CPU baseline.

### Phase 3: Documentation Governance - 1 to 3 months

1. Add architecture, contributor, and public API guides.
2. Reconcile every methodology finding above with source and oracle tests.
3. Remove duplicated setup instructions and stale experimental status.
4. Align license metadata.
5. Add docs/link/snippet/generated-output CI gates.

Acceptance criterion: documentation is generated or tested from the same contracts as
the public code paths it describes.

## Decisions Required

1. Is the web product native-only, or is a development stub a supported product mode?
2. Does `dal-public` promise ABI/source stability, or is it a convenience facade?
3. Is `MIXED` intended to be cubic-head/linear-tail or linear-head/cubic-tail?
4. Should web pricing serialize in one process, use process workers, or support
   request-scoped concurrent valuation?
5. What is the minimum portable CPU instruction baseline for distributed binaries?
6. Is `DiscountPWC_` persistence supported or explicitly unsupported?

## Verification Performed

The review used existing build artifacts and source-level probes.

| Verification                                 | Result                                   |
|----------------------------------------------|------------------------------------------|
| `ctest --test-dir build --output-on-failure` | 851/851 passed                           |
| Direct core Google Test binary               | 810 passed                               |
| Direct public Google Test binary             | 40 passed                                |
| Direct Python binding pytest                 | 188 passed                               |
| Web backend pytest                           | 32 passed, with two deprecation warnings |
| Web backend Ruff                             | Passed                                   |
| Frontend `npm run build`                     | Passed                                   |
| Root README Python pricing example           | Reproduced documented result             |
| Public `num_path=0` probe                    | Returned `PV = NaN`                      |
| Public `num_path=-1` probe                   | Process terminated with `SIGFPE`         |
| Python import thread-count probe             | Increased from 1 to 32 threads           |
| Clean backend native import probe            | Failed with `ModuleNotFoundError: dal`   |
| Standalone `dal-public` CMake configure      | Failed: missing `dal-cppConfig.cmake`    |

Not performed:

- Clean C++ rebuild from the reviewed commit.
- Windows or Excel runtime verification.
- Playwright browser suite.
- Sanitizer builds.
- External-link validation.
- Benchmark regression comparison against a stored baseline.

## Overall Assessment

The quantitative engine and test volume are substantial, and the codebase has a solid
foundation for continued development. However, the current passing suite mostly proves
internal consistency. It does not yet resolve several source-versus-methodology
disagreements or establish clean install/runtime contracts.

Recommendation: treat the current state as **request changes before declaring the
methodology authoritative or the full workspace production-ready**. Begin with the P0
numerical oracle tests and invalid-input guard, then repair the web/runtime and packaging
paths before broadening the algorithm surface.
