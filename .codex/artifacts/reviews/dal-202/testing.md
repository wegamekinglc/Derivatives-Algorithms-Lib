# DAL-202 F5 independent repair verification

Fresh independent Linux verification passes on the repaired production code.
No actionable production defect was reproduced. This report supersedes the
earlier testing report for repair acceptance; parent acceptance, documentation
correction and independent re-review remain required.

## Revision and scope

- Input head: `4e90369d7fcb9db9a24483d4741e3fb166c91a6e`.
- Input tree: `a4d86d608672b30003a4196fc31b8e5279d92c91`.
- Tested test commit: `cd33eb794925eea9bc46a202069a3355fb48f994`.
- Tested tree: `3e56d8cfa49de06aad5b1512cec13ca45b8d9b72`.
- Existing [draft PR #372](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/372):
  `feature/dal-202-compiled-observations`, base
  `feature/dal-201-historical-aad-state`, dependencies #371/#369.
- Fixed F4 ancestor: `5c954ca2fdded2ca35ad15c7fef44d90a002007d`.

The input checkout and GitHub PR agreed on head identity. Publication adds only
this report after the tested test commit. Exact published SHA/tree and GitHub
identity are recorded in the attached `publication.json`, avoiding a
self-referential commit hash here. All production files and dependency pointers
are unchanged from the input. Only these files change in this tester delivery:

- `dal-cpp/tests/script/test_exact_folding.cpp`: one test and its helper.
- `.codex/artifacts/reviews/dal-202/testing.md`: this replacement report.

Read current DAL-229/DAL-202 descriptions and bounded comment history, DAL-228's
repair thread, repository tester/test/style/publication contracts, relevant
script/AAD methodology, production APIs and nearby tests. Downloaded repair
attachments through the authenticated CLI. Archive SHA256
`811e3f9891c4cf83dd1a5d919af27d1eabad098b404df2f818358d953d1c8647`
and all 60 internal entries verified. The original-preparation source used below
also matches Git at `2dc4d43a913ed60779920e1392d1411f16fb23b6`.

## Running existing tests

Inherited implementer counts were full native 1737, native/Adept/CoDiPack 447,
XAD 446, allocation 2, docs 70. Those results are historical inputs only.
The following results were captured afresh with GCC 15.2.0, CMake 4.2.3,
Release configuration and pinned submodules:

- Full native Linux configure/build/install/CTest: **1738/1738**,
  `full-linux.log`.
- Native targeted script/simulation/AAD/compiler/visitor: **448/448**,
  `targeted-native.log`.
- Adept targeted: **448/448**, `targeted-Adept.log`.
- CoDiPack targeted: **448/448**, `targeted-CoDiPack.log`.
- XAD targeted: **447/447**, `targeted-XAD.log`.
- Focused eight exact regressions plus compiled lifetime test: **9/9**,
  `focused-repair.log`.
- Unchanged reviewer standalone reproduction: **4/4**,
  `reviewer-green.log`.
- Legacy ScriptCompiledParity/ScriptCompiledParityFuzz: **33/33**,
  `legacy.log`.
- Isolated allocation fixture: **2/2**, `allocation.log`.
- Documentation checker: **70 Markdown files**, `docs-check.log`.
- Changed-line clang-format and patch checks: pass. Lizard 1.23.0 output is
  `complexity.log`; no threshold or gate was changed.

The full-workflow summary is freshly captured:
`100% tests passed, 0 tests failed out of 1738` (14.79 seconds of CTest).
It includes core/public/portable Excel contracts and builds/installs examples;
benchmarks and Python are disabled. The targeted filter matches 18 suites.
Per-backend case inventories verify that XAD's only omission is the existing
`AADTest.TestDefaultNumberAdjointRequiresTapeNode`; no F5 case is omitted.
The standalone reviewer and allocation fixtures are outside the CTest count.

## Authoring coverage and characterizing the old failure

Added `ScriptExactFoldingTest.TestNestedFutureStateAcrossEventsRetainsSignedRisks`.
A first future event assigns `x=SCALE*H`; a later event nests `x>0` inside
`x!=0`, assigns `x*x` on the positive branch or `3*x` on the negative
branch, and pays the result. Eight signed factor combinations use
H=±1e-14/SCALE=±1e14 and H=±1e14/SCALE=±1e-14. Every combination checks tree and
compiled double/AAD pricing over 257 paths with nonzero rate 0.03.

The independent oracle is
`g(x)=x*x` for positive x and `g(x)=3*x` for negative x.
PV is `g(x)*exp(-r*T)`; SCALE risk is `g'(x)*H*exp(-r*T)`;
rate risk is `-T*PV`, T=10/DAYS_PER_YEAR. Spot/vol/div risks are zero,
and exactly five risk labels must exist. Branch inputs are ±1, outside the
fuzzy bands. The new test checks retained cross-event state, nested IF metadata,
live parameter inputs and both nonzero branch sensitivities, rather than relying
on tree/compiled agreement.

The narrow standalone run linked the original preparation definitions before
the repaired static library. It failed with actual PV **6.994248939115905**
against expected **-2.997535259621102** for H=-1e-14 and SCALE=1e14
(`red-new-test.log`). The original source incorrectly selected the zero arm.
The same new test passes against repaired preparation (`green-new-test.log`)
and in all four backend suites.

The same source-substitution characterization makes **all eight** exact tests
fail with original preparation (`red-all-exact.log`); all eight pass with the
repair. This is supplemental regression sensitivity evidence on a completed
implementation, not a claim that this tester authored the production repair.
No production fixes or failing-test repairs were made. No existing assertion,
tolerance, epsilon, gate or performance threshold was weakened. The only local
setup corrections were using the extracted archive directory for its manifest
check and supplying the repository's existing Codex identity for the commit.

## F5 acceptance evidence

- **Exact boundary soundness:** existing hard-reference tests parse/index/seal
  through model-free preparation and directly evaluate the unoptimized hard tree,
  independently of the repaired model-aware optimization. All six operators at
  80 and its two adjacent floats, plus signed zero, signed minimum subnormals
  and ±1e-14 around zero, give **54 comparison cases**. Signed nonzero parameter
  factors, selected arithmetic risks and the small future-state divisor pass.
  This adds hard-tree and analytic oracles to evaluator parity.
- **T06/T09/T10/T21:** `TestIndexFixingsSamePathAndArtifactLifetime` supplies
  F=120 and unrelated samples=999, checks mixed/repeated/retained/state outcomes
  200/160/120/160, and verifies artifact lifetime after prepared destruction.
  `TestIndexFixingsAnalyticPathRisks` independently checks all five risks with
  zero-Gaussian lognormal/discount formulas. Named/legacy shared-path adapter
  tests and repeated thread/batch scenarios also pass.
- **T18:** parameter replay tests check PV=160*exp(-r*T),
  SCALE=80*exp(-r*T), rate=-T*PV and zero spot/vol; label counts exclude a fixing
  risk. Both tree and compiled paths run.
- **T19/T20:** direct-seed and constant roots pass; hard historical decisions
  below/at/above 80 preserve branch arithmetic, including compiled replay and
  100 discarded past PAYS. Nonlinear historical repricing changes SCALE and
  strike and checks analytic risks after rebuilding preparation.
- **T22:** five strikes {79.93,79.95,80,80.03,80.07}, each tree/compiled at 8193
  paths, retain `w=(80-K+0.1)/0.2`. Primal, SCALE, K and rate risks match
  independent formulas; AAD primal matches same-epsilon fuzzy double, and
  central differences at h=0.0001 verify K risk. No hard-switch derivative is
  claimed. Existing literal-fuzzy and live-parameter regressions also pass.
- **T23/P2:** inspected the complete lifetime refactor against its predecessor.
  All **27** combinations of threads {1,2,4}, fixing sequence {80,90,80},
  and discounted/direct/constant roots remain, each with 8193 paths, both
  double/AAD prices and SCALE/rate/spot/vol assertions. Fresh Lizard reports
  complexity **4**, helper **1**. Separate BS/Dupire full-recording-per-path
  references compare primal and every risk with mark/rewind across 8193 paths,
  threads 1/2/4 and repeated calls. This does not establish remote Codacy status.
- **T16/T27:** nonexpired constant-dead-branch history failures remain strict
  across exact/fuzzy and tree/compiled. Eager AND/OR is challenged with missing
  runtime observation storage even when the left operand determines truth.
  Bound SPOT/FIX deduplication, mixed/unbound SPOT errors, today policy and all
  33 legacy parity/fuzz cases pass.
- **T31:** last-history/model/compilation failures submit zero workers.
  Compilation injection exercises both double and AAD entry points and recovery.
  Path-failure tests drain accepted batches before returning an error.
- **T32:** worker-local throwing history/index seams and stable storage pass.
  The isolated probe detects temporary ordinary/aligned allocations in its
  positive control. Each exact/fuzzy tree/compiled double evaluator completes
  8193 evaluations, including the first, with zero C++ allocation requests and
  independent expected payoffs. Source inspection of `ObservationPlan_::Read`
  confirms bounds checks and direct history/scenario indexing without lookup or
  an observation allocation.

Read-only inspection confirms exact preparation omits DomainProcessor_ and
ConstCondProcessor_, while the fuzzy helper retains the prior processing body.
Syntax-wide collection/prefetch occurs first; hard historical replay, live
ConstVar inputs, constant arithmetic, final IF metadata and compilation remain
before workers. Existing constant-processor/compiler arithmetic and opcode tests
pass. Prepared mode/epsilon mismatch tests remain enabled.

Existing deterministic primal tolerance is relative 1e-12, ordinary analytic
risk tolerance 1e-10, and normalized same-path MC tolerance 1e-8. Small/large
factor risks use magnitude-relative 1e-12 (including the new test), detecting
loss of tiny nonzero sensitivities. The existing fuzzy finite-difference
tolerance remains 1e-5.

## Reproducible commands

Run from the repository root with `../evidence` created. The fresh checkout
had no old `test_output.txt`; the full workflow created it and it was copied
to `full-linux.log`.

```bash
git submodule update --init --recursive
NUM_CORES=8 bash ./build_linux.sh > test_output.txt 2>&1
cmake --preset=Release-linux -S . -B build/Adept -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_ADEPT_AAD=ON
cmake --preset=Release-linux -S . -B build/CoDiPack -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_CODIPACK_AAD=ON
cmake --preset=Release-linux -S . -B build/XAD -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_USE_XAD_AAD=ON
cmake --build build/Adept --target dal_cpp_tests -j6
cmake --build build/CoDiPack --target dal_cpp_tests -j6
cmake --build build/XAD --target dal_cpp_tests -j6
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptExactFoldingTest.*:ScriptPastReplayTest.TestCompiledBatchLifetimeAndDirectRoots'
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*'
```

The first filter was also run with Release-linux replaced by Adept, CoDiPack
and XAD. Logs and exact matched case lists are attached.

Standalone commands, using the source files included in the evidence archive:

```bash
c++ -O3 -DNDEBUG -std=c++17 -fPIE -pie -Idal-cpp -Idal-cpp/externals/googletest/googletest/include ../evidence/preparation-before.cpp dal-cpp/tests/script/test_exact_folding.cpp dal-cpp/tests/test_main.cpp build/Release-linux/dal-cpp/libdal_cpp.a build/Release-linux/lib/libgtest.a -pthread -o ../evidence/exact-before
../evidence/exact-before --gtest_filter='ScriptExactFoldingTest.TestNestedFutureStateAcrossEventsRetainsSignedRisks'
../evidence/exact-before --gtest_filter='ScriptExactFoldingTest.*'
c++ -O3 -DNDEBUG -std=c++17 -fPIE -pie -Idal-cpp -Idal-cpp/externals/googletest/googletest/include dal-cpp/tests/script/test_exact_folding.cpp dal-cpp/tests/test_main.cpp build/Release-linux/dal-cpp/libdal_cpp.a build/Release-linux/lib/libgtest.a -pthread -o ../evidence/exact-after
../evidence/exact-after --gtest_filter='ScriptExactFoldingTest.TestNestedFutureStateAcrossEventsRetainsSignedRisks'
c++ -O3 -DNDEBUG -std=c++17 -fPIE -pie -Idal-cpp -Idal-cpp/externals/googletest/googletest/include ../evidence/repro_exact_boundaries.cpp dal-cpp/tests/test_main.cpp build/Release-linux/dal-cpp/libdal_cpp.a build/Release-linux/lib/libgtest.a -pthread -o ../evidence/reviewer-repro
../evidence/reviewer-repro
c++ -O3 -DNDEBUG -std=c++17 -fPIE -pie -ffp-contract=fast -Idal-cpp -Idal-cpp/externals/googletest/googletest/include dal-cpp/test-support/test_script_observation_allocations.cpp dal-cpp/test-support/bcg_allocation_probe.cpp dal-cpp/tests/test_main.cpp build/Release-linux/dal-cpp/libdal_cpp.a build/Release-linux/lib/libgtest.a -pthread -o ../evidence/allocations
../evidence/allocations
python3 -m lizard dal-cpp/tests/script/test_past_replay.cpp dal-cpp/tests/script/test_exact_folding.cpp
python3 .github/scripts/check_docs.py
git clang-format --diff 4e90369d -- dal-cpp/tests/script/test_exact_folding.cpp
git diff --check
```

## Limits and handoff

No Windows XLL, Python, sanitizer, full alternate-backend public/Excel suite or
performance run is claimed. Linux examples were built/installed, not exhaustively
executed. T32 measures native double C++ allocations, not AAD tape allocations,
arbitrary malloc calls, or dynamic counts of every observation load. The compiler
failure seam injects immediately before bytecode construction, not within every
possible failing opcode.

Retaining exact branches has unmeasured execution cost; DAL-223 stays deferred.
Public methodology still describes exact domain pruning and must be revisited
by DAL-230, along with its CHANGELOG decision. No documentation/production/API
or configuration change is made here.

The evidence archive includes fresh logs, source/revision hashes, the test patch,
inherited-input verification, per-backend inventories and publication metadata.
One published-head CI snapshot is captured without watching or polling; it is
not remote-gate acceptance. DAL-229 is delivered for parent acceptance, followed
sequentially by documentation and DAL-231 re-review. No merge, close intent,
dependency update, peer start or F6 start is part of this delivery.
