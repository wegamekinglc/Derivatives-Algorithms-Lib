# Common-path Monte Carlo Dupire quote curvature

Status: controlling specification; implementation not yet accepted.

## Source and problem

The user authorized the complete native-only AAD plan, requires separate PRs
after accepted increments, and requires local validation to follow the changed
behavior. #520 is merged at `262492d79d701a3b81acc9e12e4354649aa8054c`.
Its native objective cannot contain a nested or parallel Monte Carlo recording.
The existing `DupireScriptRiskPlan_` already owns the product, native Hybrid
model, quote projection, and mandatory local-vol/direct-constant input axes.
Use that financial first-order chain at each quote point instead of treating
the original calibration Jacobian as constant.

## Goals and boundary

Provide explicit Gamma, cross-Gamma, and Hessian-vector rows for European script
valuation through complete Dupire recalibration. Support the existing native
Hybrid components, tree/compiled execution, deterministic carry, fixed random
paths, and direct dependencies represented by scalar script quote bindings.
Keep scalar/global AAD mode and caller-owned definitions intact after success
or failure. Results must outlive their input handles.

Exercise-policy curvature, rate-curve calibration, mixed-mode differentiation,
bindings, automatic steps, and estimator promotion remain subsequent stages.
An externally supplied first-order direct seed does not define its derivative
and must be rejected. Worker recording limits are absent from the existing
scalar MC request; reject a curvature recording-cap request explicitly until
the worker-aware limit is connected. Do not silently enforce it on only the
calibration tape.

## Functional requirements

1. `RecalibrateDupireScriptRisk` returns a new immutable first-order plan for
   a supplied full spread matrix. Reuse the sealed implied-vol samples and
   calibration grids. Replace only the named local-vol component's surface;
   retain its index, currency, factor, spot, dividend and step, other model
   components, correlations, product settings, and execution settings.
2. Rebuild each bound scalar constant using the parsed constant identity and
   its original definition row. Serialize its new value at round-trip double
   precision. Never replace arbitrary payoff text or confuse raw ordinals
   with sorted model-component positions. Unknown/non-scalar definitions fail.
3. Freeze evaluation date and historical fixing snapshot once for a curvature
   plan. Resolve global fixing dependencies once when no explicit snapshot is
   supplied. All base/plus/minus executions use the same frozen snapshot.
4. A curvature plan owns a sealed base first-order plan, directions and steps.
   Coordinates are the full raw strike-major spread axis, independently of
   first-order output selection or report factors. Validate dimensions,
   finite/representable bumps, nonzero directions, positive finite steps,
   checked payload size, unsupported modes, and every calibration stencil
   before the first MC evaluation.
5. Evaluate the base quote gradient and both signs of every direction using
   fresh numeric recalibration, native MC parameter/direct gradients, and a
   fresh native calibration VJP. Add direct terms exactly once, with exactly
   one path normalization. Use the existing scaled central quotient.
6. Each gradient uses identical path count, RNG method, Brownian bridge,
   normal precision, compilation, smoothing and evaluation date. Native
   capability continues to report no exact higher-order support.
7. Return the owning base first-order result, raw point, directions, steps,
   complete raw input axis, and direction-major full Hessian products. Report
   method, `1 + 2M` evaluations, paths per evaluation, and numeric payload.
8. The numeric budget covers the generic point/gradient/direction/product
   double payload plus the retained base financial result's declared numeric
   payload. It is a conservative numeric-payload accounting rule; metadata,
   snapshots, temporary plans, allocator overhead, worker tape, and RSS are
   excluded. Validate the combined budget before fixing reads or simulation.
9. Reject active outer recordings before changing state. Errors identify
   planning/execution phase and base or direction/sign. A failed request must
   leave a subsequent valid request usable and an earlier result unchanged.

## Executable acceptance

- First RED: a nonzero direct-quote quadratic needs rebuilt scalar definitions;
  the old API cannot supply a rebuilt plan. GREEN checks unchanged carry/other
  components and exact raw direct Gamma on common paths.
- Independent passive full recalibration and MC values check flat/nonflat
  mixed local-vol/direct curvature at three declared outer steps. Freeze
  tolerances before observing results. Include a control that detects a frozen
  calibration map and a direct-only analytic polynomial control.
  Nested double price differences exposed calibration-stencil cancellation;
  the reference generator uses extended-precision Black calls and Dupire
  stencils before ordinary passive MC. Preserve the original steps and
  `0.15 + 0.02 * abs(reference)` tolerance. Save its 36 independent values for
  portable tests because MSVC `long double` has no extra precision. The
  regeneration tool is `dal-public/test-support/dupirecurvatureoracle.cpp`.
- Tree and compiled results share paths and agree; selected/reordered quote
  outputs leave full raw products unchanged. Signed directions and empty
  direction rows work. Caller mutation/destruction cannot change a plan/result.
- Invalid shapes, NaN/Inf, zero/unrepresentable directions, calibration domain,
  external direct seeds, exercise and recording caps reject before MC; exact
  and one-byte-short numeric budgets, frozen history, wide-mode recovery and
  nested recording preservation receive focused cases.
- Fully expired European contracts return zero without global history reads
  or workers; reject exercise before generic preparation capability checks.
- Run affected existing Dupire request/pullback tests only. Strict compilation
  covers changed units and an installed `DAL::public` consumer. Preserve core
  archive identity and retain unaffected accepted evidence.
- Scoped performance: two small flat/nonflat complete MC curvature workloads,
  a matched manual rebuilt-gradient reference, and one affected existing
  first-order request control. Two short paired rounds; no old full matrix.
- Before merge: inspect complete review bodies and inline/issue comments,
  zero actionable findings/Codacy issues, successful current-head checks,
  actual new-case execution in applicable runtime profiles, and guarded merge.

## Open questions

None needed for implementation. Worker recording caps and policy estimators
remain explicit later scope; neither is advertised as available here.
