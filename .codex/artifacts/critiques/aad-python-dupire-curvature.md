# Python Dupire curvature critique

Verdict: Proceed with caveats.

## Blocking issues

None in the bounded projection, provided the acceptance cases below remain gates.

## Significant concerns

- Base first-order quote selection/report factors must not shorten or scale raw
  HVP axes. Verify selected/reordered/report-scaled base and full signed products.
- `recording_capacity_budget_bytes=0` is present, not absent. Preserve explicit
  rejection instead of interpreting falsey zero as an unspecified cap.
- Pybind reference policies can leak mutable native matrices or nested state.
  Return copies explicitly and mutate every returned matrix in the tests.
- Release the GIL only after copying every Python input and before entering the
  native valuation mutation guard. Do not wrap active callbacks or reuse exposed
  Python references while the GIL is released.
- A quadratic direct-only oracle proves axis/sign semantics but omits calibration
  curvature. Also compare a surface-dependent objective with independently rebuilt
  plus/minus first-order calls on identical settings and paths.

## Minor notes and counter-proposals

Prefer one new binding translation unit and existing strict request helpers.
Keep finite-step terminology in the documentation and execution label. Do not
merge unrelated rate or Excel projection work into this PR. Preserve old native
library identity rather than rerunning all numerical acceptance.

Author questions: none.
