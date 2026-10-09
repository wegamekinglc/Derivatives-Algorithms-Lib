# Recalibrated rate quote curvature

Status: active implementation; single-curve and same-currency joint increment.
Controlling scope: F04 of [the implementation ledger](../plans/aad-implementation.md).
Cross-currency replay remains required in the following focused PR.

## Numerical contract

For a deterministic native scalar objective `f(theta(q), q)`, return its value,
complete raw decimal-quote gradient and central gradient secants in explicitly
requested directions. Inputs to the objective are free parameters in provenance
axis order followed by quotes in residual axis order. At every point, solve again,
recompute the analytic residual Jacobian and inverse, differentiate the objective
once, and apply the fresh common calibration pullback including direct quote terms.
Do not reuse the base inverse or differentiate solver iterations. Finite steps
are estimates, and native `higherOrder` capability remains false.

Only EXACT, native-analytic, square systems with an available locally invertible
Jacobian are admitted. Check the at-solution `J * E / tolerance` identity, with
a dimension-scaled floating-point residual allowance, at every solve. Reject
approximate and rectangular solves rather than interpreting their weighted
mapping as the derivative of the recalibration algorithm. Ill-conditioned cases
that fail the identity check must fail with a useful diagnostic.

## Ownership and routing

An immutable owning snapshot retains typed calibration definitions, copied native
instruments, deep-copied fixed native curve graphs, raw quotes, solved parameters
and owning provenance. Caller mutation must not change replay. Exact native
instrument types Deposit, FRA, STIR, Future, Swap, OISSwap and BasisSwap are
reconstructed with all conventions and futures adjustments. Reject custom
instrument/curve subclasses before invoking their virtual behavior. Memoized
curve sealing preserves aliases and rejects cyclic bases.

Single instruments are normalized once into solver order, with knots resolved
and frozen. Joint declarations retain their identity, including duplicate display
names; normalize instruments inside each declaration before solving, so residual
ordinals cannot silently permute quote bumps. Preserve parameter/quote axis
fingerprints on replay. No arbitrary callbacks reconstruct calibration sources.

## Admission, resources and failures

Validate the complete finite bump stencil, dimensions, nonzero directions,
positive steps, numeric payload budget and objective availability before calling
the objective. Non-convergence is checked when each point is solved; it is not
promised before earlier objective evaluations. Callbacks must be pure and
deterministic. Exceptions name base/direction/sign and calibration/objective/
pullback stage. Reject active outer recordings and restore caller adjoint mode
on success and failure. Returned results survive later tape reuse.

Numeric payload reports the generic quote bump/result numeric payload only;
it excludes retained calibration definitions, solver matrices, objective
temporaries, allocator overhead and RSS. An optional caller-thread recording
capacity limit covers both analytic recalibration and objective recording in
separate scopes, including existing retained capacity and cleanup reserve.

## Acceptance

1. RED before implementing the absent public API; GREEN against closed-form
   deposit calibration derivatives with mixed direct/parameter dependence.
2. Independent passive price differences at three steps for nonlinear joint
   discount/projection coupling; include unordered instruments, duplicate names
   and layered bases. Demonstrate a frozen-inverse negative control fails.
3. Ownership, unsupported types, solve modes/shapes, malformed requests, resource
   admission, exception context, caller mode and subsequent tape reuse.
4. Relevant existing calibration pullback tests, strict OFF/combined probes,
   installed consumer and scoped paired costs. Reuse unchanged accepted core
   objects with source/dependency/archive proof; no full local matrix.
5. Exact-head CI, Codacy, complete reviews and guarded merge in a new PR.
