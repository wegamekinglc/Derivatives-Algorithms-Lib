# DAL-201 / F4 independent revalidation

DAL-225, 2026-09-14. Fresh independent verification of the production batch
refactor passes on all four AAD backends. The hosted test-complexity finding
has a focused test-only repair, passing local static analysis. Hosted Codacy
success is not established by those local results. Documentation, independent
review and coordinator acceptance remain outstanding; this is not merge approval.

## Revisions and scope

- Starting published head: `9165369ed9313414f530d1c29f95cd5e00670e4e`.
- Starting tree: `68400c5dd25da463dd1e47ac76c18589deb3fcc6`.
- Final tested code: `bbaad1f646eb521eeb84806058c9e4685875d97b`.
- Tested tree: `53f38bd7b0b875ced047155f72f8c7608c15320c`.
- Published branch: `feature/dal-201-historical-aad-state`.
- Existing draft PR: https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/371.
- Base: `feature/dal-200-eq-observation-slots`; dependency #369.

The publication commit changes only this report after the tested code. Its exact
SHA/tree, remote and GitHub checks, and the single post-push CI snapshot are in
the attached `publication.json`. A commit cannot contain its own final SHA.
The only source change is `dal-cpp/tests/script/test_past_replay.cpp`.
No production, implementation report, docs/CHANGELOG, CI, benchmark policy,
submodule pointer, ignore rule or threshold changed.

Prior results from `260de4cc`, `203114ee`, `5b199ca0` and the implementer's
`5db67056` remain historical evidence in Git and prior issue attachments.
None are substituted for the fresh independent executions below.
All five earlier tester additions remain present.

## Running existing tests

Clean starting checkout, Linux x86_64, GCC 15.2.0, CMake 4.2.3, C++17.
Repository-pinned dependencies initialized with:

```bash
git submodule update --init --recursive
cmake --preset=Release-linux -S . -B build/Release-linux
cmake --build build/Release-linux --target dal_cpp_tests -j8
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptPastReplayTest.TestNonlinearHistoryAndParameterRepricing'
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptPastReplayTest.*:ScriptFixingPreparationTest.TestAad*:ScriptObservationSimulationTest.TestAad*'
```

On the unchanged starting head, the narrow test passed **1/1** (68 ms) and
the F4 baseline passed **16/16** (391 ms). Logs are
`native-baseline-narrow.log` and `native-baseline-f4.log`.
The pre-edit failure is static analysis; no behavior RED is claimed.

## Repairing the static failure

Captured the original hosted evidence without changing analyzer settings:

```bash
gh api repos/wegamekinglc/Derivatives-Algorithms-Lib/check-runs/103884485541/annotations
gh api repos/wegamekinglc/Derivatives-Algorithms-Lib/check-runs/103884485541
lizard -C 8 dal-cpp/tests/script/test_past_replay.cpp
```

At starting head, Codacy check `103884485541` is completed/action_required.
Its sole annotation identifies `test_past_replay.cpp:269`: method TEST has
cyclomatic complexity **9**, limit **8**. The previous production-function
finding is absent from this annotation; that absence does not mean the entire
Codacy check passed. Original JSON is included in the evidence.

Lizard **1.23.0**, using the identical command before and after the edit:

- Before: exit **1**, one warning; the nonlinear repricing test CCN **9**.
- After: exit **0**, no warnings; test CCN **5**, helper
  `CheckNonlinearHistoryRepricing` CCN **5**.
- Logs: `lizard-before.log`, `lizard-after.log`.

The helper contains the existing per-scenario script, analytic oracle and
all ten assertions. The TEST retains its date/pool RAII, trace and ordered
threads {1,2,4}, paths {1,257,8193}, SCALE {2,3,2}, K {159.95,160,160.05}
loops: **81 scenarios**, both double and AAD in each.
`ASSERT_NO_FATAL_FAILURE` propagates a helper assertion failure to the TEST,
preserving fail-fast behavior and scoped cleanup. Exception behavior is unchanged.

