# Native LSMC policy curvature

Status: implemented with local mathematical/lifecycle acceptance;
scoped costs and publication acceptance pending.

## Source and problem

The remaining F04 policy/estimator increment in
[the implementation plan](../plans/aad-implementation.md) requires explicit
Frozen/RetrainedBump and nested-step semantics. Existing
`dal-cpp/dal/script/lsmc.cpp` freezes a fitted policy within one native gradient
request. Calling it again at each outer bump trains a different policy, so that
composition cannot represent curvature conditional on the original policy.

## Goals and boundaries

Provide one owning C++ Black–Scholes LSMC curvature entry, reusing native training,
fuzzy pricing, historical initialization, ordered reductions and policy secants.
The point contains spot, volatility, rate, dividend yield and script constants.
Support tree and compiled execution, existing training/validation/RQMC settings
and explicit outer directions/steps. Language bindings, other model families,
quote recalibration and native mixed mode belong to later increments.

Do not promote finite-path, finite-step products to an analytic or unbiased
Hessian. Native `higherOrder_` remains false. Fully expired and non-exercise
preparations are rejected explicitly by this live-LSMC entry.

## Requirements

1. Snapshot the numeric point and bump request, retain immutable owning preparation,
   and reject nested recording before training or worker submission.
2. Validate dimensions, finite/representable bumps, numeric-output budget, positive paths,
   simulation settings, Black–Scholes domains and every outer point before training.
   Retrained mode also preflights all inner policy-bump domains/representability.
3. Frozen trains exactly the base policy and uses its coefficients, basis,
   normalization and selected degrees for base/plus/minus native gradients.
   Model states, constants and frozen continuation predictions remain active.
4. RetrainedBump trains at each outer point and uses the existing mixed native
   partial plus policy-only secant there. Its inner step for coordinate j is
   `relative * max(1, abs(x[j]))`; the native model-domain one-sided fallback
   remains explicit. Constants use central inner bumps.
5. Every evaluation uses identical absolute training/validation/pricing blocks,
   seeds, replicate keys, RNG/bridge/normal precision, smoothing and program.
   Historical state must be recomputed from bumped constants over sealed history.
6. Return base mean PV and full gradient, point, directions/steps, M-by-N products,
   immutable preparation, base regressions and execution provenance. Products are
   central differences of the declared first-order estimator, without symmetrizing.
7. Numeric-output budget uses the existing owning-double formula, excluding
   retained preparation/policy, metadata and temporaries. Recording capacity cap
   applies independently to each replay batch; report the maximum batch capacity
   and cleanup reserve, not aggregate memory or RSS.
8. Force scalar native adjoints only for the new replay requests and restore each
   caller/worker mode on all exits. Drain failed tasks and leave legacy LSMC
   pricing/risk behavior and profiling-call counts unchanged.
9. Fail with base/direction/sign context; reject nonfinite PV, gradients and
   products. A zero cap must fail and a later uncapped request must recover.

## Executable acceptance

- RED/GREEN distinguishes a genuinely frozen baseline policy from per-point
  retraining. Independent passive Gaussian-path pricing with captured regressions
  checks frozen PV, gradient and directional curvature.
- Retrained products agree with differences of the existing native retrained
  gradients and with nested passive price references at declared inner/outer steps.
- Exercise/constant/history, adaptive degree, RQMC common paths, tree/compiled,
  concurrent immutable preparation reuse, worker modes and failure recovery have
  focused cases. Model-domain, path-range, numeric/budget and unsupported inputs fail.
- A small step/path/replicate study separates outer-step bias, inner policy-secant
  error and sampling dispersion. Smooth references do not certify nonsmooth or
  retraining convergence order.
- Strict OFF/combined probes, installed consumption, four scoped cost cases,
  complete current-head CI/review/Codacy and actual runtime logs gate merge.

## Open questions

None requiring user input. Independent evidence may narrow numerical promotion;
it must not silently change the estimator or weaken acceptance assertions.
