# DAL Repository Documentation Reconciliation

- Baseline: `1589089bdf10df352ce5cf9cde963fd6b51a4f95`
- Published baseline set: 33 files from `.github/scripts/check_docs.py`
- Reconciliation head before this audit record: `ab4ca65135276cb9443afc9decdf490e921c9d72`

## Executive Summary

The baseline set contains 14 changed documents, five historical artifacts moved intact to
`.codex/artifacts/`, and 14 documents verified current without edits. The reconciliation
uses public headers, bindings, generated registrations, source, tests, examples, benchmarks,
build configuration, and the accepted Task 1-5 diffs as evidence; structural documentation
checks alone are not treated as proof of technical accuracy.

## Document Dispositions

| Baseline document | Disposition | Source evidence | Result |
|-------------------|-------------|-----------------|--------|
| `CHANGELOG.md` | `changed` | XCCY pricing/calibration source; `XccyNotionalMode_::Value_`; Python and Excel joint-result registrations | Current capability baseline now covers fixed, resettable, and MTM XCCY, immutable snapshots, staged/joint calibration, and the exact per-surface matrix split; dated history remains qualified. |
| `CONTRIBUTING.md` | `verified-current` | `build_linux.sh`; `CMakePresets.json`; `CalibrationTest`; `PublicApiTest`; `.github/scripts/check_benchmark_regressions.py` | Build, generation, targeted-test, and paired benchmark instructions match the current developer workflow. |
| `README.md` | `changed` | Registered XCCY examples; Python example 007; XCCY methodology | The overview, example navigation, and methodology shortlist now make cross-currency pricing and calibration discoverable without duplicating the detailed contract. |
| `dal-cpp/README.md` | `verified-current` | `dal-cpp/CMakeLists.txt`; `Platform.cmake`; `ThreadPool_`; generation targets | Target ownership, lazy/configurable concurrency, build options, and generated-code workflow match the core implementation. |
| `dal-excel/README.md` | `changed` | `__curveprotocol.cpp`; `__xccycalibration.cpp`; generated joint getter; `ExcelApiTest` | Snapshot duplicate/reciprocal rules and staged-versus-joint worksheet matrix visibility now match the registered Excel surface. |
| `dal-public/README.md` | `verified-current` | `dal-public/CMakeLists.txt`; installed consumer; public builders, XCCY helpers, and `PublicApiTest` | The facade, compatibility scope, installed target, header families, and tests are described accurately. |
| `dal-python/README.md` | `changed` | `dal-python/src/bindings/curve.cpp`; curve and XCCY pytest suites; Python example 007 | Staged diagnostics-only and joint options/matrix availability are separated, inverse scaling is warned, and the test inventory includes curve and XCCY coverage. |
| `dal-web/README.md` | `verified-current` | `app.native_runtime`; `DalGateway`; `valuation.py`; native valuation binding; start scripts | Native-only preflight, staged runtime, async thread handoff, GIL release, gateway serialization, and matrix support match the current web implementation. |
| `docs/README.md` | `changed` | Curve parameterization/factory source; XCCY and Jacobian methodology | Heading hierarchy is corrected, LOG_DISCOUNT exclusivity is removed, and XCCY snapshot/range/inverse topics are indexed. |
| `docs/architecture.md` | `changed` | `MarketFixingSnapshot_`; global fixing store; generic, staged-XCCY, and joint-XCCY calibration entry points | Mutable process state is distinguished from immutable operation snapshots, and calibration architecture now branches by implemented workflow. |
| `docs/experimental/aad-analytic-jacobian-curve-calibration.md` | `verified-current` | `calibration.cpp`; `jointcalibration.cpp`; `aadjacobian.cpp`; analytic-Jacobian tests | The former rollout note is a concise redirect to the current normative methodology and no longer labels shipped behavior experimental. |
| `docs/experimental/replicate-ptirds-single-currency-curve.md` | `changed` | `PTIRDSCurveTest`; `Holidays::IsBusinessDay`; calibration source | The every-day target is now distinguished from `Holidays::None()` plus `Unadjusted` DAL conventions, and comparison language is neutral. |
| `docs/installation.md` | `verified-current` | `build_linux.sh`; presets; exported DAL CMake packages; installed consumer; web preflight | Core/full/distribution builds, staged installs, downstream consumption, CPU tuning, tests, and native web startup are current. |
| `docs/methodology/aad.md` | `changed` | `Rewind`; `RewindToMark`; `MCSimulation`; tape backends | Full-recording rewind and per-path checkpoint rewind are now named and timed according to the tape implementation. |
| `docs/methodology/black_scholes.md` | `verified-current` | Black/Bachelier option and implied-volatility source; real-domain round-trip tests | Formula, units, intrinsic handling, and Bachelier real-forward/strike support match the corrected implementation. |
| `docs/methodology/dupire.md` | `verified-current` | `IVS_::Call`; `IVS_::LocalVol`; rate-aware local-vol and repricing tests | Discounting, carry, strike derivative, grid layout, and tail behavior match the current rate-aware implementation. |
| `docs/methodology/interpolation.md` | `changed` | `Cubic1_`; `NaturalCubicWeightGeometry_`; mixed interpolation source/tests | Linear error order, cubic curvature factor, endpoint third-derivative semantics, and linear-head/cubic-tail orientation are accurate. |
| `docs/methodology/log_discount_curve.md` | `verified-current` | log-DF and zero-rate curve implementations; parameterization and PTIRDS tests | Anchor layout, interpolation schemes, extrapolation, AAD propagation, persistence, and zero-rate mapping remain current. |
| `docs/methodology/matrix.md` | `changed` | tri-diagonal solve; `BandedCholesky_`; `CholeskyImpl`; asymmetric and Krylov tests | Thomas notation, factorization/solve complexity, strict-dominance qualification, and dense regularization now describe the algorithms exactly. |
| `docs/methodology/pde.md` | `verified-current` | PDE grid/operators/theta scheme source and tests; `european_fd` | Coordinate maps, nonuniform operators, prepared-state contract, boundary ownership, and example flow match the implementation. |
| `docs/methodology/quadrature.md` | `verified-current` | quadrature source; Hermite moment and Simpson refinement tests | Gauss-Hermite exactness, Simpson point semantics, and fourth-order convergence match source and independent tests. |
| `docs/methodology/random.md` | `changed` | Sobol and pseudo-random implementations; simulation batch driver; RNG tests | Seeking is implementation-specific; IRN clone/branch behavior and the bounded MRG32 uniform replay contract are exact, while normal-path replay equivalence is explicitly not promised. |
| `docs/methodology/script_engine.md` | `verified-current` | parser/preprocessor; tree, compiled, and fuzzy visitors; simulation parity tests | Language phases, eager booleans, time split, compilation, batching, RNG selection, and AAD execution remain source-accurate. |
| `docs/methodology/underdetermined_search.md` | `verified-current` | `BacktrackMinimum`; exact/approximate solver; inverse/Jacobian tests | Scaling, minimum-change solve, corrected quadratic minimizer, diagnostics, and controls match the optimizer. |
| `docs/methodology/xccy_calibration.md` | `changed` | fixing snapshot and XCCY pricing/calibration source/tests; bindings; examples; `xccy_perf` and CI wiring | Canonical/reverse FX rules, immutable dependency closure, staged/joint matrix semantics and availability, four examples, and the 24-row smoke surface are complete. |
| `docs/methodology/yield_curve.md` | `verified-current` | curve construction/calibration headers; analytic/inverse tests; registered yield-curve examples | Representation layouts, single/staged/generic-joint solves, Jacobian population, risk scaling, and examples remain current. |
| `docs/methodology/yield_curve_jacobian.md` | `changed` | joint XCCY result/solver source; `TestXccyJointJacobian`; Python and Excel registrations | Joint range placement, complementary dimensions, accepted-solution construction, tolerance scaling, population modes, and binding availability are explicit. |
| `docs/public-api.md` | `changed` | generated notional enum; core/public XCCY headers; Python bindings; Excel selectors | C++ constants are copy-safe, core versus facade staged entry points are distinguished, and staged/joint matrices are documented per surface without inventing getters. |
| `docs/superpowers/plans/2026-07-12-unified-yield-curve-interpolation-aad.md` | `moved-to-.codex` | Task 1 hash comparison and 100% rename | Preserved intact at `.codex/artifacts/plans/2026-07-12-unified-yield-curve-interpolation-aad.md`; removed from the current-state published set. |
| `docs/superpowers/plans/2026-07-12-ycinstrument-pricing-performance.md` | `moved-to-.codex` | Task 1 hash comparison and 100% rename | Preserved intact at `.codex/artifacts/plans/2026-07-12-ycinstrument-pricing-performance.md`; removed from the current-state published set. |
| `docs/superpowers/plans/2026-07-12-zero-rate-parameterization.md` | `moved-to-.codex` | Task 1 hash comparison and 100% rename | Preserved intact at `.codex/artifacts/plans/2026-07-12-zero-rate-parameterization.md`; removed from the current-state published set. |
| `docs/superpowers/specs/2026-07-12-ycinstrument-pricing-performance-design.md` | `moved-to-.codex` | Task 1 hash comparison and 100% rename | Preserved intact at `.codex/artifacts/specs/2026-07-12-ycinstrument-pricing-performance-design.md`; removed from the current-state published set. |
| `docs/superpowers/specs/2026-07-12-zero-rate-parameterization-design.md` | `moved-to-.codex` | Task 1 hash comparison and 100% rename | Preserved intact at `.codex/artifacts/specs/2026-07-12-zero-rate-parameterization-design.md`; removed from the current-state published set. |

