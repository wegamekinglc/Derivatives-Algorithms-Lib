# F04: common-path segmented Monte Carlo curvature

## Source and problem

The native bump-over-AAD driver is accepted in PR #517. F04 still requires a
financial adapter: a segmented Monte Carlo gradient cannot be nested inside its
independent recording callback. This increment composes complete first-order
segmented Monte Carlo requests without building a full-path tape.

The controlling user instructions require native AAD only, separate PRs after
each accepted increment, complete CI/Codacy/review closure, and verification
limited to the code changes' affected scope.

## Scope

Deliver a C++ Black-Scholes compiled-script entry for selected Gamma,
cross-Gamma columns and Hessian-vector estimates. Reuse the accepted segmented
path and deterministic Monte Carlo reduction. Quote recalibration curvature,
exercise policy, alternative estimators, automatic steps, native second-order
tapes and Python/Excel exposure remain separately estimated deliveries.

## Requirements

1. Accept a sealed `BlackScholesSegmentedPath_`, the full model/script parameter
   point, positive path count, explicit `BumpOverAADRequest_`, and optional
   `SegmentedMonteCarloSettings_`. Axes are exactly `ParameterLabels()`:
   spot, volatility, continuously compounded rate, dividend yield, then prepared
   script constants. No coordinate scaling or annualization is implicit.
2. For row k return `(g(x+h[k]*v[k])-g(x-h[k]*v[k]))/(2*h[k])`, where g is
   the mean segmented native AAD gradient. Evaluate the base plus exactly two
   requests per direction. Empty 0-by-N directions return the base gradient.
   Directions are neither normalized nor deduplicated; results are not
   symmetrized. Step units follow the caller's direction convention.
3. Snapshot the kernel, point, directions, steps and settings before any task.
   Every gradient request uses the same sealed preparation, absolute path
   interval, generator, scramble key, bridge and normal precision. Different
   worker counts retain the existing fixed-batch reduction guarantees.
4. Before starting any Monte Carlo task, validate all direction/step shapes,
   finite values, representable effective bumps, numeric payload arithmetic,
   and base/plus/minus model domains and checkpoint plans. Existing Monte Carlo
   admission validates positive path count, random configuration and range
   before its first submission. Admission errors identify base or direction and
   sign where applicable. A later worker failure drains its tasks before
   unwinding, and a subsequent request remains usable.
5. Share the accepted FMA bump construction and exponent-scaled central
   quotient with the generic driver. Reject non-finite values/gradients/products
   and nonzero quotient underflow. Avoid duplicate secant implementations and
   additional type-erased callbacks on the generic hot path.
6. The owning result retains base mean value/gradient, labels, point,
   directions/steps, requested MC settings and the immutable prepared script.
   Preparation includes valuation date, timeline, historical seed, compiled
   contract and smoothing. Input destruction or mutation cannot change it.
   Public result construction rejects null prepared ownership.
7. Report method `BumpOverSegmentedNativeAAD`, gradient-request count and
   numeric payload bytes. Report maximum per-path tape, checkpoint and cleanup
   reserve across all requests. These maxima are not aggregate process memory.
   The count `1+2*M` is not a reverse-sweep count: each request runs many paths
   and segments. Retain the base request's full execution metadata.
8. Numeric payload budget has the same exact scope and formula as the native
   driver: `sizeof(double)*(1+2*N+2*N*M+M)`. It excludes preparation, labels,
   execution metadata, temporary gradients, RNG buffers, tasks and allocator
   overhead. Budgets are checked with overflow-safe arithmetic.
9. Apply the minimum of the bump request's recording cap and the MC path's
   recording cap to every executing path/lane; absence means unbounded. Keep
   requested settings and record the effective cap. Checkpoint cap remains
   per path. Budgets do not imply a total request memory limit.
10. Reject entry inside a live recording before disturbing its graph. Restore
    the caller's scalar/wide mode on success and all failures, including
    allocation failure. Use an existing stack mode guard before mutation.
11. Describe outputs as finite-step differences of the declared compiled
    first-order gradient. Fuzzy conditions use piecewise linear/triangular
    kernels and hard extrema can remain nonsmooth; a positive smoothing width
    does not establish global C2 regularity, unbiased Gamma or second-order
    convergence. Native `higherOrder_` capability remains false.

## Executable acceptance

- Missing-entry RED followed by GREEN on a deterministic time-zero quadratic:
  exact Gamma, model/script cross terms, signed mixed directions and zero axes.
- Independent GBM pathwise polynomial Hessian and gradient references, using
  explicit draws at a nonzero offset, including volatility/rate/dividend/script
  cross effects; errors use justified finite-step tolerances.
- Smooth polynomial step refinement and a vanilla call Gamma reference with
  separately declared path count, finite step and sampling/discretization
  tolerance. Do not claim all payoff nodes satisfy the smooth test's order.
- Complete admission, all-bumps domain preflight with zero submissions, owning
  snapshot/provenance, caller mode/nesting, per-path cap intersection,
  worker/submission failure recovery, and fixed-path determinism cases.
- Rerun the 14 accepted generic-driver cases because its shared numerical code
  moves, plus the affected MC boundaries. Reuse byte-identical unrelated
  archive members. Compile affected units under strict warnings and validate
  an installed-header consumer. Required remote CI remains mandatory.
- Preselect two small financial cost cases against equivalent manual segmented
  secants and one existing generic-curvature control. No full RNG/thread/path
  matrix. Existing caller regressions use the established two-round +4% gate;
  new-entry overhead is reported without an invented speedup claim.
- Update current-state methodology and changelog; close every current-head
  inline and review-body finding before guarded merge.

## Open questions

None block this bounded increment. Remaining F04 deliveries retain their own
scope, estimates and acceptance criteria in the implementation ledger.
