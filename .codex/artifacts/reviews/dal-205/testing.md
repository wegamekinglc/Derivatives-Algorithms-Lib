# DAL-205 S3 independent verification — DAL-245

## Outcome and source identity

Independent Linux, Windows XLL and actual Excel verification passed. No product defect was found in the tested scope. S3 adds three Excel binding regression tests and extends the installed C++/Python/workbook oracle to cover zero-rate historical pricing. Product sources, generated registration, core/public/Python contracts and formal documentation are unchanged by S3. Parent acceptance, S4 documentation and S5 mandatory independent review remain outstanding; this is not merge approval.

Input PR: https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/376, draft, base master, branch `feature/dal-205-excel-fix-settings`.

- Input HEAD: `bda57413e8016c411c48aad4c3d551585fa1fabc`.
- Input tree: `86b3f47b7db778db2b16ed231397e43ae14e4324`.
- F7 ancestor: `9e6a55f58228a2c5b8101e08b6ab3551ae99a119`, verified with `git merge-base --is-ancestor`.
- Final committed HEAD/tree are recorded in the attached report's committed-source appendix, source identity manifest and final Excel results. This committed report cannot contain its own commit hash.

The authenticated S2 implementation report, evidence archive and workbook match the three SHA256 values in the handoff. All 76 files in S2's internal manifest passed verification. The approved API note hash is `f6895b305ee7722d96fd4fd094fb22b47340a1777123cc1aa62dd6de9da2b90f`. S2 counts are background evidence only; every result below was executed afresh by S3.

## Running existing tests

All commands ran from the repository root unless indicated otherwise. Complete logs, exact Windows drivers, configuration caches, test XML, module/binary hashes, source manifests, workbook and parsed results accompany the issue attachment.

- Full Linux workflow: `NUM_CORES=8 ADDITIONAL_CMAKE_FLAGS='-DDAL_BUILD_EXCEL_PORTABLE_TESTS=ON -DDAL_EXCEL_BUILD_TESTS=ON' bash ./build_linux.sh --python 3.13 --generate > test_output.txt 2>&1`. No old root `test_output.txt` existed. Fresh captured result: **1794/1794 passed**, 17.37 seconds. Benchmarks were excluded. The inventory contains 1607 core tests, 133 public tests, 45 portable Excel tests, six allocation/boundary cases, two public consumer/example entries and one Python suite entry.
- Full Python: `PYTHONPATH=build/Release-linux/dal-python dal-python/.venv/bin/python -m pytest dal-python/tests -q --junitxml=../s3-evidence/pytest.xml`: **641 passed**, 19.44 seconds. This is the newly built CPython 3.13 module from this checkout; its actual import path and hash are captured. The CTest Python entry overlaps this separate execution and is not another 641 unique tests.
- Portable binding binary: `build/Release-linux/dal-excel/dal_excel_portable_tests`: **45/45 passed**, including the three S3 additions. Focused execution of the three new names also passed. After final test-helper layout cleanup, the target was rebuilt and all 45 reran successfully.
- Generation: the full build executed `dal_generate`; `cmake --build build/Release-linux --target dal_check_generated` passed. Both old Product_New and seven-input MonteCarlo_Value generated `.inc`/`.htm` pairs compare byte-for-byte with F7. No generated file changed in S3.
- Installed package: `cmake -DDAL_INSTALL_PREFIX="$PWD/build/stage/Release-linux" -DDAL_CONSUMER_BINARY_DIR="$PWD/build/installed-consumer" -DDAL_BUILD_CONFIG=Release -P tests/installed-consumer/run.cmake`: configure/build and **1/1 passed**. The full build created this installation.
- Installed fixture consumer: `cmake -S dal-excel/tests/windows/native-consumer -B build/excel-fixture-consumer -DCMAKE_PREFIX_PATH="$PWD/build/stage/Release-linux"`, build, then `build/excel-fixture-consumer/dal_excel_fixture_consumer ../s3-evidence/native-diagnostics`: passed, four PV/AAD scenarios and four complete native diagnostic files.
- Python example `009.fix_settings.py` passed normally and with `python -O`, using the freshly built module.
- Sanitizer: fresh Clang Release configuration with `-DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang '-DCMAKE_CXX_FLAGS_RELEASE=-O1 -gline-tables-only -DNDEBUG' '-DDAL_ENABLE_SANITIZERS=address;undefined' -DDAL_BUILD_EXCEL_PORTABLE_TESTS=ON -DDAL_EXCEL_BUILD_TESTS=ON -DDAL_CPP_BUILD_EXAMPLES=OFF`; build target `dal_excel_portable_tests`; `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 build/sanitized-clang/dal-excel/dal_excel_portable_tests`: **45/45 passed**, no findings. Final test-helper layout was rebuilt and rerun under these settings.

