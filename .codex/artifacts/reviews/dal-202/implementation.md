# DAL-202 F5 implementation correction

DAL-228 repairs both DAL-231 Request Changes findings. This report supersedes
the earlier implementation report for correction acceptance. Earlier tester,
documentation and review reports concern predecessor revisions; independent
testing, documentation review and re-review of this repair remain required.

## Revision and scope

- Starting head: `2dc4d43a913ed60779920e1392d1411f16fb23b6`.
- Starting tree: `4b3f05c0dec568511161d3f5d6e2b30bd758e86f`.
- Tested code commit: `8f4e45e2fe7e62bfa44e5c0d7e72443654ba9a48`.
- Tested code tree: `f2fab8352974e8ae0ce6a8f764f6bfd610a22a4a`.
- Publication adds only this report. The attached `published-revision.json`
  records final SHA/tree, matching GitHub head and tested file hashes.
- Existing [draft PR #372](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/372)
  remains on `feature/dal-202-compiled-observations`, based on
  `feature/dal-201-historical-aad-state`, dependent on #371/#369. Fixed F4
  ancestor remains `5c954ca2fdded2ca35ad15c7fef44d90a002007d`.

Changed files:

- `dal-cpp/dal/script/preparation.cpp`: bypass exact tolerance-domain pruning;
  extract the unchanged fuzzy pass into `ProcessFuzzyDomains`.
- `dal-cpp/tests/script/test_exact_folding.cpp`: seven price/risk regressions.
- `dal-cpp/tests/script/test_past_replay.cpp`: explicit scenario/oracle data and
  a shared assertion helper replace kind-dependent conditional expressions.
- `.codex/artifacts/reviews/dal-202/implementation.md`: this report.

No binding, archive, settings, generated enum, dependency branch, configuration,
public documentation or performance changes are included.

## Design and behavior

P1 arose because `Domain_` uses tolerance-based signs, equality and arithmetic.
Adjacent floats can compare equal, and a small nonzero factor can collapse an
unbounded parameter domain to zero. Changing only the final comparison would
leave unsound arithmetic proofs.

Exact model-aware preparation now retains parsed branches and omits both
`DomainProcessor_` and its dependent `ConstCondProcessor_`. Strict syntax-wide
prefetch, hard historical replay, dependency-aware constants, final IF metadata
and compilation before workers still run. The compiler can fold proven constant
arithmetic using actual double operators and hard comparisons; script parameters
remain live inputs. Eager AND/OR semantics remain intact.

Fuzzy preparation executes the same passes and continuous kernels through the
extracted helper. Legacy preprocessing, typed historical seeds, hard historical
decisions and settled PAYS behavior are unchanged. No global tolerance changed.

This is an authorized conservative implementation choice, with no public
semantic deviation. Exact prepared trees may retain more branches and execute
more comparisons. No performance impact is quantified; DAL-223 remains deferred.

P2 retains threads `{1,2,4}`, fixing sequence `{80,90,80}`, 8193 paths,
discounted/direct/constant roots, double and AAD prices, and SCALE/rate/spot/vol
assertions. `ASSERT_NO_FATAL_FAILURE` propagates helper failures. Lizard 1.23.0
measures complexity 10 before and 4 after; `CheckCompiledRoot` is 1. `Prepare`
is 8 and the extracted fuzzy helper is 3. This is local evidence, not remote
Codacy acceptance. No gate was disabled or weakened.

## Independent regression oracles

`CheckHardReference` uses model-free preparation only for parsing, indexing and
sealed observations, then directly visits unoptimized future statements with
a separate hard evaluator, zero initial variables and the script's parameters.
Every exact production price must match that reference and an independent
expected value; tree/compiled parity is not the oracle.

At zero rates, branches paying 160/0 verify all supplied reproductions:

- `H > 80`, H=`nextafter(80,+infinity)`: 160.
- `H >= 80`, H=`nextafter(80,-infinity)`: 0.
- `H = 80`, H=`nextafter(80,+infinity)`: 0.
- `SCALE*H > 0`, SCALE=1e14, H=1e-14: 160 in exact and AAD modes.

The adjacent matrix checks all six comparisons at 80 and its neighboring floats,
and at zero with signed zero, signed subnormal minima and signed 1e-14 fixings:
54 combinations with hard-reference and exact tree/compiled assertions. Eight
signed factor cases combine H=±1e-14 with SCALE=±1e14 and H=±1e14 with
SCALE=±1e-14. A future assignment followed by an IF gives `max(SCALE*H,0)`;
SCALE risk is H on the selected branch, otherwise zero. Risk tolerance
`abs(H)*1e-12` detects a lost small derivative. A separate exact regression
covers a 1e-14 divisor in future state.

## RED, GREEN and refactor

Evidence paths below are relative to the repair archive; commands run at the
repository root with `../repair` as the evidence directory.

First added only the strict-positive-ULP production regression and its helpers:

```sh
cmake --build build/Release-linux --target dal_cpp_tests -j8
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptExactFoldingTest.TestStrictPositiveUlp:ScriptPastReplayTest.TestCompiledBatchLifetimeAndDirectRoots'
```

`red-focused.log`: actual 0 versus expected 160; the independent hard reference
passed. The original root/lifetime test passed. The supplied unchanged reviewer
source was also compiled and run against the starting native library:

```sh
c++ -O3 -DNDEBUG -std=c++17 -fPIE -pie -Idal-cpp -Idal-cpp/externals/googletest/googletest/include ../inputs/reviewer/repro_exact_boundaries.cpp dal-cpp/tests/test_main.cpp build/Release-linux/dal-cpp/libdal_cpp.a build/Release-linux/lib/libgtest.a -pthread -o ../repair/repro_exact_boundaries
../repair/repro_exact_boundaries
```

`red-reviewer.log`: 0/4 passed, all eight price errors reproduced. After the
minimum production change, rebuilding and repeating gave 2/2 focused and 4/4
reviewer passes (`green-focused.log`, `green-reviewer.log`). No assertion weakened.

While green, extracted the fuzzy helper, refactored the root matrix and extended
coverage. `green-expanded.log` and `green-final-native.log`: 8/8 passed.
An initial vector of root cases failed compilation because `ScriptProductData_`
is noncopyable; a fixed `std::array` preserves aggregate initialization and all
cases. Both build attempts are retained; this is not counted as semantic RED.

Supplemental RED compiles the final seven regressions with the original
`preparation.cpp` explicitly linked before the current library. Preparation is
the only production file changed, so this reconstructs the original pipeline
without altering final tests. The original source, command and output are in
the archive: **0/7 passed** against original preparation and **7/7 passed**
with repaired preparation. This supplemental check is distinct from chronological TDD.

## Fresh verification

- Native configure/build/install: passed.
- Native full CTest: **1737/1737** passed.
- Native script/simulation/AAD/compiler/domain/visitor filter: **447/447** passed,
  including all 33 legacy parity/fuzz tests.
- Isolated native allocation fixture: **2/2** passed, retaining 8193 exact/fuzzy
  tree/compiled paths and allocation-positive controls.
- Fresh alternate-backend script/AAD filters: Adept **447/447**, CoDiPack
  **447/447**, XAD **446/446** passed. Cache settings and logs are attached.
- Documentation checker: **70 Markdown files** passed.
- `git diff --check` and changed-line clang-format checks: passed.

```sh
cmake --preset=Release-linux -S . -B build/Release-linux
cmake --build build/Release-linux -j6
cmake --install build/Release-linux
ctest --test-dir build/Release-linux --output-on-failure -j6
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
python3 -m lizard dal-cpp/tests/script/test_past_replay.cpp dal-cpp/tests/script/test_exact_folding.cpp dal-cpp/dal/script/preparation.cpp
```

Alternate builds use Release-linux with `-DDAL_USE_ADEPT_AAD=ON`,
`-DDAL_USE_CODIPACK_AAD=ON` or `-DDAL_USE_XAD_AAD=ON`, examples disabled,
target `dal_cpp_tests`, and the same filter. Native full/matrix tests preceded
only whitespace formatting of the root-case initializer; a subsequent native
rebuild/focused run verifies the exact committed form. Alternate test sources
were compiled after that formatting. Publication adds only this report.

## Handoff and limits

Reviewer archive SHA256 verified:
`93167e57e7e954fcf0f2e9481f4c113fa37a1aeb3bc7f1605035b7ef64cae5a6`;
all 132 internal checks verified. These are historical inputs; fresh repair
verification is identified separately above.

The doc-writer should revise exact model-aware pruning descriptions in
`docs/methodology/script_engine.md` (named double execution, domain processor,
pipeline ordering) and decide whether CHANGELOG needs an update.

No Windows XLL, Python, sanitizer, alternate-backend full public/Excel CTest or
performance run is claimed. Allocation instrumentation covers native double
evaluation, not AAD tape allocation; constant-time observation reads remain
source-inspected. T31's compiler-failure seam is immediately before bytecode
construction. F6–F8 public exposure and master integration remain outside scope.

The attached final metadata records one post-push CI snapshot, without polling.
PR #372 stays draft; no close intent or merge was added. Parent DAL-202 owns
the DAL-229 tester → DAL-230 doc-writer → DAL-231 reviewer sequence, semantic
acceptance and integration. No peer or F6 was started.
