# DAL-205 / F8 Excel implementation — DAL-244 S2

## Current handoff: Codacy repair after S3 (2026-09-15)

The seven findings on draft PR [#376](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/376) have been addressed by a behavior-preserving refactor. This section supersedes the historical S2 execution counts below. The existing branch is `feature/dal-205-excel-fix-settings`; base remains `master`. Independent re-verification, documentation and mandatory review remain parent-owned.

Input HEAD `ae2f7d523ae78c8f99a5f84977a6e5e706ba8fc0`, tree `ffe8263c738f4199d98bd3efb548e2fd9e1c56c2`, matches GitHub and contains the S3 tester commit and F7. The three authenticated S3 attachments match their supplied SHA256 values; all 95 evidence-manifest entries verify. No S3 result is counted as fresh execution here. Final committed HEAD/tree and exact-source replay are recorded in the attached report appendix, final source audit and workbook results because this committed report cannot contain its own commit hash.

### Seven dispositions and design

1. `dal-excel/src/__scriptdiagnostics.cpp`: separate UTF-8 lead-byte classification, checked decoding and Unicode escaping from chunk assembly. `ScriptDiagnosticChunks` complexity **19 → 4**; new helpers are at most 7. Decoding checks the remaining byte count before continuation access, rejects overlong encodings, surrogates, invalid continuations and values above U+10FFFF. ASCII, surrogate-pair spelling and 30,000-character chunk boundaries remain identical.
2. `dal-excel/src/__scriptinput.hpp`: extract `IsScriptSettingCell`; `ValidateScriptSettingsRange` complexity **10 → 7**. Preserve raw blank/scalar handling, two-column validation and physical row/column error locations.
3. `dal-excel/src/__scriptsettings.cpp`: extract the omitted/default matrix predicate; `ReadRows` complexity **11 → 7**. Row traversal, first/duplicate row identity, duplicate asset/key labels, half-empty rejection and callback order remain intact.
4. In the same function rename callable parameter `read` to `applyRow`. Flawfinder 2.0.20 reproduces its name-based `read` rule at both declaration and invocation before the change, and reports zero hits afterwards. This parameter is a C++ callback, not POSIX `read`: it receives a key, a const cell and two context strings, not a buffer/count. Column 0/1 accesses follow `Cols()==2`; rows satisfy `0 <= row < Rows()`. No buffer overflow was reproduced and no suppression or boundary check was removed.
5. In the same source extract `MethodValue` and `SmoothingValue`; `MonteCarloSettings_New` complexity **11 → 7**. Keep accepted RNG names, exact bool/0/1 rules, finite-positive smoothing and nullable compiled defaults.
6. `dal-excel/tests/test_script_valuation.cpp`: split `TestTodayPolicyExactTimestampAndExplicitEmpty` (complexity 9) into `TestTodayPolicyAcrossExecutionModes` (7) and `TestExactTimestampAndExplicitEmpty` (3). The first retains all **2 models × 2 compiled modes × 2 AAD modes × 2 today policies = 16** combinations. The second retains the global-history positive control, midnight/11:00 rejection, explicit-empty no-fallback checks and missing-today error. Assertions and combinations are preserved. Portable **45 → 46**, Windows **59 → 60**, CTest **1794 → 1795** reflect this one split.
7. `dal-excel/tests/windows/run-script-fix-settings.ps1`: rename `Release-Com` and every call to `Remove-ComReference`, using PowerShell's approved `Remove` verb. COM release, workbook assertions and own-process cleanup are otherwise byte-identical.

The write domain is exactly those five files and this report. All three S3 added test bodies are byte-identical; its fixture, native/Python consumers, zero-rate oracle and testing report are unchanged. Four legacy generated registration/help files remain byte-identical to F7. No core/public/Python contract, global converter, markup, generated output, analyzer configuration or formal documentation changed.

### RED, GREEN and fresh verification

For this behavior-preserving repair, RED is the reproduced analyzer rejection rather than an invented product failure. Existing behavioral tests remain green; no assertion was weakened to obtain RED or GREEN. This is the justified deviation from adding a failing behavioral regression for a product defect. The attached `run-static.py` is a focused executable check against the original Git blobs and current files.

- **RED:** `python3 evidence/run-static.py red` replays Lizard 1.23.0 (`lizard -C 8 -w` on all four scoped C++ files), Flawfinder 2.0.20 (`--minlevel=0 --error-level=1` on settings), and the PowerShell AST/`Get-Verb` check. Underlying exits are **1/1/1**, with the exact five complexity findings, two `read` hits and one unapproved verb. Original GitHub check-run annotations `104367440846` are attached.
- **GREEN:** `python3 evidence/run-static.py green`: underlying exits **0/0/0**; no complexity above 8 in any of the four files, no settings Flawfinder hits and all hyphenated function verbs approved. PSScriptAnalyzer is absent locally; the PowerShell check uses the actual host parser and `Get-Verb`, not a claim that the full Codacy service ran locally.
- Focused portable settings-row/scalar, Unicode and split-policy tests: **5/5**; all portable tests: **46/46**. Direct binaries produce XML and complete logs. An explicit `<utility>` include was added after the initial full build; both complete portable binaries were rebuilt and rerun after this include-only change.
- Full fresh Linux configure/build/generate/install/CTest: `NUM_CORES=8 ADDITIONAL_CMAKE_FLAGS='-DDAL_BUILD_EXCEL_PORTABLE_TESTS=ON -DDAL_EXCEL_BUILD_TESTS=ON' bash ./build_linux.sh --python 3.13 --generate`: **1795/1795**, 24.33 seconds. Includes 1607 core, 133 public, 46 portable, six allocation/boundary entries, two public consumer/example entries and one Python-suite entry.
- Fresh CPython 3.13 module: `PYTHONPATH=build/Release-linux/dal-python dal-python/.venv/bin/python -m pytest dal-python/tests -q --junitxml=../evidence/pytest.xml`: **641 passed**, 9.28 seconds. This overlaps the CTest Python entry. `009.fix_settings.py` passes normally and under `-O`.
- `dal_generate` runs in the full build; separate `cmake --build build/Release-linux --target dal_check_generated` passes with no drift. Installed package consumer configure/build/test: **1/1**. The installed workbook fixture C++ consumer and Python comparison pass all four scenarios.
- Fresh Clang 21.1.8 Release `-O1 -gline-tables-only -DNDEBUG`, `DAL_ENABLE_SANITIZERS=address;undefined`, leak detection and UBSan halt-on-error: **46/46**, no sanitizer findings. Full configure/build/run commands and caches are attached.
- Additional ASan/UBSan decoder probe compiles the actual changed source and original Git source under distinct symbols. All **1,111,936 non-ASCII Unicode scalars** match Python's independent JSON encoder and produce **433 identical chunks**. All **65,792 one/two-byte inputs** match Python UTF-8 acceptance and the original decoder; ten invalid multi-byte boundaries also reject. No 31GB maximum-output allocation is claimed.
- Fresh Windows x64 XLL configure/build and all raw/typed/registration/export tests: **60/60**. Actual Excel workbook: **283 assertions / 47 captured output blocks**, registration true, saved workbook and own-process cleanup verified.
- C++/Python/Excel: **57 independent analytic PV/AAD checks**, **121 saved workbook cells** and **four complete diagnostic JSON comparisons** pass. Long JSON lengths remain 281617 and 127403 characters. Poisoned native NaN/Infinity controls fail as intended. The zero-rate historical PV160/d_SCALE80 oracle remains included.
- Hand-written clang-format, diff whitespace and documentation checks pass. Source/mirror and legacy-registration checks are attached. Performance remains advisory; no benchmark gate was introduced.

### Build identity, reuse and limitations

Linux uses GCC 15.2.0, CMake 4.2.3, Python 3.13.9, Git 2.53.0, Clang 21.1.8, clang-format 22.1.5 and native AADET. Linux release/sanitizer build directories and virtual environment were created for this run. Pinned submodules were initialized in this checkout; no prior wheel or compiled library was reused.

Windows uses a **new** mirror `C:\dal-build\dal244-codacy`, initially compared across all **3680** tracked files including pinned submodules. It reuses the installed VS/Office/CMake/Ninja environment; source and build directories are new. S3's verification driver/audit source was adapted to this run's paths; no old result is reused. The final report-only commit replay may use this run's already-built XLL when the build confirms unchanged product sources.

Windows 10.0.26200.0; PowerShell 5.1.26100.9444; Excel 16.0.20326.20144, PE0x8664/x64; VS2022 Community, MSVC19.44.35228/tools14.44.35207; CMake3.27.6; Ninja1.13.1. Release AADET with static MSVC runtime. Office paths remain the three explicit paths in the historical section below and are captured in this run's driver/cache. Rebuilt XLL SHA256: `AE7D8A5A3692FA03231E6994CE1FB08F224B794928EA275D6F79F8BA80ADDA66`.

Known GCC Debug sanitizer `Index::Composite_` typeinfo link failure was not retried or reported as passed; the fresh Clang configuration passed. General Unicode worksheet input remains limited by the existing byte-oriented converter; the workbook transports UTF-8 bytes to test the approved output contract. The maximum-row guard is inspected, not stress-tested with a roughly 31GB string. Full Windows core/public tests and alternate AAD backends were not run; Windows verification covers the complete XLL test binary, and full core/public verification ran on Linux.

This is a repair handoff for parent acceptance and independent S3 re-verification. Keep the existing PR draft; no closing keywords, merge, F9, tester/documentation/reviewer dispatch or performance gate. The final attachment records the one post-push CI snapshot; pending Codacy/CI is not a completed result. Only the parent is recalled if idle.

## Historical original S2 record

Everything below records the earlier S2 implementation, not this repair's execution results.

## Delivered behavior

The seven approved functions expose immutable product, valuation and simulation settings, typed product construction/valuation, and native Describe/Explain JSON. Legacy PRODUCT.NEW and the seven-input MONTECARLO.VALUE registrations and date conversion remain intact. MARKETFIXINGSNAPSHOT.NEW now accepts three omitted arrays and returns a non-null empty snapshot; its conversion body is unchanged, including fractional intraday timestamps.

Settings retain physical rows, reject duplicate/unknown/half-empty entries and invalid values with row/column context. Nullable handles accept omitted, empty-string and single-cell blank inputs. Value copies settings and uses native per-call preparation; explicit empty snapshots never fall back to global history. Diagnostics escape non-ASCII UTF-8 equivalently and spill headerless columns of at most 30,000 ASCII characters without truncation.

Branch: `feature/dal-205-excel-fix-settings`, base `master` at `9e6a55f58228a2c5b8101e08b6ab3551ae99a119` (tree `ce51205a29fc1a65750b04937675f78feebe2266`). The final issue handoff records the PR, exact committed SHA/tree, and attached `source-identity.json`; the report cannot embed its own commit hash. The approved API note is unchanged, SHA256 `f6895b305ee7722d96fd4fd094fb22b47340a1777123cc1aa62dd6de9da2b90f`.

## Changed files and design

- `dal-excel/src/__script.cpp`, `__value.cpp`: thin native adapters, shared legacy date conversion and two-column result formatting.
- `__script_storable.hpp`, `__scriptsettings.cpp`, `__scriptinput.hpp`: immutable native values, strict row parser, local raw Excel preflight and single-cell handle unwrapping.
- `__scriptdiagnostics.cpp`: validates UTF-8 and transports native JSON through the existing byte-oriented Excel output converter.
- `__script_test_api.hpp`, `__scripttestruntime.cpp`: existing test-only export pattern; runtime seams initialize/observe the native state inside the XLL. The test executable and statically linked XLL have distinct native globals.
- `__curveprotocol.cpp`: only approved snapshot markup/help. Seven new generated `.inc`/`.htm` pairs and the snapshot pair come from pinned Machinist; no generator/submodule changes.
- `dal-excel/CMakeLists.txt`, `tests/test_script_{settings,valuation,raw}.cpp`, `tests/test_registration.cpp`: actual typed bindings, Windows raw OPER conversion, exact registration metadata and exported function addresses; pinned RapidJSON used for full native JSON equality.
- `examples/010.script_fix_settings.json`, `tests/windows/`: executable fixture generator, isolated Excel COM runner, installed public C++ diagnostic consumer, Python/C++/Excel comparison. The generated `.xlsx` is delivered as an attachment, respecting the repository's binary workbook ignore rule.

The only implementation adjustments to the API note are local normalization of `xltypeInt` to `xltypeNum` before the existing converter (a raw boolean test exposed its union interpretation) and test-only access to XLL-owned globals. Neither changes the global converter or native API. Formal user documentation/CHANGELOG remains S4 scope.

## RED → GREEN evidence

Commands run from the repository root. Logs are in the attached evidence archive.

| Focus | RED command/result | GREEN result |
|---|---|---|
| Default product settings | `cmake --build build/Release-linux --target dal_excel_portable_tests --parallel 8`: new test header absent; compile failure (`red-default-settings.log`) | `--gtest_filter=ScriptExcelContractTest.TestDefaultProductSettings`: 1 passed (`green-default-settings.log`) |
| Product rows | Same build, then `dal_excel_portable_tests --gtest_filter=ScriptExcelContractTest.TestProductSettings*`: stub ignored valid values and invalid rows; 2 expected failures (`red-product-rows.log`) | 3 passed including defaults (`green-product-rows.log`) |
| Valuation/simulation constructors | Same build: new constructor symbols undefined (`red-valuation-simulation-build.log`) | 7 settings tests passed (`green-settings.log`) |
| Typed value/diagnostics | Same build: new product/value/diagnostic symbols undefined (`red-valuation-wrappers-build.log`) | 12 settings/valuation tests passed (`green-valuation.log`) |
| Generated Windows registration | `dal_excel_tests.exe --gtest_filter=ExcelRegistrationTest.TestScriptSettingsAndLegacyContracts`: missing new registration (`windows-red-registration.log`) | Registration suite and all exported addresses pass |
| Raw integer boolean | `dal_excel_tests.exe`: only raw integer boolean failed after correcting test-runtime ownership (`windows-runtime-and-raw-red.log`) | 55/55 before final extra legacy test (`windows-all-green.log`); final count in `windows-final.log` |

No expected production failure was hidden. Additional existing-contract regression tests passed without production changes. Final direct coverage includes BS/Dupire today policy × tree/compiled × double/AAD; historical oracle at 1/257/8193 paths; explicit global and empty-snapshot controls; exact midnight/11:00; cross-date Describe and explicit valuation date; legacy errors and shared requests; no diagnostic cache/workers; invalid UTF-8 and Unicode/long output.

## Fresh verification

| Layer | Actual command / result |
|---|---|
| Linux build | `cmake --preset Release-linux`; configure Python and Excel portable tests ON; `cmake --build build/Release-linux --parallel 8`: passed |
| Portable Excel | `build/Release-linux/dal-excel/dal_excel_portable_tests`: **42/42 passed** |
| Full native/public/consumer CTest | `ctest --test-dir build/Release-linux --output-on-failure --parallel 8`: **1791/1791 passed**, including Python CTest entry |
| Python | Built CPython 3.13 module; full `pytest dal-python/tests`: **641 passed**; current run, not inherited F7 evidence |
| Generation | `cmake --build build/Release-linux --target dal_generate` and `dal_check_generated`: passed; zero drift after staging reviewed generated output |
| Installed package | `cmake --install build/Release-linux`; `cmake -DDAL_INSTALL_PREFIX=<repo>/build/stage/Release-linux -DDAL_CONSUMER_BINARY_DIR=<repo>/build/installed-consumer -DDAL_BUILD_CONFIG=Release -P tests/installed-consumer/run.cmake`: **1/1 passed** |
| Installed public fixture | `cmake -S dal-excel/tests/windows/native-consumer -B build/excel-fixture-consumer -DCMAKE_PREFIX_PATH=<repo>/build/stage/Release-linux`; build and `dal_excel_fixture_consumer <native-output>`: passed, mixed PV `260.00000000000006`, approved tolerance `2.6e-10` |
| Cross-language oracle | `PYTHONPATH=build/Release-linux/dal-python python dal-excel/tests/windows/verify-script-fix-consumer.py --output <json> --excel-results <excel-results.json> --native-diagnostics <native-output>`: passed; full price/risk key sets and all four full diagnostic JSON documents match |
| Sanitizers | Clang 21.1.8, Release `-O1 -gline-tables-only -DNDEBUG`, `DAL_ENABLE_SANITIZERS=address;undefined`; `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 build/sanitized-clang/dal-excel/dal_excel_portable_tests`: **42/42 passed**, no findings |
| Windows build/registration/raw | x64 VS developer shell, `cmake --preset Release-windows`; `cmake --build build/Release-windows --target dal_excel dal_excel_tests --parallel 8`; `dal_excel_tests.exe`: passed before final source-identity rebuild; final fresh log delivered |
| Real Excel | `run-script-fix-settings.ps1 -Xll <this-build/dal_excel.xll> -OutputDirectory <output> -SourceSha <head> -SourceTree <tree>`: **257 assertions passed** before final identity replay; registered actual XLL, 46 output blocks, two-column prices, errors, null variants, exact timestamps, long one-column spills; final replay results delivered |

Independent numeric oracles use D=2026-09-12, H=09-11, F=09-15, P=09-22, AAPL fixing80, spot100, SCALE2. Historical PV is `160*exp(-0.05*10/365)`, scale risk half the PV, rate risk `-(10/365)*PV`, spot/vol/dividend risks zero. Retained future PV is `100*exp(0.03*3/365-0.05*10/365)` with analytic spot/rate/dividend risks. Mixed zero-rate PV260, scale80, spot1. PV tolerance `1e-12*max(1,abs(expected))`, analytic risk `1e-10`, cross-language fixed-path comparison `1e-8`. Mixed/future vega is not forced to zero.

## Windows identity and replay

Windows mirror `C:\dal-build\dal244-f8`; final committed tracked-file content and pinned submodule contents are compared with this mirror before the final rebuild. Final `source-identity.json`, Windows build log, CMake cache, XLL hash/dependency report, and Excel `results.json` establish the source-to-binary-to-workbook chain.

- Windows 10.0.26200.0; PowerShell 5.1.26100.9444; Excel file version **16.0.20326.20144**, PE machine `0x8664` (x64).
- VS 2022 Community installation **17.14.37411.7**, developer shell 17.14.35; MSVC **19.44.35228**, tools 14.44.35207; CMake **3.27.6**, Ninja **1.13.1**.
- Release, **AADET**, static MSVC runtime. XLL `C:\dal-build\dal244-f8\build\Release-windows\dal-excel\dal_excel.xll`.
- `OFFICE_EXCEL_EXE=C:/Program Files/Microsoft Office/root/Office16/EXCEL.EXE`.
- `OFFICE_MSO_DLL=C:/Program Files/Microsoft Office/root/vfs/ProgramFilesCommonX86/Microsoft Shared/OFFICE16/MSO.DLL`.
- `OFFICE_VBE_OLB=C:/Program Files/Microsoft Office/root/vfs/ProgramFilesCommonX86/Microsoft Shared/VBA/VBA6/VBE6EXT.OLB`.

Runner loads the JSON manifest and creates the executable workbook with Excel COM; `generate-script-fix-workbook.py` also creates the static `.xlsx` from that same manifest (requires openpyxl). The runner uses manual calculation and an explicit dependency order. It records actual spilled values and JSON, saves `010.script_fix_settings.executed.xlsx`, closes its own workbook, quits its own new Excel instance, and verifies cleanup of that process. It does not end any pre-existing Excel instance. All calls are foreground and complete before handoff.

The long fixture has 500 events and a name containing quotes, backslash, newline, Chinese and a non-BMP character. To exercise UTF-8 output with the unchanged byte-oriented legacy input converter, the runner supplies UTF-8 bytes as input characters. This verifies the approved diagnostic transport; it does not claim general Unicode worksheet input support. Both long documents exceed 65,535 characters. The installed public C++ consumer constructs identical named products, and the comparison parses all four actual Excel documents and tests full JSON equality.

## Recorded limitations and handoff

- An attempted GCC Debug ASan/UBSan build failed to link the unchanged `Index::Composite_` typeinfo referenced by `ParseComposite`; no core changes were made. The complete Clang ASan/UBSan configuration above built and passed. This is a build-configuration limitation, not a sanitizer pass for GCC Debug.
- Initial COM-runner failures were local format assignment and Boolean Value2 persistence; corrected runner replays pass. A final Windows build exposed formatting-induced include ordering in the raw test (`Windows.h` defines `VOID`); an explicit include-group boundary preserves DAL-before-Windows order. The consumer first used exact floating equality and was corrected to the already-approved PV tolerance; its actual value is recorded above.
- `git diff --cached --check` reports only pinned Machinist template trailing whitespace/blank EOF in new generated files (`staged-check.log`). The manual-source-only check passes, and actual `dal_check_generated` passes. Generated outputs were not edited by hand or reformatted independently.
- S2 is an implementation handoff. S3 independent testing, S4 documentation decision, S5 independent review, F8 acceptance and merge remain parent-owned. PR stays draft and does not close DAL-205 / #362. CI gets one post-push snapshot, without waiting or polling.
