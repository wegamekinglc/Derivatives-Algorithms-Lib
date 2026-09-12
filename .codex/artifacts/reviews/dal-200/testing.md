# DAL-200 F3 independent testing

Independent Linux native verification passes. No production defect reproduced.
This is DAL-217's testing handoff for parent acceptance; independent review,
documentation reconciliation and the PR merge gate remain separate.

## Revisions and scope

- Delivered starting SHA: `1cf8e7657c97df9f81d5a33862ddeab80526b540`.
- Tested test-only SHA: `a53ebac33d6c928016ad84eae919e3ac353f47cb`.
- Existing draft PR: https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/369.
- Publish destination: `feature/dal-200-eq-observation-slots`. The dedicated
  local checkout branch is `agent/dal-tester/c52d988c526f`; publication uses
  `HEAD:feature/dal-200-eq-observation-slots`, preserving the existing PR.
- The report-only successor's exact pushed SHA is recorded in the final
  DAL-217 comment and attached publication record. Its code/test tree is the
  tested SHA above; no parent cherry-pick is needed after that push.
- Starting checkout was clean and matched the captured actual PR head.
  Both revisions descend from F2 `ec8b0072fbf70dab814a543edc625c8e0bf77efa`.
- Only `dal-cpp/tests/script/test_observation_simulation.cpp` and this report
  change. The two test commits add 14 cases and file-local seams; all existing
  assertions are retained. No production, public documentation, generated
  source, build/CI configuration or submodule pointer changes.

## Running existing tests

Fresh baseline verification on the delivered starting SHA passed:
**327/327** focused tests in nine suites (342 ms) and **1,648/1,648**
CTest cases (8.37 seconds). These are this tester's executions, not copied
implementer results.

The canonical Linux build configured, compiled, installed and tested the
workspace using `NUM_CORES=12 bash ./build_linux.sh > test_output.txt 2>&1`.
No root `test_output.txt` existed before the first run; that result was archived
as `baseline-full-linux.log` before deleting the root file for the final run.

## Authoring coverage

The 14 added tests target gaps in the retained observation and simulation
contracts. They exercise payment-specific numeraires, model-backed final-history
failure, Allocate/Init faults, worker-local read guards, both adapters' binding
and cross-sample validation, both today policies, future SPOT/FIX deduplication,
dead-branch non-finite history, task draining on invalid observations/payoffs,
unsupported named modes, stochastic legacy parity and structural/RNG barriers.

All 36 observation simulation tests at the first authoring boundary passed
(105 ms). A final explicit N=1 oracle was then added and passed its narrow
filter (1/1). Final verification on the committed test SHA passed **341/341**
focused tests in nine suites (411 ms), including all 37 observation simulation
tests, and **1,662/1,662** CTest cases (7.31 seconds). No test was skipped in
these runs.

The common fixture uses D=2026-09-12, H=2026-09-11 midnight,
F=2026-09-15 and P=2026-09-22 with EQ[DAL196_TEST], H=80.
Controlled deterministic expectations do not derive their expected values
from the evaluator. New deterministic comparisons use
`1e-12*max(1,abs(expected))` or tighter, and exact cancellation is checked
with `ASSERT_DOUBLE_EQ`. Fixed-path comparisons normalize aggregated sums
by 8,193 and use absolute tolerance 1e-8. Pool counts/date changes and the new
non-finite global fixing fixture restore singleton state through scoped guards.

In the matrix below, unqualified test names belong to
`ScriptObservationSimulationTest`. Preparation regressions belong to
`ScriptFixingPreparationTest`. Each row describes executed double/tree
coverage unless it explicitly identifies legacy AAD or a rejection check.

