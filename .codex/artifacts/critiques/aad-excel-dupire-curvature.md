# Excel Dupire curvature design critique

Verdict: Proceed with caveats.

## Blocking issues

None after explicitly defining zero-row shape and complete-axis access.

## Significant concerns

- A blank direction range cannot carry Q columns. Keep `input_count`, preserve
  typed zero-by-Q matrices and test that no dummy zero direction is evaluated.
- Raw Windows coercion can turn bool/text/error into misleading numeric/blank
  values. Guard before conversion and normalize actual integer cells separately.
- Selected/reported base risks are not the raw curvature axis. Verify all Q
  coordinates and signed products against the independent discounted quadratic;
  obtain metadata through the complete existing quote plan.
- Generic recording caps do not bound parallel Dupire workers. Keep the common
  request reusable but let this native adapter reject every provided cap.
- Excel and its test executable own separate runtime state on Windows. Use the
  existing XLL-owned recording test helper for active-caller protection.
- Constructors and getters must not trigger simulation. Use work/fixing
  observers where they add evidence; delegate native planning/execution once.
- Native payload excludes worksheet ownership and detached getter copies.
  Explain the boundary in settings/help/docs and classify unequal timing costs
  as informational. Do not repeat unrelated accepted native timing matrices.

## Minor notes

Keep all input/output local names distinct in Machinist markup, preserve help
length and registration contracts, and keep helper complexity below eight.
Do not rewrite legacy risk templates or calibration axis formatters.

## Counter-proposals

Returning the existing quote-plan handle is smaller than duplicating all axis
formatting. Five common request functions can be reused by subsequent rate,
MC and LSMC increments without a general callback or backend framework.

## Author questions

None. The specification and API note provide executable acceptance boundaries.
