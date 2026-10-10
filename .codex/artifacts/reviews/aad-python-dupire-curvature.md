# Python Dupire curvature local review

Verdict: Approve locally. Fresh exact-head remote publication gates remain.

## Findings

Resolved P1 at `dal-cpp/dal/script/simulation.hpp:331`: the cancelled branch of
`RunSimulationBatches` retained its RNG capture until
the packaged task is destroyed, after its future can become ready. The existing
GCC 13 cancellation test observes one live clone after return; local repetition
fails on iteration 29 with the same seek/four-thread trace. Release that capture
before returning from the cancelled branch, retaining the zero-live assertion.
The repair resets the capture before returning. The original assertion passes
1,000 repetitions, and all 15 tests in the batch source pass. Dependent official
PIC libraries and the installed Python module are rebuilt; ten affected Python
tests and six OFF/combined strict probes pass. Source/header/object/module hashes
tie the fresh executable evidence to the sole native-header change.

No remaining actionable findings. Six successful-call cases cover both helper
call sites, tree/compiled and the separate weighted objective, with 32,785 paths,
four workers and a Sobol control. Two new Dupire costs also refresh. All 320
observations pass numeric checks. A further 40 observations confirm only the
noisy passive/tree case; no comparable case exceeds +4% in both rounds. Total
measured time is 14.075691852 seconds. The passive/tree minima remain noisy:
initial -8.55/+5.73%, then +6.39/+2.96%; this supports no sustained slowdown under
the stated rule, without claiming a speedup or exact performance equivalence.

Codacy's duplicate local cost-function name is resolved by renaming only the
weighted closure/reference. Whole-file AST equality after normalizing those two
names proves all operations and oracles unchanged. Only weighted/compiled repeats
forty paired samples (0.980116822 measured seconds, +7.78/-1.46% round minima).
Original samples remain; the full record contains 400 observations and
15.055808674 measured seconds. All native/header/module identities are retained.

The earlier binding-only review has no correctness, ownership or compatibility
findings from accepted #528 (`2f1f6f9008b6a0d9e800e718b5eaca5a2d9d4019`).
The full new binding, module/CMake registration, new tests, active specification,
API/critique, documentation and ledger changes were read. Both native libraries
remain unchanged in that earlier binding-only snapshot.

The constructor requires copied native matrices and strict numeric sequences;
budgets preserve optional zero. Native planning retains quote-dependent dimension,
finite perturbation, nonzero-row and combined payload validation. All factories
copy Python values before GIL release. Getters return passive copies; no active
number, callback or borrowed request/result state escapes. Method labels retain
the finite-step estimator, full raw axes and separate reported base projection.

The review identified missing direct `<utility>` inclusion before publication;
it is present in the final binding, followed by a rebuilt/installed extension and
strict checks. The draft scope initially described all-zero rows as accepted;
reading the native validator corrected the scope before implementation. The
planner rejects such rows, while zero-row matrices retain base-only semantics.

## Open questions

None required for this bounded Python projection.

## Tests

RED: the existing installed Python package raises `AttributeError` for the absent
public bump-request constructor. GREEN: 61 new cases pass. Installed-package
acceptance passes all 174 selected new/Dupire-risk/Dupire-request cases in 1.85
seconds, including analytic signed Gamma, independently recalibrated mixed
surface/direct products, exact and failing budgets, copied values, frozen history
and dates, repeated calls, GIL release and two calling threads. Four strict
OFF/combined module/binding checks pass. Documentation checks pass for 166 files.

Two scoped entry-cost cases complete eighty paired observations in 3.465331108
measured seconds. Both modes pass analytic checks and preserve fixed-affinity,
one-worker, two-round/ten-pair sampling. Native complete admission is more
expensive than manual first-order composition; this informational comparison
does not establish a regression or speedup. Source/module/library/header hashes
are retained, with no unrelated native timing repeated.

Codacy's three cost-driver assertion findings are repaired with explicit checks
that remain active under optimized Python. The same two affected costs have
eighty refreshed observations (3.568148566 measured seconds); original evidence
is retained. Native and binding objects/tests are unchanged, so no correctness
suite repeats. Maximum cost-function complexity remains six.

Windows previously hid successful Python case output behind one CTest summary.
Exclude `dal_python_pytest` from the ordinary CTest selector and execute that
same test once with verbose output and a no-tests error. The selected test set
is unchanged and the two selectors are disjoint; no test is added or repeated.
Linux extended jobs already enable verbose CTest output. The explicit case-log
gap justifies this narrow CI change, with fresh publication-head checks required.

Remote extended jobs and wheel jobs expose two test-only portability findings.
The tight independently recalibrated HVP comparison amplifies native multi-worker
reduction-order noise. Run that oracle in the existing one-worker subprocess
helper, retaining its original `rel=1e-10, abs=1e-8` bounds, points and paths.
Extend the helper with an optional module path; existing callers keep their default.
Separately, repeated/selected/concurrent results use row-wise `rel=1e-12,
abs=1e-10` HVP comparisons and `abs=1e-12` gradient comparisons rather than exact
floating-point equality. Detached-copy assertions remain exact. Analytic Gamma
bounds, GIL release and independent calling-thread execution remain covered.

Repair acceptance repeats only the six affected new cases and four existing
helper callers: 10/10 pass in 1.21 seconds with four native workers in the parent.
The initial 174-case installed run remains historical evidence; it is not claimed
as a fresh run of the repaired test sources. Production sources, installed module,
native libraries and timing driver were unchanged for that test-only repair.
The later cancellation repair refreshes their affected paths as recorded above.
Fresh remote checks must establish cross-platform acceptance.

The original non-PIC static archive cannot link a Python shared extension; its
failed link is retained. Fresh official PIC core/public builds and standalone
extension installation resolve the configuration boundary. That necessary
library build adds no full-suite or full-performance execution.

## Summary and residual risk

The additive Python surface projects the accepted native financial adapter.
Do not treat local OFF runtime plus combined syntax as remote diagnostic/platform
acceptance. Inspect actual new Python test execution in the extended and Windows
profiles, all required CI/Codacy and complete review bodies before guarded merge.
Excel/rate/other structured surfaces remain separate roadmap work.
