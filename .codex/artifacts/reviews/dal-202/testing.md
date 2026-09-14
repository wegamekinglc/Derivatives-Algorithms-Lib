# DAL-202 F5 independent testing

Independent Linux verification passed. No actionable production defect was
reproduced. This report awaits parent acceptance; it does not authorize master
delivery, merging the stacked draft, or starting the next expert stage.

## Revisions and scope

- Input head: `a67de449715d028a4fa623cf3a7610d47d67d74b`.
- Input tree: `5a8c08ad675dd1c93a7492ac0887c28e68f19b61`.
- Test commit: `50ba5c8a84f94a40be4a0abd7fa81c07f300c287`.
- Tested tree: `df31524ca93970a78d2b5a4cf91b20e2e19ac414`.
- Branch: `feature/dal-202-compiled-observations`.
- Draft PR: https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/372.
- Fixed F4 base: `5c954ca2fdded2ca35ad15c7fef44d90a002007d`, tree
  `2ad4c408e84bb6323528ac3e9b2f66bb77e40ce0`; dependencies #371/#369 unchanged.

The publication commit adds only this report. The attached `publication.json`
records the resulting exact head/tree and remote PR head, avoiding a self-referential
commit hash in this file. Production files are identical to the input revision.

Only these tests/fixtures and this report changed:

- `dal-cpp/tests/script/test_compiled_observations.cpp`
- `dal-cpp/tests/script/test_past_replay.cpp`
- `dal-cpp/tests/script/test_observation_simulation.cpp`
- `dal-cpp/test-support/test_script_observation_allocations.cpp`

Read the current DAL-229 and DAL-202 descriptions and comment scans, the parent
handoff thread, repository tester/test/style/publication contracts, approved
spec/API/critique attachments, script/AAD methodology, production preparation,
observation, evaluator/bytecode code, and nearby tests. No dependency branch,
production code, public interface, tolerance, gate, or benchmark changed.

## Existing tests run

Inherited evidence is the DAL-228 implementation report: native CTest 1730,
native/Adept/CoDiPack 440 each, XAD 439. Those logs were not reused as independent
results. All results below were captured afresh in this workspace on Linux,
GCC 15.2.0 and CMake 4.2.3, using pinned submodules.

| Fresh run                       | Passed | Failed | Log                              |
| ------------------------------- | ------ | ------ | -------------------------------- |
| Original head full Linux script | 1730   | 0      | `full-linux-original.log`        |
| Original head native targeted   | 440    | 0      | `targeted-native-original.log`   |
| Original legacy parity/fuzz     | 33     | 0      | `legacy-original.log`            |
| Expanded tests full Linux       | 1730   | 0      | `full-linux-expanded.log`        |
| Expanded native targeted        | 440    | 0      | `targeted-native.log`            |
| Expanded Adept targeted         | 440    | 0      | `targeted-Adept.log`             |
| Expanded CoDiPack targeted      | 440    | 0      | `targeted-CoDiPack.log`          |
| Expanded XAD targeted           | 439    | 0      | `targeted-XAD.log`               |
| Expanded legacy parity/fuzz     | 33     | 0      | `legacy.log`                     |
| Focused strengthened tests      | 3      | 0      | `focused-expanded.log`          |
| Isolated native allocation      | 2      | 0      | `allocation.log`                |

The full script configured and built core/public/examples, installed them and
ran CTest with benchmarks excluded, including portable Excel contracts. Its
fresh final summary is `100% tests passed, 0 tests failed out of 1730` (9.62 s).
The original run took 9.91 s. Timings are execution records, not benchmarks.
The targeted filter matches 17 suites. XAD alone excludes
`AADTest.TestDefaultNumberAdjointRequiresTapeNode`; no F5 test is skipped.
The isolated fixture is outside CTest and is not included in the 1730 count.

## Tests authored or strengthened

