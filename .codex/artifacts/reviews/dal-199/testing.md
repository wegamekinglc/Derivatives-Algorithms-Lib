DAL-199 F2 independent tester handoff, 2026-09-13. This is active evidence for
the documentation and independent reviewer stages. Production handoff tested:
`feb5b6fc1d6042dde801f2cc0fd66385add0493f`, based on F1 merge
`65c6b87a088ba12cddfa57dcc111250c9bfae16a`. The containing test-only commit adds
the coverage below; the orchestrator records the final PR head.

The executed F2 functional tests pass, but delivery is blocked by the inherited
FixHistory_ layout collision. Root reports the independent reviewer reproduced
an ASan heap-buffer-overflow at -O0 and classified it P1. Root is coordinating a
separate prerequisite repair. All results here are pre-repair evidence and
require proportional revalidation after integration.

## Running existing tests

- Read the complete issue acceptance and fixed design, the tester contract,
  run-tests/write-tests/unit-test-style guidance, production APIs, nearby tests,
  and implementation handoff. No production, build, generated or public-doc
  files were changed by the tester.
- Deleted the previous root `test_output.txt` before each full native run.
  The first canonical configure stopped because examples require an
  uninitialized XAD submodule. Initialized pinned XAD/Adept/CoDiPack sources
  using `git submodule update --init --recursive dal-cpp/externals/xad
  dal-cpp/externals/adept dal-cpp/externals/CodiPack`; no gitlink changed.
- Ran `NUM_CORES=12 ADDITIONAL_CMAKE_FLAGS='-DDAL_BUILD_EXCEL_PORTABLE_TESTS=ON'
  bash ./build_linux.sh > test_output.txt 2>&1` on the production handoff.
  Exit 0: `100% tests passed, 0 tests failed out of 1610`, 8.93 seconds.
  The workflow configured, built all enabled targets, installed, and ran CTest.
  Fresh log: `build/dal-199-tester-native-baseline-full.log`; initial configure
  failure: `build/dal-199-tester-native-initial-configure.log`.

## Authoring coverage

Added six tests to the existing `ScriptFixingPreparationTest` suite:

- `TestEvaluationDateDoesNotChangeDuringCapture` changes the global date from
  a history-read observer. The captured evaluation date remains September 12;
  September 12/15 remain model requests and only September 11 resolves.
- `TestMixedTemporalRequestsAssignOnlyHistoricalSlots` orders future/history/
  today/repeated-history requests against an explicit snapshot containing all
  three dates. Only the historical request gets value slot 0, retaining both
  canonical uses, with no global reads. Compile-time assertions check the
  exposed requests/values collections are const references.
- `TestExplicitSnapshotNeverFallsBackToPresentGlobalHistory` supplies an empty
  explicit snapshot while the global historical quote exists. It still fails
  MissingFixing with one final virtual call, no global read, and no submission.
- `TestInverseFxMultipleDatesReadEachSequenceOnce` requests both logical FX
  directions at two dates with only reverse raw history. Four final virtual
  calls share exactly two underlying sequence reads; results are 1.25/0.8/2/0.5.
- `TestSnapshotFailureUsesInverseDependencyAndExactDate` places the bad inverse
  quote at the second of two dates for one logical index. The error identifies
  row 2, the requested direct FX identity, and the exact failing fixing date.
- `TestNonExpiredPreparedExecutionRejectsBeforeModelOrWorkers` checks the
  intermediate prepared execution rejection in double/AAD and tree/compiled
  combinations, before model creation, further history access or submissions.

Added `SimulationTest.TestDupireAadCallerInitializationPreservesValueAndRisks`:
flat-vol Dupire matches Black-Scholes for the same 257 Sobol paths in double
and AAD, tree and compiled. It compares normalized value, spot/rate/repo risks,
and summed local-vol risk against BS vega at 1e-10. A `TapeGuard_` controls the
caller tape lifetime. This exercises the newly added caller-side double
metadata model Allocate/Init plus the existing worker-side AAD initialization.

Existing tests independently exercised canonical case/date/EQ-delivery keys,
300 uses across three events, exact midnight, today policy, finite EQ zero/
negative values, invalid and inverse FX, repricing/frozen snapshot behavior,
raw environment projection with no synthesized direct quote, unsupported/null
indices, all syntax branches and historical PAYS, structural/expired rejection,
source diagnostics, immutable value reads, and real accepted-task counters.

## Repairing failures and inherited finding

- No F2 production behavior repair was required by the executed tests.
- Test authoring initially lacked the complete Index_ declaration. Adding its
  header exposed an inherited header conflict described below. Kept raw
  environment inspection in the existing indice test file and checked mixed
  temporal slots in the preparation file without combining those headers.
  Two new FX fixtures initially used unsupported currencies; corrected them
  to registered GBP/CAD and GBP/CHF. Those failures were test-construction
  errors, not claimed as production RED/GREEN evidence.
- The intermediate run after an unsuccessful build used the old executable
  and ran only 18 tests (`build/dal-199-tester-new-coverage.log`); it is not
  acceptance evidence for the seven added cases. The final successful build
  and 25-test run below supersede it.
