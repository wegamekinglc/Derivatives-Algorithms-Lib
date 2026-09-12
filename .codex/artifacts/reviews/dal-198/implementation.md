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

CI/review blocker repair, 2026-09-13, from PR #366 head
`62b7a7bf694760df54c790560c6b11abb1683c96`:

- Converted the lexer character predicates explicitly with `!= 0`, addressing
  MSVC C4800 without changing recognized characters. Added the direct exception
  header dependency to `node.hpp`, which previously relied on incidental AAD
  backend includes. The CoDiPack compilation failure was reproduced locally
  before the header repair (`dal-198-codi-red-build.log`).
- Reproduced both Adept guard-test crashes in separate CTest processes. The
  evaluator's `Stack_<adept::adouble>` allocates active values at construction;
  Adept's active constructor dereferences its active stack, while DAL creates
  that stack lazily in `Tape()`. The tests now call `Clear(*Tape())` before
  evaluator construction and `NewRecording(*Tape())` before guard evaluation.
  All rejection assertions and backend coverage remain enabled. Isolated RED
  and GREEN are in `dal-198-adept-lifecycle-{red,green}.log` (two crashes, then
  2/2 passes).
- Added direct indice and script diagnostic regressions. Invalid delivery dates,
  increments, and integer-overflow input must retain `InvalidIndex` and the full
  original index; script errors must also retain the source column. Both tests
  failed before repair and passed after normalizing both DAL `Exception_` and
  standard `logic_error` through the shared equity diagnostic helper. Logs:
  `dal-198-ci-diagnostic-{red,green}-{build,tests}.log`.
- Extracted cohesive index-body/suffix, token, source-origin, equity-validation,
  and fixing-date helpers without changing the accepted grammar. Local Lizard
  reproduces the four reported baseline complexities, then measures
  `IndexLiteralEnd` 18→4, `Lex` 14→4, `ParseFix` 13→7 and `EquityParser` 10→4.
  Every new helper is at most 7, below the limit of 8. Unrelated existing
  high-complexity functions and all analysis/CI policies are unchanged. Command:
  `lizard dal-cpp/dal/script/lexer.cpp dal-cpp/dal/script/parser.cpp dal-cpp/dal/indice/parser/equity.cpp`;
  logs `dal-198-complexity-{red,green}.log`.

Verification uses the native `build/Release-linux` directory and separate
`build/dal-198-adept` / `build/dal-198-codi` directories. Backend configuration:
`cmake --preset=Release-linux -S . -B build/dal-198-adept -DDAL_USE_ADEPT_AAD=ON -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_PUBLIC=OFF`,
and the same command with `build/dal-198-codi` and
`-DDAL_USE_CODIPACK_AAD=ON`. The preset leaves the other AAD flags off. Each core
test binary built successfully with `cmake --build <build-dir> --target dal_cpp_tests -j4`.

- Native: `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationTest.*:ScriptLexerTest.*:ScriptPreprocessorTest.*:ScriptTest.*:IndexTest.*:IndexParseTest.*'`:
  245/245 passed (`dal-198-ci-native-green-tests.log`).
- Adept and CoDiPack: `ctest --test-dir <build-dir> -R '^(ScriptObservationTest|ScriptLexerTest|ScriptPreprocessorTest|IndexTest|IndexParseTest)\.|^ScriptTest\.TestDebugger' -j4 --output-on-failure`:
  77/77 passed on each backend (`dal-198-{adept,codi}-focused-green.log`).
  CTest isolates the two AAD lifecycle regressions from any preceding tape setup.
- Changed ranges were formatted, and working/cached diff whitespace checks pass.
  No MSVC `cl`/Windows SDK build is available locally; Windows validation remains
  a remote CI requirement. The parent owns independent testing, the documentation
  decision, review, and publication of this scoped correction.

Performance blocker repair, 2026-09-13, from PR #366 head
`78e52ebac060dca513537b9b719313fa4df4321e`:

- Retained CI RED shows `script.construct_and_parse` at +6.72% / +7.11%
  against the unchanged +4% two-round gate. Its A/A controls are below 1%.
  The tree case also failed (+5.15% / +8.00%), but its same-head A/A samples
  are bimodal, so this repair does not attribute its exact cost to the guard.
  Inputs: `dal-198-benchmark-failure/python-paired/summary.md` and the
  independent `dal-198-performance-diagnosis.md`.
- The parser now records the first FIX diagnostic while constructing its node,
  resetting this metadata at the start of each Parse. ParseEvents consumes it
  instead of recursively applying RTTI to every AST node. The product keeps
  its first recorded diagnostic across later events and ParseEvents calls.
  The complete NodeFix diagnostic, including raw spelling and source origins,
  remains the single formatting source.
- The product's preparation check now has an inline successful path and an
  out-of-line throwing path. Every existing caller and direct visitor guard
  remains in place, including past events, dead branches, tree/AAD/fuzzy
  evaluation, preprocessing, compilation, and legacy JSON rejection.
- IndexLiteralRanges returns an empty result immediately for text with no
  opening bracket. Macro-generated literals are still rescanned on subsequent
  replacements; bracketed text takes the unchanged validation/protection path.
  No benchmark threshold, inventory, timing boundary, or CI policy changed.

Correctness evidence:

- Added tests for first-FIX diagnostic identity and per-Parse reset across FIX,
  SPOT, another FIX with different origins, and empty input. A product test
  preserves the original past/dead FIX through later events and another
  ParseEvents call, then verifies a separate legacy product remains usable.
  The test-first build failed on the absent parser metadata getter
  (`dal-198-perf-red-build.log`); the performance RED above is the behavioral
  regression motivating this behavior-preserving repair.
- `cmake --build build/Release-linux --target dal_cpp_tests -j8`: exit 0
  (`dal-198-perf-green-build.log`).
- `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationTest.TestParserPreparationDiagnosticReset:ScriptObservationTest.TestProductRetainsFirstPreparationDiagnostic'`:
  2/2 passed (`dal-198-perf-focused-green.log`).
- `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationTest.*:ScriptLexerTest.*:ScriptPreprocessorTest.*:ScriptTest.*:IndexTest.*:IndexParseTest.*'`:
  247/247 passed (`dal-198-perf-regression-green.log`).
- Local Lizard measures ParseFix 8, IndexLiteralRanges 7, ParseEvents 4 and
  Lex 4; all touched functions meet the limit of 8
  (`dal-198-perf-complexity.log`). Existing unrelated complexity warnings
  remain unchanged. Changed test lines are formatted and diff checks pass.

No speedup or final benchmark acceptance is claimed at this handoff. The parent
coordinates isolated paired performance validation, independent testing and
review; this implementation stage ran no timing workload concurrently with
builds and made no publication or platform changes.
