# Monte Carlo quote curvature review

## Findings

No unresolved local correctness or style findings in the complete changed C++
files, controlling specification/API/critique, workflow filters, installed
consumer, methodology and changelog. Publication checks and external reviews
remain required before merge.

The review repaired generic preparation's exercise/fully-expired rejection
ordering and an existing macro-related dangling-else warning in the modified
request unit. Only affected cases/probes were repeated after these repairs.

## Open questions

None. Worker recording caps, policy estimators, rate curves and bindings remain
explicit subsequent work. This estimator does not claim native higher-order AD.

## Tests

- Missing-entry and history-freezing RED evidence precedes implementation.
- Fifty distinct affected cases pass: eleven new cases plus thirty-nine existing
  Dupire request/financial cases. Final repair rerun is twelve cases, including
  the modified seal-validation branch; retained old passes are not rerun wholesale.
- Thirty-six independent passive price comparisons span flat/Merton surfaces,
  all six quote columns and three declared steps. Original tolerance remains
  `0.15 + 0.02 * abs(reference)`. Nested double-stencil cancellation required
  extended-precision reference calls, not a tolerance or step change. The
  checked-in regeneration tool reproduces all thirty-six saved values exactly.
  Maximum absolute difference is 0.002105, below 0.003844 of the fixed allowance.
- Analytic direct Gamma, a deliberately frozen calibration-map control,
  history snapshot, projections, budgets, caller mutation/destruction,
  base/plus/minus worker failure, wide-mode recovery, nested recording,
  compiled/tree and fully-expired boundaries pass.
- Ten strict OFF/combined diagnostic compilation probes and one installed
  `DAL::public` consumer pass. Core archive identity is unchanged; twenty-three
  facade members are retained, one replaced and one added.
- Three selected cost comparisons pass the scoped policy: two informational
  new paths and the existing first-order control, two rounds of ten paired
  samples per side. All 120 numerical checksums agree; timed work totals 0.7812
  seconds. See the [cost report](../performance/aad-mc-quote-curvature.md).

Evidence root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261010/mc-quote-curvature`.
Files: `final.xml`, `affected.xml`, `strict.json`, `package.json`,
`repository-oracle.log`, build dependency/hash manifests and preserved RED logs.

## Summary

Local verdict: Approve implementation; publication acceptance pending.
The complete quote chain is recalibrated at each point, direct bindings use
original scalar identities, frozen history/settings define one common-path
function, and raw coordinates are independent of reporting projections.
Residual risk is cross-platform floating-point behavior, pending actual CI
execution, plus finite-step/smoothing limitations documented in the public guide.