| ID  | Executed evidence                                                                                         | Result                                                                                                                                                                                                                                                     |
|-----|-----------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| T03 | `TestUniqueHistoryAcrossPathsAndThreads`; preparation deduplication regressions                           | 100 uses/event, 3 events, paths 1/257/8193 and threads 1/2/4: one global history and one virtual fixing; settled event excluded from PV.                                                                                                                   |
| T05 | `TestFutureOnlyNoHistory`                                                                                 | Controlled observation 123 with unrelated legacy spot 999 returns 123; global-source and empty explicit-snapshot variants perform zero observed history/fixing reads.                                                                                      |
| T06 | `TestMixedAndRepeatedHistory`; new fixed-path parity and worker tests                                     | The added single-path oracle checks N=1: H=80 plus F=120 returns 200; stochastic BS/Dupire comparisons use identical path numbering and normalized means.                                                                                                  |
| T09 | `TestMixedAndRepeatedHistory`                                                                             | Two H references return 160 at N=1 and over repeated paths with one virtual fixing, independently of the payment sample.                                                                                                                                   |
| T10 | `TestRetainedFutureFixing`; new `TestPaymentNumeraireIsIndependentOfRetainedObservation`                  | Non-event F=120 survives P=999 and a later event; repeated-F subtraction is exactly zero. Independent numeraires 2/4/5 give 120/4+120/5=54, not the observation-date discount.                                                                             |
| T11 | `TestLookAheadAndModeBarriers`; preparation lookahead regressions                                         | E<F rejects even in constant-false and expired branches before workers; E=F executes.                                                                                                                                                                      |
| T12 | `TestModelBindingsBeforeHistory`; new both-adapter and timeline tests                                     | BS/Dupire binding, duplicate/conflicting/unknown-asset and unsupported future-index barriers pass with zero history/workers; cross-sample identity validation and valid empty-output samples pass.                                                         |
| T17 | `TestPastPaysAndSameDateOrdering`; `PastEvaluatorTest.*`                                                  | Historical x=80 survives; 1,024 settled PAYS consume operands without cashflow; same-date x=1 then x=x+1 produces 2.                                                                                                                                       |
| T24 | `TestKnownFixingStillDiscounts`; new fixed-path parity test                                               | Independent BS oracle 80*exp(-0.05*10/DAYS_PER_YEAR) passes; BS/Dupire mixed-history payment parity also passes with nonzero rates.                                                                                                                        |
| T25 | `TestExpiredConfigurationAndNoWork`; new structure/RNG barrier test                                       | Expired PV/risk zero, history/Allocate/GeneratePath/submissions zero; empty, definitions-only and no-PAYS products explicitly fail.                                                                                                                        |
| T26 | `TestTodayZeroDimension`; `TestTodayLegacyModesAndRngBarrier`; new both-adapter today-policy test         | BS/Dupire Model=123 versus RequireHistorical=80; sobol/mrg32/irn x BB off/on pass; invalid RNG rejects before allocation/workers.                                                                                                                          |
| T27 | Legacy/compiled suites; new future-default and fixed-path parity tests                                    | Historical and future default-bound SPOT/FIX deduplicate; unbound historical and mixed SPOT fail. Both adapters agree with legacy tree/compiled at 8,193 fixed paths, RNG x BB x threads 1/4, tolerance 1e-8.                                              |
| T31 | New final-history/setup-failure, named-mode and numerical-draining tests; existing task-group regressions | Final virtual fixing failure after model setup submits zero tasks. Allocate/Init faults precede all history. Named compile/AAD/fuzzy rejects early. NaN in a dead branch and invalid payoff drain every accepted batch for 16,385 paths and threads 1/2/4. |
| T32 | `TestRetainedFutureFixing`; new `TestFrozenObservationStorageOnWorkers`; source inspection                | Preparation is moved, caller snapshot released and evaluation date changed; caller and each worker evaluation install rejecting history/fixing seams. 8,193 paths remain 200 with stable per-thread observation addresses and fixed sizes.                 |

Additional retained tests cover multiple historical EQ/delivery and inverse-FX
identities alongside one future EQ, immutable explicit snapshots with no global
fallback, exact midnight, non-finite/invalid model inputs, and the original
brace-default preparation API. Named AAD, compiled and fuzzy rejection is
evidence of a boundary, not successful implementation of those modes.

The final narrow test was `ScriptObservationSimulationTest.TestControlledSinglePathHistoricalOracles`.
It independently asserts both 200 and 160 with one generated path and one
virtual historical fixing call. Test commits are `3ad88045941f4f2d4af8245d03ccce3528f924ce`
and `a53ebac33d6c928016ad84eae919e3ac353f47cb`.

## Failure repair

No production repair was performed, and no behavioral RED/GREEN cycle is
claimed for these additional passing contract tests. The injected failures are
asserted error-handling cases.

One authoring build failed to link when a proposed test instantiated the
deferred `Index::Composite_`: its virtual methods have declarations without
definitions. The corrected test checks unsupported Composite script rejection;
it does not instantiate that unavailable type. IR capability is additionally
checked with a real `Index::DF_` object. The original link diagnostics and
successful corrected build are retained as `additions-build.log` and
`additions-build-fixed.log`. This was a test-authoring setup correction, not
a demonstrated F3 production defect.