The preservation audit verifies all **59** existing assertion statements in the
file, all test names and all loop headers/order remain unchanged; only the
fatal-failure wrapper is added. Formatting adjusts line wrapping only.
Deterministic PV, analytic risks and pathwise comparison tolerances are retained
exactly. Narrow post-refactor execution passes **1/1** (69 ms).

Lizard's local parser/version is not asserted to match hosted Codacy's analyzer
environment. Local static GREEN is not hosted Codacy GREEN.

## Authoring new tests

No new cases were necessary for this behavior-preserving test refactor.
No cases were deleted, renamed or split. Existing coverage remains 16 F4 cases,
including the five independent additions from the earlier tester delivery.

## Final verification

The final source is the tested code/tree above. No source changed during final
verification. Initial alternative builds overlapped the edit; each backend was
rebuilt after the final commit before its regression run.

Native full build/install/core/public/portable-Excel verification:

```bash
NUM_CORES=8 bash ./build_linux.sh > test_output.txt 2>&1
```

There was no old root `test_output.txt` before invocation. Fresh captured output,
also delivered as `native-final-full.log`, ends with:

```text
100% tests passed, 0 tests failed out of 1715
Total Test time (real) = 9.01 sec
```

The build script excludes the benchmark label under its unchanged normal policy.

Alternative configurations and exact final commands:

```bash
cmake --preset=Release-linux -S . -B build/Adept -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_ADEPT_AAD=ON
cmake --preset=Release-linux -S . -B build/CoDiPack -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_CODIPACK_AAD=ON
cmake --preset=Release-linux -S . -B build/XAD -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_XAD_AAD=ON
cmake --build build/Adept --target dal_cpp_tests -j4
cmake --build build/CoDiPack --target dal_cpp_tests -j4
cmake --build build/XAD --target dal_cpp_tests -j4
./build/Adept/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
./build/CoDiPack/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
./build/XAD/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
```

- Adept fork, CMake version 4.1.1, commit
  `1e29edc6e16f969e99145f0cbff34ff0de5fe699`: **425/425**, 1131 ms.
- CoDiPack 3.1.0, commit
  `86b94d3f3c3b6659a36f8e640945a7ebe1884a4a`: **425/425**, 1277 ms.
- XAD 2.1.0-dev, commit
  `ca0146061726745870aac71f3108c7d14129d1b3`: **424/424**, 2923 ms.

All commands passed, with no failed/skipped cases. Counts differ because of
backend-specific tests. The final regression logs, configure/build logs,
compiler/CMake/Lizard versions and pinned submodule SHAs are attached.
`f4-coverage-audit.json` confirms each of the same 16 F4 test names passed in
all four final logs, rather than inferring coverage from aggregate counts.

## All 16 F4 cases and acceptance mapping

The first thirteen cases are in `ScriptPastReplayTest`:

1. `TestParameterRisk`: T18 analytic discounted PV, SCALE/rate sensitivities,
   zero spot/vol risk, model/script-only risk count.
2. `TestDirectSeedPayoff`: T19 mark-before-payoff seed, d_SCALE=80 over 8193 paths.
3. `TestConstantPayoffAndEmptySuffix`: T19 constant payoff and zero-dimensional
   suffix, threads 1/2/4, direct repeated seed/constant PayoffRoot propagation.
4. `TestHardPastDecision`: T20 >, >=, = on both sides and at equality; hard
   historical choice and selected arithmetic risk.
5. `TestAadBatchLifetime`: T23 paths 1/257/8193, threads 1/2/4, fixing 80/90/80,
   repeated pricing, direct seed and discounted risk.
6. `TestEveryPathRebuildOracle`: T23 BS/Dupire independent recording per path
   versus mark reuse, 257 fixed paths and every labelled risk.
