# DAL-203 F6 implementation handoff

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
