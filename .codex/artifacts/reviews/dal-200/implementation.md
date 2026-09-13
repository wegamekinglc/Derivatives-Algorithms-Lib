# DAL-200 F3 implementation handoff

## Constant-condition lifetime repair, 2026-09-13

The bounded DAL-216 repair is complete and ready for independent successor
acceptance. Constant-false IF preprocessing now retains the ELSE range before
replacing the owning IF node. Nested selected statements remain in order and
are recursively folded. All older sections below are historical evidence;
their acceptance statements do not approve this successor.

Starting published SHA: `a21ff62b5a50f809c392e8701cb4352bc98f700f`, tree
`c7297e29cd591d0ea96a461ecb980b918da746eb`.
Tested repair SHA: `15ae4fd086f6f51dea135e30fdf2f3864915b53d`, tree
`374ebf6d6c31d3c3006b205b50d8d62e530d8303`.
The publication adds only this report to that repair; the final DAL-216 comment
records the delivered report-commit SHA. Existing draft PR:
<https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/369>, publish
branch `feature/dal-200-eq-observation-slots`.

### Authority, cause and scope

The current DAL-216 description, parent DAL-200 F3 contract, independent DAL-217
diagnostic/report, repository implementer/style/test/publication references,
and script-engine methodology control this repair. The dedicated clean checkout
matched the live PR head and retained F2 ancestor
`ec8b0072fbf70dab814a543edc625c8e0bf77efa` and published Adept repair
`63c134c096f2f3d2fd9865cbe5c52cfd5bb8e2dd`. Historical unpublished work and
recovery patches were not modified or applied.

In the original `ConstCondProcessor_::Visit(NodeIf_&)`, replacing `*current_`
destroys the IF still referenced by `node`, then the false branch calls
`node.HasElse()` and reads `node.firstElse_`. Both ELSE-present and ELSE-absent
scripts therefore read freed storage. The minimum correction captures one
range start while the node is alive: the ELSE index when present, otherwise
the original argument count. After moving the arguments and replacing the
owner, the existing ordered move loop uses that saved value and visits the
retained collection. No subsequent access uses the destroyed node.

Changed files:

- `dal-cpp/dal/script/visitor/constcondprocessor.hpp`: the lifetime correction
  and formatting of the touched conditional.
- `dal-cpp/tests/script/test_constcondprocessor.cpp`: two nested regressions
  and the evaluator include, with existing six assertions preserved.
- `.codex/artifacts/reviews/dal-200/implementation.md`: this current handoff.

The ELSE test discards multi-statement true branches, retains a nested false
ELSE, and evaluates ordered decimal accumulation to exactly **12345**. The
no-ELSE test folds a nested false branch inside a retained true branch, discards
another top-level false block, and evaluates to exactly **1234**. Collection
checks establish recursive folding, while evaluated results detect statement
loss, reordering, and execution of discarded statements. No nearby integration
file or other production surface needed editing. There is no design deviation.

### RED, GREEN and full verification

Fresh unchanged-source RED used the independent tester's inspected
`reproduce_constcond.py` driver. Its six isolated executions produced **4 passes
and 2 ASan heap-use-after-free failures**, both exit 1:
`ScriptTest.TestConstCondAlwaysFalseIfReplacedByElse` and
`ScriptTest.TestConstCondAlwaysFalseNoElseEmptyCollection`. The failing logs
identify the invalid read in `NodeIf_::HasElse()` after owner replacement.
The added ELSE regression was then compiled and run before production edits:
the same ASan failure, exit 1. The no-ELSE regression was added next and also
failed before the fix. These are observed RED results, independent of the
misnamed historical `green-constcond.log`. F2 was not rerun in this repair turn;
its separate identical failure remains the independent tester's evidence.

Fresh repaired-source GREEN rebuilt the DAL library and runner in a new
directory: **6/6 existing cases pass**, and the combined filter passes **8/8**,
including both added cases. After formatting, affected production objects and
the runner were rebuilt and the same 6/6 and 8/8 results confirmed against the
final code bytes. The native address-only configuration is Clang **21.1.8**,
C++17, AADET, Debug `-O1 -g`, static DAL, `-fsanitize=address`, frame pointers,
and `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`. Audits verify all **130 DAL
production translation units** and Google Test were actually built with those
flags. The two narrow runner translation units use the same instrumentation.
No sanitizer suppression was supplied; system/compiler runtime libraries were
not rebuilt.

