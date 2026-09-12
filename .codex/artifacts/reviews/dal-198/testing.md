DAL-198 F1 independent tester handoff, updated 2026-09-13. This record controls the
pending documentation and reviewer stages; retire it after delivery.

F1 local testing passes after the bracket-content, debugger, CI and performance
repairs. Initial implementation was `899748dec20034e33a8375697018e8da92565218`;
the bracket repair at `23ae973a50335bd32a1d66c311db1e3068242018` was verified
with tester changes committed at `cead11e0548d6fbffcb7c44339ccb0e4c505cfbf`.
Latest independently tested production and test revision is
`7d79eeb82b99f5a000a239c9477999dd51aa3479`. The orchestrator records the final
delivery/PR SHA.

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

Verification before debugger review:

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
  Preserved log: `../dal-198-tester-final-full-linux.log`.
  The script configured, built all enabled targets, installed, and ran CTest.
- Active configuration: Linux Release, native AADET AAD, public API enabled,
  portable Excel tests enabled. Benchmarks excluded by the standard workflow.
- Inspected the production diff: no fixing/history I/O or named numeric
  placeholder was introduced. Named execution is rejected before evaluation;
  market preparation/binding and their numerical acceptance belong to later
  stages. `git diff --check` and the scoped staged whitespace check passed.

Independent verification after reviewer P2 debugger repair:

- Read `../dal-198-review.md`, the repair diff at
  `a3a9f9583ee4dcd7fbece0e61b167efa616630c7`, implementation evidence and
  existing debugger tests. The repair preserves FIX as a complete tree leaf and
  rejects legacy product JSON /1 before writing anything. No production edits
  or additional tests were necessary in this verification pass.
- `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptObservationTest.*:*Debug*'`:
  exit 0; 29 tests from 2 suites passed. Fresh log:
  `../dal-198-tester-debug-focused.log`.
- The new tree regression checks nested expressions, raw index spelling,
  explicit/omitted fixing dates, EQ/FX and both EQ delivery forms with ASCII
  and Unicode at narrow/wide widths. Human text output also retains FIX.
  The JSON regression checks `DebugSchemaUnsupported`, the /1 schema name
  and zero output bytes for past/future events and dead branches. Existing
  exact SPOT tree, text and JSON snapshots still pass.
- Removed root `test_output.txt` before rerunning
  `NUM_CORES=8 bash ./build_linux.sh > test_output.txt 2>&1` on the repaired
  revision. Exit 0; fresh authoritative summary:
  `100% tests passed, 0 tests failed out of 1588`; CTest time 7.70 seconds.
  Preserved log: `../dal-198-tester-debug-full-linux.log`. Configuration remains
  Linux Release with native AADET AAD, public API and portable Excel tests, benchmarks
  excluded. This full run includes the two debugger regressions added by the
  implementer. Whitespace and scoped staged checks passed.
- No blocker remains from independent testing. Updated documentation and a
  new independent review of the resulting head are still required.

Independent verification after CI blocker repair, 2026-09-13:

- Inspected the diff from PR head `62b7a7bf` to `ce158d11` and the implementation
  remediation evidence. Explicit `!= 0` predicates address the integer-to-bool
  conversion; `node.hpp` directly includes its exception dependency. The two
  guard tests initialize Tape before constructing active evaluator stacks and
  start recording before evaluation, retaining every rejection assertion.
  The direct delivery-diagnostic test retains `InvalidIndex` and the complete
  input for DAL exceptions and standard overflow errors. The script diagnostic
  test also requires source context for malformed dates and increments.
  Existing tests cover the extracted lexer/parser helpers; no additional tests
  or production edits were needed in this independent pass.
- Removed root `test_output.txt`, then ran
  `NUM_CORES=8 bash ./build_linux.sh > test_output.txt 2>&1`.
  Exit 0: `100% tests passed, 0 tests failed out of 1590`;
  CTest time 10.40 seconds. Preserved log:
  `../dal-198-tester-ci-native-full.log`. This includes configure, all-target
  build, install and full nonbenchmark CTest with native AADET, public API,
  portable Excel tests and examples enabled.
- Initial `cmake --build build/dal-198-adept -j4` and
  `cmake --build build/dal-198-codi -j4` both exited 2 because the inherited
  core-only caches disabled the public library while enabling portable Excel
  tests. The Excel compile could not find `dal-public/src/curvepricing.hpp`.
  Initial failure logs are `../dal-198-tester-ci-{adept,codi}-build.log`.
  Enabled the required dependency, keeping all tests enabled, with:
  `cmake --preset=Release-linux -S . -B build/dal-198-adept -DDAL_USE_ADEPT_AAD=ON -DDAL_BUILD_PUBLIC=ON -DDAL_CPP_BUILD_EXAMPLES=OFF`
  and
  `cmake --preset=Release-linux -S . -B build/dal-198-codi -DDAL_USE_CODIPACK_AAD=ON -DDAL_BUILD_PUBLIC=ON -DDAL_CPP_BUILD_EXAMPLES=OFF`.
  Both configure commands exited 0; logs
  `../dal-198-tester-ci-{adept,codi}-configure.log`.
- Re-ran `cmake --build build/dal-198-adept -j4` and
  `cmake --build build/dal-198-codi -j4`: both exited 0, including public API,
  portable Excel and allocation-test targets. Logs:
  `../dal-198-tester-ci-{adept,codi}-full-build.log`.