Linux versions: GCC 15.2.0, Clang 21.1.8, CMake 4.2.3, Python 3.13.9, Git 2.53.0; WSL2 Linux 5.15.167.4. Native AAD backend AADET. Pinned submodules were initialized in this checkout; no inherited build tree or wheel was used.

## Authoring tests and test support

The production API, complete approved API note, S2 report including its final appendix, implementation changes and nearby tests were read before authoring. Additions exercise already-correct behavior; their first executions passed. No product behavior fix was made, so there is no claimed product RED-to-GREEN cycle.

New tests in `dal-excel/tests/test_script_valuation.cpp`, using the existing suite and XLL-owned runtime seams:

1. `TestDefaultValuationCapturesDateAtEachCall`: construct valuation settings at H with no evaluation date; reuse the same handle at D and D+1. Explain and Value use the current call's date, return model100 then historical80, count the expected final fixing reads, and leave the settings date unset. Explain submits zero workers; Value provides a positive worker control.
2. `TestReusedGlobalSettingsRefreshHistoryAndKeepExplicitSnapshot`: reuse one global settings handle after history80 changes to90; Explain prepares twice each time, with one history/final-fixing read per call and zero workers. The explicit snapshot remains80, Describe is unchanged with zero reads, and Value returns160/180 as appropriate. A unique index and scoped cleanup isolate the written history; date and observer scopes restore state.
3. `TestLegacyAndTypedAadTablesAgree`: the seven-input legacy Value and new typed Value produce identical key sets and fixed-path risks within 1e-8 on a nonzero-volatility SPOT product, preserving five-row/two-column AAD output.

The standalone C++ consumer now emits `prices.json` for historical nonzero-rate, historical zero-rate, mixed and retained-future scenarios; each checks independent analytic PV/AAD values and finite output. The Python verifier checks every native/Excel result key and rejects NaN/Infinity. A zero-rate historical formula at `Values!M20` extends the common workbook and Excel assertions. Separate poisoned-native NaN/Infinity controls both fail as intended. These files are direct tests/fixtures/test support, within S3's write domain.

## Repairing failures

No existing product test failed, and no production or existing assertion was repaired or weakened. One evidence-audit assumption failed: cached workbook handle tags differ from earlier captured tags because Excel recalculates constructors when saving. The audit now validates handle type/name and nonempty tag, while still comparing all saved numeric prices and complete diagnostic chunks. The original audit failure log is retained. This is an ephemeral object-identity issue in the audit, not a product pricing failure.

## Acceptance matrix and independent oracles

- T07/T08: existing typed BS/Dupire × tree/compiled × double/AAD tests prove today Model100 vs RequireHistorical80, missing-today failure, midnight missing when only 11:00 exists, and explicit-empty no-global-fallback with a populated global history positive control. Actual Excel also runs the today policy pair and midnight/11:00 rejection. Native snapshot/date/parser regressions are included in the full suite.
- T27: legacy registration is unchanged; future-only SPOT remains valid; unbound historical SPOT, mixed missing default and unsupported argument forms reject; bound SPOT/FIX share a request. Legacy/new PV and AAD two-column results pass. The added test covers nonzero-volatility AAD parity.
- T28/T30: all constructor defaults, copies and const wrappers; two-column physical row preservation, blank-row skipping, unknown/duplicate keys, duplicate assets, half-empty/type/shape errors, case rules, bool/0/1, finite-positive smoothing, valid Date/integer serial limits, fractional/overflow/nonfinite dates, invalid native snapshots and checked path-count limits are covered. Windows raw tests exercise error/reference/nested-cell rejection, integer normalization, blank references and wrong-class/numeric handles. Actual Excel verifies omitted/string/blank/default handles and an actual non-null empty snapshot.
- T29: public v1/v2 archive and contract-only diagnostics regressions ran. Describe reads no history; Explain reads once per preparation without worker submission or cache. Positive read/worker controls pass. The new wrapper-level tests cover global date/history changes through reused handles. Wrappers store const native values, copy at consumption, and do not store runtime plans.

