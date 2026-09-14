# DAL-201 / F4 independent testing

DAL-225, 2026-09-14. Runtime correctness checks pass on all four AAD backends.
Five additional tests are delivered. F4 acceptance still needs the coordinator
to resolve the Codacy finding below, obtain the documentation decision, and
complete independent review. This report does not approve merge.

## Revisions and delivery

- Starting published head: `260de4cca02db9cf28f4bfabaa4b60f60722d744`.
- Starting tree: `5c7a9f7936f0927df8df22220556f4efa386bf67`.
- Tested code commit: `203114eebf6303c7ea7513ed32516d8689fb3f33`.
- Tested code tree: `8b412be73a936339725555aae34346541f3fd71c`.
- Branch: `feature/dal-201-historical-aad-state`.
- Existing draft PR: https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/371.
- Base: `feature/dal-200-eq-observation-slots`; dependency PR #369.
- The subsequent publication commit adds only this report. Its exact published
  SHA/tree, remote/GitHub verification, and single CI snapshot are recorded in
  the final DAL-225 comment and attached `publication.json`; they cannot be
  embedded in the commit that defines them.

The supplied implementation archive was downloaded through Multica and its
SHA256 verified as `0a6902a106a3d0b735048fd008446594ed97a4950e7aba21965e299aac18e8a5`.
Those upstream results were not substituted for the independent runs below.

## Running existing tests

Clean checkout, Linux x86_64, GCC 15.2.0, CMake 4.2.3, C++17. Dependencies
were initialized using `git submodule update --init --recursive`.

On the unmodified starting head:

```bash
NUM_CORES=8 bash ./build_linux.sh > test_output.txt 2>&1
```

Result: **100% tests passed, 0 tests failed out of 1710**, 12.72 seconds of
CTest execution. The fresh output was moved to `native-starting-full.log`
before the next full run, leaving no old root `test_output.txt`.

## Authoring tests

Only two existing test files changed; production, public bindings,
docs/CHANGELOG, benchmark policy, CI, and submodule pointers did not change.

`dal-cpp/tests/script/test_past_replay.cpp` adds:

- `TestNonlinearHistoryAndParameterRepricing`: historical parameter-dependent
  hard selection around K=160, including equality, nonlinear selected arithmetic,
  future state mutation, SCALE=2/3/2 repricing, threads 1/2/4 and paths 1/257/8193.
  Independent formulas assert PV and all seven model/script risks, including
  zero switching-parameter risk under hard branch semantics.
- `TestTodayPolicyPreservesHistoricalAndModelRisk`: H=80 combined with today's
  model spot=100 or required historical fixing=90. Checks spot, SCALE, rate,
  vol and div risks across threads 1/2/4, Sobol/MRG32/IRN and both bridge settings.
- `TestEveryPathRebuildAcrossBatchBoundary`: nonlinear historical seed and
  future EQ observations, BS and Dupire, 8193 fixed Sobol paths, threads 1/2/4.
  Reuses the existing `RebuildEveryPath` helper, which creates a fresh model,
  evaluator, historical replay and recording for every path and propagates
  directly to the start without the optimized runner's mark or PayoffRoot.
  Compares every labelled risk and normalized PV at absolute 1e-8. The separate
  analytic tests avoid relying exclusively on shared visitor behavior.
- `TestHistoricalSeedFailureDrainsAndRebuildsEveryBatch`: injects failure after
  recording historical state but before the mark, checks every expected batch
  finished replay and zero paths ran, then values the same prepared object twice
  successfully. At 16385 paths and threads 1/2/4, seed counts equal batch counts,
  path counts equal requested paths, and PV/SCALE/rate risks remain analytic.

`dal-cpp/tests/script/test_observation_simulation.cpp` adds:

- `TestAadRepricingRefreshesGlobalHistoryAndModelInputs`: reuses product data
  through global fixing 80/90/80 and rate 0.03/0.07/0.03, 8193 paths and threads
  1/2/4. Each valuation performs exactly one History and one final Fixing read
  and matches fresh analytic PV and risks. Existing RAII guards restore global
  history, evaluation date, and thread-pool state.

Initial targeted runs passed 4/4 and 1/1. No runtime failure needed repair;
these are additional coverage tests, not a claimed RED-to-GREEN production fix.
The final runs below include tightened deterministic PV tolerances of exactly
1e-12 times the positive expected value; analytic risk tolerance is 1e-10.

