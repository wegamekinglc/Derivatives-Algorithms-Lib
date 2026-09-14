# DAL-201 / F4 implementation handoff

Implemented by DAL-224 on 2026-09-14. The complexity remediation below supersedes
the initial implementer handoff. DAL-225 completed independent testing on the
preceding code; its revalidation of this refactor, documentation (DAL-226), and
review (DAL-227) remain required before the parent accepts F4.

## Complexity remediation after independent testing

Starting published head: `5b199ca0917d5c8cacb544114668d0e919137e53`, tree
`441dd13639b702e04822309b8af3e35753af71b2`. Tester commit
`203114eebf6303c7ea7513ed32516d8689fb3f33` is an ancestor; all its tests remain
byte-identical. The supplied testing archive was downloaded through Multica and
verified as SHA256
`064a6fb45c7e07b7be8609f4b9d8967a942ad87e5fce4bfdeb6be1d6766ec445`.

Tested refactor commit: `5db6705672424699d56f72a8ee76cc56a6533d29`, tree
`705985ed9cae6c3a00ead2a2d9a34f069ff790f2`. The publication commit changes only
this report; the attached `publication.json` records its exact SHA/tree. The
existing draft PR remains
https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/371 on
`feature/dal-201-historical-aad-state`, stacked on
`feature/dal-200-eq-observation-slots` and dependent on #369.

Only `dal-cpp/dal/script/simulation.hpp` and this report change in this repair.
The production change extracts the existing worker body into
`Detail::EvaluateAADBatch`, passes passive controls through `AADBatchSettings_`,
and extracts the ordered final reduction into `Detail::AggregateAADResults`.
The public function signatures and defaults are unchanged. This report also
corrects the obsolete prepared-AAD rejection statement in the initial handoff.
The related current-state methodology correction remains DAL-226's scope.

Behavior preservation was checked against the full original and final runner:

- Caller validation stays in the same order: executability, RNG, raw historical
  rejection, metadata-model construction, expired return, prepared AAD/smoothing
  match, optional compilation, metadata allocation/initialization, then tasks.
  Error types and message literals are unchanged; source line locations move
  with the refactor.
- `compiledProduct` is engaged exactly when the original `useCompiled` was true;
  failed compilation still throws before workers. Raw legacy compiled and tree
  dispatch are retained. Named/prepared AAD compiled still rejects explicitly.
- Tape activation/rewind, active model, RNG, path, evaluator, registered zero,
  and historical seed remain batch-local on the executing worker. No shared
  active Number or additional recording is introduced. SkipTo and FillNormal
  retain their ordering, including the absent RNG for zero-dimensional paths.
- `InitModel4ParallelAAD` is unchanged: register inputs, NewRecording, model
  initialization and historical replay precede Mark. Each path rewinds, generates,
  evaluates, creates a PayoffRoot, checks finiteness, seeds and propagates to Mark.
  Mark-to-start propagation still precedes harvesting script and model risks.
  Historical hard and future fuzzy visitors are unchanged.
- Each risk contribution is divided by total nPaths once, in the same loop order.
  Final worker reduction preserves its original order and performs no second
  normalization. The zero-path low-level runner creates no batches or divisions.
- The task group remains after every captured local, including the new passive
  settings. Each task captures its PathBatch by value. Both normal completion
  and exceptional unwinding drain tasks before destroying their inputs.

### Static RED and GREEN evidence

No behavior RED is invented for this responsibility extraction. The existing
Codacy check `103877652329` (`action_required`) reports
`MCAADSimulation` complexity **16 > 8** at the initial F4 implementation head
`260de4cca02db9cf28f4bfabaa4b60f60722d744`. Its original check and annotation
JSON are retained in the attached evidence. The function was unchanged between
that head and the repair starting head.

Local Lizard **1.23.0** independently reproduces that count on the starting
source. Both local static invocations use the same command:

```bash
lizard -C 8 dal-cpp/dal/script/simulation.hpp
```

Before extraction: exit **1**, one warning, `MCAADSimulation` CCN **16**.
After extraction: exit **0**, zero warnings; `MCAADSimulation` CCN **8**,
`EvaluateAADBatch` **7**, `AggregateAADResults` **3**. Full outputs are
`complexity-before.log` and `complexity-after.log` in the new evidence bundle.
No analyzer configuration, suppression, macro, threshold, or ignore rule changes.