7. `TestFutureKnownFixingKeepsFuzzyRisk`: future fuzzy primal/K/SCALE risk
   compared with analytic values and fuzzy-double evaluation.
8. `TestExpiredAadSkipsExecutionModeAndHistory`: expired zero result/risks
   with either compiled request.
9. `TestPreparedAadRejectsChangedSmoothing`: prepared AAD smoothing mismatch
   explicitly rejects.
10. `TestNonlinearHistoryAndParameterRepricing`: refactored case, all 81
    scenarios and ten assertions retained, including zero hard-switch K risk.
11. `TestTodayPolicyPreservesHistoricalAndModelRisk`: historical H plus today's
    Model/RequireHistorical choice, threads 1/2/4, three RNGs and both bridges.
12. `TestEveryPathRebuildAcrossBatchBoundary`: nonlinear seed/future/payoff,
    BS and Dupire, 8193 fixed paths, threads 1/2/4, every risk/PV within 1e-8.
13. `TestHistoricalSeedFailureDrainsAndRebuildsEveryBatch`: failure after
    historical replay and before mark, 16385 paths, all batches drained,
    zero paths on failure, then two successful valuations at threads 1/2/4.

The remaining cases are:

14. `ScriptObservationSimulationTest.TestAadRepricingRefreshesGlobalHistoryAndModelInputs`:
    global history 80/90/80 and rate 0.03/0.07/0.03, threads 1/2/4, 8193 paths,
    exactly one History and one Fixing read on every fresh valuation.
15. `ScriptFixingPreparationTest.TestAadUniqueHistoryAndNoHotPathLookup`:
    T03/T32, 100 historical uses over three events, both modes and every
    requested path/thread combination. Worker history/fixing observer seams
    throw if accessed; observation address/size stays stable.
16. `ScriptFixingPreparationTest.TestAadPreparationFailureAndPathDrain`:
    T31, preparation/history/model/compiled failures submit zero workers;
    path failures drain all batches and subsequent valuation recovers.

The broader regression also covers raw historical AAD rejection, named compiled
rejection before model setup/history, legacy tree/compiled dispatch, invalid
model-before-history order, last-history failure, path diagnostic order and
16385-path numerical-error drains. Legacy double zero-path and BatchPlan tests
pass. Prepared/named positive-path guards and prepared AAD mode guards remain
unchanged by direct source comparison; no additional exhaustive error-order
matrix or new zero-path AAD case is claimed.

## Production refactor inspection and remaining limits

Compared the original and extracted runner, including full helper bodies:
validation order and error literals are retained; optional compiled engagement
matches the old dispatch; active model/evaluator/seed/zero stay in the worker.
Input registration and NewRecording precede model initialization/history replay,
then Mark. Each path rewinds, generates/evaluates, constructs its payoff root and
propagates to Mark. Mark-to-start propagation precedes risk harvesting; risks
are divided by total paths once. The task group drains before captured settings,
prepared state and result storage are destroyed. Batch offsets and reduction
order are unchanged. The independent analytic/per-path/backend tests above
found no changed result or lifecycle behavior.

Model-aware prepared AAD tree supports matching AAD mode and smoothing.
Named/prepared AAD compiled remains explicitly rejected; compiled historical
parity belongs to F5. The implementation report has corrected this distinction.
`docs/methodology/script_engine.md:119` and the unsupported-execution section
still carry obsolete blanket rejection language; DAL-226 owns that correction.

No Python, Windows XLL, sanitizer, performance, full alternative-backend
public/Excel, or F5 compiled-parity runs are claimed. No coverage percentage,
general allocation profiler result or proof of absence of cross-thread active
values is claimed. Runtime evidence and source inspection are the stated limits.
F3 performance remains deferred under DAL-223.

The final issue attachments record the single post-push CI snapshot without
watching or polling. Hosted Codacy/required checks, DAL-226 and DAL-227 remain
separate acceptance steps. DAL-225 hands this revalidation back in_review.
