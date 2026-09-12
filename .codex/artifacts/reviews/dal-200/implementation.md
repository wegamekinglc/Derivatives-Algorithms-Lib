# DAL-200 F3 implementation handoff

## Codacy remediation handoff, 2026-09-13

Before SHA: `5d578729386be2038552b73395ab14eef9e206e2`.
After production SHA: `061db30b28618b9f2806970791d4d4309aecc574`.
The following report-only commit records this verification without changing the
tested production tree. DAL-216's final comment and attached publication record
identify the exact delivered PR head. The branch remains
`feature/dal-200-eq-observation-slots`, draft PR
<https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/369>.

GitHub check run `103626628207` reported exactly three new cyclomatic-complexity
findings, at limit 8. The installed Lizard 1.23.0 reproduces all three original
counts. After the cohesive helper extractions:

- `dal-cpp/dal/model/base.hpp`: `Model_::ValidateTimeline` decreases **9 → 6**.
  Private `ValidateSampleOutputs` is **4**. Each sample's time check still
  precedes its output-count, parsed-index capability, and common-index checks,
  before advancing to the next sample. The same canonical name is carried
  between samples; all predicates and diagnostic messages are retained.
- `dal-cpp/dal/script/preparation.cpp`: `PreparedScriptBuilder_::ModelPlan`
  decreases **9 → 6**. Private `BindModelObservations` is **4**. Binding and
  request validation still precede construction of the sorted union timeline,
  payment-numeraire addresses, and retained fixing slots. Model Allocate/Init
  still precede history capture. The plan and its storage keep their ownership.
- `dal-cpp/dal/script/simulation.hpp`: `MCDoubleSimulation` decreases
  **10 → 7**. Internal `Detail::EvaluateDoubleBatch` is **4**. It reuses the
  existing worker state, seeks to the same first path, and keeps draw,
  GeneratePath, path validation, evaluation, payoff validation and summation
  in their original order. Capturing the existing `PathBatch_` by value replaces
  captures of its two fields. Task-group declaration, lifetime and draining,
  caller-side validation, expired fast return, zero-dimensional RNG behavior,
  tree/compiled selection and historical replay are preserved.

No behavioral RED is claimed for this pure refactor. The prior implementation's
RED/GREEN record below remains intact; fresh existing regressions validate this
repair. No tests, assertions, suppressions, complexity thresholds, analyzer/CI
configuration, public projection or deferred capabilities changed. The only
four changed files are the three production files above and this report.
There is no design deviation from the authorized repair.

### Commands and fresh results

Commands ran from the repository root. Logs are in the evidence archive attached
to DAL-216; native AADET, GCC 15.2.0, Release, public C++ and portable Excel
contracts enabled. Examples remain disabled, as in the original handoff.
All commands below exit 0.

```bash
gh api repos/wegamekinglc/Derivatives-Algorithms-Lib/check-runs/103626628207/annotations --paginate
git submodule update --init --recursive dal-cpp/externals/googletest dal-cpp/externals/rapidjson dal-cpp/externals/machinist
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF
cmake --build build/Release-linux -j12
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationSimulationTest.*:ScriptFixingPreparationTest.*:ScriptObservationTest.*:ScriptTest.*:SimulationTest.*:ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*:PastEvaluatorTest.*:ModelTest.*'
ctest --test-dir build/Release-linux --output-on-failure -j12
cmake --build build/Release-linux --target dal_check_generated -j8
lizard --version
lizard dal-cpp/dal/model/base.hpp dal-cpp/dal/script/preparation.cpp dal-cpp/dal/script/simulation.hpp
clang-format --dry-run --Werror --lines=36:57 dal-cpp/dal/model/base.hpp
clang-format --dry-run --Werror --lines=181:230 dal-cpp/dal/script/preparation.cpp
clang-format --dry-run --Werror --lines=189:208 --lines=278:296 dal-cpp/dal/script/simulation.hpp
git diff 5d578729386be2038552b73395ab14eef9e206e2 --check
git diff --cached --name-status
git diff --cached --check
```

The initial full build and the incremental rebuild after formatting both pass.
Focused verification passes **327/327** in nine suites (460 ms), adding all
`ModelTest` cases to the prior 287-test filter. Full CTest passes
**1,648/1,648**, 8.00 seconds. This includes retained-F/cancellation, history
read counts, model/mode barriers, settled PAYS replay, today-only RNG/BB,
legacy tree/compiled determinism across threads/requests, and task-draining
regressions. Generated-source checking passes with zero generated files written
and no submodule-pointer drift. Changed ranges are clang-formatted; whitespace
checks pass. Existing Excel source `#pragma once` warnings remain in the clean
build log; the build has no errors.

`complexity-before.log` and `complexity-after.log` preserve all Lizard output;
the six changed/new functions are each below limit 8. The unchanged AAD
`MCSimulation<AAD::Number_>` still measures 13 and is outside these three
reported findings and this repair's write scope. Lizard's default exit status
alone is not a limit-8 assertion; the per-function counts above are the local
evidence. No completed remote Codacy pass is inferred from it. Exactly one
non-blocking CI snapshot is taken after push and delivered in the evidence
archive; remote acceptance and independent F3 review remain outstanding.

