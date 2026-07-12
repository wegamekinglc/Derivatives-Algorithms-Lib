# Yield-Curve Instrument Pricing Performance - Whole-Branch Code Review

- Date: 2026-07-12
- Range reviewed: `401d5b7a..2b849146`
- Head reviewed: `2b849146`
- Mode: read-only re-review of the supplied updated whole-feature diff and prior review fixes

## Findings

### Critical

None.

### Important

None.

### Minor

None.

## Resolved Findings

1. **Resolved - the implementation plan now clearly marks its embedded source as historical and non-copyable.**
   - A prominent warning immediately before the stale source block states that it is a historical pre-calibration snapshot, is neither authoritative nor copyable, and directs readers to the final benchmark source at `docs/superpowers/plans/2026-07-12-ycinstrument-pricing-performance.md:92-98`.
   - The warning explicitly identifies the required `dal/curve/ycimp.hpp` include and measured per-case operation and basket-pass counts, which are present in the authoritative source at `dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:13-39,276-318`.

2. **Resolved - the design now distinguishes nominal noise guidance from current measured evidence.**
   - `docs/superpowers/specs/2026-07-12-ycinstrument-pricing-performance-design.md:127-133` now says 2-4% is only a nominal expectation for quiet paired runs, records the current 3.59%-46.42% unpinned-VM spread, links the performance artifact, and requires conclusions to use the run-specific measured noise floor.
   - The stated range matches the ten-run evidence at `.superpowers/sdd/task-3-verification-report.md:64-91` and the performance review at `.codex/artifacts/perf/ycinstrument-pricing-performance.md:13-30`.

## Strengths

- **Pricing and routing fixture:** the benchmark builds a map-backed `CurveBlock_` with an OIS discount curve and base-layered 3M/6M forwards before timing at `dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:88-105`. The seven representative contracts exercise discount, projection, alias, convexity, and two-tenor basis pricing at `dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:107-126`.
- **Untimed correctness gates:** finite values, projection-vs-discount routing, Future convexity, STIR/FRA inheritance, OISSwap/Swap inheritance, and BasisSwap 6M sensitivity are checked before measurement at `dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:129-166`.
- **Timing isolation:** PRECOMPUTE deliberately measures rate-handle lifecycle replacement, while PRICE constructs one rate before `Bench::Run` and measures only repeated `Rate_::operator()` calls at `dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:185-212`. Basket instruments and rates are likewise built before timing at `dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:257-263,293-309`.
- **Normalization and stable output:** normalized median/min/max preserve the ten-sample count at `dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:180-183`. Seven phase pairs plus three basket/per-instrument pairs produce exactly 20 stable labels at `dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:168-177,288-318`; BasisSwap labels explicitly contain `PASSIVE`.
- **Dead-code protection:** every PRICE and basket measurement accumulates a checked data-dependent checksum and calls `Bench::DoNotOptimize`; PRECOMPUTE retains and validates the final handle at `dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:185-212,276-290`.
- **Cross-platform integration:** the standard optional AAD libraries, non-MSVC pthread linkage, and install rule are present at `dal-cpp/benchmarks/ycinstrument_perf/CMakeLists.txt:1-23`. The target is registered immediately before `curve_calibration_perf` in CMake, Linux, and Windows at `dal-cpp/benchmarks/CMakeLists.txt:16-17`, `.github/workflows/cmake-linux.yml:418-419`, and `.github/workflows/cmake-windows.yml:255-256`.
- **Report-only boundary:** the workflows run and publish the executable, while `.github/scripts/check_benchmark_regressions.py` remains unchanged and contains no `ycinstrument_perf` entry.
- **Scope discipline:** the feature range changes only the benchmark target/source, the two reporting arrays, and its design/implementation-plan documents. No production pricing, calibration, shared benchmark-harness, or test source changed.

## Verification

Fresh re-review checks on `2b849146`:

- `python3 .github/scripts/check_docs.py` - 31 Markdown files passed.
- Direct assertions for the historical/non-copyable warning, `ycimp.hpp` and calibrated-count guidance, nominal-noise wording, measured 3.59%-46.42% range, and run-specific conclusion rule - passed.
- `git diff --name-status ff258a70..2b849146` - exactly the implementation-plan and design documents changed.
- `git diff --check 401d5b7a..2b849146` - passed.

The immediately preceding whole-branch gate remains valid because the review-fix commit is documentation-only:

- `ycinstrument_perf` and `dal_cpp_tests` built successfully.
- One executable run produced 20 unique measured labels, all reporting 10 repetitions.
- Focused `YCInstrumentTest.*` passed 11/11; full core tests passed 876/876.
- `clang-format --dry-run --Werror -sort-includes=0` passed on the new source; `platform.hpp` remains first at `dal-cpp/benchmarks/ycinstrument_perf/ycinstrument_perf.cpp:5`.
- Occurrence checks found exactly one target entry in CMake and each workflow, and zero entries in the calibrated regression script.

Residual risk: Windows and alternate-AAD-backend compilation were not reproduced locally during this review; the CMake pattern matches existing benchmark targets and the workflow additions provide those cross-platform gates. The executable is new on this branch, so there is no master baseline from which to make a branch-vs-baseline performance conclusion.

## Verdict

Approve

No Critical, Important, or Minor findings remain. Both prior Minor findings are resolved in `2b849146`.
