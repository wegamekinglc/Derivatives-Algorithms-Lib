# Monte Carlo quote curvature design critique

Verdict: Proceed with caveats.

## Blocking issues

None in the revised controlling specification. Implementation acceptance must
prove the complete recalibration chain with independent passive values.

## Significant concerns

1. Direct seed matrices are first-order data. Reject them; only bound scalar
   definitions provide rebuildable direct curvature. Map by parsed identity,
   preserving unsorted Hybrid raw ordinals and round-trip numeric values.
2. Repeated global history reads would change the function being differentiated.
   Freeze dependencies before executing the base and retain the snapshot for
   all later rebuilt plans, including repeated execution of the same plan.
3. Scalar MC lacks worker recording caps. Explicit rejection is required;
   wrapping only the caller tape would create a misleading resource guarantee.
4. Exercise gradients can contain a policy secant. Reject exercise until the
   separately planned inner/outer-step and estimator acceptance is complete.
5. First-order quote selection/reporting must not alter raw direction meaning.
   Products cover the complete raw quote axis and base projection stays explicit.

## Minor notes and counter-proposals

Reuse the existing scaled quotient and calibration preflight. Keep rebuild as
a small explicit first-order operation so financial tests can inspect its
model/constant identities. Preserve old archives/source evidence; compile only
the changed facade closure and run affected tests.

## Author questions

None. These constraints are implementation choices within the approved plan.
