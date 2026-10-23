# DAL-418 test complexity repair and accepted master integration

The tiny-accrual tree/compiled regression now separates its independent expected
value and deterministic scenario construction into two private helpers. The
original TEST retains every matrix loop, both instrument constructors, both
execution backends and its fatal payment assertions. This local change awaits
the original reviewer's limited review; publication is not authorized in this
phase.

## Actual integration and scope

Normal merge `c590d15398a5f5c54933f14371628a95f8661eef` preserves these parents:

- Original PR head `1d42e54c1240a8114bcb895d6895870008160b8c`.
- Accepted DAL-414 master `2baceaf083258a657dc954642e7c279507fb5721`.

Its tree is `7fba43db4b6d063c4db2da63763a2e1040e1e8fd`. The two actual
conflicts are `.codex/artifacts/README.md` and `CHANGELOG.md`. Resolution keeps
both exact texts with a blank separator. The methodology guide equals Git's
automatic three-way merge. All 30 independent upstream paths have the actual
master's blobs; all 3,002 entries outside the incoming 33 paths retain the
original head's identity. Three-way inputs, first/second-parent diffs and the
actual remerge diff are in the issue attachment.

The subsequent repair changes only
`dal-cpp/tests/model/test_gsr_rateoption_script.cpp` and this activity record.
`TinyAccrualExpectedValue` contains the original independent financial expression;
`TinyAccrualPath` returns the original deterministic scenario as new state.
Assertions remain in the TEST, preserving immediate fatal-failure behavior.
No shared test framework, source instrumentation or public helper is introduced.

The BG-1 production blob remains reviewed
`f607d520596275f6e0bc2fb49f0c096d47174121`. The original XCCY/preparation code
and regressions, upstream product/build/dependency/quality settings and all
earlier role reports remain unchanged. This record supplements the historical
[publication record](dal-418-publication.md); it does not rewrite prior results.

## Structural RED and GREEN

Old head's complete CI result remains 35 SUCCESS / 1 Codacy ACTION_REQUIRED.
Check `120113924540` exposed exactly one fully paginated annotation: the target
TEST had CCN 14 against limit 8. The earlier foreground watch's GraphQL EOF and
same-head read-only recovery remain recorded; neither was a CI rerun.

Lizard 1.23.0 reproduces that structural RED with exit 1. The same tool/threshold
on the repaired source exits 0. Per-function metrics are:

| Function                 | Before              | After |
|--------------------------|---------------------|-------|
| Target TEST              | 14                  | 7     |
| TinyAccrualExpectedValue | Extracted from TEST | 5     |
| TinyAccrualPath          | Extracted from TEST | 4     |

The file has 50 functions after extraction, all CCN <= 8. This is local structural
GREEN; Codacy acceptance on a future published head remains a separate gate.
No functional failure was invented for a refactor of an already passing test.

```bash
lizard -l cpp -C 8 -w test-before.cpp
lizard -l cpp -C 8 -w dal-cpp/tests/model/test_gsr_rateoption_script.cpp
```

The first command uses the exact old Git blob preserved in the attachment.

## Preserved observations and focused verification

The input sets remain location 0/1/2, CALL/PUT and LIBOR 0/.04/100000: 18 groups.
Each group checks both importer tiny source and independently handwritten decimal
source, giving 36 instrument scenarios and 72 tree/compiled payment observations.
The independent formulas retain N=100, DF=.91, numeraire=1.7, tiny=1e-7, K=.03,
the original leg accruals and CALL/PUT direction. Every sample retains numeraire
1.7 and every observation retains the LIBOR-name mapping to LIBOR or .91.

An exact source comparison proves that all other source bytes, oracle operations,
matrix literals, evaluator calls and assertions are preserved. Compiler-only
GCC 14.3 coverage of the maintained source actually records 18 oracle calls,
36 path constructions and 72 payment checks: 36 exact-zero ASSERT_EQ executions
and 36 original relative-error <=1e-12 ASSERT_LE executions. No branch is hidden
in a macro or production helper. The diagnostic target passes 1/1 separately.

The actual integrated checkout rebuilds `dal_cpp_tests` with GCC 14.3, CMake
4.2.3 and its existing Release configuration. The maintained target passes 1/1
for the changed TEST, then 116/116 across these selected contracts, with no
failures, disabled tests or skips:

- `GSRRateOptionScriptTest.*`: 22.
- `CalibrationPreparationTest.*`: 42.
- `ScriptInstrumentTest.*`: 42.
- `ScriptLexerTest.*`: 8.
- DAL-414's two new `GSRScriptRepairTest` owned-axis/completed-summary cases: 2.

```bash
cmake --build build/dal408 --target dal_cpp_tests -j 4
DAL_NUM_THREADS=1 build/dal408/dal-cpp/dal_cpp_tests \
  --gtest_filter='GSRRateOptionScriptTest.TestTinyAccrualTreeCompiledAmountsAndNumeraire'
DAL_NUM_THREADS=1 build/dal408/dal-cpp/dal_cpp_tests \
  --gtest_filter='GSRRateOptionScriptTest.*:CalibrationPreparationTest.*:ScriptInstrumentTest.*:ScriptLexerTest.*:GSRScriptRepairTest.TestMissingHeldOutSamplingOwnsTheRequestedHeldOutAxis:GSRScriptRepairTest.TestHeldOutFailureRetainsCompletedFitSummaryAndItsOwnAxes'
```

Modified-range clang-format, patch integrity and repository Markdown checks pass.
Exact argv, XML, CCN metrics, source/binary/library hashes, coverage data and
minimal review patches are attached to the issue. Temporary tools, logs and
binary evidence remain outside the delivery tree. The build retains existing
nodiscard warnings in unrelated unchanged tests; it is not claimed warning-free.

## Remaining gates

The original reviewer reviews only this test extraction and the mechanical
integration evidence. Accepted product and documentation conclusions remain
applicable; there is no new API or numerical decision. Numerical budgets,
sampling, tolerances and timeouts are unchanged. No production hot loop changes,
so this repair needs no new benchmark matrix.

The old full Linux/native Windows/install/Python results apply to the previous
source only. Accepted DAL-414 changes GSR, bindings and build inputs, so the new
integration still needs its applicable complete local gates after limited review,
then publication and complete exact-head CI. This phase performs no push, ready
operation, CI rerun or merge. PR #620 remains at its old remote head and has not
become mergeable remotely through these local commits. DAL-418/DAL-408 and their
parent stay in progress for the coordinator's acceptance.
