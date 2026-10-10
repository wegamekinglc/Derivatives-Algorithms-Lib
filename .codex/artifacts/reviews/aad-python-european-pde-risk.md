# European PDE owning interface review

## Findings

No open correctness or API finding in the complete local diff. Read-first
review covers shared financial extraction, request admission, ownership,
chronological reports, mode/graph protection, resource accounting, Python
strict inputs/GIL, installed consumers and current-state documentation.

The physical-settings validator and cost-driver checks were split before
publication to satisfy the repository's existing complexity limit of eight.
They preserve financial calculations and checks. Subsequent affected numerical,
strict and installed acceptance passes; all production/retained cost functions
have complexity at most eight.

## Open questions

None blocking the bounded financial interface. Generic recording/equation
callbacks and higher-order capability stay C++-owned; Excel remains subsequent
work and is not accepted by this review.

## Tests

- Six new public C++ and six affected existing financial cases pass, including
  exact fresh-thread capacity, caller graph preservation and report ordering.
- 32 Python cases pass: independent complete dense programs/three bump sizes,
  nondefault/negative-rate points, boundary/terminal risks, strict/range/error
  admission, ownership, GIL synchronization and concurrent callers.
- 16 strict OFF/combined probes and installed consumers 3/3 pass; accepted
  native core/402-header identity and all 52 fresh production dependencies
  are checked. The example is rebuilt as an affected caller.
- Scoped cost evidence retains 160 observations over two grid sizes; both
  comparable caller rows pass the sustained 4% rule. The one medium-grid
  round above 4% and unequal owning-boundary costs are explicitly reported.

## Summary

The change closes the financial C++/Python PDE boundary without duplicating
the accepted program. Results are first derivatives of a fixed discretization,
with distinct model-error and physical-solve acceptance.
Residual risk: local runtime uses the accepted OFF native core; actual
extended/sanitizer/MSVC executions, complete remote reviews/Codacy/CI and both
final publication audits remain mandatory before merge.

Verdict: Approve.