Lizard is not claimed to be the exact hosted Codacy analyzer/version/configuration;
that equivalent environment is unavailable locally. A green local complexity
check and green runtime tests do not establish hosted Codacy success. The single
post-push snapshot is delivered separately, without watching or polling CI.

### Refactor runtime verification

The new verification uses Linux x86_64, GCC **15.2.0**, CMake **4.2.3**, C++17,
and repository-pinned submodules initialized with
`git submodule update --init --recursive`. Source did not change during these
builds. Only the report differs from the exact tested code commit above.

```bash
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF
cmake --build build/Release-linux -j8
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptPastReplayTest.*:ScriptFixingPreparationTest.TestAad*:ScriptObservationSimulationTest.TestAad*'
ctest --test-dir build/Release-linux --output-on-failure -j4
```

The focused filter passes **16/16** before extraction on the starting code,
and **16/16** after extraction. Native full CTest passes **1715/1715**, zero
failures, **16.50 seconds**, including core, public C++, and portable Excel.
Logs: `baseline-f4.log`, `green-f4-native.log`, `ctest-native.log`, and the
corresponding configure/build logs. No new behavior or tests were introduced;
the existing analytic, oracle, and failure-recovery tests protect this refactor.

Alternative-backend builds and regression commands on the same tested source:

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

- Adept fork (CMake version **4.1.1**), pin
  `1e29edc6e16f969e99145f0cbff34ff0de5fe699`: **425/425**, 1261 ms.
- CoDiPack **3.1.0**, pin `86b94d3f3c3b6659a36f8e640945a7ebe1884a4a`:
  **425/425**, 1288 ms.
- XAD **2.1.0-dev**, pin `ca0146061726745870aac71f3108c7d14129d1b3`:
  **424/424**, 2608 ms.

Each selected run passed without failed or skipped cases. All sixteen F4 tests
(eleven original plus five tester additions) ran on all four backends. This
includes threads 1/2/4, paths 1/257/8193, 8193-path BS/Dupire fresh-recording
oracles, and the 16385-path seed-failure drain/recovery test. Preparation-read
counts, worker lookup rejection, direct/constant roots, nonlinear and hard/fuzzy
risks, repeated pricing and legacy compiled/tree tests retain their existing
assertions and tolerances. Alternative logs are `regression-Adept.log`,
`regression-CoDiPack.log`, and `regression-XAD.log`; each configure/build log
is also attached. All build and test processes completed before publication.

`git clang-format --diff HEAD^ HEAD` produced no formatting diff for the code
commit. `git diff --check` and staged whitespace checks passed. No production
scope deviation was needed. No docs/CHANGELOG, CI/Codacy configuration, ignore
rules, benchmark code/policy, public bindings, submodule pointers, or tests changed.
This refactor does not claim Python, Windows XLL, sanitizers, full alternative
public/Excel suites, performance measurements, or F5 compiled parity. Native
verification built all configured targets and ran full CTest; it did not run
installation or examples. F3 performance remains deferred in DAL-223. Hosted
Codacy, independent revalidation, documentation and review remain separate gates.

## Initial implementation evidence (historical)

The remaining sections record the original implementation at `4e4c6637`.
Their 1710/420/420/419 counts are historical, not results for the refactor head.

## Revision and scope

- Starting published F3 commit: `e8943ee2a399229cf36be412accb92ea1872b10e`.
- Starting tree: `ab062df7c6897dd594b3b444fc01f5b8172977f4`.
- Tested implementation commit: `4e4c663704d9378befcf69c086940e6e4361c111`.
- Tested implementation tree: `82ee22f351ea6c0f0d19ffed9dc5ca8e4fa9984c`.
- Branch: `feature/dal-201-historical-aad-state`.
- Stacked draft PR base: `feature/dal-200-eq-observation-slots`, dependency
  https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/369.
- The publication commit adds this report only. The DAL-224 final comment and
  attached `publication.json` record its exact published head/tree and PR URL.

The controlling scope is the current DAL-224/DAL-201 description. Read the
approved spec, API note, and critique from DAL-197 attachments
`01a09606-bb52-7a4c-842c-80c53efa9a23`,
`01a09606-c6b7-778f-9517-26bb692c7b3f`, and
`01a09606-cc2d-7c77-8a00-4772c19c0c3d`, plus the script/AAD methodology and
repository implementation/test/Git contracts. No new public binding contract
or design deviation is introduced.

## Behavior and changed files