Final-code compatibility verification:

- **366/366** native script/model cases: preprocessing, constant/domain/IF
  processing, tree/compiled/fuzzy parity, F3 preparation and observations,
  simulation, and BS/Dupire behavior.
- Canonical `build_linux.sh`, GNU C++ **15.2.0**, Release/native AADET:
  **100% tests passed, 0 tests failed out of 1681**. This is fresh discovered
  CTest output, including core, public and portable Excel targets, with the
  script's unchanged benchmark exclusion.
- Clang **21.1.8**, Adept, Debug `-O1 -g`, address-only sanitizer:
  **43/43** focused tape/compiled parity/fuzzy/constant-condition cases pass.
  `AADTapeTest.TestGradientCapacityGrowsAfterSeeding` retains its exact
  accumulated-adjoint assertions **3 then 2083**. The formerly failing
  `ScriptCompiledParityTest.TestParity_Number_NestedIf_Fuzzy` also passes
  **10/10** additional repetitions. All 130 DAL, 16 Adept and four Google
  Test/Mock translation units were instrumented; the five selected test/runner
  translation units were compiled from the CMake database with the same flags.
- `dal_check_generated`, full changed-test clang-format, touched-header-range
  clang-format, and Git whitespace/scope checks pass. No generated files drifted.

The first canonical configure stopped because the fresh checkout lacked the
pinned XAD source used by an existing example target. Initializing that submodule
allowed the unchanged build workflow to pass. The first auxiliary Adept link
also stopped because the selected build targets omitted Google Mock/main static
libraries referenced by its CMake link line; building those existing targets
resolved it. Both setup logs are retained. Neither failure led to a production,
dependency-pin, test, or build-policy change.

### Reproduction and evidence

Commands run from the workspace unless noted. The attached drivers and each
output directory's `commands.json` preserve expanded configure/build/compile/
link/test commands and exit codes.

```bash
python3 evidence/dal217/dal-217-constcond-diagnostic/reproduce_constcond.py Derivatives-Algorithms-Lib evidence/build-red-asan evidence/red-existing
# Before the fix, compile the updated test source with the RED link command,
# changing only the runner output, and run each new filter separately:
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 evidence/red-added-else/constcond-asan --gtest_filter=ScriptTest.TestConstCondNestedFalseElsePreservesStatementOrder --gtest_fail_if_no_test_selected
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 evidence/red-added-noelse/constcond-asan --gtest_filter=ScriptTest.TestConstCondNestedFalseNoElsePreservesStatementOrder --gtest_fail_if_no_test_selected
python3 evidence/dal217/dal-217-constcond-diagnostic/reproduce_constcond.py Derivatives-Algorithms-Lib evidence/build-green-asan evidence/green-existing
python3 evidence/dal217/dal-217-constcond-diagnostic/reproduce_constcond.py Derivatives-Algorithms-Lib evidence/build-green-asan evidence/green-final
python3 evidence/repair/run_checks.py green-added
# From Derivatives-Algorithms-Lib, with no stale test_output.txt:
NUM_CORES=8 bash ./build_linux.sh > test_output.txt 2>&1
# Back in the workspace:
python3 evidence/repair/run_checks.py native-focused
python3 evidence/repair/run_checks.py adept-asan
python3 evidence/repair/audit_instrumentation.py evidence/build-red-asan evidence/red-existing
python3 evidence/repair/audit_instrumentation.py evidence/build-green-asan evidence/green-final
python3 evidence/repair/audit_instrumentation.py evidence/build-adept-asan evidence/adept-asan
```

Final native ASan runner SHA256:
`993b0e5e60ba063e15ba47326bbc1800250e62ee1f4a0f08a212dafce62f9243`.
Final Adept ASan runner SHA256:
`a8a60b7a7865e21993e9a9ebd342c1dd361e4b270fac243f2e55f8e6fdeee7b7`.
The raw attachment includes library/source/object hashes, actual-object
instrumentation audits, compile databases, CMake caches, source patch, RED/GREEN
logs, the canonical full log, and the drivers. Source and binary identities
distinguish the pre-fix, pre-format GREEN and final GREEN results.

### Remaining limits and serial handoff

This repair changes no numerical path generation, finite-output diagnostics,
frozen history, observation/payment addressing, duplicate-date handling,
normalized shared-path MC tolerance **1e-8**, or supported execution mode.
No new named AAD/compiled/fuzzy capability, public/Python/Excel API, dependency
pin, documentation/CHANGELOG, benchmark inventory, or threshold is introduced.