- Inherited finding for independent reviewer triage: `dal-cpp/dal/storage/
  globals.hpp:21` defines Dal::FixHistory_ with a public vector of timestamp/
  value pairs, while `dal-cpp/dal/indice/fixings.hpp:25` defines the same name
  with a private map and explicit constructor. Including both fails to compile.
  Both files are unchanged from the F1 baseline. The tester reproduced the
  compile failure; the subsequent ODR/runtime finding above was independently
  reproduced by the reviewer, not by this tester's commands.
- Minimal reproducer `build/dal-199-tester-fixhistory-repro.cpp` includes
  platform.hpp, storage/globals.hpp, then indice/index.hpp, followed by an empty
  main. `c++ -std=c++17 -I dal-cpp -fsyntax-only
  build/dal-199-tester-fixhistory-repro.cpp` exits 1 with the duplicate-definition
  error; log: `build/dal-199-tester-fixhistory-repro.log`. Root has the finding
  and is coordinating the prerequisite repair; tester made no production
  workaround.

## Final native verification

- `cmake --build build/Release-linux --target dal_cpp_tests -j4`: exit 0 after
  the fixture correction, `build/dal-199-tester-fixture-build.log`.
- `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptFixingPreparationTest.*:FixingEnvironmentTest.*:SimulationTest.TestDupireAadCallerInitializationPreservesValueAndRisks'`:
  exit 0, 25 tests across three suites passed, including all seven additions.
  Log: `build/dal-199-tester-new-coverage-green.log`.
- Fresh canonical full command, after deleting `test_output.txt`:
  `NUM_CORES=12 ADDITIONAL_CMAKE_FLAGS='-DDAL_BUILD_EXCEL_PORTABLE_TESTS=ON'
  bash ./build_linux.sh > test_output.txt 2>&1`.
  Exit 0: `100% tests passed, 0 tests failed out of 1617`, 11.16 seconds.
  Log: `build/dal-199-tester-native-final-full.log` and root `test_output.txt`.
- Configuration: Linux Release, GCC 15.2.0, native AADET, core/public/portable
  Excel tests and examples enabled. Python and benchmarks off; standard script
  excludes the benchmark label. Discovery is 1,479 main core cases, four core
  allocation/boundary cases, 105 public cases, and 29 portable Excel cases;
  `build/dal-199-tester-native-discovery.log`.

## Alternate AAD verification

The changed simulation header warrants compiling the core and running focused
AAD lifecycle, simulation, parity and preparation checks on Adept and CoDiPack.
The full backend/integration matrix remains F9 scope.

- Configured independent Linux Release/GCC 15.2.0 directories with
  `cmake --preset=Release-linux -S . -B build/dal-199-adept
  -DDAL_USE_ADEPT_AAD=ON -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_PUBLIC=ON
  -DDAL_BUILD_EXCEL_PORTABLE_TESTS=ON` and
  `cmake --preset=Release-linux -S . -B build/dal-199-codi
  -DDAL_USE_CODIPACK_AAD=ON -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_PUBLIC=ON
  -DDAL_BUILD_EXCEL_PORTABLE_TESTS=ON`. Both exited 0. Exactly one alternate
  backend is enabled in each cache; the native cache remains all-off.
- `cmake --build build/dal-199-adept --target dal_cpp_tests -j6` and
  `cmake --build build/dal-199-codi --target dal_cpp_tests -j6` both exited 0.
  These builds compile the complete main core test executable. Alternate
  public/Excel tests were configured but not built/run; their full execution
  is claimed only for native above. Configure/build logs use
  `build/dal-199-tester-{adept,codi}-{configure,build}.log`.
- For each directory, ran `ctest --test-dir build/dal-199-<backend>
  --output-on-failure --parallel 6 -R
  '^(ScriptFixingPreparationTest|FixingEnvironmentTest|FixingSnapshotTest|ScriptObservationTest|SimulationTest|ScriptCompiledParityTest|ScriptCompiledParityFuzzTest)\.|^ScriptTest\.Test(SimulationTaskGroup|AadSimulationPropagatesTaskFailure|CompiledAadOperandStacks)'`.
  Adept exited 0: `100% tests passed, 0 tests failed out of 92` (0.30 seconds).
  CoDiPack exited 0: `100% tests passed, 0 tests failed out of 91` (0.52 seconds).
  Logs: `build/dal-199-tester-{adept,codi}-focused.log`.
- Every new test passes in separate CTest processes on each backend. The
  extra Adept case checks compiled operand-stack lifetime. Both backends also
  pass accepted-task draining after submission failure, draining every task
  before rethrow, and AAD failure followed by successful reuse. These are
  actual runtime results, not compile-only claims.

## Scope limits

F2 preparation is verified; future FIX valuation, historical state/AAD replay,
prepared model/compile fault injection, allocation-free evaluator observation
reads, and the full path/thread/numeric-mode matrix remain later integration
stages. The frozen-value test reads the plan directly, not a working future FIX
evaluator. The expired fast path's lack of Allocate/GeneratePath follows the
inspected early return; no model allocation/path observer is exposed. The
submission observer is dynamically exercised on real legacy simulation tasks,
and existing task-group exception/draining regressions remain covered.

No local Windows/XLL, Python, XAD runtime, sanitizer, coverage or performance
claim is made. Sequential global fixing capture is not an atomic market
snapshot, and concurrent writes during capture are outside the stated contract.
Documentation decision, independent review (including the inherited finding),
remote checks and publication remain with the orchestrator.
