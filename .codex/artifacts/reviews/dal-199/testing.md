DAL-199 F2 independent tester handoff, 2026-09-13. Latest production revision
tested: integrated merge `e93c73877f4f2d18be247b8fe0e4890f22ad147c`, including
the public dump correction `87edd0ac` and approved prerequisite repair
`c121fa1d`. The containing test-only commit adds the parse-time clock regression
and this updated evidence. The orchestrator records the final PR head.

Fresh integrated native verification passes 1,625/1,625 tests and the integrated
actual-header ASan reproducer passes both link orders. The earlier inherited P1
is resolved. Fresh Adept passes 95 focused core and 13 public script tests;
CoDiPack passes 94 focused core and 13 public script tests. Earlier native/backend
counts below are retained only as historical provenance. See the final integrated
verification section for current commands and results. Documentation
reconciliation and final independent review remain separate delivery stages.

## Initial F2 verification before repairs

Original production handoff: `feb5b6fc1d6042dde801f2cc0fd66385add0493f`, based
on F1 merge `65c6b87a088ba12cddfa57dcc111250c9bfae16a`. Initial test additions
were committed at `fef17a0af1e048d2863c74f74114da4cf039a20d`. At that stage,
the functional suite passed but the independent reviewer confirmed an inherited
P1 FixHistory_ layout collision with an ASan heap-buffer-overflow at -O0.
Those results did not clear that blocker; the prerequisite and integrated
verification below now supersede it.

### Running existing tests

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

### Authoring coverage

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

### Repairing failures and inherited finding

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

### Native verification before repairs

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

### Alternate AAD verification before repairs

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

### Limits of the initial pass

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

## Final integrated verification

Read the updated implementation handoff and both independent reviews before
testing the clean integrated head. P2 requires the public dump wrappers to
capture D before parsing each fresh private copy, explicitly partition, and
preserve legacy unresolved-variable rendering. P1 requires unique map/vector
identities and a clean rebuild. This pass changes only a public test and this
report; no production, public docs, changelog, generated or build files changed.

### Public dump and preparation coverage

Added `ScriptTest.TestPublicDumpCapturesDateBeforeParsingIndex` in
`dal-public/tests/test_script.cpp`. A unique registered index parser changes
the global evaluation date from September 12 to September 23 while the private
dump copy is being parsed. Public tree output still marks September 11 past
and September 22 future; legacy output retains only September 22 and unresolved
variable index -1. Resetting D and repeating the tree dump produces identical
output from another fresh parse. Its index's Fixing method throws if called.
The test passes on its first build; no production repair was needed.

The implementer's four P2 regressions also pass. Together with existing public
tests they cover JSON /1 past/today/future phases, tree phases, live-only legacy
output, unchanged legacy variable formatting, changed D on the same stored
product, empty-live legacy rejection, repeatability after resetting D, raw
constant-false branches, inspectable past/dead-branch FIX tree/text, and the
unchanged DebugSchemaUnsupported rejection for FIX JSON. Throwing history,
final-fixing and submission observers record zero calls for dumping. Inspected
ProductForDump is shared by all three wrappers and captures D once before
Product(), then partitions; only JSON/tree index variables. None preprocesses,
folds syntax, performs model setup, or submits work.

The same integrated native run re-exercises every F2 preparation test: captured
D and today policy, exact timestamps, canonical EQ/delivery/date identities,
logical-versus-sequence FX counts and virtual inverse semantics, raw snapshot
projection, explicit snapshot authority, invalid and finite values, fresh versus
frozen plans, original branch/historical PAYS collection, source diagnostics,
expired/structural paths, execution barriers, and actual task draining. The
merged FixHistoryTest cases verify both header orders and preserved map/vector
behavior in that same executable.

### Clean native build and install

Verified the following build directory did not exist before configuration;
it and the install prefix are independent of every pre-repair directory. After
deleting root test_output.txt, ran:

```bash
NUM_CORES=12 DAL_BUILD_DIR=build/dal-199-integrated-native DAL_INSTALL_DIR=build/stage/dal-199-integrated-native ADDITIONAL_CMAKE_FLAGS='-DDAL_BUILD_EXCEL_PORTABLE_TESTS=ON' bash ./build_linux.sh > test_output.txt 2>&1
```

Exit 0 for fresh configure, all-target build, install and CTest:
`100% tests passed, 0 tests failed out of 1625`, 12.23 seconds.
Logs: root `test_output.txt` and `build/dal-199-integrated-native-full.log`.
Linux Release/GCC 15.2.0/native AADET; public, portable Excel, core tests and
examples enabled, Python/benchmarks off. Discovery: 1,482 main core cases,
four allocation/boundary cases, 110 public cases and 29 portable Excel cases;
`build/dal-199-integrated-native-discovery.log`. The new public regression and
all four P2 regressions pass as separate CTest processes.

### Integrated actual-header sanitizer and identity checks