Independent DAL-217 full successor acceptance is still required, including
the integrated Adept repair. This turn does not claim full Adept CTest, UBSan,
CoDiPack, XAD backend, Windows, or Python runtime acceptance. The native build
uses XAD only for its existing comparison example.
The prior Python performance gate remains **88/90, two failures**, and
duplicate-date thread `PRRT_kwDOBtahP86h0kcB` remains unresolved. No expensive
performance gate was repeated or inferred from noisy controls. DAL-218's
documentation decision, DAL-219's independent successor review, and required
current-head CI remain parent-owned gates. The PR stays draft with no F3
closing lines or merge. DAL-216 goes to `in_review` for this bounded repair,
followed by the authorized parent active-run/rerun check.

---

## Adept gradient-array repair, 2026-09-13

Repair commit: `63c134c0` on `feature/dal-200-eq-observation-slots`, directly on top of
the recorded head `4c346c04`. It repairs the current-head Linux Adept abort listed as
open in the summary above: all five Adept legs passed 1,669/1,670 CTest cases and then
aborted `ScriptCompiledParityTest.TestParity_Number_NestedIf_Fuzzy` with a glibc
`sysmalloc` allocator assertion.

Root cause, reproduced with a CI-identical static Release build (intermittent, roughly
5% per process at four threads) and pinpointed with an AddressSanitizer build: Adept's
`Stack` grows its gradient array only inside `initialize_gradients()`, while
`register_gradient()` keeps handing out indices up to the all-time high-water mark
`max_gradient_`. DAL's custom `Tape_::compute_adjoint` (`dal-cpp/dal/math/aad/tape.hpp`)
froze the array at a worker thread's first seed and never resized it. A later recording
window with more simultaneously live variables — here a branchier fuzzy path in the
compiled parity evaluation — then referenced gradient slots past the allocation. ASan
reports the heap-buffer-overflow at `tape.hpp:119`, zero bytes after the 151-double
array allocated at the first seed. The condition is latent on the base revision as well:
the F2 tree reproduces the identical overrun, so this is not introduced by DAL-200; the
restructured generation loop changed per-path registration patterns enough for CI to
observe it.

The repair is confined to the Adept backend facade: `Tape_::EnsureGradientCapacity()`
grows the gradient array on demand, preserving accumulated adjoints and zeroing the new
tail, and runs before every reverse sweep and every adjoint seed/read. Numerics are
unchanged when capacity suffices, so the non-Adept backends and the native benchmark
gate are unaffected by construction. The vendored Adept sources are not modified.

Verification on the repair commit:

- New backend-neutral regression `AADTapeTest.TestGradientCapacityGrowsAfterSeeding`:
  deterministic heap corruption (`malloc(): invalid size`) on the unrepaired base,
  exact adjoint accumulation (3.0 then 2083.0) after the repair.
- CI-identical Adept static Release build, gcc-14: full CTest **1,671/1,671 pass**; the
  previously aborting parity test ran 40 times at four threads with zero failures.
- AddressSanitizer build: 169 focused tape/parity/observation/simulation/model tests
  pass with zero sanitizer reports; the same build caught the overrun on its first run
  before the repair.
- gcc-15 Adept and native/xad/codipack focused parity/tape suites pass;
  `dal_check_generated`, changed-range clang-format and whitespace checks pass.

A separate artifact was observed only in address+undefined sanitizer builds: UBSan
reports a null `adept::_stack_current_thread` load at the first tape touch. It
reproduces identically on the F2 base, never occurs in Release or address-only builds,
and is unrelated to this PR. The Python performance acceptance above remains open and
is unchanged by this repair.

---

## Correctness and performance remediation, 2026-09-13

Incoming head: `9bc4f000554945199b8edda73b9a90b2508c65be`.
Tested production successor: `fe12a75041f6f3a9db042847a4bb76b6f952a3d1`.
F2 baseline: `ec8b0072fbf70dab814a543edc625c8e0bf77efa`.
The verification successor and final DAL-216 comment identify the delivered PR head; it adds a finite-extremes test and this report without changing production code.
Draft PR: <https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/369>.

### Duplicate-date result and justified deviation

