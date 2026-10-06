# Prepared native blocked request replay

Verdict: Approve this internal execution increment; #484 remains draft.

## Findings and ownership

No unresolved finding in the exercised replay paths. `blockedreplay.hpp` copies
ordered output/input selections and settings before submitting tasks. It replays
the complete absolute path range for each fixed-width output block over one
prepared product and an immutable model-data handle. Every batch owns a result
slot; reduction follows batch order and normalizes values and gradients once.
Padded lanes retain the same fixed width as earlier blocks.

The request owns synchronized tape/scratch budgets. Before task submission it
checks retained result extents, initial tape payload plus per-worker cleanup
headroom, and known fixed/root/batch/future scratch lower bounds. Scratch limits
can reduce planned width. Model/path/evaluator growth and cached tape capacities
are admitted at allocation boundaries; complete model-specific minimum preflight
before history remains part of the public producer acceptance.

The existing `SimulationTaskGroup_` drains before captured slots, program,
selections or budgets are destroyed, including an exception raised after an
accepted submission. Failed blocks cannot publish a partial result. Errors retain
the underlying budget cause and add first output position, stable output ID,
width and replay attempt. The first strategy performs no narrower runtime retry.

Successful results own values, the raw two-dimensional Jacobian, actual widths,
completed replay/path counts and aggregate admitted payload peaks. Retained
numeric buffers remain charged through the final request boundary and then
outlive its ledger as owning result storage. Temporary scratch is destroyed
while attached. Compiled programs, axes/positions and execution-width metadata
are constructed before numeric scratch attachment and excluded from that limit;
allocator metadata and third-party allocations are also excluded. No RSS claim
is made. These exclusions must remain explicit in public methodology.

## Verification

- `aad-blocked-replay-red-01.log` captures the missing interface.
- `aad-blocked-replay-selection-red-02.{json,log}` reproduces caller mutation
  causing an output-range failure after submission. The unchanged owning-result
  assertions pass with captured selections/settings.
- `aad-blocked-replay-green-07.{json,log}` passes five focused OFF cases using
  fresh replay test code and matching rebuilt core/public shared libraries.
  Ordered 1/4/16/64 output rows match independent scalar batches, and `J^T w`
  matches the accepted weighted driver for tree/compiled and one/four workers.
  All comparisons retain tolerance `1e-10`.
- Known budget/invalid-axis tests show zero submissions. A scratch limit one
  byte below observed execution peak fails after submission, reports stable
  block/output/budget identity, drains, preserves prior owning results and
  permits a successful repeat. Injected post-submission failure also recovers.
- Empty input selection preserves native fuzzy pricing with shape `(1,0)`.
  The fixture uses a valuation-date observation for an exact threshold and
  retains its exact half-weight assertion. The earlier future-date zero-vol
  fixture's exp/log rounding failure is preserved in `green-04` evidence.
- The six sanitizer selections append replay coverage and retain prior cases.
  Both TSan selections also include all capacity cases for cold initialization
  and concurrent cleanup. Full instrumented replay acceptance belongs to
  current-head CI; focused OFF results do not imply sanitized public delivery.

## Remaining acceptance

Public ordered-ID planning, owning axes/report factors/provenance, full caller
model/product/history snapshots, passive empty-column execution and C++/Python/
Excel entry points remain required. Public scalar row oracles and frozen central
differences remain open. Complete model-specific known preflight, cross-platform
consumers, old-entry performance, current-head CI/Codacy/reviews and guarded
merge follow the finished delivery. Portfolio integration stays in a later PR.
