# XCCY Examples and Benchmark Coverage - Specification

## Source

- Pull request: #230, `feat: add reset-aware XCCY pricing and joint calibration`
- User request and approved balanced design: 2026-07-14
- Related methodology: `docs/methodology/xccy_calibration.md` and `docs/methodology/yield_curve_jacobian.md`

## Problem Statement

PR #230 adds broad reset-aware XCCY pricing and calibration behavior, but its runnable examples jump from fixed-notional staged calibration to a large joint recovery problem. The existing `xccy_perf` target covers future pricing and several calibration modes, but it omits a started-trade basket, reset-aware staged calibration, and execution in benchmark CI. Python exposes the joint workflow but has no runnable joint/reset-aware example.

## Goals

- Add an approachable C++ example that compares the three XCCY notional modes and demonstrates historical fixing snapshots for a started MTM trade.
- Add a runnable Python example for reset-aware joint domestic, foreign, and basis calibration.
- Extend `xccy_perf` from 21 to 24 stable output rows without creating a mode-by-solver cross product.
- Execute `xccy_perf` as an informational smoke benchmark on Linux and Windows CI.
- Cover the untested domestic-leg-spread par-quote branch with a deterministic hand-calculated core test.

## Non-Goals

- No production API, binding, enum, serialization, or pricing behavior changes.
- No Excel example workbook or Excel surface changes.
- No quote-risk tutorial or large joint-calibration scaling matrix.
- No benchmark error-path cases.
- Do not add `xccy_perf` to the paired base/head regression gate in this PR; `master` has no matching target or case set.
- Do not treat noisy basket timings as regression thresholds.

## Functional Requirements

- **FR1 - C++ notional-mode example**: Add the executable target `xccy_reset_pricing` under `dal-cpp/examples/xccy_reset_pricing/` and register it through the existing examples CMake lists.
- **FR2 - Future-trade comparison**: The C++ example shall compute the par quote for a common future-start XCCY swap as `FIXED`, `RESETTABLE`, and `MARK_TO_MARKET` in one deliberately non-flat market. It shall print each mode, reset count, MTM-delta count, next domestic notional, and par quote.
- **FR3 - Started-trade snapshot**: The C++ example shall build a started MTM cashflow plan, obtain `RequiredHistoricalFixings`, construct one immutable `MarketFixingSnapshot_`, and compute the trade's par quote at an intraday valuation time. It shall print the required fixing identities/times and a finite par quote.
- **FR4 - Self-validation**: The C++ example shall exit non-zero unless all reported numeric values are finite and the following invariants hold: `FIXED` has zero resets and MTM deltas; `RESETTABLE` has one reset for every period after the first and zero MTM deltas; `MARK_TO_MARKET` has one reset and one MTM-delta slot for every period after the first. The deliberately non-flat fixture shall also produce pairwise-distinct par quotes, and the started trade shall consume at least one historical FX fixing and one historical fixing for each rate index.
- **FR5 - Python joint example**: Add `dal-python/examples/007.xccy_joint_calibration.py`. Using only the installed public Python surface, it shall adapt the known-convergent joint fixture in `dal-python/tests/test_xccy_joint.py` to build domestic and foreign curve declarations, a reset-aware XCCY basis declaration containing one started MTM trade, an explicit immutable fixing snapshot, and a `JointXccyCalibrationSpecBuilder_`, then call `CalibrateJointXccyMarket`. The script shall use fixed trade dates, positive fixing identities/timestamps/values, and an explicit quoted spread; it shall not depend on unbound plan, required-fixing, or precompute APIs.
- **FR6 - Python diagnostics**: The Python example shall print convergence, named parameter/residual ranges, FX forwards, and maximum absolute residual. Explicit failure checks that remain active under `python -O` shall terminate non-zero unless calibration converges; residual ranges are contiguous and sum to both the residual-vector size and Jacobian row count; parameter ranges are contiguous and sum to the Jacobian column count; FX forwards are finite; and the maximum residual is within the configured tolerance.
- **FR7 - Started MTM basket benchmark**: Populate the in-progress MTM pricing case with ten swaps sharing the original start `Date::AddMonths(today, -3)` and maturing at `Date::AddMonths(start, 12 * year)` for years 1 through 10. Before precomputing any rate, build one authoritative union snapshot from deduplicated fixing requests across all ten plans. Existing basket machinery shall emit exactly `XCCY in-progress MTM 10-instrument BASKET / pass` and `XCCY in-progress MTM 10-instrument PER-INSTRUMENT` as the two additional labels.
- **FR8 - Reset-aware staged benchmark**: Add one mixed reset-aware staged `ANALYTIC +DIAG` calibration row named `XCCY reset-aware basis ANALYTIC +DIAG (15 instruments, 5 knots)`. Its 15 instruments shall include one started MTM trade and future resettable/MTM trades quoted from a known market, and the dry run shall validate configured repricing tolerance and diagnostic dimensions.
- **FR9 - Stable benchmark matrix**: `xccy_perf` shall emit exactly 24 uniquely named rows. Existing 21 row labels and workloads shall remain unchanged.
- **FR10 - CI smoke execution**: Add `xccy_perf` to the head-only benchmark arrays in both `.github/workflows/cmake-linux.yml` and `.github/workflows/cmake-windows.yml`. The binary must run successfully and publish its normal output; it shall not be added to `.github/scripts/check_benchmark_regressions.py`.
- **FR11 - Domestic-spread test**: Add a Google Test in `dal-cpp/tests/curve/test_xccypricing.cpp` with `spreadOnForeignLeg_ == false` and a hand-calculated expected par quote. The test shall follow the repository's gtest-first, `TEST`, and `ASSERT_*` conventions.
- **FR12 - Current-state documentation**: Register the new C++ target in `dal-cpp/examples/CMakeLists.txt`; list the C++ workflow in `docs/methodology/xccy_calibration.md`; and list the Python workflow in `dal-python/README.md`. Documentation shall describe only the current surface and shall not include implementation history.