The reported prepared legacy `t,t,T` event/sample mismatch does **not** reproduce
on the incoming production tree. The new three-date regression passed before
production edits; no behavioral RED or mapping repair is claimed for that report.
`AppendEvent` in `dal-cpp/dal/script/preprocessor.cpp` coalesces rows
in a date-keyed map and concatenates their statements in input order.
`ScriptProduct_::ParseEvents` parses each map entry once.
`ScriptProductData_::Product()` in `dal-cpp/dal/script/event.hpp` constructs a fresh
product, and `PreparedScriptBuilder_` always obtains the product through it.
Consequently `t,t,T` produces two event streams and two samples, with an identity
`EventToSample`; all-same-date produces one, and distinct dates produce three.
Repeated raw `ParseEvents` calls are a different route, excluded after preparation;
a duplicate raw model timeline also fails allocation before workers. No new
compiled restriction or nonidentity mapping API is introduced.

`TestLegacyPreparedDuplicateDates`, `TestLegacyPreparedSameDate`, and
`TestLegacyPreparedDistinctDates` exercise both tree and compiled settings through
preparation, direct evaluation of an exactly sized path, and 257 simulated paths.
Controlled sample spots are 10/20/40 and numeraires 2/4/8. Ordered statements set
`x=SPOT()`, then `x=2*x+SPOT()`, paying at each event and finally paying `x+SPOT()`.
The respective exact per-path values are 32.5, 40, and 25. Tests assert event,
compiled-stream and sample counts, identity mapping, and reject all history reads.
The independent tester must confirm this source-based disposition before the
parent resolves `PRRT_kwDOBtahP86h0kcB`.

### Production repair and scope

The full path validation traversal was measurable overhead. Exact Black-Scholes
double simulation now uses `BlackScholes_::CheckedPaths_`, an owned worker-local
snapshot of spot/log-spot, drifts, standard deviations, numeraires and scenario
storage. It initializes and validates fixed numeraires once, chooses observation
filling once for each generated path, and checks every emitted spot. Observations
are copies of that checked spot. Its public path access is const; it retains no
caller-owned model/definition storage. Worker reuse removes the redundant generic
scenario and avoids allocating snapshots for each batch. Construction rejects
unallocated, inconsistent and derived models.

The shared generation loop retains Gaussian draw order and Black-Scholes arithmetic.
For AAD, `GeneratePathAndValidate` checks actual emitted spots and numeraires during
generation, including numeraires not requested by a definition. It performs no
AAD caching across rewind marks. Exact type selection occurs outside the path
loop. Derived/custom generators still execute their virtual override once and
receive full validation of the resulting shape, including appended samples and
observations. Dupire uses that generic route.

Validation completes generation before diagnosing any invalid sample. The cold
ordered scan retains the first offending field, existing `InvalidModelPath`
messages, and `ScriptError_`. Negative finite spots, positive subnormal numeraires,
zero spot from finite underflow, NaN/Inf rejection, intermediate overflow followed
by later recovery, pre-evaluation failure, payoff checks and accepted-task draining
are covered. No correctness check is removed or made debug-only. The numerical
snapshot is double-only, so the AAD mark/rewind/propagation lifetime remains intact.
The shared helper computes initial log-spot before copying today's sample instead
of after; both consume the same parameter node and the native AAD regressions pass.
Other AAD backends require the independent tester's successor verification.

Changed files are `dal-cpp/dal/model/base.hpp`,
`dal-cpp/dal/model/blackscholes.hpp`, `dal-cpp/dal/script/simulation.hpp`,
`dal-cpp/tests/model/test_blackscholes.cpp`,
`dal-cpp/tests/script/test_observation_simulation.cpp`,
`dal-cpp/tests/script/test_simulation.cpp`, and this report.
Independent tests/tolerances, compiled bytecode, published documentation/CHANGELOG,
public/Python/Excel surfaces, dependencies, generated files, CI policy, RNG defaults
and benchmark inventory remain unchanged. Named compiled/AAD/fuzzy execution stays
rejected; prior retained-fixing and frozen-history coverage remains green.

### RED, GREEN and correctness verification

- Checked generation RED: building the new focused model regression failed because
  `GeneratePathAndValidate` did not exist (`red-checked-generation.log`). Minimum
  implementation GREEN passed the focused test (`green-checked-generation.log`).
- Owned snapshot RED: `cmake --build build/Release-linux --target dal_cpp_tests -j8`
  exited 2 because `CheckedPaths_` did not exist (`red-owned-paths.log`). GREEN
  `--gtest_filter=ModelTest.TestBlackScholesOwned*` passed the initial snapshot test.
