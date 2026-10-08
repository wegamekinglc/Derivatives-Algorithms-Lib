# Sampled theta-step design critique

Verdict: **Proceed with caveats**. This controls the numeric
PDE increment; local implementation acceptance passes and exact-head platform
acceptance remains pending.

## Blocking issues resolved in the controlling specification

- The existing dense diagnosed solve constructs inverse columns and cannot
  satisfy linear work. Use adjacent-pivot tridiagonal factors and omit the
  reciprocal-condition interface; accuracy/pivot admission is not conditioning.
- Endpoint RHS overwrite must also remove E's endpoint rows in the pullback.
  Explicit provenance selects old-state versus external endpoint contributions.
  The mixed-boundary full-step and three-step oracles distinguish this defect.
- theta=0 is a no-factorization path, while its theta derivative generally
  remains nonzero. theta=1 retains old-state and boundary risks. Legal one-sided
  endpoint references cover these distinctions.
- Coefficient rows refer to rates, drifts and variances at the physical grid.
  Volatility risk requires the upstream variance mapping; passive mesh/provider
  derivatives cannot be inferred from identity or three provider probes.
- A compensated residual that scans n zeros per row would still be quadratic.
  Share a private row primitive with at most three actual tridiagonal entries,
  while preserving the existing dense order and testing transposed edge entries.

## Significant implementation caveats

- Adjacent swaps create second-superdiagonal fill; transpose substitution must
  reverse the combined swap/elimination sequence. Last-pivot n=2 and consecutive
  swap references are separate gates, not interchangeable with a normal Thomas
  solve example. The public strong-diffusion step has pivot rows 1,2,2,3,4
  and 25 independent risk coordinates, preventing a private-factor-only gate
  from overlooking the public assembly/transpose/contraction path.
- Physical accuracy must survive product overflow, FMA cancellation and tiny
  ratios. The seven exact-binary 2000-digit references retain an error around
  8.691694759794e-311. A nonzero rounded-away derivative must reject unsupported
  range rather than produce a false finite success.
- Cumulative coefficient risks sum across layers and rollback steps. Single-step
  oracles alone do not verify this dependency; the independent three-step fixture
  has 33 coordinates and 99 complete-rollback differences.
- Shape/range validation, supported nonnegative variance and inclusive policy
  boundaries must precede arithmetic. Constructors/failed reverse must refund
  caller buffers without publishing a partial result; cache reuse stays valid.
- One cached factorization is shared by forward layers and const reverse calls.
  Copies own their buffers; each reverse owns independent result/scratch.
  Concurrent const reverse must exercise different seeds and source destruction.
- Resource and cost claims require actual capacities, construction/reverse
  overlap and linear size differences. There is no justification for a full PDE
  or repository benchmark Cartesian matrix merely from adding this operator.

## Scope and open questions

No user clarification is needed. Native event/report axes and Python/Excel
surfaces follow numeric acceptance. New condition estimators, active meshes and
provider mappings require separate contracts. Private layout and exact capacity
formulas will be finalized against executable minimum implementation evidence.
