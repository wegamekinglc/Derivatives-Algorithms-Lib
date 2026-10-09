# Native Dupire quote curvature

Status: implementation, scoped correctness and costs accepted; publication pending.

## Source and delivery boundary

This implements the first provider in F04's full-recalibration quote-curvature
delivery. Native scalar curvature (#517) and common-path MC curvature (#519)
are accepted. The current first-order Dupire snapshot owns base IVS samples,
fixed grids and the numerical local-volatility surface. Its pullback computes
quote adjoints at that snapshot. Differencing valuations while retaining one
calibration Jacobian omits calibration curvature.

The original 20–28 quote-curvature allowance is divided into native Dupire
smooth objectives (6–8), complete Monte Carlo quote integration (6–8) and
curve-provider integration (8–12). These are subdivisions of the
existing estimate. Policy/estimator selection, native mixed mode, bindings and
general promotion keep their existing separate allowances.

## Mathematical contract

Let c(q) be the native local-volatility calibration on its retained fixed base,
grids and bands. For a scalar native objective phi(c,q), each quote point runs
the complete numerical calibration, objective gradient and calibration VJP:

```text
g(q) = Dc(q)^T * partial_c phi(c(q),q) + partial_q phi(c(q),q)
product[r,j] = (g_j(q + h_r v_r) - g_j(q - h_r v_r)) / (2 h_r)
```

Every g uses its own calibrated surface, objective seeds and calibration
pullback. Direct quote terms and mixed surface/quote terms are included once.
This is a finite-step estimator of the implemented first-order method. It does
not change native higher-order capability or claim an exact analytic Hessian.

## Requirements

1. Recalibrate a `DupireCalibrationSnapshot_` with replacement spread values,
   retaining its base samples, carry, quote axes, fixed surface grid and band
   convention. Do not sample an external IVS again. The original remains valid.
2. Admit replacement spread shape, finiteness and native stencil domains
   passively. Recalibration and admission preserve unrelated recordings and
   caller adjoint mode. Expose the same admission for the outer request.
3. Accept one existing `AAD::NativeScalarFunction_`. Its input order is local
   vol nodes in strike-major order, followed by complete quote spreads in
   strike-major order. The callback must obey the native scalar recording
   contract; parallel simulation or nested recordings inside it are excluded.
4. The outer point is the retained complete spread vector. Directions have M
   rows and Q columns, with one explicit positive finite step per row. Reuse
   robust FMA bump admission and scaled secants from the accepted driver.
5. Validate all numeric and bumped calibration domains before the first
   objective callback. Snapshot the calibration, callback and bump request.
   Copying a callback does not deep-copy caller references in its captures;
   callers must keep captured objective data immutable for the request.
6. Run one full calibration/gradient chain at the base and two per direction;
   stream perturbed results. Empty directions still evaluate the base. Preserve
   submitted order and signed/mixed direction values without normalization.
7. Return owning base value, complete quote gradient, original quote point,
   directions, steps, Hessian products, base calibration and method/count
   metadata. Do not retain bumped calibration sets or a dense Hessian unless
   identity directions explicitly request it.
8. Reject nested outer evaluation before invoking the callback. Restore scalar
   or wide caller mode after success, callback/calibration failures and memory
   failures. Existing recordings are never silently cleared or reused.
9. Apply an optional recording capacity budget to both the objective recording
   and calibration pullback of each complete quote gradient. Numeric result
   payload admission uses `BumpOverAADPayloadBytes(Q,M)`. This counts retained
   result doubles; snapshot storage, temporary calibration/gradient arrays,
   tape capacity and allocator metadata are separate resources.
10. Error context identifies the outer base/direction/sign and failing stage.
    No partially formed result is published. Valid later calls recover.
11. Keep first-order APIs and results unchanged. Add installed C++ consumption
    coverage; Python/Excel projection remains a later binding delivery.

## Acceptance

- Recalibration first fails against the missing entry, then agrees with an
  independent fresh native calibration on flat and nonflat base IVS data.
- Recalibration never resamples a destroyed/mutated original IVS, owns submitted
  spread values, preserves grids/carry/algorithm and rejects invalid domains.
- Independent scalar value differences validate quote bucket, cross and signed
  HVP estimates. A nonlinear calibration case distinguishes full recalibration
  from a frozen first-order pullback. Do not use only manual copies of the
  production secant loop as mathematical references.
- Validate direct-only objectives and surface/quote cross terms, base gradient,
  zero directions, all-bump early admission, budgets, callback failure recovery,
  outer-recording preservation, wide caller mode and independent callers.
- Freeze steps/tolerances before observing the independent comparisons. Require
  adjacent successful steps where numerical stencil noise is material; record
  any finite-step/roundoff limitation explicitly.
- Compile affected code with strict warnings in ordinary and combined lifetime/
  profiling configurations; exercise an installed-only `DAL::cpp` consumer.
- Existing Dupire pullback and generic/MC curvature boundaries use scoped
  checks. Preserve unaffected object/executable identities rather than repeating
  unrelated correctness or performance matrices.
- CI must actually execute new suites in existing sanitizer, extended and MSVC
  profiles. Inspect every current-head review body and inline comment, Codacy
  annotations and checks before guarded merge.

## Cost selection

Freeze two new complete requests: a small flat-base parallel direction and a
small nonflat-base mixed request. Compare with equivalent manual recalibration,
fresh objective differentiation and fresh calibration pullback. Use identical
inputs, guards, result consumption and native precision. New overhead is
informational; no speedup claim is required. Select one existing Dupire pullback
control only if its changed object code affects that hot path. Reuse accepted
generic/MC evidence when their source/object identities remain unchanged.

## Open questions

None require user clarification. Curve quote-instrument rebuilding and complete
Monte Carlo quote wrappers remain separately visible follow-up work inside the
declared quote-provider scope. Binding projection has its separate allowance.
