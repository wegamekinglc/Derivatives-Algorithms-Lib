# Rate-trade curvature review

Status: local code and scoped cost acceptance complete; remote acceptance pending.
Verdict: Approve local implementation; merge remains gated on remote evidence.

Before publication, inspect financial methodology and units, all four snapshot
ownership paths, parameter-coordinate binding, layered/fixed graph reconstruction,
trade geometry lifetime, fixing merge and early admission, and exact reverse
counts. Confirm no public pricing-formula duplication or per-trade Jacobian.

Require independent passive original-calibrator references, affected existing
caller checks, diagnostics probes, installed consumption and scoped cost evidence.
## Findings and dispositions

No outstanding correctness findings in the local implementation. The review
checked all four owning solved-market paths, parameter-block coordinate binding,
layered reconstruction and staged fixed roots after provenance capture. Native
pricing formulas are reused; the weighted scalar sum precedes the reverse sweep.
Request-local cashflow scratch refers to stable owning trade geometry. Captured
fixings supplement saved observations, preserve explicit-snapshot semantics and
validate reciprocal FX observations through the native snapshot constructor.

RED/GREEN evidence repaired ignored explicit fixings, inconsistent curve/PV
currency, missing staged fixed roots and missing row/ID exception context.
Geometry and per-row objective failures now retain that context. Zero-weight
rows still undergo native validation and finite-PV checks. Strict compilation
also required explicitly spelling the existing quote-risk aggregate defaults;
this changes no initialized values.

## Tests

The focused executable passes 57/57 tests: 19 new public/core tests, 24 existing
public quote-curvature tests and 14 existing core parameter-Jacobian tests.
Independent price references use the original passive native calibrators and
`PriceRateTrades`, with two directional patterns, three outer steps and two
inner steps for Richardson comparison. Coverage includes all seven trade
families, all four calibration families, layered curves and historical rate/FX
observations. Ownership, concurrent reuse, weights, duplicate IDs, mode/budget
recovery and early numeric rejection have separate tests.

Ten strict warning-as-error probes pass across diagnostics OFF/combined for
the four affected production/test units and installed consumer. The installed
consumer passes 1/1 and checks the exported financial entry against deposit
value, gradient and curvature formulas. Existing accepted library members are
reused only with archive, object and dependency hashes; changed objects and the
consumer executable are rebuilt.

## Remaining acceptance and risk

The initial remote Codacy report identified three test/harness complexity
findings. Shared preparation now uses typed family overloads; the performance
dispatcher uses focused runners. Local Lizard finds no function over the
configured limit of eight in the three affected files. Sixteen affected public
tests and their two strict probes pass; production archive identities are
unchanged. Fresh cost-binary identity analysis precedes scoped repeat sampling.

Five scoped cost comparisons pass with 200 observations and matching checksums;
existing-call round deltas satisfy the sustained 4% gate. Inspect every full remote review
body, issue comment, inline thread and Codacy annotation. Merge only after
current-head CI, actual runtime-profile logs and tested-tree proof pass. Required
sanitizer/Windows profiles have not yet run for this increment. Rectangular or
approximate calibration, mixed mode and language bindings remain outside this
entry's declared capability.