All languages use D=2026-09-12, H=09-11 midnight, F=09-15, P=09-22, AAPL history80, spot100, SCALE2 and 257 fixed paths. Independent formulas use `tp=10/365`, `tf=3/365`:

- Historical zero rate: PV160, d_SCALE80, d_rate=-160*tp; spot/vol/dividend risks0.
- Historical r=.05: PV=`160*exp(-.05*tp)` =159.78097197125695; d_SCALE=79.89048598562847; d_rate=-4.377560875924848; spot/vol/dividend risks0.
- Retained future r=.05/q=.02: PV=`100*exp(.03*tf-.05*tp)` =99.88773429802069; d_spot=PV/100, d_rate=(tf-tp)*PV, d_div=-tf*PV. Fixing and payment samples are distinct.
- Mixed zero rate: PV260, d_SCALE80, d_spot1. Finite-path future/mixed vega is not assumed zero.

PV tolerance is `1e-12*max(1,abs(expected))`; analytic AAD absolute tolerance1e-10; cross-language fixed-path comparisons1e-8. A separate S3 audit imports no DAL and recomputes 57 numeric checks across all three languages using standard date arithmetic/math. It also verifies 121 cached workbook cells and deep equality of all four complete Excel/C++ diagnostic documents, including long 281617/127403-character documents and Unicode/quote/backslash/newline content.

## Windows build, registration and actual Excel

S3 created its own `C:\dal-build\dal245-s3` source mirror and compared all 3679 tracked files, including initialized submodule sources. The XLL was freshly built in this directory, independently of S2's binary. Source hashes, later changes and final committed-source replay are recorded in the evidence manifest.

The captured driver enters the VS x64 developer shell, configures `Release-windows` with the actual Office paths, builds `dal_excel dal_excel_tests --parallel 8`, runs the three new tests and all **59/59 Windows tests**, and checks binary dependencies. Exact new/legacy registration names, nullable argument order/types, help limits, duplicate names and every exported address pass. This is targeted Windows XLL verification; a full Windows core/public suite was not run. The full core/public suite ran on Linux.

Windows10.0.26200.0; PowerShell5.1.26100.9444; Excel16.0.20326.20144, PE0x8664/x64; MSVC19.44.35228/tools14.44.35207; CMake3.27.6; Ninja1.13.1. Release AADET, static MSVC runtime. Office configuration:

- `OFFICE_EXCEL_EXE=C:/Program Files/Microsoft Office/root/Office16/EXCEL.EXE`
- `OFFICE_MSO_DLL=C:/Program Files/Microsoft Office/root/vfs/ProgramFilesCommonX86/Microsoft Shared/OFFICE16/MSO.DLL`
- `OFFICE_VBE_OLB=C:/Program Files/Microsoft Office/root/vfs/ProgramFilesCommonX86/Microsoft Shared/VBA/VBA6/VBE6EXT.OLB`

Real Excel execution uses `dal-excel/tests/windows/run-script-fix-settings.ps1 -Xll <absolute-own-XLL> -OutputDirectory <own-output> -SourceSha <source> -SourceTree <tree>`. The expanded workbook passed **283 assertions**, **47 captured output blocks**, RegisterXLL=true and own-process cleanup=true. Saved workbook values were independently checked. The exact committed-source replay supplies final SHA/tree and XLL hash; precommit runs are explicitly labelled as working-tree execution. No existing Excel process was terminated.

## Limits and handoff

- Clang ASan/UBSan is fresh S3 evidence. GCC Debug sanitizer was not rerun; the S2 Composite_ typeinfo link failure is retained as a known configuration limitation, not reported as a pass.
- Pinned Machinist output retains its existing template whitespace warnings. S3's hand-written diff and actual generated drift checks pass; no generated formatting edit was made.
- Unicode diagnostics were checked through the existing byte-oriented UTF-8 transport fixture. General Unicode worksheet input is not claimed. The maximum 1048576-row diagnostic-output guard was inspected, not stress-tested with a roughly 31GB string.
- No benchmark/performance gate, CI watch/poll, merge, closing keyword or F9 dispatch. CI receives one post-push snapshot; pending jobs do not invalidate delivery of this independent local test report.
- Final report, exact source identity, command logs, test XML, XLL, fixture and actually executed workbook are attached to DAL-245 for parent acceptance. Formal documentation and independent reviewer remain separate stages.