Fresh scratch: `build/dal-199-integrated-repro/`. Copied the earlier three-file
reproducer and header-order sources but recompiled against this integrated
checkout's actual headers, without linking a previously built DAL library.
Both header orders pass `c++ -std=c++17 -I dal-cpp -fsyntax-only
build/dal-199-integrated-repro/headers-map-first.cpp
build/dal-199-integrated-repro/headers-vector-first.cpp` (exit 0, headers.log).
From that scratch directory:

```bash
c++ -std=c++17 -O0 -g -fsanitize=address -fno-omit-frame-pointer -I ../../dal-cpp -c map.cpp vector.cpp main.cpp
c++ -fsanitize=address map.o vector.o main.o -o map-first
./map-first
c++ -fsanitize=address vector.o map.o main.o -o vector-first
./vector-first
```

Fresh compilation, both links and both executions exit 0 with no ASan
diagnostics. Logs: compile.log, asan-map-first.log and asan-vector-first.log in
that scratch directory. nm output in object-symbols.log and linked-symbols.log
confirms that the map TU emits only IndexFixHistory_ destructor identity and
the vector TU only FixHistory_, with distinct addresses in the executable.
No compiler-specific object size is asserted. Source search confirms one
definition per name and no compatibility alias. The fresh integrated core
archive exports IndexFixHistory_::Find and no old FixHistory_::Find;
`build/dal-199-integrated-native-symbols.log`.

This is new integrated-source sanitizer evidence, superseding the initial
pass's lack of sanitizer coverage and its P1 blocker. The approved core rename
and mandatory clean-rebuild migration are now present in the merged changelog
and index methodology; external callers must still avoid mixing old objects.

### Fresh integrated alternate backends

To establish integrated compatibility after both header/identity and public
dump changes, created new Adept and CoDiPack build directories, each verified
absent before configuration. No pre-repair backend object directory was reused.
Configured with:

```bash
cmake --preset=Release-linux -S . -B build/dal-199-integrated-adept -DDAL_USE_ADEPT_AAD=ON -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_PUBLIC=ON -DDAL_BUILD_EXCEL_PORTABLE_TESTS=ON
cmake --preset=Release-linux -S . -B build/dal-199-integrated-codi -DDAL_USE_CODIPACK_AAD=ON -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_PUBLIC=ON -DDAL_BUILD_EXCEL_PORTABLE_TESTS=ON
cmake --build build/dal-199-integrated-adept --target dal_cpp_tests dal_public_tests -j6
cmake --build build/dal-199-integrated-codi --target dal_cpp_tests dal_public_tests -j6
```

All four commands exit 0. These are fresh full core/public test executable
builds in Linux Release/GCC 15.2.0. Exactly one alternate AAD backend is enabled
per cache. Portable Excel remains configured but is not built/run in these
targeted alternate checks; its full runtime evidence is native above.
Configuration/build logs: `build/dal-199-integrated-{adept,codi}-{configure,build}.log`.

For each backend, ran:

```bash
ctest --test-dir build/dal-199-integrated-<backend> --output-on-failure --parallel 6 -R '^(FixHistoryTest|ScriptFixingPreparationTest|FixingEnvironmentTest|FixingSnapshotTest|ScriptObservationTest|SimulationTest|ScriptCompiledParityTest|ScriptCompiledParityFuzzTest)\.|^ScriptTest\.Test(SimulationTaskGroup|AadSimulationPropagatesTaskFailure|CompiledAadOperandStacks)'
./build/dal-199-integrated-<backend>/dal-public/dal_public_tests --gtest_filter='ScriptTest.*'
```

- Adept: CTest exits 0, `100% tests passed, 0 tests failed out of 95`
  (0.30 seconds); public script tests exit 0, 13/13 pass.
- CoDiPack: CTest exits 0, `100% tests passed, 0 tests failed out of 94`
  (0.59 seconds); public script tests exit 0, 13/13 pass.
- Logs: `build/dal-199-integrated-{adept,codi}-{focused,public}.log`.
  The extra Adept case checks compiled operand-stack lifetime. These runs
  include the three type-identity regressions, all preparation/history checks,
  double/AAD tree/compiled simulation parity, actual task draining and AAD
  failure/reuse, all P2 public regressions, and the added parse-time clock test.

### Final testing disposition

No integrated test/build/sanitizer failure occurred, and no production fix was
needed from this testing pass. The new test strengthens the captured-date
boundary; the public and preparation suites cover the repaired current behavior.
Working and exact scoped staged whitespace checks pass. All foreground builds,
tests and sanitizer runs completed before handoff.

The prior P1 blocker is superseded by the reviewed prerequisite repair and the
fresh integrated lifetime evidence above. The public dump P2 behavior now passes
independent tests. The doc writer still needs to reconcile the knowingly stale
F2 debug wording, then the reviewer must assess the final combined head. No local
code-testing blocker remains. No Windows/XLL, Python runtime, XAD runtime,
full-suite sanitizer, benchmark or completed F9 integration-matrix claim is made;
future FIX execution/replay and the previously documented stage limits remain.