- Allocation guard RED: the new
  `ModelTest.TestBlackScholesOwnedPathsRequireConsistentAllocation` exited 139 on
  an unallocated model (`red-owned-allocation.log`). After adding the guard, the
  same command exited 0; expanded/shrunk definition storage also throws.
- Final Release build exits 0. Focused script/preparation/observation/model,
  legacy double/AAD, tree/compiled and task-draining suites pass **357/357**.
  Full CTest passes **1678/1678** (core, public and portable Excel contracts).
- GCC ASan+UBSan build and run pass **75/75** model, simulation and observation
  cases, including all three duplicate-date regressions. The three test translation
  units and relevant inline generation/evaluation/simulation code are instrumented;
  the linked static DAL library and GoogleTest are the regular Release build.
  This is focused memory-safety evidence, not a fully instrumented library claim.
- Fresh isolated Python tests pass **402/402** on F2 and **402/402** on the final
  candidate. The current head benchmark smoke tests also pass **109/109** on F2.
- `dal_check_generated` writes zero files and passes. Changed-range clang-format,
  whitespace and submodule-scope checks pass. Lizard 1.23 reports new/changed
  helpers at complexity 1–6, `MCDoubleSimulation` at 8; the existing AAD entry point
  remains 13. No suppressions or policy changes.

Final native commands (from the repository root):

```bash
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF
cmake --build build/Release-linux -j8
DAL_NUM_THREADS=4 build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationSimulationTest.*:ScriptFixingPreparationTest.*:ScriptObservationTest.*:ScriptTest.*:SimulationTest.*:ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*:PastEvaluatorTest.*:ModelTest.*'
DAL_NUM_THREADS=4 ctest --test-dir build/Release-linux --output-on-failure -j8
cmake --build build/Release-linux --target dal_check_generated -j8
```

Sanitizer command:

```bash
g++ -std=c++17 -O1 -g -DNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer -I dal-cpp -isystem dal-cpp/externals/googletest/googletest/include dal-cpp/tests/script/test_observation_simulation.cpp dal-cpp/tests/script/test_simulation.cpp dal-cpp/tests/model/test_blackscholes.cpp dal-cpp/tests/test_main.cpp build/Release-linux/dal-cpp/libdal_cpp.a build/Release-linux/lib/libgtest.a -lpthread -o ../remediation-evidence/final-paths-asan
DAL_NUM_THREADS=4 ASAN_OPTIONS=detect_leaks=0 ../remediation-evidence/final-paths-asan --gtest_filter='ScriptObservationSimulationTest.*:SimulationTest.*:ModelTest.*'
```

### Performance attribution and reproducibility

The CI synthetic merge `e635be54e5c3cad432295a18c6767d2588ad8bd8` has F2 and the
incoming head as parents. GitHub's commit API verifies its tree
`ad91a695a9a79e7a157d240a874f527e5e74d6f4` equals the incoming head tree; the retained
CI data therefore measures that production content. It fails **13/90 total cases**,
all among the **16 MC cases**, not 90 MC cases. Barrier comparison Greeks use
repeated compiled double bumps. All 74 non-MC CI cases pass. Retained CI A/A
vanilla-tree instability does not explain every paired MC regression.

Local builds use isolated F2, incoming and candidate worktrees; GCC/G++ 14,
Release, native architecture ON, native AADET, external AAD backends OFF,
CPython 3.13 and identical interpreter/configuration, DAL_NUM_THREADS=4.
The host is WSL2 on an Intel Core i9-13900HX and is a noisy environment; timing
controls and both positive and negative excursions must be retained. Correctness
builds use GCC 15.2. Final manifests record full compiler/Python/CMake versions,
SHA/blob/SHA256 identities, pinned submodules and every native module identity.
The existing public/core compile-flag difference is preserved on both sides.
No installed or remotely archived binaries were executed.

Initial local reproduction fails 5/90: barrier comparison Greeks 16384
+7.33/+7.91%, Greeks 65536 +10.72/+9.80%, barrier compiled double +22.52/+15.42%,
barrier tree double +15.04/+9.85%, vanilla compiled double +8.39/+9.78%.
A fast validity accumulator and separated diagnostics alone did not consistently
improve MC performance. A **diagnostic-only** omission of validation in the isolated
candidate (never the published tree) improved compiled barrier by 7.99/10.38%
against incoming code, establishing traversal cost; vanilla effects were mixed.
The patch is retained with its unsafe diagnostic label, not proposed as a fix.

