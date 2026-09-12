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