## Prior Audit Reconciliation

| 2026-07-10 finding group | Current status | Evidence |
|--------------------------|----------------|----------|
| Numerical and simulation correctness | Resolved except for one documented current RNG limitation | Rate-aware Dupire is gated by flat-vol local-vol and repricing tests; `BacktrackMinimum` has an independent oracle test; Bachelier supports real forward/strike pairs; `ValueByMonteCarlo` rejects non-positive path counts with public/Python/Excel tests; Sobol policy/clone, mixed orientation, and quadrature exactness have focused tests. `docs/methodology/random.md` now retains the current limitation: MRG32 seeking is not replay-equivalent for normal-path substreams. |
| Runtime and concurrency | Resolved and documented | `ThreadPool_` is lazy, lifecycle-locked, and capped by `DAL_NUM_THREADS`; native valuation uses `py::gil_scoped_release`; evaluation-date and gateway locks retain the intentional process serialization boundary. Core, web, architecture, and Python documentation agree. |
| Web product behavior | Resolved and documented | `app.native_runtime` and both launchers enforce the native-only contract; `DalGateway` constructs mutable/nested `DoubleMatrix_` surfaces; `valuation.py` moves native work off the event loop while the gateway lock states the serialization boundary. |
| Packaging and installation | Resolved for the documented contract | `dal-cppConfig.cmake` and `dal-publicConfig.cmake` exports plus `tests/installed-consumer/` establish relocatable downstream CMake consumption. Full builds provision Python dependencies and install into `build/stage/<preset>` rather than relying on source-root artifacts. |
| Build configuration | Resolved and documented | `core-dev`, `full-dev`, and `distribution` presets split workflows; `Platform.cmake` applies configuration-aware optimization and makes `DAL_ENABLE_NATIVE_ARCH` opt-in; installation and contributor guides describe the same staged/generation behavior. |
| Public API and distribution metadata | Resolved or accurately bounded | The public layer is documented as a source-compatible convenience facade rather than an ABI boundary; Monte Carlo path validation is enforced before conversion; root and Python metadata use MIT. XCCY docs now spell generated enums exactly and distinguish unavailable Python/Excel matrix accessors from core/public fields. |
| Documentation governance and coverage | Resolved for current published docs | The shipped analytic-Jacobian note redirects to methodology; commands, component links, architecture, contribution, installation, and public API guides exist and are checker-owned. Historical implementation plans/specs moved intact to `.codex/artifacts/`; current-state docs contain no rollout narrative. |

