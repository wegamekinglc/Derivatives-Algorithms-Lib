# Common-path Monte Carlo curvature API

Audience: C++ quantitative developers using the accepted sealed segmented
Black-Scholes script kernel. Future Python/Excel wrappers can expose the same
typed point, matrix directions, steps and settings; this PR adds no bindings.

```cpp
auto result = Script::EvaluateBlackScholesMonteCarloCurvature(
    kernel, parameters, paths, bumps, settings);
auto gamma = result.HessianProducts()(0, 0);
```

The five arguments put required inputs first and default MC settings last.
`bumps` reuses `AAD::BumpOverAADRequest_`; coordinate columns match the kernel's
model and script labels. A unit spot row requests Gamma; a unit other row
requests the corresponding cross-Gamma column; a mixed row requests an HVP.

`MonteCarloCurvatureResult_` owns `Base()` (the complete first-order MC result),
`Point()`, `Directions()`, `Steps()`, `Settings()` (requested settings),
`Prepared()` (immutable preparation reference), `HessianProducts()` and
`Execution()`. `Base().MeanValue()`, `Base().MeanGradient()` and
`Base().ParameterLabels()` retain existing vocabulary. `PreparedHandle()` on
the sealed kernel returns shared immutable ownership for the result; it cannot
construct or mutate a preparation. Execution contains the effective recording
cap, request count, numeric bytes and maximum per-path resource observations.

The point, bumps, MC settings and kernel are snapshotted before submission.
The result retains preparation even after its kernel and inputs are destroyed.
Only const views are exposed. Numeric-budget scope excludes retained prepared
data and metadata, just as the accepted numeric driver excludes metadata.
The public result constructor rejects a null prepared owner before an instance
can expose `Prepared()`.

Rejected alternatives: wrapping MC inside a scalar native callback would nest
independent recordings; a fresh full-path tape would lose the segmented memory
contract; duplicating finite-difference validation/quotients risks divergent
edge behavior. A shared internal template loop retains concrete callbacks and
allows the base MC result to move without copying its gradient.

Errors retain existing model/random admission explanations, adding base or
direction/sign context. Invalid domains at any bump fail before task submission.
The effective recording cap is the minimum of the two supplied per-path caps.
Finite-step estimates carry no native higher-order capability promise. Smooth
conditions, hard extrema and sampling error are explained in methodology.

Compatibility: existing generic and segmented MC signatures and results stay
source-compatible. The kernel receives one const ownership accessor. No
generated Excel sources or existing Python argument conventions change.
