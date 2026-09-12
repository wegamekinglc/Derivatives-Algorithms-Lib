DAL-198 F1 independent tester handoff, 2026-09-12. This record controls the
pending documentation and reviewer stages; retire it after delivery.

F1 testing passes after the scoped bracket-content repair. Initial implementation
was `899748dec20034e33a8375697018e8da92565218`; final production revision is
`23ae973a50335bd32a1d66c311db1e3068242018`, tested with the test changes committed
alongside this record. The orchestrator records the final delivery/PR SHA.

Running existing tests:

- Read the accepted issue/design, tester contract, run-tests, write-tests,
  unit-test style, production APIs and nearby tests before verification.
- Removed the previous root `test_output.txt`, then ran
  `NUM_CORES=8 bash ./build_linux.sh > test_output.txt 2>&1` on the initial
  implementation. Exit 0; fresh authoritative summary:
  `100% tests passed, 0 tests failed out of 1583`; CTest time 10.67 seconds.
  Preserved as `../dal-198-tester-baseline-full-linux.log`.

Authoring tests:

- Added `ScriptObservationTest.TestBracketContentsPreserveIndiceIdentity`.
  It first proves names are accepted unchanged by `Index::Parse`, then checks
  FIX inside MAX, explicit fixing date, exact raw spelling, macro protection
  and schedule-placeholder protection. Cases include dots, slashes, commas,
  balanced/unbalanced parentheses inside brackets, and compound delivery.
- Added `TestSameDateSourceOriginsAfterMacroExpansion` to verify the second
  same-date statement retains its original table row, expanded line/column,
  offset and event date after a macro produces an index literal.
- Added `TestNullParserRetainsSourceContext` using a unique registered parser
  prefix; a null result must throw a script error retaining index and source.
- Extended existing preparation checks to direct normal/fuzzy domain visitors,
  product AAD/fuzzy evaluation and fuzzy compilation. Existing tests already
  cover direct double/AAD/fuzzy/past/compiler visitors, PreProcess with domain
  processing enabled or skipped, and dead branches on past/future event dates.
- Moved the new lexer test to the existing `ScriptLexerTest` suite, complying
  with the repository's one-suite-per-file convention.

Repairing failures:

- The bracket-content test failed before any production repair:
  `Index::Parse("EQ[AAPL(US)]")` succeeded, while
  `x = MAX(FIX(EQ[AAPL(US)], 2026-09-11), 0)` threw `InvalidIndex` from
  `lexer.cpp:35`. The accepted grammar preserves bracket contents and delegates
  name validation to indice; parentheses were rejected only by the lexer.
- Narrow RED command:
  `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter=ScriptObservationTest.TestBracketContentsPreserveIndiceIdentity`.
  Exit 1, 0/1 passed; `../dal-198-tester-boundary-red.log`.
- Reported the failure to the orchestrator and implementer. The implementer
  repaired only the lexer bracket-content rejection and committed its evidence
  at `23ae973a`; tester made no production edits.
- Rebuilt with `cmake --build build/Release-linux --target dal_cpp_tests -j8`
  (exit 0; `../dal-198-tester-green-build.log`), then ran
  `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationTest.TestBracketContentsPreserveIndiceIdentity:ScriptObservationTest.TestInvalidSyntax'`.
  Exit 0, 2/2 passed; `../dal-198-tester-boundary-green.log`. Valid bracket
  punctuation passes while nested brackets, quotes and malformed syntax fail.

Final verification:

- `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationTest.*:IndexParseTest.*:IndexTest.*:ScriptLexerTest.*:ScriptPreprocessorTest.*:ScriptTest.*'`:
  exit 0, 241 tests from 6 suites passed; `../dal-198-tester-focused.log`.
  This covers T01/T02, complete EQ/FX/delivery identity, strict ISO fixing
  dates distinct from delivery, malformed/unknown/null/trailing rejection,
  FIX reservation, macro-generated literal protection, source context,
  PreparationRequired and existing SPOT/frontend regressions.
- Removed `test_output.txt` again, then freshly ran
  `NUM_CORES=8 bash ./build_linux.sh > test_output.txt 2>&1` after all source and
  test edits. Exit 0; authoritative summary:
  `100% tests passed, 0 tests failed out of 1586`; CTest time 8.39 seconds.
  Final log is root `test_output.txt` and `../dal-198-tester-final-full-linux.log`.
  The script configured, built all enabled targets, installed, and ran CTest.
- Active configuration: Linux Release, native AADET AAD, public API enabled,
  portable Excel tests enabled. Benchmarks excluded by the standard workflow.
- Inspected the production diff: no fixing/history I/O or named numeric
  placeholder was introduced. Named execution is rejected before evaluation;
  market preparation/binding and their numerical acceptance belong to later
  stages. `git diff --check` and the scoped staged whitespace check passed.

Limits: no Windows XLL, Python binding, alternate AAD backend, sanitizer or
coverage run was performed. An attempted extra PastEvaluator_<AAD::Number_>
probe did not compile: the existing visitor list registers only the double
past evaluator and the template constructor is incompatible with Number_.
That unsupported probe was removed without production changes; the preexisting
restriction is for the later past-AAD stage. Supported PastEvaluate and direct,
future and fuzzy AAD rejection paths pass here. No F1 blocker remains; the
documentation decision and independent code review are still required.
