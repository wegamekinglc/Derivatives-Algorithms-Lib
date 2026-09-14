# DAL-202 F5 independent fuzzy arithmetic verification

Fresh independent verification of the fuzzy arithmetic correction passes.
No actionable production defect was reproduced. This report supersedes the
preceding testing report; parent acceptance, documentation correction and
mandatory independent re-review remain required.

## Revisions and scope

- Input head: `73658a5ffe8d57f009c9d6d5aba1abe4e80435cf`.
- Input tree: `03eaa35a8dcd91a5d7354102f8b00c707a4e63ee`.
- Tested test commit: `1e1e04a7cfdff126db330a2292f38bc3f93954dc`.
- Tested tree: `0cff8c9308c0205d83494d3c86165718aa7fca6a`.
- Production implementation: `f2e6cc353b0ae1f10cf24eb226f92d6a21645ba5`.
- Existing [draft PR #372](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/372),
  branch `feature/dal-202-compiled-observations`, base
  `feature/dal-201-historical-aad-state`, dependencies #371/#369.
- Fixed F4 ancestor: `5c954ca2fdded2ca35ad15c7fef44d90a002007d`.

Initial checkout was clean and matched the requested head/tree and GitHub PR.
Authenticated implementation archive SHA256:
`f1d3ded23bde9782688cfcbb8c60c96f6c2c165a5f4c67b449d70e0942268bb8`.
All 73 internal hashes, six supplied tested-source hashes, and attached
implementation report equality with Git were independently verified. Those
predecessor logs establish provenance only, not the fresh results below.

This tester changes only `dal-cpp/tests/script/test_fuzzy_arithmetic.cpp` and
this report. Publication adds only this report after the tested test commit.
Attached `publication.json` records exact published SHA/tree, source equality,
clean status and PR identity, avoiding a self-referential commit hash here.
No production, public documentation, configuration, dependency, tolerance, gate
or performance change is included.

## Running existing tests

Fresh GCC 15.2.0/CMake 4.2.3 Release results, including the new test:

- Full native Linux configure/build/install/CTest: **1743/1743**, `full-linux.log`.
- Native targeted script/simulation/AAD/compiler/visitor: **453/453**.
- Adept targeted: **453/453**.
- CoDiPack targeted: **453/453**.
- XAD targeted: **452/452**, retaining its existing backend exclusion.
- All **33 ScriptCompiledParity/ScriptCompiledParityFuzz cases** pass on each
  backend: 30 legacy-source cases and 3 F5 observation cases.
- Unchanged fuzzy-divisor reviewer reproduction: **6/6**, `reviewer-fuzzy.log`.
- Unchanged exact-boundary reviewer reproduction: **4/4**, `reviewer-exact.log`.
- Isolated native-double allocation fixture: **2/2**, `allocation.log`.
- Documentation structure checker: **70 Markdown files**, `docs-check.log`.
- Changed-file clang-format and patch whitespace checks: pass.

Fresh full-workflow summary:
`100% tests passed, 0 tests failed out of 1743`, with 14.50 seconds of CTest.
Core/public/portable Excel contracts run; examples build/install. Python and
benchmarks are disabled. Targeted inventories/XML retain every matched case;
All alternate inventories omit native-only
`AADTest.TestDefaultNumberAdjointRequiresTapeNode`. Adept additionally runs
`ScriptTest.TestCompiledAadOperandStacksHaveEvaluationStateLifetime`; CoDiPack
additionally runs `AADTest.TestCoDiPackTapeIsPerThreadAcrossThreadLifetimes`.
XAD has no additional case. These are existing conditional tests; no F5 case
is omitted. The attached suite audit records the exact differences.
Standalone reviewer/allocation fixtures are outside the CTest count.

## Authoring coverage and regression sensitivity

Added one test,
`ScriptFuzzyArithmeticTest.TestNestedFractionalStateAcrossEventsAndWidths`.
Six fixtures combine two signed tiny divisors with three epsilon settings:
omitted width/default smooth=0.01, omitted width/smooth=0.2, and explicit
width=0.2 overriding smooth=0.01. A first future event computes x through the
tiny divisor, nests two fractional IFs, and carries y to a later payment event.

Both conditions have w=0.75 at x=epsilon/4. The nested true arm assigns x or
2*x; the outer false arm assigns 3*x. Independently,
`y=x*(3-w-w*w)`, hence `PV=0.421875*epsilon` and
`d_SCALE=0.1328125*epsilon` at SCALE=2. Rate risk is
`-10/DAYS_PER_YEAR*PV`; spot/vol/div risks are zero, with exactly five labels.
Both prepared fuzzy-double paths and both AAD paths must match those formulas.
AAD runs 257 paths. Central differences of the unoptimized reference at
SCALE=2 +/- 0.0001 check the analytic SCALE derivative to 1e-10.

The reference helper now visits all future events and accepts the selected
smoothing width. It uses model-free PrepareScript for parsing, indexing and
sealed observations, then IFProcessor solely for nesting/affected-variable
metadata. It performs no domain/constant optimization. Analytic checks precede
prepared evaluation, so shared optimized tree/compiled agreement cannot hide
a wrong branch weight or reference metadata error.

The inherited four tests and 25 signed literal/known/computed fixtures retain
all assertions, including adjacent floats around 2e-14, tiny products,
fractional PV=0.0375/d_SCALE=0.025, and live denominator derivative=-0.5.

Independent RED links original `preparation.cpp` from `00e96f9a` before the
repaired library. The new narrow test passes its raw analytic/finite-difference
checks, then fails with `Division by {0}` (`red-nested.log`). All five final
arithmetic tests fail against original preparation (`red-all-fuzzy.log`).
The new test and full suite pass on repaired native/Adept/CoDiPack/XAD.
This characterizes regression sensitivity of an already implemented repair;
the tester did not author the production fix.

## Repairing failures

No production defect or failing-test repair was needed. Expected RED failures
remain in evidence; they are not failures of the delivered revision. Existing
oracles/tolerances were not weakened. An initial archive verification attempt
used nonexistent `manifest.sha256`; verification succeeded using its actual
`SHA256SUMS`. There was no previous root `test_output.txt` to remove.

## F5 acceptance evidence

- **Exact boundary repair:** eight committed exact-folding tests and four
  original reproductions pass. Unoptimized hard-tree/analytic oracles retain
  six operators at 80 and adjacent floats, signed zero/subnormals and +/-1e-14,
  signed factor risks, tiny future divisors and nested cross-event state.
- **T06/T09/T10/T21:** same-path observation cases assert 200/160/120/160 with
  retained F=120 and unrelated samples=999. Artifact lifetime survives prepared
  destruction. Zero-Gaussian discounted formulas check primal and every risk.
  Named/legacy shared-path adapters remain covered.
- **T18:** historical SCALE*H checks PV=160*exp(-r*T), SCALE=80*exp(-r*T),
  rate=-T*PV, zero model risks and no fixing-risk key.
- **T19/T20:** direct seed/constant roots pass; hard historical decisions
  below/at/above thresholds retain selected arithmetic risks. Past PAYS are
  evaluated/discarded. Nonlinear historical parameter repricing and rebuilt
  preparation retain independent analytic oracles.
- **T22:** five strikes in the 0.2 band, both execution paths and 8193 paths
  each, retain analytic weights and SCALE/K/rate risks. AAD primal matches
  same-epsilon fuzzy double; smooth K central differences pass. The new test
  adds default/explicit widths and nested cross-event fractional metadata.
- **T23:** all 27 combinations remain: threads {1,2,4}, H sequence {80,90,80},
  discounted/direct/constant roots and 8193 paths. Double/AAD price and
  SCALE/rate/spot/vol assertions are unchanged. Fresh Lizard gives complexity
  **4** at test lines 179-197 and **1** for CheckCompiledRoot at lines 37-48.
  Separate BS/Dupire full-recording-per-path oracles retain all risks across
  batches and repeated calls. No remote Codacy acceptance is claimed.
- **T16/T27:** nonexpired dead-branch missing history fails across exact/fuzzy
  tree/compiled preparation. Eager AND/OR is challenged with absent runtime
  observation storage. SPOT/FIX binding, deduplication, today policy and legacy
  compatibility cases pass.
- **T31:** history/model/compilation failures submit zero workers; double and
  AAD compiler-failure entry points recover. Path-failure tests drain accepted
  batches before returning errors.
- **T32:** throwing history/index seams and stable evaluator/observation storage
  pass. Isolated allocation instrumentation detects ordinary/aligned positive
  controls, then measures zero C++ allocations during 8193 exact/fuzzy
  tree/compiled double evaluations, including the first. Source inspection
  confirms checked direct history/scenario indexing in ObservationPlan_::Read.

Production inspection confirms syntax-wide collection/prefetch precedes
execution. Both prepared exact and fuzzy paths retain parsed branches.
The removed ProcessFuzzyDomains pass is unnecessary for parsed continuous
kernels/default-or-explicit epsilon. Hard history, ConstVar inputs, ordinary
constant arithmetic, final IF metadata and compilation remain in preparation.
Original smoothing kernels and legacy domain preprocessing are unchanged.

Existing deterministic tolerance remains relative 1e-12, analytic risks 1e-10,
and normalized same-path MC comparisons 1e-8. Existing fuzzy K finite
differences retain 1e-5. New bounded small-price checks use absolute 1e-12 and
risk/central-difference 1e-10; no hard-switch derivative is claimed.

## Reproduction and evidence

Run from the repository root:

```bash
git submodule update --init --recursive
NUM_CORES=8 bash ./build_linux.sh > test_output.txt 2>&1
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptFuzzyArithmeticTest.*'
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
```

Alternate builds use Release-linux with `-DDAL_USE_ADEPT_AAD=ON`,
`-DDAL_USE_CODIPACK_AAD=ON` or `-DDAL_USE_XAD_AAD=ON`, examples disabled,
target dal_cpp_tests and the identical filter. Attached `commands.md` and
`run-targeted.sh` contain exact configure/build/test, standalone reviewer,
allocation, RED and publication commands. Evidence includes fresh logs,
XML/inventories, source/binary hashes, caches, revision metadata, the test patch
and SHA256SUMS. Archive and checksum are attached to DAL-229.

## Limits and handoff

No Windows XLL, Python, sanitizer, full alternate-backend public/Excel suite or
performance run is claimed. Linux examples were built/installed, not exhaustively
executed. Native-double C++ allocation instrumentation does not measure AAD tape
allocations, arbitrary malloc or every dynamic observation-load count. The
compiler-failure seam injects immediately before bytecode construction.

DAL-223 performance remains deferred. DAL-230 owns stale ProcessFuzzyDomains
and domain-analysis descriptions in `docs/methodology/script_engine.md`, plus
the CHANGELOG decision. Documentation checking validates structure only.
One post-push CI snapshot is evidence of that moment, not final gate acceptance;
no watch/poll, merge, close intent or dependency rewrite is performed.
DAL-229 returns in_review for parent acceptance, then sequential documentation
and mandatory DAL-231 re-review. No peer or F6 is started by this tester.
