# D04 scalar projection critique

Verdict: Proceed with caveats.

## Blocking issues

None for the explicitly passive scalar projection boundary. It must not be
promoted as a complete structured valuation request or as completed D04.

## Significant concerns

- Ordinal IDs are local to the complete axis definition. Preserve that definition
  and numeric snapshot, and validate the source dimension/name ordering before
  selecting columns. A globally stable quote axis belongs to F01.
- Existing gradients are means and payoff storage is a path sum. Independent
  constants must catch double normalization rather than mirroring implementation.
- Equal display labels must remain separate numerically; only legacy projection
  can reject their unrepresentable dictionary keys.
- Numeric payload budget is deliberately narrower than total memory. Naming,
  docs and errors must state it; returned report copies are separate allocations.
- The projection function cannot establish expensive-work-before-validation
  ordering, LSM estimator metadata, history identity or language parity. The
  subsequent public entry must implement and test these before D04 closes.

## Minor notes

Physical-unit metadata may be unknown, but raw/native unit and report factor
must be explicit. Method labels supplied by the caller are conversion metadata;
the public planner must derive authoritative execution labels. No new enum is
needed for a passive converter unless the implemented public contract requires it.

## Required tests

Use arithmetic reference values, reordered selection, model/script label
collisions, exact/insufficient budgets, nonfinite and dimension errors, repeated
report getters, and result-A/failure-B/result-C independence. Keep the existing
legacy implementation and its hot loops unchanged in this first increment.
# Public planner review

Proceed with the public execution decision in the API note. Reusing
`MonteCarloSettings_::enableAad_` avoids conflicting method selectors; the new
default is confined to the new entry. Before publication, test rejection before
history/tasks, native empty-column smoothing, prepared/unpartitioned axis parity,
LSM/RQMC mean normalization and `RetrainedBump` labelling. Freeze execution
settings before callbacks. Store only passive copies in results and make language
getters incapable of mutating them. Public snapshot metadata describes actual
execution; arbitrary converter metadata remains caller-supplied.