- Expanded `TestKnownFixingFuzzyBandAndFiniteDifference` from one strike to
  K={79.93,79.95,80,80.03,80.07}, in both compiled and tree modes. For width
  0.2, independently calculate `w=(80-K+0.1)/0.2`, discounted PV `160*w`,
  SCALE risk `80*w`, K risk `-800`, and rate risk `-T*PV`. Spot/vol risks are
  zero. Compare each AAD primal to same-epsilon fuzzy double and K risk to
  central differences with h=0.0001. K=80 is smooth for this spread kernel;
  no derivative is claimed at its endpoints or a hard historical switch.
- Extended nonlinear historical parameter repricing to compiled execution.
  Both execution modes now cover threads {1,2,4}, paths {1,257,8193}, SCALE
  {2,3,2}, and historical strikes {159.95,160,160.05}; each repricing rebuilds
  preparation. Independent polynomial/discount formulas check all seven risks.
- Strengthened compiler-failure injection for double and AAD public core
  pricing entry points. Verify one historical fixing read, exact injected
  error text, one `BeforeCompilation` callback, zero submitted workers, then
  successful 8193-path repricing after removing the observer.
- Added an isolated allocation fixture using the repository's existing global
  allocation probe. Its positive control detects ordinary and aligned temporary
  allocations even when freed before measurement ends. Exact/fuzzy tree and
  compiled double evaluation each perform 8193 repeats with zero C++ allocation
  requests, correct independent payoffs (140 exact, 105 fuzzy), and throwing
  history/index read seams. State and scenario allocation occur before the
  measured region; the first evaluation is included without warm-up.

These are coverage improvements against already working behavior, so their
first functional runs were green. No failing production test was repaired and
no red-to-green production-fix claim is made. The only tooling interruption was
missing local Git author metadata; the commit used the repository's existing
Codex author identity. Formatting and `git diff --check` pass.

## Acceptance evidence

| IDs                 | Independently run evidence and oracle |
| ------------------- | ------------------------------------- |
| T06/T09/T10/T21      | `TestIndexFixingsSamePathAndArtifactLifetime` supplies F=120 and unrelated samples=999 for mixed, duplicate, retained and historical-state cases; checks 200/160/120/160 and artifact ownership after prepared destruction. `TestIndexFixingsAnalyticPathRisks` checks all spot/vol/rate/div/SCALE risks against discounted lognormal zero-Gaussian formulas for all four cases, not just evaluator parity. |
| T18                 | `TestParameterRisk` and `TestCompiledParameterRisk`: PV=160*exp(-r*T), SCALE=80*exp(-r*T), rate=-T*PV, T=10/DAYS_PER_YEAR; zero model spot/vol risk and no extra fixing risk label. |
| T19/T20             | Direct-seed and constant roots, compiled batch lifetime, strict/non-strict/equality historical decisions around H=80, and 100 discarded historical PAYS pass. The added nonlinear repricing checks preserve branch arithmetic risks after parameter changes. |
| T22                 | Five-point analytic fuzzy weights and all stated risks, same-epsilon fuzzy-double primal and central differences pass. Legacy live-parameter and literal-fuzzy regressions remain enabled. |
| T23                 | Compiled and tree lifetime tests run repeated 8193-path batches on threads 1/2/4. BS and Dupire full-recording-per-path references check primal and every risk against mark/rewind simulation. No active Number is deliberately transferred across threads. |
| T16/T27             | Strict missing-history prefetch across exact/fuzzy and tree/compiled, eager AND/OR evaluation, shared bound SPOT/FIX requests, unbound/mixed SPOT errors and legacy parity/fuzz regressions pass. |
| T31                 | Last-history and model-setup failures precede submission; strengthened compiler observer checks both numeric entry points and recovery. Tree/AAD/compiled path-failure tests collect all submitted batches before returning. |
| T32                 | Existing worker-local throwing read seams and storage tests pass; isolated allocation counters strengthen the address-only evidence. Source inspection confirms `ObservationPlan_::Read` uses a bounds-checked request and either historical-value or scenario-slot indexing, with no index I/O, loops, or observation-container allocation. |