Fusing checks alone left four full-gate failures. Moving the checked double method
into a core explicit specialization did not remove the barrier regression and was
abandoned. Owned snapshots improved the controlled compiled-barrier comparison to
+3.98/−0.17% versus F2 and vanilla to −12.35/−11.33%. The first full owned-snapshot
comparison passed all long barrier gates but failed short vanilla price 16384
(+12.06/+9.71%). The final version reuses the snapshot in worker state and removes
the redundant scenario allocation. These are distinct recorded source changes,
not blind reruns of unchanged binaries. All diagnostic patches, raw results and
failed setup/build attempts are retained. One early comparison stopped at zero
cases because compiler cache spellings differed; both were normalized to the same
G++ 14 compiler before further measurement.

Every acceptance run uses the unchanged current complete inventory, ten samples per
side, two alternating rounds, best-of-ten minima and the strict +4% rule in both
rounds. Diagnostic subsets are labeled and never substituted for final coverage.
`final-verification-commands.json` records exact executed commands and exit codes.
`final-source-identity.json` and the gate manifests identify the clean production
SHA `fe12a75041f6f3a9db042847a4bb76b6f952a3d1` and actual build/module hashes.
Raw logs, reports, controls and source patches are delivered in the attached
remediation archive. Runtime-local paths in the logs are provenance, not links to
delivered files.

The later bitwise finite-value reduction was also rejected: its isolated
compiled-barrier comparison regressed +9.74/+8.55% versus F2. The first subset
launch overlapped a finishing native test build and is explicitly excluded in
`bit-reduction-overlap-note.md`; the isolated result, not that confounded run,
is used for the decision. No bitwise or floating-sum validation is published.

The owned-generator core specialization was also abandoned: its isolated
compiled-barrier deltas (+10.24/+3.73%) did not demonstrate a repeatable improvement.
The saved disassembly confirms native FMA generation, but this is not sufficient
performance evidence to justify changing the linked implementation. The measured
`fe12a750` source was restored; all nine native executable hashes still match the
successful native gate. The final additional finite-exponent regression passes on
that unchanged production code, including ordinary 1/4/16 spots and valid spots
near the maximum finite double. No behavioral RED is claimed for that extra
positive coverage or for the discarded performance-only ablations.

### Final measured outcome: performance acceptance remains open

The final complete F2-to-`fe12a750` Python gate exits **1**: **88/90 pass**.
These two workloads still fail the unchanged rule:

| Remaining workload                 | Round 1 | Round 2 |
|------------------------------------|---------|---------|
| comparison.mc_barrier_greeks_65536 | +4.35%  | +5.91%  |
| mc.barrier.double.tree             | +4.17%  | +4.70%  |

The other 14 MC cases and all 74 non-MC cases pass. Compiled barrier double is
−0.39/−3.47%; barrier Greeks 16384 is +1.86/+2.06%. These results improve on the
reproduced incoming hot-path failures, but they do **not** meet complete Python
performance acceptance. No green or resolved-performance claim is made.

Final same-binary Python A/A passes 90/90, but remains noisy in both directions:
barrier Greeks 65536 is −5.10/+1.21%; barrier double/tree is +2.47/+0.57%; barrier
Greeks 16384 is +9.81/+1.76%; short vanilla price is +25.43/+3.45%. The baseline A/A
fails vanilla compiled double at +4.06/+4.16%, and has other one-round excursions.
These controls limit precision and prohibit crediting the approximately 50%
vanilla-tree speedup to this repair. They do not erase the two failed paired cases.

The unchanged nine-target native gate exits **0**, with no failures at 10 samples
per side × 2 rounds and +4%. The informational `script_mc_perf` run also exits 0;
it is not counted as a tenth native gate target.

The implementer delivers the validated source repair and unresolved performance
measurements for the existing independent tester, documentation reconciliation
where needed, and independent reviewer. Repeat the complete comparison and controls
on a second controlled host at the successor production blobs; determine whether
the two residual cases are stable before accepting this requirement or routing
further production repair. The draft PR and duplicate-date review thread remain
open. No F3 acceptance, thread resolution, required-CI success, merge or deferred
F4 capability is claimed.

---

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
