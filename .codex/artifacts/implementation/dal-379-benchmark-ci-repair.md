DAL-379 PR #602 benchmark migration and Codacy repair. Baseline:
`0b5b675556287b9565564bea779a53dd0d879b77`, branch
`agent/dal-orchestrator/9f36be3b414c`, target `master`. The delivery comment records
the final commit. Existing validation/review records remain active and unchanged.

## Call inventory and precision

- `dal-cpp/benchmarks/pde_perf/pde_perf.cpp`: the two evaluations in the four
  `--early-exercise` cases already use the new core API. Workloads, output names,
  two warmups and ten timed repetitions are unchanged. Its default European
  `ThetaScheme_` cases are outside the early-exercise migration.
- `dal-cpp/examples/methods/american_put_mc/american_put_mc.cpp`: replace the
  embedded CN-then-projection reference with the core American LCP API. Preserve
  spot/strike=100, vol=.20, r=.05, q=0, evaluation 2026-09-20, maturity
  2028-03-20, upper=300, 601 nodes, 600 ordinary steps, explicit theta=.5 and
  no Rannacher damping. Explicit payload=256 MiB and work=1e9 allow the new
  planner's monotonicity subdivisions and complete exercise-region output.
  CLI, target, table columns and independent MC/analytic checks are unchanged.
- `dal-python/benchmarks/dal_comparisons/exercise_pde.py`: replace the NumPy
  tridiagonal implementation with public `EarlyExercisePdeResult_New`, returning
  its owning grid/values for the existing interpolation/cache protocol. Queries
  still use the same spot grid and actual bumped spots. The surface request's
  canonical spot=strike, explicit upper=4*strike, and deterministic carry preserve
  the spot-independent Bermudan surface; the public request never uses auto upper.
- Its two direct callers are `exercise_scenarios.pde_surface` (comparison PV and
  all Delta/Vega/Rho bumps) and `lsmc_rqmc_coverage.bermudan_put_pde`. The latter
  also serves `compare_lsmc_policy_sensitivities.case_reference` and its Greek
  bumps. Exact dates replace floating exercise times. Comparison schedules remain
  91-day/7-day exercise over 546 days from 2025-01-15; weekly “American” rows
  remain **Bermudan on those dates**, matching the MC approximation. RQMC/policy
  dates remain 2026-09-20, 2027-09-20 and 2028-03-20. Rate, yield, strike,
  volatility and spatial grids are unchanged.

There is no separate C++ facade benchmark directory. Python benchmarks call the
public binding; core benchmarks/examples include only core headers and retain
their original component/target. `script_mc_perf` has early-exercise MC workloads
but no PDE call to migrate. The independent test-support `bermudan_pde.hpp`
and all its test callers remain independent and unchanged.

**Explicit numerical configuration adjustment:** preserving dense legacy CN at
2401 nodes fails the new strict linear-error check on its first step: residual
`6.319554597791948e-6`, limit `1e-12`, time index 279551. The identical request
fails identically on the immutable pre-repair Python binary. No tolerance or
failure rule is changed. Python reference drivers uniformly request the existing
default fully implicit method with eight times the requested temporal density.
For unequal intervals the global step count is eight times the maximum integer
ceil(steps_i * total_days / interval_days_i), so every interval meets at least
eight times its original minimum. Fine/coarse comparison densities are 32/16
steps/day, with the original 2401/1201 spatial nodes. Original independent CRR,
European-limit, PV and Greek convergence tolerances all pass unchanged.
This is a prescribed benchmark configuration, with no automatic method fallback.
Driver limits are explicitly 1e6 steps, 1e10 work units, 1 GiB payload and
512 MiB workspace. These execution allowances do not relax numerical precision.

## Codacy and numerical preservation

