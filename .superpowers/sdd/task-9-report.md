# Task 9 Report: Excel Resettable and Joint XCCY Surface

## Status

- Starting reviewed head: `8dd73f2e950e7af8c7ce2ec5239543659c3dc548`
- Scope: Excel storables, worksheet implementations, generated registration/help sources, the internal smoke-test linkage seam, and Excel API smoke tests
- Result: locally complete on Linux; the exact pushed head still requires the prescribed Windows `dal_excel` and `dal_excel_tests` CI build

## Implemented surface

- Added `StorableFxResetConvention_`, `StorableMarketFixingSnapshot_`, `StorableCrossCurrencySwapConfig_`, and `StorableJointXccyCalibrationResult_`.
- The joint result wrapper retains the solved domestic block, foreign block, and basis curve handles so the dedicated Excel accessors cannot outlive their result dependencies.
- Added `XCCYRESETCONVENTION.NEW` with fixing lag, holiday center, business-day convention, and fixing-time validation.
- Added `MARKETFIXINGSNAPSHOT.NEW` over parallel index-name, fixing-time, and value arrays. It validates equal lengths and duplicate observations before constructing the core immutable snapshot; core validation continues to enforce non-empty names, valid timestamps, and positive finite values.
- Added the prerequisite `CROSSCURRENCYSWAPCONFIG.NEW` handle factory, which captures nested convention handles, reset mode and identities, and notionals.
- Added `CROSSCURRENCYSWAP.CONFIG.NEW(tradeDate, start, maturity, marketRate, config)` for configured/resettable swap construction.
- Added `CALIBRATE.JOINTXCCY` for one domestic discount declaration, one foreign discount declaration, and one basis declaration. It accepts a valuation timestamp, currency-pair handle, collateral currency, FX spot, the three instrument/knot groups, an optional fixing-snapshot handle, and optional two-column settings.
- The settings dictionary maps curve names, LIBOR bases, parameterizations, log-DF schemes, per-block smoothing, solver controls, solve/Jacobian modes, and matrix-output flags.
- Added dedicated result-handle getters for `domesticBlock`, `foreignBlock`, and `basisCurve`.
- Added the generic matrix getter for `fxForwards`, `marketRates`, `modelRates`, `residuals`, `jacobian`, `parameterRanges`, and `residualRanges`.
- Unknown joint result attributes list all ten accepted views and direct handle views to the dedicated getters.
- Preserved the existing `CROSSCURRENCYSWAP.NEW` and `CALIBRATE.XCCYMARKET` registrations and kept all legacy generated includes ahead of the additive registrations in their original order.

## Review repair: Windows smoke-test linkage

The first Task 9 commit exposed the generated worksheet entry points but did not export the directly called C++ smoke-test helpers. On Windows, `dal_excel_tests` links to the XLL import library rather than compiling those implementation sources, so its handwritten declarations could compile but could not link.

- Added the internal `dal-excel/src/__xccy_test_api.hpp` header declaring only the nine Task 9 helpers used by `test_excel_api.cpp`.
- When `DAL_EXCEL_BUILD_TESTS` is enabled, `dal_excel` compiles those declarations with `__declspec(dllexport)` and `dal_excel_tests` consumes them with `__declspec(dllimport)`.
- With Excel tests disabled, the export definition is not applied, so the internal smoke-test seam is absent from production-only XLL export tables.
- The annotation is empty for the portable Linux harness.
- The implementations and smoke test include the same header; the test's handwritten declarations were removed.
- The fixing-snapshot smoke coverage now also asserts that duplicate `(indexName, fixingTime)` observations are rejected.

## Generated files

Machinist added these 18 Excel sources/help files and changed no `dal-cpp/dal/auto` file:

- `dal-excel/auto/MG_XccyResetConvention_New_public.inc`
- `dal-excel/auto/MG_XccyResetConvention_New_public.htm`
- `dal-excel/auto/MG_MarketFixingSnapshot_New_public.inc`
- `dal-excel/auto/MG_MarketFixingSnapshot_New_public.htm`
- `dal-excel/auto/MG_CrossCurrencySwapConfig_New_public.inc`
- `dal-excel/auto/MG_CrossCurrencySwapConfig_New_public.htm`
- `dal-excel/auto/MG_CrossCurrencySwap_Config_New_public.inc`
- `dal-excel/auto/MG_CrossCurrencySwap_Config_New_public.htm`
- `dal-excel/auto/MG_Calibrate_JointXccy_public.inc`
- `dal-excel/auto/MG_Calibrate_JointXccy_public.htm`
- `dal-excel/auto/MG_JointXccyCalibrationResult_Get_DomesticBlock_public.inc`
- `dal-excel/auto/MG_JointXccyCalibrationResult_Get_DomesticBlock_public.htm`
- `dal-excel/auto/MG_JointXccyCalibrationResult_Get_ForeignBlock_public.inc`
- `dal-excel/auto/MG_JointXccyCalibrationResult_Get_ForeignBlock_public.htm`
- `dal-excel/auto/MG_JointXccyCalibrationResult_Get_BasisCurve_public.inc`
- `dal-excel/auto/MG_JointXccyCalibrationResult_Get_BasisCurve_public.htm`
- `dal-excel/auto/MG_JointXccyCalibrationResult_Get_public.inc`
- `dal-excel/auto/MG_JointXccyCalibrationResult_Get_public.htm`

