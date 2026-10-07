# F03 explicit linear-solve accuracy

Status: active; native coordinate PR #496 is merged at 3d597db8.
Source: approved AAD plan sections 4.3.3/4.3.5/H.1, native-only scope and the
project-wide change-scoped performance requirement.

## Problem, goals and boundary

Existing owning diagnostics report forward backward errors and conditioning.
Callers also need owning transpose-error reports and declared acceptance limits
before treating numerical contributions as derivatives of the physical equation.
Add an optional numeric wrapper. Preserve ordinary numeric/native caches and
work; native report harvesting, implicit roots, PDE and bindings remain later
increments. Introduce no hidden regularization, refinement or iterative method.

## Inputs, outputs and requirements

1. Accept finite dense square A, compatible nonempty B, an explicit passive
   forward/transpose backward-error policy and the existing pivot tolerance.
   Validate finite limits in [0,1] before numeric allocation; equality passes.
   Zero demands a zero reported floating-point error, not certified exactness.
2. Own one accepted diagnosed LU/X cache, immutable policy and an exact physical
   transpose snapshot. Later source mutation/destruction must change neither
   contributions nor residual verification. Never reconstruct A from rounded LU.
3. Check each existing forward componentwise report against its forward limit.
   Return solution and conditioning/forward diagnostics through const getters.
4. Each Reverse/ReverseRhs performs one cached transpose solve and checks
   A^T Lambda=W with the existing compensated/scaled residual evaluator. Return
   owning contributions and a separate per-RHS transpose-error vector.
5. Use one result shape: full reverse contains dense A/B contributions; RHS-only
   leaves the matrix contribution empty and omits its allocation/work/range checks.
   Requested nonfinite contributions, bad seeds/shapes, exhausted buffers and
   unmet accuracy limits throw without publishing partial results.
6. Reverse calls are pure const, repeated/concurrent safe, and their detached
   results survive cache destruction. Failure leaves the numeric cache usable
   for a later valid request. This differs from native graph invalidation.
7. Keep conditioning separate from backward error. Preserve large finite risks
   in accepted ill-conditioned systems; never clip or silently regularize them.
   Entry derivatives refer to the supplied physical matrix; external parameter
   mappings and regularization choices require their own chain rule/identity.
8. All numeric buffers obey existing active budgets, including retained transpose,
   construction scratch, full/RHS-only outputs and reports. Exact healthy peaks,
   one-byte-short rejection and refunds must be demonstrated.

## Executable acceptance

- Missing-header RED; non-symmetric analytic 2x2/multiple-RHS full/RHS-only GREEN.
- Independent rational residuals, including delta=2^-27, A=1+delta, X=1-delta:
   error delta^2/(2-delta^2) rounds to 2^-55. Assert returned bits first, then
   inclusive equality and the nextafter-below rejection independently for both
   policies. B=0/W=1 separates forward-zero from transpose-nonzero acceptance.
- Invalid policies/input/seed shapes and values, passive-matrix unused overflow,
   huge finite risks, signed/zero/multiple seeds and successful failure recovery.
- Ownership, detached reports, concurrent const reverse and exact budget boundaries.
- Verify unchanged archive members and fresh legacy binary identity; reuse their
   accepted timing. New optional-interface cost selects three relevant boundaries.
- Installed consumer, current-state docs, strict warnings, formatting/complexity,
   exact-head CI/Codacy/review and actual new cases in all relevant configurations.

## Open questions

None for the numeric wrapper. Native channel/event report harvesting is a
separate interface decision; this increment does not close all F03 requirements.