- `dal-cpp/dal/script/visitor/evalstate.hpp`: an optional typed historical seed
  preserves active dependencies when each path restores variables. The existing
  double initialization remains available to legacy and price-only evaluators.
- `dal-cpp/dal/script/visitor/evaluator.hpp` and `pastevaluator.hpp`: allow typed
  observation reads and hard historical replay with active script parameters.
  Historical PAYS still consumes its RHS without adding settled payments.
- `dal-cpp/dal/script/visitorlist.hpp`: registers `PastEvaluator_<AAD::Number_>`
  with the AST visitor machinery. This minimal integration file is necessary
  for the typed historical program to execute.
- `dal-cpp/dal/script/preparation.hpp` and `preparation.cpp`: prepare a
  conservative tree and IF metadata, replay doubles once before workers, and
  expose historical replay using the sealed observation plan. No domain or
  constant folding is added. These builder changes are necessary to admit F4
  through the existing core preparation entry points.
- `dal-cpp/dal/script/simulation.hpp`: shares the existing AAD batch runner
  between raw legacy and prepared products. Each batch constructs all active
  model, parameter, evaluator, and seed state on its worker, registers inputs,
  opens the recording, replays history, and marks. Path contributions propagate
  to the mark, then each batch propagates to the start. Risk normalization still
  occurs once against the total requested path count. Task-group draining is
  retained. Expired products return before unsupported execution dispatch;
  changed prepared AAD/smoothing settings fail explicitly.
- `dal-cpp/dal/math/aad/aad.hpp`: `PayoffRoot` adds a registered zero input to
  produce a fresh path root, including for passive constants. The extra input
  is worker-local, has value zero, and never enters reported risk labels.
- `dal-cpp/tests/script/test_past_replay.cpp`: analytic risk, direct seed,
  constant/empty suffix, hard past decision, repeated batch/pricing, independent
  per-path recording, future fuzzy, expired and mode-boundary regressions.
- `dal-cpp/tests/script/test_observation_simulation.cpp`: exact preparation-read
  counts, rejecting worker lookup seams, stable observation storage, preparation
  failure and path drain tests. The obsolete blanket named-AAD rejection becomes
  a compiled-mode rejection for both double and AAD; the newly supported AAD
  behavior has positive analytic and fuzzy tests.

The historical program and plan contain no shared active Number. The typed seed
exists only within its batch evaluator. Model-aware prepared AAD tree execution
is supported with matching AAD mode and smoothing. Named/prepared AAD compiled
execution remains explicitly rejected; raw future-only legacy compiled/AAD remains covered.
Raw historical AAD requires preparation. Compiled/domain integration belongs to F5.

## RED, GREEN, and refactor

All commands below run from the repository root. Native focused builds used
`cmake --build build/Release-linux --target dal_cpp_tests -j8`.

1. RED: `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter=ScriptPastReplayTest.TestParameterRisk`
   failed with `UnsupportedExecutionMode: prepared AAD evaluation` before the
   production change. GREEN: the same test passed after typed replay and AAD
   preparation/runner integration. Logs: `red-parameter.log`, `green-parameter.log`.
2. RED: `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter=ScriptPastReplayTest.TestDirectSeedPayoff`
   returned SCALE risk `0.31246185768338824` instead of `80` for 8193 paths.
   GREEN: after the fresh payoff root, `--gtest_filter='ScriptPastReplayTest.*'`
   passed both initial tests. Logs: `red-seed.log`, `green-seed.log`.
3. RED: `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptPastReplayTest.TestExpiredAad*:ScriptPastReplayTest.TestPreparedAadRejects*'`
   failed both tests: expired compiled dispatch threw, and changed smoothing did
   not throw. GREEN: after moving the expired return and guarding prepared mode,
   the 12-test integration filter below passed. Logs: `red-mode.log`,
   `integration-native.log`.
4. The first per-path oracle run used the RNG constructor's initial sequence
   offset, while MC explicitly calls SkipTo. Corrected the oracle to
   `SkipTo(0)` after inspecting the generator, keeping the required 1e-8 tolerance.
   No production RNG ordering changed. Edge tests then passed without further
   production changes. Build integration also required registering the new
   visitor and using DAL Vector's exposed accessors.
5. Refactor while green: shared the AAD runner and existing hard visitor instead
   of duplicating either; formatted changed lines with git-clang-format and the
   new test file with clang-format. `git diff --check` and formatting checks pass.

Integration filter:

