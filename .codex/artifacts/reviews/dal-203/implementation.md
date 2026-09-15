# DAL-203 F6 implementation handoff

## Build repair handoff — 2026-09-15

The reported public-test build failure and the separately diagnosed Codacy finding are repaired and locally verified. This supersedes the readiness statement of the original handoff below; parent acceptance, independent testing, documentation and mandatory review remain outstanding. PR #374 stays draft on `feature/dal-203-public-settings`, base `master`, without closing intent or merge.

- Repair input: `eb62b4abed2215f4f9e0079afc5858c82faf713e`, tree `8f00a775ed7aae0045d13b1b5314642ce20eb35d`.
- **Final tested repair commit:** `ab111ad6ca5bcef003f27f96da8e39ad78b7c037`.
- **Final tested repair tree:** `bc3f543279fa18a1c70ef4c2323512e19be6ffdf`.
- First repair commit `a1c39dedd6cd9b62c6721f4f719b1a7be4ebaa7d` adds the RapidJSON dependency and diagnostics extraction. The second adds the shared fixture dependency discovered by the full standalone build.
- The following commit changes only this report. Its published SHA/tree, remote equality and the single new-head CI snapshot are recorded in the final DAL-234 comment and attached `publication.json`. No new-head CI result is inferred from local success.

### Changes and dependency contract

`dal-public/CMakeLists.txt` now locates RapidJSON with `DAL_PUBLIC_TEST_RAPIDJSON_INCLUDE_DIR` and adds it only to `dal_public_tests` with PRIVATE visibility. The source checkout supplies the pinned `dal-cpp/externals/rapidjson/include` by default. A standalone source bundle can provide the header directory explicitly or supply a RapidJSON prefix through ordinary CMake discovery. Lookup occurs only when `DAL_PUBLIC_BUILD_TESTS=ON`; unavailable test headers produce a configure error.

