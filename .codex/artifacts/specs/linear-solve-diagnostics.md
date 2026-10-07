# F03 numeric linear-solve diagnostics

Status: active numeric diagnostic increment after merged #490 and #491.
Source: F03 solver precision/conditioning requirements and the user's
native-only, sequential-PR and change-scoped performance requirements.

## Delivery boundary

Return meaningful numerical diagnostics without adding work or retained state
to ordinary numeric or recorded solves. Keep native recording diagnostics,
structured coordinates, implicit calibration and PDE operators as subsequent
deliveries. Do not claim forward-error bounds, regularization, rank truncation,
or higher-order differentiation.

## Requirements and API

1. `DiagnosedLinearSolve_(A, B, tolerance)` owns an ordinary numeric pullback and
   passive diagnostics. Its `Solve()` and `Diagnostics()` accessors are const.
   The constructor binds diagnostics to the same A/B snapshot used for solving;
   callers cannot accidentally pair a factorization with different inputs.
2. Reuse the existing normalized pivoted LU. Solve all columns of a scaled
   identity to obtain the normalized inverse, without refactorization or a
   physical inverse that could overflow for uniformly tiny A.
3. Report `reciprocalConditionInfinity_ = 1/(||A||inf ||inverse(A)||inf)` in
   floating-point arithmetic. Scale the norms and order divisions to avoid
   overflowing a norm/product. A representationally tiny reciprocal can be zero.
   A minimum pivot is not a condition number; do not add an arbitrary rejection
   threshold for an accepted but ill-conditioned system.
4. `LinearSolveBackwardErrors(A, B, X)` independently checks any finite candidate
   solution. Return one componentwise backward error per RHS:
   `max_i |(AX-B)_ij| / (sum_k |A_ik X_kj| + |B_ij|)`.
   A zero denominator corresponds to an exact zero equation and reports zero.
5. Scale products using binary exponents before multiplication. Preserve product
   roundoff with explicit FMA and cancellation with compensated summation; avoid
   depending on extended-range `long double` or platform FP contraction.
6. Reject invalid shapes and non-finite inputs with DAL exceptions. Preserve the
   ordinary singular/pivot/range policy. Diagnostic inverse range failures may
   reject the opt-in constructor even when a particular ordinary RHS is solvable.
7. Use tracked numeric allocations. Retain only the pullback and RHS-sized error
   vector. Release inverse/identity scratch and refund failed reservations.
8. Keep ordinary class layout, caches and runtime implementation unchanged. A
   friendship declaration grants the wrapper factor reuse without publishing
   unvalidated factor access.

## Acceptance

- RED for missing API, then analytic 2x2 condition/RHS results GREEN.
- RED for a residual hidden by product rounding, then positive compensated result.
- Independent analytic nonsymmetric/permuted, diagonal and 1x1 condition values;
  extreme uniform scaling, subnormal A and overflowing unscaled inverse norms.
- Wrong candidate solutions, zero RHS/rows, extreme-product cancellation,
  invalid inputs, owning results and unchanged repeated pullbacks.
- Exact numeric-buffer peak succeeds; one byte less rejects and refunds.
- Fresh narrow tests and actual required platform/sanitizer CI logs.
- Ordinary runtime identity proof; only n=2/32 and m=1/4 diagnostic cost cases.
  No portfolio, Monte Carlo, PDE or full parameter-matrix timing is needed because
  none of their runtime code changes. Retain raw process samples and provenance.

## Methodology sources

The norm/reciprocal definition follows the
[LAPACK condition interface](https://www.netlib.org/lapack/explore-html/d4/daf/group__gecon_ga4f9b830e19e12c7f082ddb497a57af18.html).
The residual metric follows the
[LAPACK componentwise backward-error definition](https://www.netlib.org/lapack/explore-html/d5/da4/group__gerfs_gaf9908a6db85a278e5756cbded5f49819.html).
This implementation computes inverse columns directly; it does not implement
LAPACK's norm estimator or iterative refinement.

Open questions: none blocking this numeric increment.