Read all 13 annotations of check `117641033442`. Split Plan into anchor selection,
coefficient bounds, base allocation and stable emission; split boundary candidate
enumeration, triangular sweeps, backward-error normalization, request/settings
validation and rollback assembly/solve/projection/diagnostics. All changed
functions pass the original complexity <=8 and length <=200 limits using Lizard.
No rule, threshold, enum, public header, default, residual definition or failure
publication contract changed. Ten double scratch arrays and region reservations
are preserved; a private pointer/reference context adds 88 bytes to the measured
workspace ledger. Planned budget envelopes remain unchanged.

Exercise style specializes the private rollback loop. The three triangular/error
kernels and their calling loop retain inline visibility of penalty/shift constants.
Immutable per-step operands and owning-buffer descriptors fit the existing fixed
scalar allowance (compile-time size bounds). Row assembly returns its own success
flag, rather than reloading an indirect byte status after each array write; the
first failed row still publishes the same failure before rollback stops.

The CRR helper still computes its own tree and payoff and strictly rejects invalid
probabilities with REQUIRE. The public small-LCP test still independently assembles
and enumerates all eight active sets; extraction retains every original assertion.
Excel parsing is separated by contract/market/carry/date responsibilities, and the
OPER validator shares one scalar-type predicate without broadening accepted types.

The CWE-120/20 annotation resolves to the templated callback named `read`, not a
buffer-reading syscall. Its two matrix accesses follow the explicit two-column
check and `0 <= row < Rows()` loop. Rename the typed callback to `consumeRow`;
no bounds checks are removed. Flawfinder 2.0.20 reproduces the old identifier hits
at risk level 1 and finds no hits in the repaired file. Generated markup/stubs
have no content changes, so prior Machinist consistency evidence is reused.

## RED and GREEN evidence

- RED: `pytest .../test_exercise_comparisons.py -k test_benchmark_pde_uses_public_api_and_exact_exercise_dates`
  fails because the original benchmark never calls the public API. The added
  call/parameter test and the unequal-interval/date test now pass.
- RED: Lizard `-C 8 -L 200 -w` reproduces the original complexity/length findings.
  GREEN: the same limits pass all affected C++ files and the new Python adapter.
- RED/GREEN: `python3 -m flawfinder --error-level=1 --minlevel=1` on the original
  Excel file exits 1; the repaired source exits 0 with zero hits.
- Dense-CN migration RED: three original convergence/European tests reject the
  native failure. Fully implicit temporal refinement passes their unchanged
  independent bounds; the original failure and baseline reproduction logs remain.

Final targeted commands (Release, GCC 15.2, C++17, native AAD, Eigen/native-arch
OFF, static core/public, CPython 3.13.9, pybind11 3.0.4):

```sh
cmake --build build/dal379-core-boundary --target pde_perf american_put_mc -j 8
cmake --build build/dal379-implementation --target dal_cpp_tests dal_public_tests dal_excel_portable_tests _dal -j 8
ctest --test-dir build/dal379-implementation --output-on-failure \
  -R '^(EarlyExercisePdeTest|PublicEarlyExercisePdeTest|ExcelEarlyExercisePdeTest|ExcelEuropeanPdeRiskTest)\.|^ScriptExcelContractTest\.TestProductSettingsRejectBadRows$'
PYTHONPATH=build/dal379-implementation/dal-python DAL_NUM_THREADS=4 python3 -m pytest dal-python/tests/test_early_exercise_pde.py -q
PYTHONPATH=build/dal379-implementation/dal-python DAL_NUM_THREADS=4 python3 -m pytest \
  dal-python/benchmarks/tests/test_exercise_comparisons.py \
  -k 'test_pde or test_benchmark_pde or test_rqmc_reference or test_lattice' -q
DAL_NUM_THREADS=4 build/dal379-core-boundary/dal-cpp/examples/methods/american_put_mc/american_put_mc 131072 16384
python3 .github/scripts/check_docs.py
```

34 CTest entries, 11 Python binding tests and 20 benchmark tests pass. The core
configuration has `DAL_BUILD_PUBLIC=OFF`, examples/benchmarks ON; actual builds
and linking succeed without public. The example completes all existing checks
and reports American PDE=7.0264. Documentation checks pass 73 Markdown files.
Linux SDK-declaration-shim syntax checking of existing raw OPER tests passes;
it is not Windows compilation/runtime evidence.

