DAL-198 F1 implementation handoff, 2026-09-12. This artifact controls the pending
tester, documentation and independent reviewer stages; retire it after delivery.

Implemented the accepted F1 frontend scope. `Lex` emits positioned tokens whose
variant distinguishes `IndexLiteral_`; the existing `Tokenize` string projection
remains available for schedules and existing callers. The lexer and each macro or
placeholder replacement pass share the complete index boundary scan. Macro order
outside literals is unchanged, and newly expanded literals become protected.

`NodeFix_` stores raw spelling, the parsed immutable `Index_` handle, optional daily
fixing date, and source offset/line/column. Preprocessed source origins also retain
the table row and expanded event date, including concatenated same-date events.
FIX only accepts an unquoted index and optional contiguous strict ISO date. FIX
definitions/variables report `ReservedIdentifier`; old zero-argument SPOT remains.
Indice rejects unknown/bare/null results, malformed EQ/FX and trailing input, and
validates EQ delivery increments without dropping empty compound components.

Minimal `event.cpp`/`event.hpp` changes propagate source origins and reject named
observations before legacy preprocessing, compilation, past evaluation or tree
evaluation. The product captures the diagnostic before condition folding, so dead
branches cannot bypass the guard and old SPOT paths avoid per-path AST scans.
Direct double/AAD, fuzzy, past, compiler and domain visitors reject NodeFix with
`PreparationRequired`; the debugger has a structural FIX visitor. No fixing I/O,
binding, observation plan, or public pricing API was added. These remain later
stages of the accepted design.

Verification:

- Initialized missing checkout submodules with `git submodule update --init --recursive`.
- Configured `cmake --preset=Release-linux -S . -B build/Release-linux`.
- Built `cmake --build build/Release-linux --target dal_cpp_tests -j8`.
- Initial RED: `dal_cpp_tests --gtest_filter='ScriptObservationTest.*:IndexParseTest.TestParseRejectsBareNameAndNullParser:IndexParseTest.TestParseRejectsMalformedAndTrailingInput'`:
  all seven tests failed for the intended missing behavior. The initial lexer
  emitted `FX`, FIX parsed as a variable, names were macro-expanded, parenthesized
  trailing syntax was ignored, FIX was not reserved, and invalid indices survived.
- Further RED: `ScriptObservationTest.TestScheduleSourceContext` lacked `row=2`;
  `ScriptObservationTest.TestPreparationRequiredProductEntryPoints` returned the
  generic compile prerequisite; `IndexTest.TestParserRejectsIncompleteCompoundDelivery`
  accepted `EQ[IBM]>3M&`. Each passed after its corresponding change.
- GREEN: `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationTest.*:IndexParseTest.*:IndexTest.*:ScriptLexerTest.*:ScriptPreprocessorTest.*:ScriptTest.*'`:
  238 tests passed. This includes strict date and malformed syntax cases, literal
  identity/raw case/source metadata, macro-generated protection, delivery forms,
  dead branches with and without domain processing, and tree/compiled/AAD/fuzzy/
  past preparation guards alongside existing SPOT regressions.
- Formatted changed C++ ranges with repository clang-format and checked
  `git diff --check` and `git diff --cached --check` before committing.

Evidence logs are in the parent workdir under `dal-198-*-build.log` and
`dal-198-*-tests.log`. Independent full Linux verification and review are owned by
the following specialist stages. No published docs or changelog were changed;
dal-doc-writer decides those changes. The orchestrator records the commit/PR SHA.

Independent tester repair: the bracket scanner initially rejected parentheses
inside an index name even though `Index::Parse("EQ[AAPL(US)]")` preserves that
identity. Removed only that extra restriction: bracket content reaches indice
unchanged, while nested brackets and quotes still fail. The tester owns the new
regressions and confirmed RED in `dal-198-tester-boundary-red.log`.

Tester-collected GREEN after the repair:

- `cmake --build build/Release-linux --target dal_cpp_tests -j8`: exit 0;
  `dal-198-tester-green-build.log`.
- `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationTest.TestBracketContentsPreserveIndiceIdentity:ScriptObservationTest.TestInvalidSyntax'`:
  exit 0, both tests passed; `dal-198-tester-boundary-green.log`. These exercise
  punctuation, commas and unmatched parentheses within names while retaining
  malformed quote/nested-bracket rejection. No test files belong to this repair
  commit; the tester continues the independent expanded and full verification.

Independent reviewer P2 repair: existing human tree rendering had no FIX leaf
case, and legacy product JSON /1 silently emitted a `kind=fix` node without its
identity/date. Human text and tree output now retain complete
`FIX(index[, date])` syntax, including both delivery and fixing dates. The /1
product JSON entry checks the product's captured FIX presence before writing and
throws `DebugSchemaUnsupported`, including observations in past events and dead
branches. No schema /2, new debug API, or I/O was added.

- RED: `dal_cpp_tests --gtest_filter='ScriptObservationTest.TestDebugger*'`
  failed both new regressions, reproducing the missing tree leaf and successful
  incomplete /1 JSON; `dal-198-debug-red-tests.log`. The tests were subsequently
  placed in the file's existing `ScriptTest` suite per repository convention.
- GREEN: `cmake --build build/Release-linux --target dal_cpp_tests -j8`, followed
  by `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationTest.*:*Debug*'`:
  build exit 0 and 29/29 tests passed; `dal-198-debug-green-build.log` and
  `dal-198-debug-green-tests.log`. Tests cover nested FIX expressions, explicit
  and omitted dates, FX/EQ/delivery names, ASCII/Unicode and narrow/wide tree
  rendering, rejection before JSON output for past/future/dead-branch uses,
  and existing exact SPOT debug snapshots. The documentation correction belongs
  to dal-doc-writer; the parent coordinates independent retesting/review.
