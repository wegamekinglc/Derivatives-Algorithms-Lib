# Rate quote curvature local review

## Findings

No unresolved local production correctness or style findings. Full changed
source, tests, installed consumer, controlling specification/API/critique,
workflow filters and published documentation were reviewed. Publication checks
and external reviews remain required before merge.

The review/testing repaired a real wide-adjoint-mode entry defect: passive
calibration now uses one shared scalar-mode boundary for both factories and
replay, restoring the caller mode on return or exception. Its RED and focused
GREEN are retained. Capacity tests distinguish peak payload from cleanup reserve.

## Open questions

None within this increment. Cross-currency replay, trading adapters and
rectangular/approximate solver derivatives remain explicit subsequent work.
Only smooth exact square-system native objective curvature is delivered here.

## Tests

- Twelve new cases and twelve relevant existing common calibration-risk cases
  pass (24/24). No full local matrix was run.
- Closed-form mixed deposit objective validates value, direct/parameter gradient,
  Gamma, cross-Gamma and signed HVPs; a frozen-inverse negative control fails.
- Twenty-four independent passive price-curvature references cover four quote
  columns, three steps and both unlayered/layered joint discount/projection
  coupling, with unordered instruments and duplicate curve display names.
- Native instrument conventions/futures adjustment, deep fixed-base ownership,
  unsupported subclasses, singular/rectangular/approximate failures, budget
  admission, base/plus/minus objective errors, solver failure, wide mode,
  recovery, active recording and owning results pass.
- Six strict OFF/combined diagnostic probes pass; installed `DAL::public`
  consumer passes 1/1. All 25 previous facade objects remain identical; one
  object is added. Core archive is unchanged.
- Three scoped cost comparisons retain 120 paired process samples and 1.3504
  seconds of timed work. The existing control's executables are byte-identical,
  both round deltas remain below 4%, and new-path overhead is informational.
  See the [cost report](../performance/aad-rate-quote-curvature.md).

Evidence root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261010/rate-quote-curvature`.

## Summary

Local implementation verdict: Approve, subject to complete publication gates.
Residual risks are cross-platform floating-point
behavior and finite-step estimator conditioning; both are explicit boundaries.