Actual protocol smokes also complete successfully (reduced budgets are functional
smokes, not accuracy or performance acceptance samples):

```sh
PYTHONPATH=build/dal379-implementation/dal-python DAL_NUM_THREADS=4 python3 \
  dal-python/benchmarks/lsmc_rqmc_coverage.py --outer-seeds 2 --replicates 2 \
  --training-paths 64 --pricing-paths 64 --pde-grid 101 --pde-steps 50
PYTHONPATH=build/dal379-implementation/dal-python DAL_NUM_THREADS=4 python3 \
  dal-python/benchmarks/compare_lsmc_policy_sensitivities.py --cases bermudan_sparse \
  --pricing-paths 128 --replicates 2 --seeds 17 41 --pde-grid 101 --pde-steps 50
```

Eight full before/after public result snapshots (including exact hexadecimal
floating values, dates, all steps/regions, and WORK_LIMIT/MAX_ITERATIONS/linear
failures) match except for the accounted +88 workspace bytes. Final A/B/CN-A
execution retains respectively 500/500/31876 steps, region capacities
100500/200500/6407076, payloads 2141144/4147544/136053912 bytes, and measured
workspace 53232/85232/1308272 bytes, all within the unchanged plans. The core
tests also recheck exact/one-byte-short budgets, failures and independent prices.

## Performance and remaining boundaries

Only the four changed native early-exercise paths are sampled. Baseline is the
named pre-repair commit, built before source edits and retained as an immutable
static binary snapshot with its CMake cache; head uses the same Release/core-only
configuration. No new source worktree was created, per the task constraint.
Every round alternates base/head first, uses ten process samples per side, takes
the minimum, and retains the strict 4% threshold in both confirmation rounds.
The benchmark itself retains two warmups and ten repetitions. Binary hashes and
all raw outputs are retained. Shared-host measurements cannot guarantee an idle
machine; no other owned build runs during the final accepted sampling.

Initial samples correctly detect Bermudan/Daily regressions. Inlining the
assembly boundary reduces them but does not meet the gate. Retained results
include all failed variants. A diagnostic phase probe (100 identical Bermudan
calls) located most overhead in assembly: old 130 ms versus extracted 171 ms.
Restoring constant propagation through the sweep kernels, caching fixed operands
and buffers, and returning row success directly resolve it without numeric changes.

Final paired results (milliseconds, combined best of 20; round deltas each use
their own best of ten):

| Case     | Base min | Head min | Round 1 delta | Round 2 delta | Verdict     |
|----------|----------|----------|---------------|---------------|-------------|
| Bermudan | 4.526    | 4.041    | -10.716%      | -11.162%      | improvement |
| American | 12.178   | 11.462   | -6.402%       | -5.797%       | improvement |
| Daily    | 37.889   | 38.436   | +2.183%       | +0.029%       | no change   |
| NearZero | 0.650762 | 0.620469 | -4.655%       | -4.559%       | improvement |

Overall: **no regression** in the selected four paths under the unchanged gate.
The first failures are preserved separately, not relabeled as noise or omitted.

The pre-push snapshot of `0b5b6755` has no running checks and no failed checks
other than Codacy ACTION_REQUIRED. This is historical evidence, not acceptance
of the repair head. New-head CI/Codacy and review remain the orchestrator's gate.

Unchanged European kernels, AAD/MC/RNG/calibration matrices and independent
test-support code are excluded; prior valid evidence is reused, not reported as
new runs. Full shared coverage and the baseline mixed-link AAD issue are outside
this repair. Windows/native OPER execution and exact-head required checks remain
CI boundaries; local shim/portable checks do not substitute. No other roles are
triggered; the orchestrator owns independent follow-up and merging. Parent issue
remains `in_progress`.
