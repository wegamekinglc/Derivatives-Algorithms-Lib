# LSMC financial projection critique

Verdict: Proceed with caveats.

## Blocking issues

None. The native algorithm and its estimator semantics are already accepted.
Keep this PR to a closed owning public/Python projection.

## Significant concerns

- Frozen's oracle must reuse the retained baseline policy at every outer point.
  Ordinary bumped Frozen pricing retrains and is not a valid curvature oracle.
- RetrainedBump has two distinct step layers and nonsmooth fit/selection branches.
  Expose both; compare actual-step gradient secants rather than claiming exact
  Hessian symmetry, unbiased Gamma or generic quadratic convergence.
- Prepared historical state must replay current supplied script constants.
  Test a preparation whose original constant differs from the numeric point.
- Copy all numerical requests and plan state before any submission/GIL release.
  Plan metadata must share immutable storage; detached Python getters may copy.
- Simulation getters refer to sealed settings. Explicit false AAD must fail;
  omitted settings may use the established native-risk default.
- Policy coefficients alone are insufficient: retain scalar/multivariate basis,
  normalization, selected degree, fallback and validation diagnostics.
- Numeric payload excludes policy/preparation; recording caps are per batch,
  can fail after training, and do not cap full process allocation.
- Public tests must register DAL themselves and run in independent CTest-like
  processes, avoiding the initialized-launcher blind spot repaired in #531.

## Minor notes and alternatives

Reuse existing strict conversion and passive snapshot types. Do not broaden
native algorithms or add a generic planner framework for two different kernels.
Keep old timing with identity proof; new complete boundary costs cover only
Frozen and RetrainedBump. No new performance matrix is justified.

Author questions: none.
