# DAL-202 F5 fuzzy arithmetic correction

DAL-228 repairs the remaining DAL-231 P2 finding: prepared fuzzy evaluation
rejected finite nonzero divisors through tolerance-domain arithmetic. This report
supersedes the previous implementation correction report. Fresh independent
testing, documentation decision and re-review remain required.

## Revisions and scope

- Starting head: `00e96f9a5cb3d0cd20ddff7efaeee0c2eba69de2`.
- Starting tree: `cb08e80644ca2fb1ae0339cd6c592442bd7f15ef`.
- Tested code: `f2e6cc353b0ae1f10cf24eb226f92d6a21645ba5`.
- Tested tree: `42a0ca246e365e1140cd3772f21c5b7b59d91720`.
- Publication adds only this report. Attached `published-revision.json` records
  the final SHA/tree, GitHub head and equality with tested source hashes.
- Existing [draft PR #372](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/372)
  remains on `feature/dal-202-compiled-observations`, based on
  `feature/dal-201-historical-aad-state` at fixed F4
  `5c954ca2fdded2ca35ad15c7fef44d90a002007d`, dependent on #371/#369.

Changed files:

- `dal-cpp/dal/script/preparation.cpp`: remove `ProcessFuzzyDomains` and its call.
- `dal-cpp/tests/script/test_fuzzy_arithmetic.cpp`: four focused regressions.
- `.codex/artifacts/reviews/dal-202/implementation.md`: this report.

No domain, kernel, compiler, binding, archive, settings, generated enum,
dependency branch, public documentation, configuration or performance changes.

## Design and behavior

The removed pass evaluated every future expression using `Domain_`, which
uses tolerant equality/sign arithmetic. `Interval_::Inverse` therefore
identified finite ±1e-14 divisors as zero and threw before execution.
Multiplication could also collapse a small nonzero factor to zero, affecting
computed denominators. A literal-specific division bypass would not repair
those adjacent expressions.

For prepared fuzzy products, the old pass already forced every comparison to
continuous runtime evaluation and retained its branches. Freshly parsed nodes
already have those flags; the parser preserves explicit epsilon or its default
sentinel. The tolerance-domain pass and its dependent condition-pruning pass
provide no required metadata here. Removing them conservatively retains the
parsed arithmetic, branches and existing runtime kernels. Legacy preprocessing
and domain classes remain unchanged; no epsilon semantics are adjusted.

Both exact and fuzzy model-aware preparation still seal the complete syntactic
observation plan, replay hard history, analyze constant/parameter dependencies,
compute final IF nesting and affected variables, finalize future constant
arithmetic using ordinary double operations, and compile before workers.
Typed historical seeds, discarded past PAYS, eager booleans and preparation/
path failure boundaries are preserved. The earlier exact-pruning correction
and lifetime test are unchanged.

This is the authorized conservative implementation choice, with no public
contract or financial-methodology deviation. It omits an unsound optimization
rather than substituting a new domain proof. Performance remains deferred in
DAL-223; no speed or threshold claim is made.

## Regression oracles and RED/GREEN

The unchanged reviewer source was built against the original library:
`red-repro.log` records **2 exact controls passing and 4 AAD cases failing**
with Division by {0}. After repair, `green-repro.log` records **6/6 passing**.
For either sign of H/divisor, PV=2 and d_SCALE=1.

Chronological TDD first added only the signed-divisor regression:
`red-focused.log` fails after its unoptimized fuzzy reference passes.
Removing the domain pass gives `green-focused.log`: **1/1 passing**.

The final four regressions cover 25 arithmetic fixtures:

- Signed literal divisors ±1e-14, including SCALE sensitivity.
- Signed known divisors at 1e-16, 1e-14, both adjacent floats around 2e-14,
  exactly 2e-14 and 1e-12.
- Computed divisors using multiplication, addition/subtraction, square root,
  power, log/exp, min/max; tiny factors whose product is order one; a live
  SCALE denominator with analytic derivative -0.5.
- Signed divisors inside the explicit 0.2 smoothing band: x=0.05,
  weight=0.75, PV=0.0375, d_SCALE=0.025.

Each fixture checks an independent unoptimized fuzzy-double tree, prepared
fuzzy-double tree and compiled execution, and AAD tree/compiled primal plus
analytic SCALE/rate/spot/vol risks. No production tree/compiled agreement alone
serves as the oracle.

During extension, the new in-band oracle exposed missing IF affected-variable
metadata in the reference helper. `green-expanded.log` records that 3/4 run.
The reference now runs only IFProcessor_ after model-free preparation; it still
bypasses domain/constant optimization. Expected values and tolerances were
unchanged. `green-final.log` records **4/4 passing**.

Supplemental RED links the final tests with original `preparation.cpp` before
the repaired library: **0/4 pass**, all with the reported domain exception.
This is distinct from chronological RED and isolates the sole changed production
file. No additional production change was needed for the expanded cases.

Reproduce focused RED/GREEN with:

```sh
cmake --build build/Release-linux --target dal_cpp_tests -j8
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptFuzzyArithmeticTest.*'
```

The evidence archive's `commands.md` includes exact reviewer compilation,
original-source substitution, configure/build/test and publication commands.
All expected failing and successful logs are retained.

## Fresh verification

- Native Release configure/build/install: passed.
- Full native CTest: **1742/1742** passed.
- Script/simulation/AAD/compiler/domain/visitor filter: native **452/452**,
  Adept **452/452**, CoDiPack **452/452**, XAD **451/451** passed.
  XAD retains the existing backend-specific exclusion.
- All **33 ScriptCompiledParity/Fuzz suite cases** pass on all four backends:
  30 legacy-source tests and 3 F5 observation tests; the manifest lists each.
- Unchanged earlier exact-boundary reviewer reproductions: **4/4** passed.
- Isolated native observation allocation fixture: **2/2** passed, including
  positive control and 8193 exact/fuzzy tree/compiled evaluations.
- Lifetime matrix: unchanged 27 combinations, threads 1/2/4, H=80/90/80,
  8193 paths, three roots and all price/risk assertions. Local Lizard 1.23.0
  remains test complexity **4**, helper **1**; prepared builder is **7**.
- Documentation checker: **70 Markdown files** passed.
- Diff whitespace and new-file clang-format checks: passed.

```sh
cmake --build build/Release-linux -j4
cmake --install build/Release-linux
ctest --test-dir build/Release-linux --output-on-failure -j4
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
```

Alternate builds use Release-linux with one of `-DDAL_USE_ADEPT_AAD=ON`,
`-DDAL_USE_CODIPACK_AAD=ON`, `-DDAL_USE_XAD_AAD=ON`, examples disabled,
target dal_cpp_tests and the same filter. All runs use the exact tested source;
publication changes only this report. Cache settings and source hashes are attached.

## Evidence and handoff

Authenticated reviewer archive SHA256:
`9a0b1355ec24c0e7e578417e949b6a09ce0959c1b55db6ed19cb0aa3bedb6301`.
All 139 internal entries and attachment/archive report equality were verified.
The archive's predecessor test results are historical inputs; all results above
are fresh executions for this correction.

The doc-writer should update references to `ProcessFuzzyDomains`, prepared
fuzzy domain analysis and initial variable domains in
`docs/methodology/script_engine.md`, and decide docs-index/CHANGELOG changes.
The documentation checker validates structure, not these now-stale descriptions.

No Windows XLL, Python, sanitizer, full alternate-backend public/Excel suite or
performance result is claimed. Allocation instrumentation covers native double
evaluation, not AAD tape allocation or arbitrary malloc; observation indexing
remains source-inspected. T31's compiler seam remains immediately before bytecode
construction. No remote Codacy or final semantic acceptance is claimed.

One published-head CI snapshot is attached; no watch/poll, dependency rewrite,
merge or close intent. DAL-228 returns to in_review. Parent DAL-202 owns
sequential DAL-229 independent testing, DAL-230 documentation decision and
mandatory DAL-231 re-review, then acceptance and integration. No peers or F6
were started.