The original commit used Cheng Li's GitHub noreply identity. The fresh checkout
had no Git author configured, so its first commit attempt failed without
creating a commit; repository-local configuration restored that existing
identity before the successful commit. No global Git configuration changed.

## Original implementation handoff

Implementation branch: `feature/dal-200-eq-observation-slots`, based on refreshed
`origin/master` at F2 squash `ec8b0072fbf70dab814a543edc625c8e0bf77efa`.
The containing commit is the implementation handoff. DAL-216's final comment
records the exact published SHA and draft PR. Independent tester, documentation,
and reviewer acceptance remain outstanding; this report does not accept F3.

## Behavior and design decisions

- The model-aware core `PrepareScript(data, modelPointer, valuation, simulation,
  snapshot, contract)` collects the original AST, checks naming/lookahead and
  model bindings, constructs the union timeline, and allocates/initializes the
  model before historical I/O. A new core `MCSimulation<T_>(data, modelData,
  paths, valuation, simulation, snapshot, contract)` uses this order. The model
  pointer makes its initialization explicit. The model-aware overload requires
  valuation and simulation arguments, preserving the original two/three-argument
  F2 calls with brace-initialized defaults. Existing model-data constructors
  and public/archive entry points retain their signatures.
- The F2 overload without a model retains its history-preparation semantics and
  explicit nonexpired execution rejection. Pricing does not call that overload
  and then attempt a late model-capability check.
- `ModelIndexBinding_` explicitly maps `spot` to one ordinary EQ. BS/Dupire
  advertise that capability and validate requested index outputs. Missing,
  conflicting, duplicate and unknown-asset bindings, future FX/delivery/second
  EQ requests, and malformed names fail before history and workers. Multiple
  historical EQ/delivery/FX requests coexist with the future EQ.
- The plan owns sorted unique sample dates, existing `DAYS_PER_YEAR` year
  fractions, requested output definitions, and a separate `eventToSample`
  payment mapping. Each AST observation receives a request ID resolving to a
  history value or `(sampleId, outputId)`. Values remain in the whole scenario
  until path evaluation ends. Reads perform checked integer addressing only.
- The plan has a stable heap address across `PreparedScript_` moves because the
  initialized model retains a pointer to its definition vector. Callers of the
  low-level model-aware preparation retain the prepared object while using that
  initialized model. Preparation exposes const product/plan access.
- Named tree double evaluates the unoptimized AST with sealed history. Past
  events replay once into a double initial state; historical PAYS evaluates and
  consumes its RHS without accumulating cash. Stable same-date statement order
  is preserved. Model-aware preparation accepts separate core contract settings
  for the legacy SPOT default identity; archive/public projection is deferred.
- Named double deliberately does not run the existing domain/constant-folding
  pipeline. This avoids introducing the parameter-dependency and fuzzy/AAD
  optimization work assigned to later stages. Named compiled/fuzzy and live
  prepared AAD execution explicitly reject. Legacy historical AAD also rejects
  instead of silently freezing parameter-dependent historical state. Valid
  future-only legacy SPOT tree/compiled/AAD entry points remain supported.
- Samples allocate only requested index outputs. Dupire supports empty forward
  arrays. Historical-only future payments retain payment numeraires and discount
  normally. Expired products validate structure, names, lookahead and ordinary
  configuration, then return zero PV/risk without history, model Allocate/path
  generation or accepted tasks; unused output capability is not required.
- Today-only paths use valid t=0 samples and no zero-dimensional RNG or BB.
  RNG names still validate before workers, including expired/zero-dimensional
  cases. Model domains and finite deterministic initialization are checked;
  non-finite generated spots/observations/numeraires and payoffs raise path
  errors through the existing draining task group.
- F2's virtual fixing, exact-midnight, today-policy, immutable explicit snapshot
  and no-global-fallback semantics remain intact. Global capture remains a
  sequential per-series snapshot; concurrent writes during capture are outside
  the established contract.

## RED then GREEN evidence

Configuration:

```bash
git submodule update --init --recursive dal-cpp/externals/googletest dal-cpp/externals/rapidjson dal-cpp/externals/machinist
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF
```

Native AADET, GCC 15.2.0, Release, public C++ and portable Excel tests enabled.
Examples were disabled after the initial default configure reported the absent
pinned XAD sources required by examples. No submodule pointer changed.

Each cycle built with
`cmake --build build/Release-linux --target dal_cpp_tests -j12` before running
`./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='<filter>'`.
Only a successful build was used for runtime RED/GREEN evidence.

- `ScriptObservationSimulationTest.TestFutureOnlyNoHistory`: RED build failed
  because `modelBindings_` and `MonteCarloSettings_` were absent. GREEN: 1/1.
  Final coverage also injects observation 123 with unrelated legacy spot 999,
  for both global-source and explicit-empty-snapshot preparation.
