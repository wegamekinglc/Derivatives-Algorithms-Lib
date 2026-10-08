# Sampled theta-step interface

Active numeric PDE API under the [specification](../specs/aad-sampled-theta-step.md).

## Owning numeric boundary

One required configuration object avoids a long positional argument list:
SampledThetaStepInputs_ holds physical grid x_, row discount rates_, drifts_,
variances_, positive dt_, theta_ in [0,1], an n-by-layers oldValues_ matrix,
two explicit old-versus-external boundary provenance flags and externalValues_
of shape 2-by-layers when either boundary is external. Require n>=3, layers>0,
strictly increasing finite grid, n-2 finite interior coefficients and finite
nonnegative variances. Rates/drifts may be signed. All shape/range constraints
apply before arithmetic; never infer boundary provenance from pointer aliasing.
An all-old-boundary request may omit externalValues_; unused external risk is zero.

SampledThetaStepPullback_(inputs, explicitLinearAccuracyPolicy,
optionalRelativePivotTolerance) owns the sampled discrete method and exposes
Solution(), Policy(), forward errors, then Reverse(solutionSeeds). One
n-by-layers seed matrix defines one objective summed over the layer outputs.
Returned oldValues_ risks retain n-by-layers, external boundary risks retain
2-by-layers, coefficient risks have n-2 rows aggregated over layers, and dt_
and theta_ risks are scalar aggregate contributions. The transpose-error axis
has one entry per layer. Independent objectives use separate const Reverse
requests with independent output/scratch; native channels are a later mapping.

The numeric result owns data rather than provider pointers. Retain only
actually required passive grid/stencil data, physical generator entries,
dt/theta/provenance, accepted old/new values and tridiagonal factors. Method
ownership must remain independent of every caller buffer's mutation/destruction.
No parameter-dependent mesh or coefficient-provider derivative is implied.
Variance coordinates are variances; volatility requires the external 2*sigma
chain rule. This step derives the discrete theta method, not a continuum PDE
or certified sensitivity error.

## Boundary and layer derivative semantics

Interior A=I-dt*theta*L and E=I+dt*(1-theta)*L. A's boundary rows are identity;
overwrite RHS boundary rows with the declared boundary values. For A^T lambda=w,
old-state risk is E^T P lambda plus lambda endpoint contributions only for
old-derived endpoints. External endpoint risks are their lambda entries.
Generator risk sums dt*lambda[i]*(theta*new[j]+(1-theta)*old[j]) across layers;
map through the physical Dx/Dxx/rate terms at interior rows only. Time and theta
risks sum the complete discrete contractions. A reported objective's complete
boundary dependence includes any upstream map applied outside this operator.

At theta=0, A is identity and no factorization occurs; theta risk can still be
nonzero, so skipping theta derivatives would be wrong. At theta=1, the old
state's interior identity contribution remains. Endpoint theta finite-difference
references use legal one-sided second-order differences. Old-derived boundary
coordinates receive both interior stencil and endpoint provenance contributions.

## Linear solver choice and accuracy

Use a normalized adjacent-pivot tridiagonal LU, retaining multiplier, diagonal,
first and second upper diagonal plus local swap flags. Forward and transpose
substitution reuse those factors. The accepted structure is documented by
[LAPACK DGTTRF](https://www.netlib.org/lapack/double/dgttrf.f) and
[LAPACK DGTTS2](https://www.netlib.org/lapack/double/dgtts2.f).
Implement repository-native C++ with no external backend dependency. Reject
unsupported pivots/range under an explicit relative policy; do not silently
switch to dense LU. A tiny pivot compared with normalized matrix scale is a
supported-domain decision, not a condition-number certificate.

Physical forward and transpose componentwise errors evaluate at most three
entries per equation, with actual transposed neighbors, scaled products/FMA
roundoff and compensated residuals. Share a private compensated physical row primitive, preserving dense evaluation
order and bounding each tridiagonal equation to at most three entries. The
extraction requires actual dense oracle and caller identity analysis; it does
not justify a full timing matrix.

Do not reuse the dense inverse-column condition diagnostic: it introduces
quadratic work for tridiagonal systems. A linear-work condition estimate, if
added, must have a distinct estimator name and cannot be a certified bound.
This increment exposes physical accuracy/pivot policy. Any condition estimator
requires a separate accepted contract.

## Required executable acceptance

- Missing-interface RED, then one minimum n=3 analytic step/risk fixture.
- Independent 80-digit dense small-step values/transposes and the prepared
  121 coordinate full-solve differences (363 comparisons), including the
  public strong-diffusion adjacent-pivot fixture.
- n=3/nonuniform n=5, theta=0/0.5/1, one/two layers and each boundary provenance;
  distinct tests for endpoint overwrite, dt/theta derivatives and variance units.
- Adjacent swap/second-upper-fill, singular/near-pivot, normalized finite-range
  and actual transpose accuracy cases; no hidden dense fallback.
- Own data, repeated/concurrent const reverse, zero seeds, exact/subnormal/range
  paths, actual result/cache/scratch capacity, exact and one-byte-short refunds.
- Small/medium/large shape resource/work differences demonstrate O(n) storage
  and O(n*layers) work without timing a full Cartesian matrix.
- Current passive ThetaScheme_ and provider behavior retain existing objects;
  run pde_perf only if changed bytes/caller analysis actually require it.
- Publish numeric acceptance before native event/binding integration in a new PR.

Final decisions: share only a private compensated physical row kernel, preserve
dense evaluation order and bound tridiagonal row visitation to three entries.
Expose no reciprocal-condition diagnostic in this first increment; use explicit
accuracy and normalized pivot admission. Copies own independent cache buffers;
copy assignment is transactional, allocating a complete temporary cache before
replacement. Failed capacity admission preserves the destination solution,
policy, errors and reverse risks; self-assignment requires no allocation.
moves preserve the destination and leave the source destructible/assignable.
The [specification](../specs/aad-sampled-theta-step.md) controls acceptance.

## Concrete public names for the first RED

The public header is dal/math/pde/sampledthetastep.hpp in namespace Dal::PDE.
SampledThetaStepInputs_ uses the field names above. SampledThetaStepAdjoints_
uses matching oldValues_, externalValues_, rates_, drifts_, variances_, dt_
and theta_ fields, plus transposeBackwardErrors_. These fields are owning
coordinate risks, including the independent transpose observation per layer.
The forward accessor is ForwardBackwardErrors(). No solve counter, factor
buffer or internal lambda reference is part of the public interface.

SampledThetaStepPullback_ takes const SampledThetaStepInputs_&,
const LinearSolveAccuracyPolicy_& and the optional relative tolerance.
Solution(), ForwardBackwardErrors() and Policy() return const references
owned by the cache. Reverse(const Matrix_<>&) const returns
SampledThetaStepAdjoints_ by value. Copies remain independent owners; moves
preserve the destination and leave the source assignable/destructible.

Constructor validation runs before allocating or capturing large input
buffers. Invalid shape, policy or coefficient arguments identify the offending
constraint rather than failing a caller-memory admission first.
