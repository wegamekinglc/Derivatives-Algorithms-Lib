# Excel European PDE review

## Findings

No blocking finding remains in the local typed implementation. Two local
findings are repaired before publication: preserve physical scalar row/column
context, and avoid an unnecessary calibration-header dependency. Windows
caller-graph checks use the existing XLL-owned recording helper because the
test executable and statically linked XLL own separate native runtime state.

The first Windows builds identify a generated local-variable collision between
the settings input range and output handle. Rename the markup output and
regenerate both files. Codacy identifies complexity in the cost bridge, driver
and one test; factor shared row copies, commit only the observation driver and
split diagnostic/execution assertions without removing coverage. Lizard's
limit-eight scan is clear. Repaired bridge timing accepts the same two-case
scope with 80 observations; initial observations remain historical evidence.

## Open questions

None in the numerical or interface design. Actual Windows exports and final
publication evidence are pending, so the current verdict is Comment Only.

## Tests

- First RED requires the missing header; the subsequent getter RED reports
  missing definitions. The contextual-admission RED rejects the physical value
  but demonstrates missing worksheet location. All logs are retained.
- Fresh portable execution passes all ten ExcelEuropeanPdeRiskTest cases:
  two accepted prices, all six adjoints, every chronological diagnostic,
  all resource counters, copied settings/requests/spills, nondefault negative
  rate, budgets/row bounds, null/archive failures and caller graph/seed recovery.
- Twelve strict OFF/combined probes and all three installed consumers pass.
  The new public validation entry is called from the installed consumer.
  The two actual public-header production
  dependents are rebuilt with accepted configuration and have identical
  object hashes; accepted public/core archives and Python module are immutable.
- Machinist generates 13 wrappers plus 13 help pages; dal_check_generated and
  documentation checks pass. No native solver/core source changes exist.
- Windows tests inspect all 13 registration contracts and call every generated
  export, with scalar/integer/range/type/NUL/budget checks. Actual remote runtime
  is a publication gate, not a local pass claimed by these source tests.
- Performance selection accepts two affected financial requests, with 80
  observations, calibrated two-round alternating pairs and 8.167774303 measured
  seconds. Every loop exceeds 80ms. Unequal public/Excel boundary costs are
  disclosed; no algorithm speedup or comparable-contract regression claim is made.
- Full remote CI/Codacy/reviews and actual Windows runtime remain pending at
  this local acceptance snapshot.

## Summary

The wrapper preserves the accepted closed financial program and raw derivative
units. Local metadata overloads reuse the existing storable template through
ADL without editing legacy risk headers. An inline public settings-validation
projection preserves the binding dependency direction. Native payload and
recording caps exclude worksheet ownership/cell overhead.

Verdict: Comment Only until actual Windows runtime and all
exact-head publication gates are inspected. The next Excel family starts after
this PR merges.