## Verification

| Gate | Result |
|------|--------|
| Baseline disposition completeness | The exact plan assertion passed: 33 unique rows equal the current 28-file checker set plus the five moved baseline paths. |
| Documentation checker | `python3 .github/scripts/check_docs.py` passed for 28 current-state Markdown files. |
| Branch whitespace and scope | `git diff --check 1589089b..HEAD` passed; the complete branch diff contains only Markdown, and protected `CLAUDE.md`, `.claude/`, `AGENTS.md`, `dal-cpp/dal/auto`, and `dal-excel/auto` paths are unchanged. |
| C++ XCCY examples | `xccy_curve_calibration`, `xccy_reset_pricing`, and `xccy_mtm_calibration` each exited 0; outputs included the staged 15-instrument fit, four pricing cases, and the converged `25x25` joint solve with five named ranges. |
| Python XCCY example | Installed-surface `dal-python/examples/007.xccy_joint_calibration.py` exited 0 with a converged `3x3` solve, named ranges, and FX forwards. |
| XCCY benchmark smoke | The executable output parsed with the corrected single-backslash whitespace regex to exactly 24 unique timing rows. The doubled raw-regex backslashes in the written task command are not executable evidence. |
| Task reviews | Task 1 and Task 2 were accepted clean; Tasks 3, 4, and 5 were accepted after scoped follow-up commits and re-review. The accepted head before this artifact is `ab4ca651`. |
| Whole-branch review | Not yet performed. A fresh final reviewer is intentionally scheduled after this audit commit. |
