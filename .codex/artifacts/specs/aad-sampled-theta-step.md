# Owning sampled theta-step

Active F03 numeric PDE increment on feature/sampled-theta-step-pullback, after
accepted native-root [#502](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/502).
Native event and binding integration follow numeric acceptance in later PRs.

## Source and problem

F03 requires derivatives of the discrete PDE solve. The current ThetaScheme_
samples finite-difference coefficients, prepares a passive tridiagonal solve,
and overwrites endpoint RHS values from the target or old state. It exposes no
owning transpose or coefficient pullback. Provider identity/probes do not define
an immutable numerical boundary. Existing Thomas absolute-pivot guards and a
dense accuracy wrapper cannot meet the new normalized-policy and linear-work
requirements together.

Source: dal-cpp/dal/math/pde/thetascheme.hpp and thetascheme.cpp,
pdeoperators.cpp, matrix/linearsolvepullback.cpp, linearsolvediagnostics.cpp,
and tests/math/pde/test_thetascheme.cpp. Independent mathematical evidence is
pde-step-oracle-preparation.json, pde-tridiagonal-pivot-oracle-preparation.json
and pde-physical-residual-oracle-preparation.json in this evidence directory.

## Goals and delivery scope

The next PR adds an optional C++ numeric sampled step, owning forward solution,
checked cached transpose and first-order pullback. It supports rate, drift,
variance, old-state, external-boundary, dt and theta coordinates. Physical mesh
locations are passive. It leaves current ThetaScheme_ and provider semantics
on their accepted path. Native event and Python/Excel integration follow numeric
acceptance in subsequent PRs. It supplies neither a nonlinear calibrator nor a
continuum PDE or sensitivity-error certificate.

## Input and output contracts

1. SampledThetaStepInputs_ groups finite strictly increasing x_ with n>=3;
   n-2 rates_, drifts_ and nonnegative variances_; finite dt_>0; finite theta_
   in [0,1]; finite oldValues_ with n rows and positive layer count; explicit
   left/right externalBoundaries_ flags; and externalValues_ with 2-by-layers
   shape when either flag is true. Signed rates and drifts are valid. When both
   flags are false, an empty external matrix is valid. Reject all inconsistent
   nonempty shapes. Supplied nonempty external buffers must be finite, even for
   an unused side. Boundary provenance is independent of pointer aliasing.
2. SampledThetaStepPullback_ takes this required configuration, a required
   LinearSolveAccuracyPolicy_ and optional relative pivot tolerance
   64*epsilon. Both accuracy limits are finite and in [0,1]; tolerance is finite
   and strictly between zero and one. Validate before numerical assembly.
3. Solution() has n-by-layers shape. ForwardBackwardErrors() has one entry per
   layer. Policy() preserves the explicit accepted policy. Reverse(seeds) is
   const and takes finite n-by-layers seeds for one objective summed over output
   layers. Its owning result preserves n-by-layers old-state and 2-by-layers
   external-boundary risks, n-2 coefficient risks summed across layers, scalar
   dt/theta risks and one actual transpose error per layer. Unused external
   risks are exact zeros. Separate objectives use separate const Reverse calls.
4. No source buffer/provider is retained by reference. Capture enough physical
   stencil/generator, old/new state, provenance and factors to survive source
   mutation/destruction and concurrent const reverse. Avoid retaining unused
   raw coefficients or external boundary arrays after their effects are owned.
5. No reciprocal-condition number is exposed in this first increment. Accuracy
   observations and normalized pivot admission are distinct from conditioning.
   A future estimator would require a separately named interface and evidence.

## Discrete method and derivatives

6. At interior rows L=mu*Dx+0.5*variance*Dxx-rate*I, with the repository's
   nonuniform centered stencils. A=I-dt*theta*L; E=I+dt*(1-theta)*L. Boundary
   rows of A/E are identity. Replace RHS endpoints with declared old/external
   values before solving. Preserve the actual sampled method and physical
   coordinate units. Variance risk requires an outside 2*sigma chain rule to
   become volatility risk.
7. For A^T lambda=seeds, old-state risk is E^T P lambda plus lambda endpoints
   only when that boundary comes from the old state; P zeros boundary rows.
   External risks are the corresponding lambda endpoints. Each interior
   generator contribution is dt*lambda[i]*(theta*new[j]+(1-theta)*old[j]),
   aggregated across layers. Map it through Dx, 0.5*Dxx and -I for drift,
   variance and rate. Only interior rows contribute generator derivatives.
8. dt risk is sum lambda[i]*(theta*(L*new)[i]+(1-theta)*(L*old)[i]); theta
   risk is dt*sum lambda[i]*(L*(new-old))[i], summing interior rows/layers.
   theta=0 uses identity A without a factorization but still computes the theta
   derivative. theta=1 preserves old-state identity and boundary contributions.
   Endpoint theta finite-difference references stay within [0,1].

## Numerical domain and complexity

9. Use repository-native normalized adjacent-pivot tridiagonal LU, including
   second-superdiagonal fill and reverse swaps for the transpose. One capture
   factorization is reused for all layers and reverse calls. n=1/n=2 internal
   factor tests exercise final-pivot edges although public PDE n>=3. Reject
   singular/unsupported normalized pivots; no hidden dense fallback or inverse
   column solves. theta=0 stores no LU and runs no factorization.
10. Admit only representable finite stencil, generator, physical A/RHS, solve
    and contraction values. Clearly reject unsupported scaling/substitution or
    nonzero contribution range loss; preserve representable subnormal values.
    Cancellation to an exact zero is valid. Relative pivot policy describes
    this supported domain rather than certifying an inverse condition bound.
11. Observe physical forward/transpose componentwise backward errors under
    inclusive explicit limits. Each physical equation visits at most three
    actual entries, with transposed neighbor positions and compensated FMA
    product residuals. A narrowly shared private scaled-product/compensated-row
    kernel will preserve dense evaluation order while permitting bounded
    tridiagonal iteration. Run affected dense residual oracles and use actual
    caller identity to select any extra performance evidence. No n-zero scan
    per tridiagonal row is allowed.
12. Factor capture/storage is O(n); forward/reverse and physical error checking
    are O(n*layers). Retained cache and each request's result/scratch are
    O(n+n*layers). Report actual buffer capacities and their overlapping peaks,
    with exact-limit success, one-byte-short failure, refunds and recovery.
    No inferred RSS or cleanup-reservation occupancy is reported as capacity.

## Executable acceptance and scoped performance

13. Missing-interface RED precedes a minimum n=3 analytic step GREEN. Independent
    80-digit small dense oracles cover six n=3/n=5, one/two-layer fixtures with
    theta=0/0.5/1 and all boundary provenance forms. A seventh strong-diffusion
    public-step fixture triggers two consecutive adjacent pivots and retains
    25 additional coordinate references. The combined 121 coordinates retain
    three full-step differences each (363 checks); both physical transpose
    and coefficient contractions must survive that public pivot path.
14. Adjacent pivot, second-upper fill, last pivot and transpose tests use seven
    prepared independent matrices. Seven physical residual references use
    exact binary inputs and 2000-digit arithmetic, including FMA error 2^-55,
    representable tiny ratios, overflowing products and subnormal products.
15. Add ownership, copy/move as supported, repeated/concurrent const reverse,
    zero seeds, shape/range rejection, actual transpose-limit rejection and
    one-byte capacity cases. The prepared complete three-step rollback covers
    33 shared-coefficient, initial-state, time/theta and boundary coordinates
    at three differences each (99 checks), with n=5/two layers and mixed
    provenance. Its maximum final reference error is about 1.347e-14.
16. Measure only newly affected numerical boundaries and smallest defensible
    small/medium/large cases. Resource differences and operation structure must
    demonstrate linear growth. Reuse unchanged current ThetaScheme_/PDE caller
    binaries; run pde_perf only when actual passive caller bytes change. Any
    shared dense residual change adds its direct callers only. Keep the accepted
    paired protocol/threshold; do not run the full Cartesian benchmark matrix.
17. Keep public methodology current-state, record the new capability in the
    changelog, install-test the new header, verify actual new test execution in
    applicable CI profiles, reconcile exact-head Codacy/review, and merge this
    numeric increment before native event integration.

## Open questions

No user input is required. The implementation's owning stencil/factor layout
and concrete capacity formulas pass exact and one-byte-short executable checks.
Any broader provider/mesh derivative or condition estimator is a separate scope.