## TDD evidence

### RED

The repository's Linux CMake configuration intentionally skips `dal-excel`, so the first requested target build established the platform boundary:

```text
cmake --build build --target dal_excel_tests -j 4
gmake: *** No rule to make target 'dal_excel_tests'.  Stop.
```

After the Machinist declarations were added but before generated outputs were accepted, the drift target failed on exactly the 18 missing Excel files listed above. This was the intended generated-source RED.

The review-repair RED used hidden-default ELF visibility as a local analogue for the MSVC import-library boundary. Before the internal header existed, `readelf` reported the directly called helper as hidden:

```text
Dal::XccyResetConvention_New(...)  FUNC  GLOBAL HIDDEN
```

After switching the smoke test from handwritten declarations to the intended shared header, its portable compile also failed with `__xccy_test_api.hpp: No such file or directory`, confirming that the declaration boundary had not yet been implemented.

### GREEN

A Linux-portable test-only compile of the Excel implementation sources exercised the same public functions without Office/COM dependencies:

```text
/tmp/dal_excel_api_tests '--gtest_filter=ExcelApiTest.*'
[==========] Running 4 tests from 1 test suite.
[  PASSED  ] 4 tests.
```

The smoke suite covers configured resettable swap construction, rejection of non-parallel and duplicate fixing observations, an actual small three-block joint calibration plus every result view, and the complete unknown-attribute error list.

With `DAL_EXCEL_TEST_API_EXPORTS` enabled, the same hidden-default probe now reports the helper with default/exported visibility:

```text
Dal::XccyResetConvention_New(...)  FUNC  GLOBAL DEFAULT
```

## Final verification

Generation and generated-source drift:

```text
cmake --build build --target dal_generate -j 4
[100%] Built target dal_generate

cmake --build build --target dal_check_generated -j 4
[100%] Built target dal_check_generated
```

The generation log reported `Wrote 0 files` for DAL core on the final pass; `git diff --name-only -- dal-cpp/dal/auto` is empty.

Underlying public API regressions:

```text
build/dal-public/dal_public_tests \
  '--gtest_filter=CurveProtocolTest.*:CurveInstrumentTest.*:XccyCalibrationTest.*'
[==========] 33 tests from 3 test suites ran.
[  PASSED  ] 33 tests.
```

Underlying core XCCY regressions:

```text
build/dal-cpp/dal_cpp_tests \
  '--gtest_filter=XccyBasisJacobianTest.*:XccyJointCalibrationTest.*:XccyJointJacobianTest.*:XccyMarketTest.*:XccyPricingTest.*'
[==========] 64 tests from 5 test suites ran.
[  PASSED  ] 64 tests.
```

Additional checks:

- `_WIN32` syntax-only compilation passed for all three changed Excel implementation sources, including all generated `.inc` registrations, using a Linux `__declspec` compatibility shim.
- `_WIN32` syntax-only compilation passed for `test_excel_api.cpp` through a wrapper that loads Google Test before defining `_WIN32`.
- The hidden-default visibility probe confirmed that all nine internal test helpers are exported only when `DAL_EXCEL_TEST_API_EXPORTS` is defined and remain hidden without it.
- `clang-format --dry-run --Werror -sort-includes=0` passed for the six manually changed C++ files.
- `git diff --cached --check` passed for the manually maintained source, test, and report files. As with the pre-existing Machinist outputs, generated `.inc`/`.htm` files retain template-emitted trailing whitespace and terminal blank lines; they were not hand-edited because doing so would create generated drift.
- `python3 .github/scripts/check_docs.py` passed for 38 Markdown files.

## Files in task commit

- `.superpowers/sdd/task-9-report.md`
- `dal-excel/CMakeLists.txt`
- `dal-excel/src/__curve_storable.hpp`
- `dal-excel/src/__curveprotocol.cpp`
- `dal-excel/src/__curveinstrument.cpp`
- `dal-excel/src/__xccycalibration.cpp`
- `dal-excel/src/__xccy_test_api.hpp`
- `dal-excel/tests/test_excel_api.cpp`
- the 18 generated files listed above

## Platform boundary and concerns

The Linux host cannot build the real Excel add-in or `dal_excel_tests` because `dal-excel/CMakeLists.txt` omits those targets off Windows. The local portable compile, generated-registration syntax checks, and core/public regressions are green, but the prescribed exact-head Windows CI must still build `dal_excel` and run `dal_excel_tests`; no local Windows success is claimed.
