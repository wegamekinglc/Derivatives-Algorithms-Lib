# DAL-202 F5 implementation

Compiled execution now uses the sealed observation plan, retains historical
parameter dependencies on each worker recording, and preserves future fuzzy
conditions. This is an implementation handoff for independent testing,
documentation, and review, not completed master delivery.

## Revision and scope

- Reviewed F4 base: `5c954ca2fdded2ca35ad15c7fef44d90a002007d`.
- F4 tree: `2ad4c408e84bb6323528ac3e9b2f66bb77e40ce0`.
- Tested implementation: `462c302772e01e3c2559fe8e1383ff3e81b46cc1`.
- Implementation tree: `8d54004ed2ea2efe4183f92a1295107327423230`.
- Branch: `feature/dal-202-compiled-observations`.
- Draft PR base: `feature/dal-201-historical-aad-state`, depending on
  [F4 #371](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/371)
  and [F3 #369](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/369).
- The publication commit adds this report only. The final DAL-228 comment and
  attached `publication.json` record its exact head, tree, and PR URL.

Read the current DAL-228/DAL-202 descriptions, their empty thread scans, the
approved spec/API/critique attachments on DAL-197, script/AAD methodology, and
repository role/style/test/publication contracts. The controlling attachments
are `01a09606-bb52-7a4c-842c-80c53efa9a23`,
`01a09606-c6b7-778f-9517-26bb692c7b3f`, and
`01a09606-cc2d-7c77-8a00-4772c19c0c3d`.

## Design and behavior

`LoadObservation` carries an observation ID and uses `ObservationPlan_::Read`
for historical values and retained model outputs. Future payment numeraires
use `EventToSample`, independently of observation slots. The compiled artifact
owns a const shared reference to its plan, so it remains valid after the
prepared product is destroyed. No historical/index lookup occurs in bytecode.

Preparation seals all syntactic history before optimization, replays doubles
once, tracks historical constants separately from live parameters, and seeds
future domains from that dependency state. Historical PAYS does not accumulate
state in either the analysis or replay. Final IF and constant metadata precede
compilation. Requested historical and future streams are built before workers.
The historical stream uses hard comparisons and a consuming `Discard` opcode.
Each AAD batch replays it into typed state before the existing tape mark and
retains the existing fresh payoff-root protocol.

Parameter domains are no longer their current numeric value. Known historical
arithmetic may fold, but parameter-dependent arithmetic stays active. Prepared
future fuzzy comparisons conservatively retain the F4 runtime epsilon kernel;
they do not derive discrete bounds or delete branches from hard domain flags.
This also avoids assuming a fuzzy interpolation has only its branch endpoints.
Legacy singleton fuzzy expressions similarly retain their runtime kernel.
Eager AND/OR nodes and their enclosing control flow remain evaluable. Final IF
metadata is rebuilt after folding, including in legacy preprocessing.

Prepared simulation cannot switch compiled/AAD/epsilon settings; evaluator
builders reject changed smoothing. Named double exact and AAD fuzzy execution
are supported in both tree and compiled modes. Raw historical AAD still requires
preparation, and unsupported model/index capabilities retain their prior errors.

## Changed files

- `dal-cpp/dal/script/visitor/compiler.hpp`: observation/discard opcodes,
  plan-aware lowering, and local observation/path pointers in compiled state.
- `dal-cpp/dal/script/visitor/domainproc.hpp`: parameter ranges, historical
  observation domains, initial domains, and conservative fuzzy conditions.
- `dal-cpp/dal/script/visitor/constprocessor.hpp`: historical constant/dependency
  tracking, discarded PAYS, and refreshed expression metadata.
- `dal-cpp/dal/script/visitor/constcondprocessor.hpp`: preserve eager boolean
  evaluation during folding.
- `dal-cpp/dal/script/event.hpp` and `event.cpp`: common stream construction,
  shared plan ownership, payment sample mapping, final legacy IF metadata.
- `dal-cpp/dal/script/preparation.hpp` and `preparation.cpp`: preparation order,
  cached bytecode, compiled historical seed replay, and fixed smoothing.
- `dal-cpp/dal/script/simulation.hpp`: fixed prepared execution-mode checks.
- `dal-cpp/dal/script/detail/simulationobserver.hpp`: one default no-op
  `BeforeCompilation` hook for deterministic preparation-failure injection.
  This directly required internal test seam is the only extra integration file;
  it adds no public binding/configuration contract.
- `dal-cpp/tests/script/test_compile_parity.cpp`: parameter/fuzzy regressions
  and refreshed legacy opcode coverage. Const is now reachable; explicit boolean
  AST literals still cover fuzzy true/false opcodes. No coverage is removed.
- `dal-cpp/tests/script/test_compiled_observations.cpp`: independent analytic
  path oracles, retained fixing/sentinel tests, artifact lifetime, and fuzzy FD.
- `dal-cpp/tests/script/test_past_replay.cpp`: compiled parameter/constant/direct
  roots, historical decisions/PAYS, repeated batches, full-recording oracles,
  and fixed settings.
- `dal-cpp/tests/script/test_observation_simulation.cpp`: strict prefetch,
  compiler failure, eager reads, stable storage, task draining, and bound SPOT.
- `.codex/artifacts/reviews/dal-202/implementation.md`: this active report.

No public Python/Excel/facade/archive surfaces, generated enums, benchmark
infrastructure, thresholds, or dependency branches changed. The opcode enum
already uses the documented NTTP/integer-stream exception; no Machinist markup
changed. No new API design or pricing-policy deviation was needed.

## RED, GREEN, and refactor

Commands run from the repository root. Focused builds use
`cmake --build build/Release-linux --target dal_cpp_tests -j8`.

1. `--gtest_filter=ScriptPastReplayTest.TestCompiledParameterRisk` failed with
   the intermediate compiled-mode rejection; the same command passed after
   bytecode observation/seed integration. Logs: `red-compiled-parameter.log`,
   `green-compiled-parameter.log`.
2. `--gtest_filter='ScriptCompiledParityTest.TestParameterConditionRemainsLive:ScriptCompiledParityTest.TestLiteralFuzzyBandRemainsFractional'`
   returned -2 instead of -4, and 160 instead of 120. Both passed after domain
   corrections. `green-folding.log` records 53 passing relevant cases.
3. `--gtest_filter='ScriptPastReplayTest.TestCompiled*:ScriptPastReplayTest.TestPreparedCompiledModeCannotChange:ScriptObservationSimulationTest.TestCompiledBranchPrefetchBeforeWorkers'`
   exposed the missing compiled-mode guard. The expanded suite passed 376 cases
   after the guard. Logs: `red-modes.log`, `green-modes.log`.
4. `--gtest_filter='ScriptObservationSimulationTest.TestCompilationFailureBeforeWorkers:ScriptObservationSimulationTest.TestCompiled*'`
   showed that the injected compiler failure was not called. The preparation
   hook makes the five barrier/storage/eager tests pass. Logs:
   `red-barrier.log`, `green-barrier.log`.
5. `--gtest_filter=ScriptPastReplayTest.TestPreparedCompiledModeCannotChange`
   additionally failed because evaluator construction accepted changed epsilon.
   Both builders now reject it; `regression-native.log` includes the GREEN.

Initial test construction needed three corrections: unused SCALE must actually
appear in a settled expression to obtain a risk label; the existing default
payoff rule requires registering x before pay; model path generation needs a
Gaussian vector of SimDim size. The first storage test used an empty vector
and crashed. The corrected fixtures retain all analytic assertions/tolerances.
These setup failures are retained in the RED logs, not claimed as product bugs.

Refactor while green: share stream construction, carry conservative historical
dependency state into final metadata, and format only changed lines/new tests.
`git diff --check` and `git clang-format --diff HEAD` pass.

## Fresh verification

Linux, GCC 15.2.0, CMake 4.2.3, C++17. Submodules were initialized at repository
pins. All logs are fresh for the tested implementation above.

```bash
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF
cmake --build build/Release-linux -j8
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
ctest --test-dir build/Release-linux --output-on-failure -j4
```

Native focused: **440/440 passed**, 1252 ms. Full native CTest:
**1730/1730 passed**, 17.10 seconds, including core/public/portable Excel.
Logs: `regression-native.log`, `ctest-native.log`, `build-native-full.log`.

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

- Adept pin `1e29edc6e16f969e99145f0cbff34ff0de5fe699`:
  **440/440 passed**, 1535 ms.
- CoDiPack pin `86b94d3f3c3b6659a36f8e640945a7ebe1884a4a`:
  **440/440 passed**, 1576 ms.
- XAD pin `ca0146061726745870aac71f3108c7d14129d1b3`:
  **439/439 passed**, 6906 ms.

No failures or skips in these selected runs. The existing backend-specific
test inventory accounts for the count difference. All fifteen F5 additions
run on all four backends. Logs are `configure-<backend>.log`,
`build-<backend>.log`, and `regression-<backend>.log`.
`python .github/scripts/check_docs.py` also passes (68 Markdown files).
Every build/test process finished before publication.

## Acceptance evidence and limits

- T06/T09/T10/T18/T21: same-path double values 200/160/120/160 with historical
  fixing 80, retained future fixing 120, and payment spot 999. Separate BS path
  formulas independently check primal and all spot/vol/rate/div/SCALE risks for
  all four scripts in tree and compiled AAD. They use zero Gaussian draws and
  the analytic lognormal expression, not one evaluator as the sole oracle.
- T18: PV `160*exp(-r*T)`, SCALE `80*exp(-r*T)`, rate `-T*PV`, spot/vol zero;
  `T=10/DAYS_PER_YEAR`. Risk labels contain no fixing parameter.
- T19/T20/T23: 8193 paths, threads 1/2/4, H=80/90/80 repeated pricing, direct
  historical and constant roots; strict/non-strict/equality historical branches
  on both sides and at the threshold, plus 100 discarded settled PAYS.
  BS and Dupire compare all risks against a new complete recording per path
  over 8193 fixed Sobol paths, repeated on each thread count.
- T22: future H-K=0.05 with width 0.2 gives 0.75 weight, discounted PV 120,
  SCALE risk 60 and K risk -800 before discount. Nested fuzzy metadata is used;
  AAD primal matches fuzzy double and K matches central FD with h=0.0001.
- T16/T27/T31: missing history in a dead branch fails in every tree/compiled
  exact/fuzzy preparation; bound SPOT/FIX share requests; existing legacy suites
  remain enabled; an injected preparation compiler failure submits zero workers.
  Compiled numerical failure drains all batches before returning.
- T32: compiled evaluation works after rejecting all fixing/index access seams;
  8193 repeated evaluations preserve observation and variable storage addresses.
  Artifact lifetime is also tested after destroying its prepared owner.

Deterministic PV tolerance is relative 1e-12, analytic risks absolute 1e-10,
normalized shared-path MC comparisons 1e-8. No tolerances were loosened.
Storage assertions are not a general allocator profiler or a performance claim.
No benchmarks/optimization experiments were run; DAL-223 remains deferred.
No Python, Windows XLL, sanitizers, installation/examples, or full alternative
public/Excel suites are claimed. Independent tester, doc-writer, and reviewer
remain required. Do not add closing intent or merge this draft into F4 as
completed master delivery.