## Commands and configuration

Commands ran from the repository root. Logs are attached to the DAL-217
comment in `dal-217-evidence.tar.gz`.

```bash
git submodule update --init --recursive dal-cpp/externals/googletest dal-cpp/externals/rapidjson dal-cpp/externals/machinist dal-cpp/externals/xad
NUM_CORES=12 bash ./build_linux.sh > test_output.txt 2>&1
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationSimulationTest.*:ScriptFixingPreparationTest.*:ScriptObservationTest.*:ScriptTest.*:SimulationTest.*:ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*:PastEvaluatorTest.*:ModelTest.*' > ../dal-217-evidence/baseline-focused.log 2>&1
clang-format -i dal-cpp/tests/script/test_observation_simulation.cpp
cmake --build build/Release-linux --target dal_cpp_tests -j12 > ../dal-217-evidence/additions-build-fixed.log 2>&1
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationSimulationTest.*' > ../dal-217-evidence/additions-suite.log 2>&1
cmake --build build/Release-linux --target dal_check_generated -j8 > ../dal-217-evidence/generated-check.log 2>&1
cmake --build build/Release-linux --target dal_cpp_tests -j12 > ../dal-217-evidence/single-path-build.log 2>&1
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationSimulationTest.TestControlledSinglePathHistoricalOracles' > ../dal-217-evidence/single-path.log 2>&1
rm test_output.txt
NUM_CORES=12 bash ./build_linux.sh > test_output.txt 2>&1
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationSimulationTest.*:ScriptFixingPreparationTest.*:ScriptObservationTest.*:ScriptTest.*:SimulationTest.*:ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*:PastEvaluatorTest.*:ModelTest.*' > ../dal-217-evidence/final-focused.log 2>&1
clang-format --dry-run --Werror dal-cpp/tests/script/test_observation_simulation.cpp
git diff --check
git diff --cached --name-status
git diff --cached --check
```

Final commands above pass. The first authoring build's link failure is separately
identified under Failure repair. The generated check passed with zero generated
files written; formatting and whitespace checks passed.

Configuration: Linux x86_64 on WSL2, GCC 15.2.0, CMake 4.2.3, Release,
native AADET (`DAL_USE_XAD_AAD`, `DAL_USE_CODIPACK_AAD`,
`DAL_USE_ADEPT_AAD` all OFF). Core C++, public C++ and portable Excel tests
are ON. Examples are ON and built; Python, benchmarks, native-architecture
tuning, sanitizers and coverage instrumentation are OFF. Pinned XAD source
was initialized for the examples; this does not establish an XAD-backend test
result. Full build logs retain existing Excel `#pragma once` warnings.

## Evidence limits and next stage

The worker observer is thread-local
(`dal-cpp/dal/indice/detail/fixingobserver.cpp`), so merely installing the
existing caller-side guard cannot cover a pool worker. The new wrapper installs
that guard inside every actual evaluation and checks per-thread storage.
There is no independent parser/environment-lookup or global-allocation hook in
this revision. Source inspection supplements the runtime seams:
`EvaluatorBase_::Visit(NodeFix_)` and bound `NodeSpot_` call
`ObservationPlan_::Read`, which uses only request IDs, sealed doubles and
sample/output offsets. That code does not call index methods, parse names or
access environments. Stable observation addresses/sizes are tested; no claim
of zero allocations throughout all evaluator/worker setup is made.

Today-only success is supplemented by `CreateRNG` inspection: it validates
the name and returns null before any RNG/BB construction when dimension is
zero. No global random-constructor counter was added. Unsupported IR/Composite
script forms are rejected during parsing/capability validation; complete
parsing or valuation support for them is not claimed.

Named AAD, compiled, fuzzy and public/archive/Python/Excel projection remain
deferred and unavailable in F3. The legacy AAD regressions run on native AADET
only. Windows/XLL, alternative AAD backends, Python/wheels, sanitizers, measured
coverage percentages and the full F9 cross-stage matrix were not executed.
Existing public/portable Excel regression passes do not establish named FIX
projection support.

Parent should reconcile this testing handoff, then explicitly promote the
existing DAL-218 documentation task and DAL-219 independent review in order.
No task is activated here except the authorized parent continuation. CI is
read once after publication, never watched or polled; that exact snapshot is
attached. No independent-review approval, F3 completion, merge or F4 activation
is claimed.
