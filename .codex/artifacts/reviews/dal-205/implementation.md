# DAL-205 / F8 Excel implementation — DAL-244 S2

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
