# Excel European PDE review

## Findings

No blocking finding remains in the local typed implementation. Two local
findings are repaired before publication: preserve physical scalar row/column
context, and avoid an unnecessary calibration-header dependency. Windows
caller-graph checks use the existing XLL-owned recording helper because the
test executable and statically linked XLL own separate native runtime state.

## Open questions

None in the numerical or interface design. Actual Windows exports and final
publication evidence are pending, so the current verdict is Comment Only.

## Tests

- First RED requires the missing header; the subsequent getter RED reports
  missing definitions. The contextual-admission RED rejects the physical value
  but demonstrates missing worksheet location. All logs are retained.
- Fresh portable execution passes all nine ExcelEuropeanPdeRiskTest cases:
  two accepted prices, all six adjoints, every chronological diagnostic,
  all resource counters, copied settings/requests/spills, nondefault negative
  rate, budgets/row bounds, null/archive failures and caller graph/seed recovery.
- Ten strict OFF/combined probes pass. The two actual public-header production
  dependents are rebuilt with accepted configuration and have identical
  object hashes; accepted public/core archives and Python module are immutable.
- Machinist generates 13 wrappers plus 13 help pages; dal_check_generated and
  documentation checks pass. No native solver/core source changes exist.
- Windows tests inspect all 13 registration contracts and call every generated
  export, with scalar/integer/range/type/NUL/budget checks. Actual remote runtime
  is a publication gate, not a local pass claimed by these source tests.
- Performance selection is two affected financial requests, with calibrated
  two-round alternating pairs. Costs and full remote CI/Codacy/reviews remain
  pending at this implementation snapshot.

## Summary

The wrapper preserves the accepted closed financial program and raw derivative
units. Local metadata overloads reuse the existing storable template through
ADL without editing legacy risk headers. An inline public settings-validation
projection preserves the binding dependency direction. Native payload and
recording caps exclude worksheet ownership/cell overhead.

Verdict: Comment Only until scoped costs, actual Windows runtime and all
exact-head publication gates are inspected. The next Excel family starts after
this PR merges.