## Final verification on tested code

Native full build, install, core/public/portable Excel CTest pass:

```bash
NUM_CORES=8 bash ./build_linux.sh > test_output.txt 2>&1
```

**100% tests passed, 0 tests failed out of 1715**, 8.60 seconds CTest time.
The script excludes benchmark-labelled tests by its normal policy.

Alternative configurations (all initialized repository-pinned submodules):

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
  `1e29edc6e16f969e99145f0cbff34ff0de5fe699`: **425 passed**, 1221 ms.
- CoDiPack 3.1.0, commit `86b94d3f3c3b6659a36f8e640945a7ebe1884a4a`:
  **425 passed**, 1628 ms.
- XAD 2.1.0-dev, commit `ca0146061726745870aac71f3108c7d14129d1b3`:
  **424 passed**, 2990 ms.

No failed or skipped cases in the selected runs. All five added tests and all
eleven implementation-stage F4 tests ran on every backend. Counts differ due
to backend-specific tests. Initial alternative builds overlapped test authoring;
all three were rebuilt after the exact tested commit before these final runs.
All foreground build/test processes completed before handoff.

Formatting: `git clang-format` reported no changes; `git diff --check` and
`git diff --cached --check` passed. No coverage percentage is claimed.

## F4 acceptance mapping and limits

- T03/T32: independently ran the 100-node, three-event unique-history test in
  both modes over all requested path/thread combinations, plus rejecting
  worker-side history/fixing observer seams. Existing audits check observation
  vector address/size stability; they are not a general allocator profiler.
- T18: analytic discounted history PV, SCALE/rate risks, zero spot/vol risk,
  and model/script-only risk labels. Added nonlinear and today-policy oracles.
- T19: direct historical seed payoff risk=80, passive constant payoff,
  zero-dimensional/today suffix and explicit repeated PayoffRoot propagation.
- T20: `>`, `>=`, `=` on both sides and at equality use hard history selection.
  The added parameter-dependent branch also has zero K risk. Compiled history
  parity remains F5, explicitly unsupported in current F4 scope.
- T23: threads 1/2/4, paths 1/257/8193, repeated pricing and batches, two-model
  per-path oracle, plus 16385-path recording failure and recovery checks.
- T31: preparation failures submit zero tasks; invalid path/payoff tests drain
  submitted batches. Added failures during worker historical seed recording
  also drain and recover, without reusing old adjoints.
- Future known-fixing fuzzy primal and K/SCALE risk tests independently passed;
  the implementation's analytic and fuzzy-double comparisons remain intact.

No Windows XLL, Python, sanitizers, full alternative-backend public/Excel suites,
performance measurements, or F5 compiled parity were run. No general proof of
absence of cross-thread active values is claimed: evidence consists of source
inspection of worker-local construction and the multithread/backend lifecycle
tests. F3 performance remains separately deferred in DAL-223.

## Findings for coordinator handoff

1. **Codacy finding on F4 production code remains unresolved.** At starting head,
   check run `103877652329` concludes `action_required`, with one annotation:
   `dal-cpp/dal/script/simulation.hpp:364`, method `MCAADSimulation` has
   cyclomatic complexity 16, limit 8. This is the function introduced by F4's
   generalization of the prior AAD specialization; much of its body is inherited,
   but the current F4 diff owns the reported method and adds dispatch branches.
   Captured exact check and annotation JSON are attached. Route production/static
   analysis remediation to the existing DAL-224 implementer, then reverify the
   changed code. No production refactor or gate weakening was performed here.
2. **Documentation correction required.**
   `.codex/artifacts/reviews/dal-201/implementation.md:64` incorrectly says
   prepared AAD remains rejected. Model-aware prepared AAD tree execution is
   supported with matching AAD mode and smoothing; named/prepared AAD compiled
   execution remains rejected. The positive tests above verify this boundary.
   `docs/methodology/script_engine.md:119` and its unsupported-execution section
   also retain F3-era rejection statements. Route report correction and current
   method documentation to the coordinator/DAL-226; do not restart supported
   AAD implementation based on the stale prose.

Local correctness is green; these findings and the remaining roles prevent an
unconditional F4 acceptance or merge conclusion. DAL-225 delivers its testing
report in_review and returns control to the parent coordinator.
