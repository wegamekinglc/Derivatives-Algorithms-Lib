# PR 651 Codacy repair acceptance

Baseline source: `1c1b36c2ac7194924f1db2446366a143b5539a85`.
Scope: eight Codacy annotations on that head; no numerical algorithm or workbook
content changes are intended.

## Selected coverage

- `calibration_internal.hpp`: typed yield and XCCY solver completion, covered by
  existing exact/approximate/failure tests and end-to-end calibration timings.
- `curvescript_internal.hpp`, `curvescriptevaluation_internal.hpp`, and
  `curvescriptlowering.cpp`: scalar/affine cashflow arithmetic, observation
  binding, and forward pattern recognition. Run the curve-family tests,
  including small/overflow scratch, history, projection, and denominator cases.
  Time the actual 13-quote public yield fixture in all three interpolation
  schemes, with AAD and cached preparation.
- `gen_script_calibration_examples.py`: optimized-Python validation and shared
  workbook formatting. Run the validation regression in normal and `-O` modes
  and compare generated workbook content and formatting with the baseline.

Performance sampling uses the repository benchmark harness: three warmups, ten
internal samples, and ten inner loops per case. Collect two rounds of ten
interleaved process samples per side, alternating the first side; use minimum
reduction and the existing 4% threshold. Rebuild the affected library before
head sampling. Baseline and head benchmark executables must be retained
separately with hashes and identical compiler/configuration settings.

These four calibration cases are informational coverage outside the scheduled
nine-target regression gate. Unchanged tape, matrix, PDE, RNG, portfolio risk,
and Monte Carlo engines are excluded because the refactor changes only the
typed calibration adapter and its deterministic script evaluation. Shared
header recompilation alone does not expand the selection to every caller.

## Correctness and static checks

The three validation assertions now call a shared runtime check that raises
`RuntimeError`. The regression first failed for seven invalid-result scenarios
under both normal Python and `python -O`; after the repair, all three test
methods pass in each mode, including successful execution of all four fits.
Failures cover fit status, parameter count, residual tolerance, wrong basis,
and nonfinite results. The test supplies calibration results through a fake DAL
module; actual numerical execution remains covered by the C++ and Excel tests.
Worksheet imports are stubbed because these tests exercise only validation;
the regression also passes with site packages disabled, so CI does not need
an additional openpyxl dependency to discover it.

All five originally flagged functions are within Codacy's complexity limit of
eight, measured with Lizard 1.24.3, which reproduces the original annotations:

| Function                | Before | After |
|-------------------------|--------|-------|
| CompleteCurveSolve      | 16     | 6     |
| EvaluateIndependent     | 9      | 6     |
| Measure                 | 10     | 8     |
| Lowerer_::Forward       | 9      | 6     |
| generate_yield_workbook | 9      | 5     |

Every function in the five changed production files and the new test file has
complexity at most eight. Bandit 1.9.4 reports zero issues and zero analysis
errors for both Python files. Ruff 0.17.3 and clang-format 23.1.3 checks pass.
No analysis rules or thresholds were disabled.

- Existing focused C++ tests: **196/196 passed** using freshly compiled test
  sources and the rebuilt native core library.
- Native Windows Excel binding tests: **306/306 passed** using the rebuilt XLL.
- Related core/public CTest examples: **5/5 passed**.
- Python validation regression: **3/3 passed** normally and **3/3 passed** with
  `python -O`, each exercising seven invalid-result subcases and one valid case.
- Both generated workbook ZIP/XML packages, including styles, formulas, and
  dimensions, match the baseline exactly except for timestamp properties.
  The committed `.xlsx` files were not regenerated or modified.
- Documentation integrity: **124 Markdown files passed**.

## Performance results

The development workstation is Windows on an Intel Core i9-13900HX (24 cores,
32 logical processors). Both binaries use MSVC 19.44, C++17, `/O2`, static
runtime, native AAD, `DAL_NUM_THREADS=1`, `InitGlobalData(1)`, native architecture
disabled, and `DAL_USE_EIGEN=OFF`. The benchmark uses the existing public yield
fixture and core XCCY fixture, with numerical fit checks on every invocation.

The baseline benchmark was compiled and smoke-tested before editing production
files. Its linked core archive matched the accepted verification archive's
SHA-256 `bdd6fa018b12335f247bfee9247e9f06dd7478b643e774cd3927a3430d3867d6`.
That establishes applicability of the reused baseline build to the baseline
source. Baseline and candidate benchmark executables were retained separately.
Ninja's existing dependency records omitted header dependencies, so all 16
affected core/public object files were explicitly invalidated before rebuilding
the candidate libraries, examples, XLL, and tests. No stale object was accepted
for a changed header's callers.

Two rounds of ten alternating process samples per side were collected after
compilation finished. The combined minimum ratios are:

| Case       | Baseline minimum | Candidate minimum | Ratio  |
|------------|------------------|-------------------|--------|
| LOG_LINEAR | 736.87 us        | 741.49 us         | 1.0063 |
| LOG_CUBIC  | 828.73 us        | 830.59 us         | 1.0022 |
| MIXED      | 754.74 us        | 761.71 us         | 1.0092 |
| XCCY       | 380.46 us        | 371.68 us         | 0.9769 |

No case exceeds the 4% threshold in both confirmation rounds. LOG_CUBIC was
1.0471 in the first round and 1.0022 in the second; the first-round slowdown
did not reproduce. These are scoped informational results on a development
workstation, not a certification of the scheduled nine-target regression gate.

Baseline benchmark SHA-256:
`950e2bc90845886351696860801f5afa470f558150ed4b8c7de5b09d549b5ff3`.
Candidate benchmark SHA-256:
`c16782362f5f90fe9314b5235530b99ecd6f06a8db5a38786241b18f57e4d136`.
Candidate core archive SHA-256:
`92aa712201205152dae8b528fe47fa3984586fa430b9f6cc190ef6febd0f10b5`.
Candidate XLL SHA-256:
`fd56c6e80125d2915d5eeb057ce49eac5d703f7474065a16d473a186511627dc`.

Raw process outputs, parsed samples, and reductions are retained locally under
`build/pr651-codacy/paired/`, including `results.json`. The affected-object
manifest is `build/pr651-codacy-invalidated-objects.json`. The candidate patch
is captured in `build/pr651-codacy/candidate.patch`.

## Commands and limits

```powershell
python -S -m unittest discover -s .github/scripts/tests -p test_script_calibration_examples.py -v
python -S -O -m unittest discover -s .github/scripts/tests -p test_script_calibration_examples.py -v
cmd /c build\pr651-native.cmd
cmd /c build\pr651-codacy-focused.cmd
build/pr651-native/dal-excel/dal_excel_tests.exe --gtest_output=xml:build/pr651-codacy/excel-tests.xml
ctest --test-dir build/pr651-native --output-on-failure -R '^(interpolate_curve|interpolate_curve_script|xccy_script_calibration|curve_calibration|xccy_curve_calibration)$'
python build/pr651-codacy-paired.py
```

The full test suite, Linux/Python binding builds, native workbook recalculation,
and the scheduled performance matrix were not repeated. The five functional
findings in the earlier PR verification report remain open; this repair targets
the eight Codacy annotations. Remote Codacy analysis validates the published
commit separately from the local static checks recorded here.