The complete standalone suite also exposed an existing dependency in `dal-public/tests/test_curvepricing.cpp`: it included a core curve fixture through the core source root, which is unavailable through an installed DAL::cpp. That dependency predates F6 (commit `22343eee`, PR #346). The repair adds `DAL_PUBLIC_TEST_FIXTURE_DIR`, defaulting to `dal-cpp/tests/curve`, and updates the include to `jointxccyquoteriskfixtures.hpp`. Only the fixture directory is added privately, so installed core headers remain the source of DAL declarations. A detached public source bundle must provide the matching shared fixture directory as well as GTest and RapidJSON to build its tests. This is a necessary test-dependency registration adjustment within the authorized standalone-build scope.

`dal-cpp/dal/script/diagnostics.cpp` extracts request rendering into the internal `WriteObservationRequest` helper. The same stream operations, fields, nulls, ordering and plan values are retained. Lizard reports complexity 8 for `ExplainPreparedScript` and 7 for the helper, meeting the existing limit of 8. The real archive and diagnostic JSON tests remain unchanged and run before and after extraction.

Those three files are the entire repair code diff from `eb62b4ab`; this report is the fourth changed file in the publication. DAL::cpp/DAL::public export metadata, installed headers, public signatures, Python/Excel source, CI rules, performance policy and submodule revisions are unchanged. Installed consumer target-property assertions confirm no RapidJSON, GTest or shared-fixture dependency escapes into either library interface.

### CI diagnosis at the old head

One targeted capture read all failed-check annotations and downloaded all 26 failed job logs. **25 build jobs** independently report missing `rapidjson/document.h`, including Clang 18–20, GCC 13–15, MSVC, ASan/UBSan, CoDiPack isolation and the extended Python build. The remaining Windows gate fails because `BUILD_RESULT=failure`; its build-script check succeeds. The preserved snapshot also contains 11 successes, one skipped check and seven running checks; their unknown outcomes were not relabeled.

Codacy is a real code finding, not an external permission/configuration failure: check `104243929830` annotates `diagnostics.cpp:107` with complexity 14, limit 8. `lizard -C 8 -w dal-cpp/dal/script/diagnostics.cpp` independently reproduced exit 1 with CCN 14 before extraction and returns exit 0 afterward. The rule and its threshold are unchanged. Raw evidence is in `old-checks.json`, `old-failure-diagnosis.json`, per-job logs and annotations. The new-head snapshot is captured once after push and delivered separately.

### Fresh RED, GREEN and isolation evidence

The host has `/usr/local/include/rapidjson/document.h`; no copy exists in `/usr/include`. With the original generated test flags, both JSON translation units compile using that host header. `old-system-fallback.log` records the actual `-H` resolution. This explains why prior local passes did not prove a clean CI build.

All repair build/test validations below hide that directory in a process-local mount namespace:

```sh
bwrap --bind / / --proc /proc --dev-bind /dev /dev \
  --tmpfs /usr/local/include/rapidjson -- <command>
```

`isolation.log` proves both system locations lack the header inside that namespace. No system installation or global include flags were changed; CPATH, CPLUS_INCLUDE_PATH and CXXFLAGS are unset. Vendored/source or explicitly supplied test dependencies remain visible. An initial namespace attempt omitted the proc/dev mounts and failed CMake compiler setup; the corrected invocation above is the one used for the actual RED/GREEN evidence.

The added evidence probe `compile_json_tests.py` loads the actual CMake compilation database, selects both existing JSON test files, and retains their complete target flags while replacing object output with `-fsyntax-only -v -H`. It asserts both files are present and requires both compiles to succeed. This exercises the real parser assertions without adding a duplicate behavioral unit test.

```sh
cmake --preset=Release-linux -S . -B build/Release-linux
python3 ../repair-evidence/compile_json_tests.py build/Release-linux/compile_commands.json
cmake --build build/Release-linux --target dal_public_tests --parallel 8
build/Release-linux/dal-public/dal_public_tests --gtest_filter='Script*:ValueTest.*'
```

- RED: `red-json-compile-proc.log`, exit 1, both files fail at line 6 with missing RapidJSON. The copied old standalone CMake configuration also reproduces that exact failure (`standalone-red-json.log`).
- GREEN: `green-json-compile.log`, exit 0; both `-H` traces resolve the pinned vendored header. `green-public-build.log` builds the real target; `green-public-before-refactor.log` and `green-public-after-refactor.log` each pass 40 tests.
- Final fresh configuration: `final-fresh-configure.log` and `final-fresh-json.log`, exit 0, generated independently in `build/repair-fresh` with both declared private dependencies.
- Standalone fixture RED: `standalone-vendored-build.log`, missing `tests/curve/jointxccyquoteriskfixtures.hpp`. Final GREEN: `standalone-final-vendored-build.log` and `standalone-final-vendored-tests.log`, full target and 132 tests pass.
- Missing RapidJSON and missing fixture directories fail clearly at configure time. Tests disabled builds successfully without either dependency. `final-missing-fixture-corrected.log` is the valid fixture-negative probe; the first attempt used relative package prefixes and failed earlier at DAL package discovery, and is retained as setup evidence only.

Every command has a matching JSON receipt with cwd, full arguments, timestamps and exit status. `commands.md` indexes them. Original source/test files, final compile databases, include traces, XML and package-interface evidence are included in the attachment.

### Final verification and limits

- **1,775/1,775 CTests**, including **402 Python tests**, portable Excel contracts, examples and public consumers: `final-native.log`, using the canonical `NUM_CORES=8 bash ./build_linux.sh --python 3.13` inside the namespace. `native-ctest-last.log` retains the Python result.
- **413 core + 40 public** focused tests: final native XML/logs. Core filter is `Script*:AAD*:MonteCarlo*:MCSimulation*:FixingSnapshot*`; public filter is `Script*:ValueTest.*`.
- **Clang ASan/UBSan: 413 core + 40 public + consumer**, all pass with `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1`. Fresh build uses `-O1 -gline-tables-only -DNDEBUG`, sanitizers `address;undefined`, and no system RapidJSON. No sanitizer report.
- **CoDiPack: 404 core + 40 public + consumer**, all pass. The nine-test difference remains the compile-time native-backend-only coverage described in the original report.
- **Standalone with installed DAL::cpp and vendored test dependencies: 132/132**. **Detached dal-public source with installed DAL::cpp, GTest and explicitly supplied RapidJSON/fixture directories: 132/132**. The detached case uses only installed DAL headers and copied test dependencies; no core source root is on its compile path. Prefix-based RapidJSON discovery also compiles both JSON test files.
- **Installed package consumer 1/1; installed script consumer/example 2/2**, with no test dependencies on the library interfaces and system RapidJSON hidden.
- Patch integrity and source identity checks pass. The accepted API note remains byte-identical. No Machinist input/output changed in this repair, so its earlier valid regeneration evidence is retained rather than represented as a new run.

The initial native full pass predates the final fixture-only adjustment; `final-native.log` and the final backend rebuild/test logs verify the completed tree. Sanitizer/backend checks were rerun because this repair also touches production diagnostic rendering. Earlier evidence remains available as `prior-implementation-evidence.tar.gz`, byte-identical to attachment `01a0a328-f2c3-72c5-8224-99051f8f5f55`, SHA256 `a90a3dbe57c3b0eff04dc2d975bea714f3c3b1c8be860a91cf3deef305e8928f`.

Toolchain: Linux x86_64/WSL2, GCC 15.2.0, Clang 21.1.8, CMake 4.2.3, Python 3.13.9, GTest 1.16.0, Lizard 1.23.0, bubblewrap 0.11.1; full versions are attached. Local Clang still emits existing REQUIRE2/dangling-else warnings. No local MSVC/XLL, Clang 18–20, GCC 13/14, XAD/Adept runtime, TSAN or performance run is claimed. Remote CI and Codacy acceptance remain as captured after publication. The parent must accept this repair before advancing the existing DAL-235 → DAL-236 → DAL-237 chain.

## Original implementation handoff — prior-head evidence

## Outcome and source identity

Implemented the approved public C++ settings, archive v2, contract Describe and valuation Explain scope. The implementation is ready for the parent's independent tester stage. This report does not accept the entire F6 expert chain or authorize merge.

- Branch: `feature/dal-203-public-settings`; PR base: `master`.
- Approved baseline and rechecked `origin/master`: `634ee939099ebee862d71b458afcca672d4ad199`.
- Baseline tree: `709b8cd28d000fb91b69432337cb3527ae67a9ab` (contains merged F5 #372).
- **Tested code commit:** `a5601c851a699d1478f1029ea9c796c2b7badd4a`.
- **Tested code tree:** `6fee8ae5379cda7610bbbc150006bb5668419b0e`.
- The next commit adds only this report. Its SHA/tree, the published remote head and draft PR URL are recorded in the final DAL-234 comment and attached `publication.json`; they are distinct from the tested code identity above.
- API note copied verbatim to `.codex/artifacts/api-notes/dal-203-script-public-settings.md`; SHA256 `8331bdde174fcebe355ef668abb9e723116948ed6e9cac425177468ec0f27795`.
- Accepted API evidence input SHA256: `17948dcfb02bf7baba4efc257274707efff967cd38a2b2bce0291beb47e2f139`. Original spec/API/critique and methodology were read before implementation. Input declarations and F5 evidence were not substituted for fresh implementation verification.

## Design and behavior

The old 3–8 argument Value overload forwards to the new typed overload. Both use the existing model-aware preparation and simulation path. The public result remains PV and d_ risk keys: only aggregated PV is divided by path count; the existing normalized risks are returned unchanged.

Core settings retain the original aggregate field order, with evaluationDate_ and the immutable fixing snapshot handle appended. Public names are aliases to those core types. Settings and handles are copied before callbacks. Explicit valuation dates cause no global date read or write; the fallback captures once. Native preparation also owns its execution settings before history reads. Distinct explicit snapshot sources and conflicting canonical contract defaults fail before pricing; equal defaults retain the product's original spelling. An explicit empty snapshot never falls back to global history.

Product data owns copied contract settings. The default writer is manual-build archive v2 with optional default_index. The frozen v1 reader remains byte-identical, reads a hard-coded real v1 JSON golden through the registry, and subsequently writes v2. Market data, valuation settings, slots, plans, bytecode and AAD seeds are excluded.

Describe emits `dal.script-product/2` directly from the unpartitioned parsed contract. It preserves every event and branch, raw input rows, schedule/macro origins, original and canonical index names, explicit versus event-date identity, original SPOT locations and preorder node IDs. It reads no current date or history and creates no model. Empty/definition-only/assignment-only products are valid syntax views with a null payoff where absent. Old JSON /1 rejects any FIX or nonempty default with a /2 pointer.

Explain emits `dal.script-valuation/1` from a real default-price preparation. It reports unique historical/model requests, every original use, canonical bindings, exact fixing times and values, sample slots, timeline/definitions and both full-contract and live-event mappings. It submits no workers, generates no paths, and retains no implicit cache for the next call. Skipped expired requests have null resolution data.

Validation preserves existing source semantics and adds setting field/value/constraints plus observation source/identity information. The old Python-visible positive-path error phrase is preserved. Raw legacy pricing fixtures, independent analytic and fixed-path oracles, hard/fuzzy behavior and AAD/worker lifecycle code remain in place.

## Changed files

The code commit contains 26 files:

| Area | Files |
| --- | --- |
| Accepted contract | `.codex/artifacts/api-notes/dal-203-script-public-settings.md` |
| Public API | `dal-public/src/script.hpp`, `script.cpp`, `value.hpp`, `value.cpp` |
| Settings/preparation | `dal-cpp/dal/script/settings.hpp`, `settings.cpp`, `preparation.cpp`, `simulation.hpp`, `observationplan.hpp` |
| Contract/source diagnostics | `dal-cpp/dal/script/event.hpp`, `event.cpp`, `node.hpp`, `parser.cpp`, `diagnostics.hpp`, `diagnostics.cpp`, `visitor/debugger.hpp` |
| Archive generation | `dal-cpp/dal/auto/MG_ScriptProductData_v2_Read.inc`, `MG_ScriptProductData_v2_Write.inc`; `dal-cpp/cmake/normalize-calibration-generated-enums.cmake` |
| Tests and executable consumers | `dal-public/tests/test_value.cpp`, `test_script_archive.cpp`, `test_script_diagnostics.cpp`; `dal-public/test-consumer/script.cpp`, `dal-public/examples/script_settings.cpp`, `dal-public/CMakeLists.txt` |

No dal-python/dal-excel product files, submodule gitlinks, CI/performance implementation, published docs or CHANGELOG changed. Some nearby formatting in touched C++ files follows the repository clang-format configuration.

## RED → GREEN evidence

Each runtime RED below was observed before its production fix. Compile REDs identified missing API declarations. Logs retain initial failures and setup corrections; no numerical oracle or existing assertion was relaxed.

Focused build command:

```sh
cmake --build build/Release-linux --target dal_public_tests --parallel 8
build/Release-linux/dal-public/dal_public_tests --gtest_filter='<test or suite>'
```

| Focus | RED evidence and result | GREEN evidence and result |
| --- | --- | --- |
| Historical public legacy entry | `red-legacy-initialized.log`: PreparationRequired instead of analytic PV/risk | `green-legacy.log`: historical SCALE/rate/spot/vol oracles pass |
| Typed settings API | `red-settings-build.log`: missing settings aliases/overload | `green-settings.log`: 9 tests pass |
| Setting errors | `red-validation.log`: missing stable field/constraint | `green-validation.log`: 10 tests pass |
| Native tail conflicts | `red-native-tail.log`: conflicting default silently accepted | `green-native-tail.log`: 11 tests pass |
| Archive versioning | `red-archive.log`: writer still v1 | `green-archive.log`: 25 tests pass |
| Describe | `red-describe-build.log`: missing Describe entry | `green-describe.log`: contract schema assertions pass |
| Legacy debug guard | `red-debug-v1.log`: unused default accepted by /1 | `green-debug-v1.log`: guard passes |
| Explain | `red-explain-build.log`: missing Explain entry | `green-explain.log`: actual plan assertions pass |
| Observation errors | `red-observation-errors.log`: missing requested identity/source constraints | `green-observation-errors.log`: 29 tests pass |
| Native execution copy | `red-copy.log`: history callback changes compilation after entry | `green-copy.log`: owned settings still compile and retain entry D/history |
| Describe lookahead context | `red-describe-context-corrected.log`: missing midnight/statement/node | `green-describe-context.log`: both context tests pass |
| Public execution copy | `red-public-copy.log`: date callback changes caller RNG, consumer exits 134 | `green-public-copy.log`: consumer exits 0 with the original settings |
| Existing Python error compatibility | `full-native-ready.log`: 2 path-count regex failures, 400 Python passes | `full-native-delivered.log` and `native-ctest-last-delivered.log`: all 402 Python tests pass |

The public-copy RED/GREEN command builds `dal_script_api_consumer` and runs that executable. Other GREEN logs use the public test binary and expose exact test filters/counts. Test-authoring corrections retained in evidence include registry setup, C++ raw-string delimiters/Cell comparisons, the index parser's preserved company-name spelling, the supported `1CD` schedule tenor, and the model factory's existing Dal::Exception_ plus stable vol/constraint checks. Intermediate backend builds that included a new RED probe before its matching production object were superseded by fresh complete rebuilds.

## Acceptance matrix

All names below are directly present in the delivered source and fresh test logs.

| Parent case | Direct public evidence | Independent/core evidence retained and rerun |
| --- | --- | --- |
| T07 TodayPolicy | `ScriptApiTest.TestTodayPolicy`: BS/Dupire × tree/compiled × double/AAD; global D differs; Model=100 and zero reads; RequireHistorical=80 and one final fixing; missing today fails before workers | `ScriptFixingPreparationTest.TestTodayPolicyAndFutureOnly`; `ScriptObservationSimulationTest.TestTodayPolicyAndRngForBothAdapters` |
| T15 Repricing | `TestRepricingAndExplainNeverCache`, `TestExplainDoesNotFreezeGlobalMarket`: history80→90→explicit80, SCALE PV160→180→160, d_SCALE80→90→80, old plan stays80; independent Explain/Value reads | `TestRepricing`, `TestEvaluationDateDoesNotChangeDuringCapture`, `TestAadRepricingRefreshesGlobalHistoryAndModelInputs` |
| T25 ExpiredAndEmpty | `TestExpiredAndEmpty`, `TestInvalidSettingsOnExpiredProduct`: BS/Dupire zero PV/risks, no reads/workers; invalid handles/settings/paths still fail; Value/Explain reject empty/definitions-only/no-PAYS while Describe succeeds | `TestExpiredSimulationAndSubmissionSeam`, `TestExpiredConfigurationAndNoWork`, `TestExpiredMutatedModelsSkipSetup`: native Allocate/Init/GeneratePath and submission counts |
| T27 Legacy | Independent consumer compiles/links/runs old product3 and Value3–8 calls; `ValueTest.*`; `TestDefaultSpotDeduplicatesAndLegacyGuards`; `TestLegacyEntryPreparesHistoricalParameterRisk` | Raw legacy duplicate/same/distinct-date fixtures; `TestNamedAndLegacyFixedPathParityAcrossAdapters`; compile parity/fuzz and analytic historical AAD/rate oracles |
| T28 Settings | `TestSettingsExplicitDateAndCopiedContract`, `TestSettingErrorsHaveFieldValueAndConstraint`, `TestNativeTailArgumentsRejectConflictingSettings`, `TestNativePreparationCopiesExecutionSettingsBeforeHistory`; consumer date reads/writes and callback mutation probe; installed negative brace probe | `TestPreparationValidatesConfiguration`, `TestModelBindingsBeforeHistory`, `TestOriginalPreparationBraceDefaults`, `TestExplicitSnapshotNeverFallsBackToPresentGlobalHistory` |
| T29 IndexVersioning | `ScriptArchiveTest.TestIndexVersioning`; `TestDescribeContractOnly`, `TestDescribeMacroAndScheduleSources`, `TestDescribeLookAheadIncludesObservationIdentity`, `TestLegacyJsonRejectsUnusedDefault`, `TestExplainPreparation`, `TestExplainRetainedFixingAndPaymentSamples` | Existing raw/dead-branch dump rejection; exact timestamp, FX inverse dependency, unique history/request/source tests; frozen v1 reader unchanged |
| T31 PreparationBarrier | `TestPublicPreparationFailuresSubmitNoWorkersAndRecover`: old/new and double/AAD, last historical read, model setup and compilation errors before workers; path numerical failure then successful recovery; Explain history/model failures submit zero | `TestFinalHistoryFailureAfterModelSetupSubmitsNoWorkers`, `TestModelSetupFailuresPrecedeHistoryAndWorkers`, `TestCompilationFailureBeforeWorkers`, `TestCompiledPathFailureDrainsEveryBatch`, `TestAadPreparationFailureAndPathDrain`, task-group drain tests |

The public adapter cannot inject a custom counting model through its BS/Dupire factory. Public tests therefore count real external history/workers and analytic results; unchanged native counting-model tests prove Allocate/Init/GeneratePath and accepted-task drain behavior. Explain defaults to tree execution, so its tests do not claim a compiled-only failure path.

## Fresh verification

Host: Linux x86_64/WSL2; GCC 15.2.0, Clang 21.1.8, CMake 4.2.3, clang-format 22.1.5, Python 3.13.9. Full outputs, tool versions, commands, JUnit XML, source SHA256 manifest and code patch are in the evidence attachment.

| Verification | Result | Evidence |
| --- | --- | --- |
| Canonical native build/install/CTest | **1775/1775 pass**, including **402 Python tests**, portable Excel contracts, examples and public consumer | `full-native-delivered.log`, `native-ctest-last-delivered.log` |
| Native script/AAD/MC/fixing core filter | **413/413 pass** | `native-core-delivered.log/.xml` |
| Native public script/archive/value filter | **40/40 pass** | `native-public-delivered.log/.xml` |
| Clang ASan+UBSan, leak detection enabled | **413 core + 40 public pass**, standalone consumer exits0; no sanitizer report | `asan-*-delivered.log/.xml`, `asan-delivered-build.log` |
| CoDiPack backend | **404 core + 40 public pass**, standalone consumer exits0 | `codi-*-delivered.log/.xml`, `codi-delivered-build.log` |
| Existing installed package consumer | **1/1 pass** | `installed-base-delivered.log` |
| New consumer/example through installed DAL::public | **2/2 pass** | `installed-api-delivered.log` |
| Untyped fourth `{}` negative compilation | Expected ambiguous overload failure | `installed-brace-negative-corrected.log` |
| Machinist regeneration/drift | `dal_generate` and `dal_check_generated` exit0 | `generate-normalized.log`, `check-generated-final.log` |
| Scope/patch integrity | git diff checks pass; v1 reader, sibling product source and gitlinks unchanged | `code.patch`, `code-files-sha256.json`, `submodules.txt` |

The final full build command was `NUM_CORES=8 bash ./build_linux.sh --python 3.13`. Core filters are `Script*:AAD*:MonteCarlo*:MCSimulation*:FixingSnapshot*`; public filters are `Script*:ValueTest.*`. Sanitizers use `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1` and Clang `-O1 -gline-tables-only -DNDEBUG`. CoDiPack uses `-DDAL_USE_CODIPACK_AAD=ON`; its nine-test count difference is compile-time native-backend-specific coverage, not a reported test failure. Full reproducible commands are in `commands.md`.

## Design choices, limitations and next owner

No substantive API/spec deviation was required. HasPayoff is shared syntax inspection needed by Describe and preparation; it also prevents empty-variable underflow. Diagnostics retain passive metadata instead of reparsing rendered labels. Two v2 outputs were added to the generator's existing normalization whitelist because legacy Machinist templates emitted trailing whitespace; generated files were not hand-patched.

The deliberate C++ fourth-argument `{}` ambiguity remains outside the compatibility promise. C++ int cannot detect fractional values already truncated before entry, and typed structs do not have unknown string keys; language-layer validation belongs to F7/F8. Public binding code in those stages was not implemented here.

Windows/MSVC/XLL, other AAD backends, TSAN and performance benchmarks were not run locally. Clang emitted existing REQUIRE2/dangling-else warnings; no sanitizer finding resulted. Performance is advisory and no performance implementation/report gate was added.

This is implementer self-verification. DAL-235 independent testing, DAL-236 documentation and DAL-237 mandatory review remain parent-controlled work. Publish one draft PR without closing intent or merge, record one CI snapshot, mark DAL-234 in_review, and explicitly wake the idle parent only after the final comment and attachments are readable.