- `ctest --test-dir build/dal-198-adept --output-on-failure --parallel 4 -LE benchmark`:
  exit 0, `100% tests passed, 0 tests failed out of 1582`, 17.25 seconds.
  `ctest --test-dir build/dal-198-codi --output-on-failure --parallel 4 -LE benchmark`:
  exit 0, `100% tests passed, 0 tests failed out of 1582`, 17.63 seconds.
  Logs: `../dal-198-tester-ci-{adept,codi}-full-tests.log`.
  CTest ran each case in its own process: both formerly crashing guard tests
  and both delivery-diagnostic regressions individually passed on each backend.
- Final caches confirm exactly one alternate backend enabled in each separate
  directory; native flags remain all off. All three use Linux Release/GCC
  15.2.0 with public API and portable Excel tests enabled. Alternate examples
  remain off; Python and benchmarks are off throughout. CTest discovery differs
  by nine native-only cases and one backend-only case on each alternate build,
  giving the eight-test count difference; no tests or policy were suppressed.
  Discovery evidence: `../dal-198-tester-ci-discovery.log`.
- Independently ran
  `lizard dal-cpp/dal/script/lexer.cpp dal-cpp/dal/script/parser.cpp dal-cpp/dal/indice/parser/equity.cpp`:
  the four reported functions are now CC 4/4/7/4; every new helper is at most 7.
  The full command exits 1 for unchanged `ParseVarConstFunc` CC 16; a separate
  Lizard API assertion covering the 14 affected/new functions exits 0 with all
  below the limit of 8. Logs: `../dal-198-tester-ci-complexity.log` and
  `../dal-198-tester-ci-complexity-scope.log`. This is local analysis evidence;
  remote Codacy and Windows CI results still require the new pushed head.
- Working and scoped staged whitespace checks pass. Local verification is
  complete; documentation decision, independent review and remote checks remain
  with the orchestrator's delivery route.

Independent correctness verification after performance repair, 2026-09-13:

- Inspected the complete diff from `78e52eba` to the clean implementation head
  `7d79eeb82b99f5a000a239c9477999dd51aa3479`, current APIs, implementation
  evidence and relevant tests. Parser-owned diagnostic state resets at each
  Parse and is copied into the product before parser reuse. Existing product
  and visitor guards remain in place. The no-opening-bracket shortcut leaves
  macro-generated literals protected by subsequent replacement scans.
- Existing new tests cover first-FIX raw spelling/source, per-Parse reset across
  FIX/SPOT/FIX/empty input, and retaining the first past/dead-branch observation
  across later events and another ParseEvents call. Existing tests cover
  macro expansion, full identity, source origins, direct and product execution
  guards, PreProcess with domain processing enabled/skipped, and legacy debug
  rejection. No material missing coverage, test edits or production repairs
  were identified during this pass.
- Removed root `test_output.txt`, then ran
  `NUM_CORES=8 bash ./build_linux.sh > test_output.txt 2>&1`.
  Exit 0: `100% tests passed, 0 tests failed out of 1592`;
  CTest time 17.08 seconds. Logs: root `test_output.txt` and
  `../dal-198-tester-perf-native-full.log`. The native workflow configured,
  built all enabled targets, installed and ran the complete nonbenchmark suite.
- `cmake --build build/dal-198-adept -j4` and
  `cmake --build build/dal-198-codi -j4` both exited 0, rebuilding all configured
  targets. Logs: `../dal-198-tester-perf-{adept,codi}-build.log`.
- `ctest --test-dir build/dal-198-adept --output-on-failure --parallel 4 -LE benchmark`:
  exit 0, `100% tests passed, 0 tests failed out of 1584`, 15.69 seconds.
  `ctest --test-dir build/dal-198-codi --output-on-failure --parallel 4 -LE benchmark`:
  exit 0, `100% tests passed, 0 tests failed out of 1584`, 16.46 seconds.
  Logs: `../dal-198-tester-perf-{adept,codi}-full-tests.log`.
  Both new tests and all preparation-guard cases passed independently in CTest
  processes on each of the three backends.
- Caches retain the previous Linux Release/GCC 15.2.0 configuration: native
  AADET with examples on; separate Adept and CoDiPack directories with examples
  off; public API and portable Excel tests on in all three. No backend flags,
  test exclusions or policy were changed. Working/staged whitespace checks pass.
- All foreground builds and tests completed before notifying the orchestrator
  that CPU work was finished. This pass ran no timing workload; the reported
  CTest durations are correctness-run metadata under concurrent workloads and
  establish no speedup or benchmark acceptance. Paired performance validation,
  documentation decision, independent review and publication remain separate.

Limits: no local MSVC/Windows XLL, Python binding, XAD backend, sanitizer or
coverage run was performed. Adept and CoDiPack full configured suites are now
verified above, superseding the earlier alternate-backend coverage limitation.
An attempted extra PastEvaluator_<AAD::Number_> probe did not compile: the
existing visitor list registers only the double
past evaluator and the template constructor is incompatible with Number_.
That unsupported probe was removed without production changes; the preexisting
restriction is for the later past-AAD stage. Supported PastEvaluate and direct,
future and fuzzy AAD rejection paths pass here. No local testing blocker remains;
the documentation decision, independent review and remote checks are still required.