## Non-Functional Requirements

- **Performance**: Record one representative AADET wall-clock run from the repository root using `DAL_NUM_THREADS=4 /usr/bin/time -f %e build/dal-cpp/benchmarks/xccy_perf/xccy_perf`. The prior 21-row fixture took 7.61 seconds on the development host, so 15 seconds is an advisory investigation threshold on the same host/configuration, not a portable CI gate. CI validates successful completion only.
- **Determinism**: Fixtures shall use fixed dates, explicit calendars/conventions, deterministic quotes, and immutable fixing snapshots. No mutable global or shared test fixture state may be introduced.
- **Compatibility**: Existing example names, benchmark row labels, public bindings, and default pricing/calibration behavior must remain unchanged.
- **Portability**: New C++ sources and workflow edits must build on supported GCC, Clang, and MSVC configurations without platform-specific timing assumptions.
- **Differentiability**: The reset-aware staged analytic benchmark must use the existing analytic Jacobian path and must not introduce AAD tape state outside existing calibration ownership.

## Inputs and Outputs

| Surface | Inputs | Observable output |
|---------|--------|-------------------|
| `xccy_reset_pricing` | Fixed dates, USD/EUR curves, FX spot, three notional modes, started-trade observations | Mode comparison table, required fixings, finite started-trade result, exit status |
| `007.xccy_joint_calibration.py` | Domestic/foreign declarations, reset-aware XCCY instruments, fixing snapshot, solver options | Convergence, ranges, FX forwards, maximum residual, exit status |
| `xccy_perf` | Existing fixed fixtures plus one union-snapshot basket and one mixed staged spec | 24 stable benchmark rows and successful dry-run validation |
| Core unit test | Domestic-leg spread configuration and deterministic market | Hand-calculated par quote assertion |

## Acceptance Criteria

- [ ] The domestic-spread expected quote is derived independently from its cashflows, and the focused core pricing test passes without production-code changes.
- [ ] `bin/dal_cpp_tests --gtest_filter='XccyPricingTest.*'` passes, including the new domestic-spread case.
- [ ] `xccy_reset_pricing` builds, runs, prints both sections, validates its observations, and exits 0.
- [ ] `007.xccy_joint_calibration.py` runs against the built Python package under normal Python and `python -O`, prints the required diagnostics, validates its result with explicit checks, and exits 0.
- [ ] `xccy_perf` builds, completes its dry runs, emits exactly 24 unique rows, and exits 0.
- [ ] A `DAL_NUM_THREADS=4` AADET wall-clock run is recorded from the build-tree `xccy_perf` binary; any same-host result above the advisory 15-second threshold is investigated and reported rather than automatically failed.
- [ ] The Linux and Windows benchmark workflows list and execute `xccy_perf` as a head-only smoke target.
- [ ] `.github/scripts/check_benchmark_regressions.py` and its eight-target regression set remain unchanged.
- [ ] The full core/public/Python test workflow and documentation integrity check pass.
- [ ] Formatting and diff checks are clean, and a DAL reviewer reports no blocking findings.

## Open Questions

None. The balanced cross-language scope was approved on 2026-07-14.
