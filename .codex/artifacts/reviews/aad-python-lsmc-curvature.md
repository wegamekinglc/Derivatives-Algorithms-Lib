# Owning LSMC financial boundary review

## Findings

No blocking local findings. Reviewed the complete new public header/source,
binding, tests, installed/script consumers, registration changes and current
documentation against the specification, API note and critique.

The first Python run found one history fixture without the explicit default
index required by its combined SPOT/named-FIX contract. Add that contract setting;
the focused repair and all 35 cases pass without changing production behavior.
The performance bridge first mixed integer/double template constructor arguments;
use explicit double literals and repeat only its two strict probes before timing.
Neither repair changes native LSMC or prior bindings.

## Methodology and ownership

Factory-only preparation binds the concrete model type and seals contract,
settings and history; every numeric model/constant coordinate comes from the
evaluation point. O(1) plan copies share immutable metadata. Numeric requests
are copied before submission; Python copies typed arguments before GIL release.
An observer mutates the caller's plan, point and bumps after submission without
changing retained results. Evaluation performs no history reads.

Frozen references reuse the retained policy at all points. Independent finite
path price/gradient/HVP replay covers tree and compiled modes, mixed/signed
directions and historical constant replay. RetrainedBump agrees with complete
declared first-order estimators at base and outer points; its base price has an
independent fixed-policy check. Outer steps and inner policy secants remain
distinct, with no exact Hessian or generic convergence claim.

Budget metadata describes numeric payload and per-batch recording bounds.
Numeric/domain rejection occurs before submissions; recording failures may
follow training. Policies, nested arrays, settings and results are detached
Python values, and independent callers own evaluation state.

## Tests

- Five C++ cases pass with standard Google Test main, also separately in five
  fresh processes; explicit per-case registration avoids hidden initialization.
- All 35 new Python cases and six selected existing actual callers pass.
- Fourteen strict OFF/combined probes, installed consumers 3/3, installed script
  consumer, documentation links and whitespace checks pass.
- Two boundary cost cases pass 80 calibrated alternating-pair samples in
  8.17993001 measured seconds. Unequal contracts are informational only.
- Native source/header/configuration and old object identity justify preserving
  prior accepted timing; no unrelated local full suite or matrix repeats.

## Open questions and residual risk

None blocking local publication. Actual MSVC/extended/sanitizer runtime, Codacy,
complete external review bodies/threads and two final merge audits remain
pending. Local strict probes are not runtime acceptance for diagnostic modes.

## Summary

Verdict: Approve for publication; merge remains gated on current-head CI/review
and actual platform evidence. Structured-interface audit and Excel start only
after this separate PR merges.