```bash
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptPastReplayTest.*:ScriptFixingPreparationTest.TestAad*:ScriptObservationSimulationTest.TestNamedCompiledRejectsBeforeModelSetup'
```

## Verification on the tested implementation

Linux, GCC `15.2.0`, CMake `4.2.3`, C++17. Submodules were initialized at the
repository-pinned revisions with `git submodule update --init --recursive`.

```bash
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF
cmake --build build/Release-linux -j8
ctest --test-dir build/Release-linux --output-on-failure -j4
```

Native: **1710/1710 passed**, including core, public C++, and portable Excel;
0 failed, 16.37 seconds. `ctest-native.log` contains the full test list/results.

Alternative configurations and builds:

```bash
cmake --preset=Release-linux -S . -B build/Adept -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_ADEPT_AAD=ON
cmake --build build/Adept --target dal_cpp_tests -j4
cmake --preset=Release-linux -S . -B build/CoDiPack -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_CODIPACK_AAD=ON
cmake --build build/CoDiPack --target dal_cpp_tests -j4
cmake --preset=Release-linux -S . -B build/XAD -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_XAD_AAD=ON
cmake --build build/XAD --target dal_cpp_tests -j4
```

Run each alternative binary with this common filter:

```bash
./build/Adept/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
./build/CoDiPack/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
./build/XAD/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
```

- Adept fork `1e29edc6e16f969e99145f0cbff34ff0de5fe699`: **420 passed**.
- CoDiPack `3.1.0`, `86b94d3f3c3b6659a36f8e640945a7ebe1884a4a`: **420 passed**.
- XAD `2.1.0-dev`, `ca0146061726745870aac71f3108c7d14129d1b3`: **419 passed**.
- No failed/skipped tests in these selected runs. Counts differ because Adept's
  operand-stack lifetime and CoDiPack's concurrency test are backend-specific.
  All 11 new F4 tests run on all four backends.

Logs: `regression-Adept.log`, `regression-CoDiPack.log`, `regression-XAD.log`.
Earlier alternative runs during development used a narrower filter and, for
Adept, caught the two mode-boundary RED tests before their production rebuild;
the final common-filter results above supersede those intermediate runs.

## Acceptance evidence and limits

- T18: PV `160*exp(-r*T)`, SCALE risk `80*exp(-r*T)`, rate risk `-T*PV`,
  zero spot/vol risk, and exactly the four model labels plus SCALE.
- T19: 8193-path direct-seed risk is 80; constant payoff and otherwise empty
  suffix root propagation run successfully on every backend.
- T20: historical `>`, `>=`, and `=` at 79.95, 80, and 80.05 against a width-0.2
  threshold use hard selection and preserve selected SCALE arithmetic risk.
- T23: threads 1/2/4, paths 1/257/8193, sequential pricing at H=80/90/80,
  direct and discounted payoffs. The single-thread 8193 case spans two batches.
  A separate full-recording-per-path oracle matches every model/script risk for
  BS and Dupire over 257 fixed Sobol paths and all three thread counts.
- T03/T32: 100 historical nodes across three events, both modes, all requested
  path/thread combinations: exactly one History and one final Fixing read.
  Worker-local rejecting history/fixing seams cover historical seed replay and
  future evaluation; observation storage address/size stays unchanged during
  evaluation. This is a storage/read invariant check, not a general allocator
  profiler or a performance benchmark.
- T31: missing history, invalid model, and unsupported compilation submit zero
  workers. Invalid numeric payoff yields one failed evaluation for every
  submitted batch before error return; a following valuation succeeds.
- Future known-fixing fuzzy primal and K/SCALE sensitivities match the analytic
  smoothing expression and an independent fuzzy-double evaluation.

Deterministic PV uses the requested relative 1e-12 scale; analytic risks use
absolute 1e-10; shared-path MC comparisons use absolute 1e-8 on normalized
results. No tolerances or benchmark policies were weakened.

No docs/CHANGELOG, public bindings, F3 branch, CI policy, or performance work
changed. No Python, Windows XLL, sanitizers, alternative-backend full public
suites, or performance measurements were run in this implementer task. F5
compiled parity is explicitly unsupported rather than claimed here. Independent
testing/review must assess remaining acceptance combinations and lifecycle risks.

F3 performance remains the separate DAL-223 backlog item. Its previously
recorded failing required checks are not erased by this F4 correctness evidence.
The draft F4 PR must retain its dependency and must not gain parent-closing
lines or be merged until the remaining F4 roles and required gates are complete.