Deterministic primal tolerances remain relative 1e-12, analytic risks 1e-10,
normalized same-path MC comparisons 1e-8. Existing finite-difference tolerance
1e-5 is retained; no epsilon, assertion, or numerical tolerance was weakened.

## Reproduction

Commands run from the repository root. An original `test_output.txt` was absent
in this fresh checkout; it was created by the first run, copied into the evidence,
then freshly overwritten by the second full workflow.

```bash
git submodule update --init --recursive
NUM_CORES=8 bash ./build_linux.sh > test_output.txt 2>&1
cmake --preset=Release-linux -S . -B build/Adept -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_ADEPT_AAD=ON
cmake --preset=Release-linux -S . -B build/CoDiPack -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_CODIPACK_AAD=ON
cmake --preset=Release-linux -S . -B build/XAD -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_XAD_AAD=ON
cmake --build build/Adept --target dal_cpp_tests -j4
cmake --build build/CoDiPack --target dal_cpp_tests -j4
cmake --build build/XAD --target dal_cpp_tests -j4
```

Each native/Adept/CoDiPack/XAD binary was run with this exact filter:

```bash
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*'
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationSimulationTest.TestCompilationFailureBeforeWorkers:ScriptPastReplayTest.TestNonlinearHistoryAndParameterRepricing:ScriptCompiledParityTest.TestKnownFixingFuzzyBandAndFiniteDifference'
```

The first command was also executed with `Release-linux` replaced by `Adept`,
`CoDiPack`, and `XAD`. Backend build/configure logs are included. Builds were
refreshed after the test edits; the complete native workflow was repeated.

Isolated allocation fixture, deliberately avoiding a global allocation override
in the ordinary test executable (substitute any local output path if desired):

```bash
c++ -O3 -DNDEBUG -std=c++17 -fPIE -pie -ffp-contract=fast -Idal-cpp -Idal-cpp/externals/googletest/googletest/include dal-cpp/test-support/test_script_observation_allocations.cpp dal-cpp/test-support/bcg_allocation_probe.cpp dal-cpp/tests/test_main.cpp build/Release-linux/dal-cpp/libdal_cpp.a build/Release-linux/lib/libgtest.a -pthread -o ../evidence/script_observation_allocations
../evidence/script_observation_allocations
git diff --check
git clang-format --diff a67de449 -- dal-cpp/tests/script/test_compiled_observations.cpp dal-cpp/tests/script/test_past_replay.cpp dal-cpp/tests/script/test_observation_simulation.cpp dal-cpp/test-support/test_script_observation_allocations.cpp
```

## Limits and handoff

T32 allocation measurement covers the native double exact/fuzzy evaluators;
it does not count AAD backend tape allocations, arbitrary C malloc calls, or
every possible expression shape. The constant-time read conclusion is source
inspection, not a dynamically instrumented count of every observation load.
Worker I/O rejection uses thread-local observers installed on the executing
thread; coordinator-only observer counts are not presented as worker proof.
The compiler seam injects immediately before bytecode construction, not inside
an arbitrary compiler opcode; it remains an internal default no-op observer.

No Windows XLL, Python, sanitizer, alternative-backend full public/Excel suite,
or benchmark run is claimed. This Linux run did build/install examples but did
not execute every example. No benchmark or DAL-223 performance experiment ran.
The isolated allocation fixture requires the explicit command above and is not
registered in CTest because build infrastructure is outside this test-only scope.

The parent's inherited CI snapshot reported Codacy failure. Publication captures
one new CI snapshot in `ci-snapshot.json`; unresolved CI remains the coordinator's
integration work, and checks are not weakened or polled. Doc-writer and reviewer
remain parked pending parent acceptance. No closing intent or merge is added.
Fresh logs, submodule pins, selected CMake settings, tested source hashes,
publication metadata and this report are attached as an archive with SHA256.
