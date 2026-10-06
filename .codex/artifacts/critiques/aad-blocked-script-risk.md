# Budgeted Jacobian design critique

Verdict: Proceed with caveats. Passive planning can proceed; budget enforcement
and Monte Carlo integration require the allocation inventory and root oracle
before their implementation is accepted.

## Blocking issues

No unresolved blocker for the first passive-planner increment. The subsequent
budget phase must identify every included numeric allocation and reserve before
allocation. A scan after growth, or a one-worker high-water statistic reported as
a request-wide limit, blocks that phase's acceptance.

## Significant concerns

- Existing recording scopes accept only the owning thread's default tape.
  A separate tape per request is not already a supported isolation mechanism;
  do not bypass owner/nesting validation to obtain a budgeted tape.
- Existing scopes rewind retained blocks. A smaller budget after a larger
  request must admit or release that retained capacity deliberately, and must
  preserve cleanup poison/recovery semantics. Requested width is not capacity.
- Node channel layout is fixed when recorded. Per-block mode selection must
  happen outside an active scope. Prefix roots and unused tail lanes need an
  independent oracle before the production driver can share the abstraction.
- Runtime narrowing can replay a request only after all tasks drain and with
  frozen history/RNG/path IDs. Count failed attempts and guarantee strictly
  decreasing widths; do not combine partially computed rows from different
  execution attempts.
- Current scalar result helpers assume one row. Reuse validators where their
  contracts fit, but do not silently relax legacy payoff-only selection or
  reported-matrix row handling.

## Minor notes

Keep width-one scalar/vector behavior explicit. Result, recording and scratch
budgets need distinct names and documented overlap. Diagnostic node sizes and
the eager initial blocks must be measured in each compiled ABI. Metadata and
process RSS exclusions belong in user-facing methodology, not just this file.

## Counter-proposals

Keep the first driver on one sealed script with complete path replay per block.
Use an explicit conservative width until P03 measures alternatives. Extend
compile-time execution policies only when accepted scalar behavior/performance
remain demonstrable; avoid duplicating the whole preparation and task-draining
logic for another payoff mode.

## Author questions to resolve locally

Which prepared model/evaluator capacities can be determined before their
allocations, and which grow dynamically? Which owner holds the aggregate-worker
reservations? Does a fixed-width tail with zero padding simplify cleanup enough
to justify its unused capacity? Record answers with the corresponding RED/GREEN
tests; no new user approval is required for these implementation choices.
