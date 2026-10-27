# PR #651 verification

Verified head: `9abfdbd4923506cf515b381b28f2c6b67e4c95cf`.
Base: `1c23b21a6184798aed4a559968e97d29a8267bc9`.
Verdict: **Request Changes**.

## Findings

1. **P1 — Finite-difference solver applies parameter scales twice.**
   `dal-cpp/dal/curve/curveinstrumentcalibration.cpp:682` and
   `dal-cpp/dal/curve/xccybasiscalibration.cpp:969` differentiate `F(z)`, which
   already evaluates residuals at `theta0 + S*z`, then multiply the resulting
   columns by `S` again. The AAD branch correctly applies the scale once.
   Independent one-parameter deposit and XCCY probes reproduce exact-solve
   `CalibrationBudgetExceeded` at scale `0.001`, whereas AAD succeeds in four
   residual evaluations for the same specifications. At scale `0.0001`, the
   deposit approximate solve also produces `CalibrationNumericalFailure` under
   finite differences, while AAD succeeds. The comparison helpers at
   `dal-cpp/tests/curve/test_script_curve_calibration.cpp:39` and `:142` repeat
   the extra multiplication, so their solver-parity assertions do not provide
   an independent derivative oracle. Remove the duplicate scale and add a
   derivative or scale-invariance regression using an independent oracle.

2. **P1 — Workbook handle references are text.**
   `scripts/gen_script_calibration_examples.py:156` stores reference tokens
   returned by `factory_layout` directly as strings. Both committed workbooks
   contain these values: 21 cells across the three yield sheets and 11 cells
   in XCCY. For example, `LOG_LINEAR!E8` contains `$B$4`, and
   `Calibration!I5` contains `$F$4`. Native Excel reports
   `Repository object not found` because the factories receive those literal
   tokens instead of repository handles. Emit cell-reference formulas in
   handle-valued table cells.

3. **P1 — Factory settings ranges include unsupported header rows.**
   `scripts/gen_script_calibration_examples.py:283` passes the complete
   `field/value` table to factories that expect data rows only. The definition
   header created around `:174` has the same problem. Native Excel reports
   `InvalidCalibrationInput: unknown key field`, including on
   `Script Instrument!AA2` and yield-sheet valuation and solver handles.
   There are 35 affected calls across the two workbooks. Pass only the data
   rows for settings matrices; retain the headers required by typed row tables.

4. **P1 — Yield route data overlaps the knot table.**
   `scripts/gen_script_calibration_examples.py:314` writes knot indices into
   `G4:G18`; the route table constructed around `:333` occupies `D17:G18`.
   Its empty tenor does not clear the existing value in `G18`, which remains
   the numeric knot index `14`. After correcting findings 2 and 3 in memory,
   native Excel still rejects every yield market with
   `row=2; column=tenor; ... expected text without NUL`. Separate the two input
   tables. Clearing the unintended tenor in memory allows all three solves.

5. **P2 — Report formulas neither spill correctly nor have clear output areas.**
   The report anchors at `scripts/gen_script_calibration_examples.py:392`,
   `:394`, `:553`, and `:555` are saved as ordinary legacy formulas. Native
   Excel opens them as `=@CALIBRATIONREPORT...`; once the input defects are
   corrected, each shows only `ordinal`. Promoting them to dynamic-array
   formulas exposes seven occupied spill regions: yield `A23` and `A41` on
   each scheme, and XCCY `A28`. Their 22-column observation reports and
   nine-column parameter reports overlap existing settings tables. Excel
   returns `xlErrSpill` (2045; COM value `-2146826243`). Use supported dynamic
   array metadata or explicit array ranges and reserve clear output areas.

## Native Excel verification

Built a fresh Windows XLL and registered it in a separate Excel 16.0 COM process.
`INIT.GLOBALDATA(1)` returned true. Both source workbooks were opened read-only,
and every temporary diagnostic change was discarded. Source SHA-256 values
were checked before and after execution and remained identical.

The original workbooks failed. Diagnostic repairs were applied only to their
in-memory copies: reference formulas, settings ranges, yield route overlap,
and report placement/formula mode. The corrected inputs produced:

| Calibration       | Observations | Maximum absolute raw residual | Result    |
|-------------------|--------------|-------------------------------|-----------|
| LOG_LINEAR        | 13           | 7.17758e-11                   | Succeeded |
| LOG_CUBIC_NATURAL | 13           | 1.32857e-11                   | Succeeded |
| MIXED             | 13           | 2.73350e-11                   | Succeeded |
| XCCY              | 1            | 1.28023e-15                   | Succeeded |

XCCY fitted basis forward: `0.00299999999999885`, matching `0.003`.
For all four calibrations, increasing one market quote by `0.0001` updated the
fitted measure and parameter while retaining `Succeeded` and the `1e-10`
raw residual tolerance. XCCY's bumped basis forward was
`0.002899516309404269`. These are results from diagnostic copies, not passing
results for the committed workbooks.

Both committed workbooks match the generator's cell values, types, and number
formats after normalizing date/datetime representation. The failures originate
in generation, rather than stale binary workbook contents.

## Tests and provenance

- Fresh MSVC 19.44 Release build, C++17, static runtime, native AAD, native
  architecture disabled. `DAL_USE_EIGEN=OFF` uses the built-in matrix kernel
  because the Eigen submodule is absent locally.
- **196/196** existing core tests passed. A focused executable compiled the
  unchanged test sources for curve script families, scripted curve calibration,
  XCCY instrument calibration, underdetermined optimization, preparation, and
  CG/BCG, with the repository's existing test main and Google Test library.
- **306/306** native Windows Excel binding tests passed using the fresh XLL.
- **5/5** CTest examples passed: `curve_calibration`, `interpolate_curve`,
  `xccy_curve_calibration`, `interpolate_curve_script`, and
  `xccy_script_calibration`.
- Native and public script interpolation examples had identical 27 displayed
  node/forward rows. The public XCCY example fitted basis `0.003`.
- Documentation integrity passed for 122 Markdown files; PR diff whitespace
  validation passed.
- Existing working-tree user content and untracked external directories were
  preserved. No product source, generator, generated registration, or workbook
  was edited. The verification run did not submit a GitHub review or comment.

Commands and captured evidence are under `build/`:

```powershell
cmd /c build\pr651-native.cmd
cmd /c build\pr651-focused.cmd
cmd /c build\pr651-probe.cmd
build/pr651-native/dal-excel/dal_excel_tests.exe --gtest_output=xml:build/pr651-validation/excel-tests.xml
ctest --test-dir build/pr651-native --output-on-failure -R '^(interpolate_curve|interpolate_curve_script|xccy_script_calibration|curve_calibration|xccy_curve_calibration)$'
python build/pr651-excel.py
python build/pr651-validation/check-results.py
```

Raw native Excel results: `build/pr651-validation/excel-results.json`.
Validated numerical summary: `build/pr651-validation/summary.json`.
Core and Excel XML results: `build/pr651-validation/core-tests.xml` and
`build/pr651-validation/excel-tests.xml`.
Independent scale probes: `build/pr651-probe.log`.

Fresh XLL SHA-256:
`c38a0d20aa191dcdaddef5fa201d26d9e1f84a41eb4e7ef007f546cdb8b5cb76`.
Core library SHA-256:
`bdd6fa018b12335f247bfee9247e9f06dd7478b643e774cd3927a3430d3867d6`.

The old `build/verify` cache produced a mixed-object link failure and was
discarded as evidence. Accepted results above use `build/pr651-native` only.

## CI, Codacy, scope, and open questions

Codacy's eight annotations were inspected. Its three security-category warnings
are Python `assert` statements at generator lines 701, 702, and 741. Compiling
`validate_examples` with optimization removes all three assertion checks
(three normal assertion opcodes versus zero optimized). This weakens the
validation script under `python -O`; it does not establish an exploitable
security vulnerability. The remaining five annotations concern complexity.

This was a focused correctness and workbook verification. Changed solver and
preparation paths map to the 196 core tests, Excel constructors and adapters
to the 306 binding tests, and public entry points to the five examples and four
native workbook calibrations. The unchanged core examples were smoke-tested in
the shared build; public-disabled independence was not rebuilt locally.
No full suite, Linux/Python rerun, or paired performance measurement was run.
The PR already acknowledges approximately 1.994x yield-calibration time and
remains a draft. That performance objective remains open; neither its timing
claim nor the complete performance regression gate is certified by this report.

No user clarification is needed to reproduce the five findings. Product repairs
remain outside the scope of this verification report.