- `ScriptObservationSimulationTest.*` at the nine-test boundary stage: RED
  passed the first four value/addressing tests, then past PAYS overflowed the
  static stack and today-only RNG setup terminated with exit 139. A separate
  filter selecting `TestExpiredConfigurationAndNoWork`,
  `TestInvalidModelBeforeHistory`, and `TestDupireOutputCapability` failed 3/3:
  invalid RNG/spot were accepted and Dupire rejected the EQ output. GREEN: 9/9.
- `ScriptObservationSimulationTest.*:PastEvaluatorTest.TestUnboundHistoricalSpotRejected:ScriptTest.TestEventWithPastDate:ScriptTest.TestScriptProductWithPastDate`:
  RED 11 passed / 7 failed. Failures covered the old 30.0 placeholder, missing
  SPOT/FIX deduplication, expired/unbound guards, silent non-finite paths and
  overflowing deterministic model initialization. GREEN with the entire
  `PastEvaluatorTest.*` suite: 21/21.
- Filter selecting `ScriptObservationSimulationTest.TestUniqueHistoryAcrossPathsAndThreads`,
  `TestPreparationValidatesConfiguration`,
  `TestRawHistoricalDeadBranchSpotRejected`,
  `TestTodayLegacyModesAndRngBarrier`, and
  `TestRequestedOutputsAreValidatedByAdapter`: RED 1 passed / 4 failed. Invalid
  preparation RNG and raw dead-branch historical SPOT were accepted; invalid
  AAD RNG submitted one worker; the BS adapter accepted an FX output. GREEN
  for `ScriptObservationSimulationTest.*:PastEvaluatorTest.*`: 24/24.
- `ScriptObservationSimulationTest.TestHistoricalAadRejected`: RED 0 passed /
  1 failed because historical AAD silently ran. Its new pre-worker rejection
  passes in the final focused and full runs.
- `ScriptObservationSimulationTest.TestOriginalPreparationBraceDefaults`: RED
  0 passed / 1 failed because the new pointer overload intercepted the original
  `PrepareScript(data, {})` call and raised `InvalidModel`. Requiring both
  valuation and simulation arguments on the new overload restores both F2
  brace-default call forms; final focused/full runs pass.

Additional final assertions cover exact retained-F cancellation to zero with
F=120/P=999; H=80 + F=120 = 200; duplicate H=160 with one read; discounted
H-only payment at r=.05; 300 uses across three events and paths `{1,257,8193}`
with threads `{1,2,4}`; frozen historical EQ/delivery/inverse-FX data; all three
RNG names with both BB settings for today-only named double and legacy
double/AAD tree/compiled; and every accepted task completing before a path
error returns. Observation buffer addresses remain stable across repeated
evaluation. This is not a claim of a global allocator/performance benchmark.

## Fresh final verification

```bash
cmake --build build/Release-linux -j12
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationSimulationTest.*:ScriptFixingPreparationTest.*:ScriptObservationTest.*:ScriptTest.*:SimulationTest.*:ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*:PastEvaluatorTest.*'
ctest --test-dir build/Release-linux --output-on-failure -j12
cmake --build build/Release-linux --target dal_check_generated -j8
git diff --check
```

All commands exit 0. Focused: **287/287**, eight suites, 327 ms. Full CTest:
**1,648/1,648**, 7.29 seconds, including core, public C++ and portable Excel
contracts. Generated-source verification passes with no generated diff. Changed
ranges and the new test file were clang-formatted. The log archive attached to
DAL-216 preserves RED, GREEN, build, full CTest and generation outputs.

No named AAD/compiled integration, alternative AAD backend, Windows XLL,
Python, examples or performance acceptance is claimed. The independent tester
owns broader verification. CI is read once after publication; merge and overall
stage acceptance belong to the parent orchestrator.

## Changed-file scope

- `dal-cpp/dal/model/{base,blackscholes,dupire}.hpp`: explicit bindings,
  capability/domain/timeline validation and EQ output filling.
- `dal-cpp/dal/math/aad/sample.hpp`: requested output storage.
- `dal-cpp/dal/script/{settings,preparation,event,simulation}.{hpp,cpp}`:
  preparation order, settings, replay, timeline and MC integration.
- `dal-cpp/dal/script/{node,observationplan}.hpp` and
  `dal-cpp/dal/script/visitor/{evaluator,pastevaluator}.hpp`: observation IDs,
  integer reads and historical PAYS/legacy guards.
- `dal-cpp/tests/script/test_observation_simulation.cpp`: 23 focused F3 tests.
- `dal-cpp/tests/script/test_event.cpp` and
  `dal-cpp/tests/script/visitor/test_pastevaluator.cpp`: replace superseded
  placeholder expectations with required errors, retaining future-only cases.
- This implementation report. No published documentation, public/binding API,
  generated file, agent definition or submodule pointer is changed.
