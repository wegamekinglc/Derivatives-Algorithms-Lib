# Budgeted prepared-script Jacobian: F02 second delivery

Status: controlling contract for `feature/aad-blocked-script-risk`, based on
merged #483 (`d1600a15`). Implementation and acceptance are open. Weighted VJP
is already accepted; this delivery must not repeat or replace its acceptance.

## Source and problem

The detailed AAD plan at `de5dd8b2`, sections 4.2.2–4.2.3, 5.3.1 and B3b,
requires finite output blocks, explicit forward replay and enforced capacity
budgets. Its native-only amendment remains authoritative. Current source has
vector adjoints but no prepared-script blocked-Jacobian request.

`dal-cpp/dal/math/aad/tape.hpp` allocates channel storage when a node is created.
`dal-cpp/dal/math/aad/recording.hpp` captures mode/width and rejects changes
inside an active recording. `dal-cpp/dal/script/simulation.hpp` discards each
path suffix after propagation and accumulates the prefix until batch completion.
Consequently, changing channel width on an existing graph or treating the full
Monte Carlo history as a retained deterministic graph is invalid.

## Goals and delivery boundary

Return selected output means and their full selected-input Jacobian for one
prepared non-exercise script, using native AAD and C++/Python/Excel interfaces.
Bound recording and scratch storage, retain passive owning results/provenance,
and preserve the accepted scalar and weighted routes.

This increment excludes cross-trade timeline merging, EXERCISE/LSM, fully
expired products, quote-space composition, structural sparsity, second order,
adaptive worker caches and automatic benchmark-driven width tuning. These
remain in the overall ledger. Reject unsupported products explicitly.

## Inputs and outputs

The additive request combines the existing ordered output/input/report-factor
selection and numeric result budget with a positive maximum block width,
optional tape-payload capacity budget and optional numeric scratch capacity
budget. Omission selects the default payoff. An explicit output selection uses
the same `payoff`/`output:<ordinal>` IDs and labels as weighted risk.

The result owns `m` output values, an `(m,n)` raw Jacobian, selected and complete
output/input axes, report factors and the sealed preparation/execution snapshot.
It also records actual block widths, replay attempts and budget scope/high-water
capacities. Getters expose no active number or borrowed recording. A single row
stays two-dimensional; `n=0` stays `(m,0)`.

## Requirements

R01. Existing scalar/weighted signatures, defaults, arithmetic, messages and
normalization remain compatible. Ordinary calls construct no Jacobian planning
objects, block arrays or budget accounting exclusively for this new request.

R02. Resolve ordered output/input IDs with existing DAL string semantics before
history callbacks or workers. Reject empty/repeated/unknown outputs, repeated or
unknown inputs, unsupported slot kinds and malformed factors. Distinct output
IDs aliasing one node remain distinct rows. Preserve requested row/column order.
Recheck indexed identities against the prepared product and model.

R03. Snapshot caller choices, evaluation date, model/product data, settings and
history once. Every output block uses that same prepared state. Callbacks and
Python GIL release cannot expose caller mutation to later blocks. Do not query
history again during replay or a narrower-width retry.

R04. The mathematical result is the mean of final selected scalar script slots
and their finite-sample native gradients. Keep existing fuzzy/native versus
explicit passive price-only semantics. Do not discount a slot again. Normalize
value sums and derivative sums exactly once and apply report factors only to a
detached reported matrix.

R05. Validate width in `[1, AAD::ADJ_SIZE]` before execution; choose actual width
no larger than requested width or the output count. Start with explicit bounded
widths, not an unmeasured claim that the widest block is optimal. Before each
block, close the prior scope, select its mode/width and register fresh inputs.
Never resize the channels of an already recorded graph.

R06. The first Monte Carlo strategy replays the complete requested path range
once per output block. Recreate/reset RNG state and skip to the same absolute
path index for each batch/block. Keep Sobol/random stream, Brownian bridge,
compiled mode and batching settings fixed. Record replayed forward work in
diagnostics and performance results; do not claim one forward for the full
Jacobian.

R07. Seed each selected output in its own lane, including aliases, constants,
direct inputs and checkpoint-prefix outputs. Ensure path-local roots before
suffix propagation. Consume suffix channels per path, retain prefix channel
sums through the batch, then reverse that prefix once per batch/block. Unused
lanes of a fixed-width tail block are explicitly zero. No seed or gradient from
one block may contaminate another.

R08. Check all extent additions/multiplications before allocation. The retained
numeric result is `sizeof(double) * m * (1+n)`; axes, text and optional execution
metadata are separately identified. Reported-matrix getter copies are outside
the retained raw-result payload, as with the accepted scalar contract.

R09. Define budget scope in terms of measured internal capacities, not RSS.
Tape payload includes all allocated node, derivative, pointer and vector-adjoint
blocks, including scalar adjoints embedded in nodes and unused block tails.
Numeric scratch includes request-owned result accumulators, roots and numeric
model/path/evaluator buffers used by this driver. Include all concurrent worker
reservations rather than one worker's maximum. Account reusable capacities and
transient overlap; allocator metadata, caller-owned immutable data, strings,
third-party RNG state and process RSS must be identified as exclusions.

R10. Preflight known result/minimum/scratch bounds before history or workers.
Reduce width when known bounds require it; reject before execution when width
one cannot fit a known bound. Unknown control-flow growth is checked before
each corresponding capacity allocation, including cached capacity admission.
Allocation-boundary guards must remain absent from the per-node/per-edge path
of ordinary unbudgeted scalar/weighted execution.

R11. A runtime budget failure publishes no partial result. Drain every submitted
task and close failed recordings before returning or retrying. A narrower-width
retry is allowed only after full draining, with the sealed state and the same
path range; its width decreases strictly and all attempted work is recorded.
Width one exhaustion fails with budget kind, requested/required bytes, block
and output identity. Preserve earlier completed result handles.

R12. Explicit empty input selection returns `(m,0)` without switching the native
estimator to passive pricing. Passive settings may select only empty inputs.
All output values are validated even when no risk column is returned. Reject
nonfinite values, raw risks or reported risks with stable row/input identity.

R13. C++ results are owning values; Python bindings copy before releasing the
GIL and return detached arrays; Excel requests are immutable handles and spill
copied row/column tables. Existing strict integer/path/range/boolean parsing and
the corrected shared Excel integer conversion remain in force.

## Executable acceptance

- Planner RED/GREEN: valid/default/output-permuted requests; widths 0, 1,
  maximum and maximum+1; tail blocks; empty inputs; duplicate/unknown IDs;
  exact result/scratch budgets and byte/extent overflow. Invalid requests show
  zero history callbacks and zero submitted tasks.
- Native graph RED/GREEN: analytic two-input outputs, aliases, constants and
  prefix/direct-input roots; widths 1/2/4/16, tail padding, consecutive graphs,
  repeated seeds, exception cleanup and success/failure/success recovery.
- Prepared Monte Carlo: 1/4/16/64 outputs, tree/compiled, one/four workers,
  fixed absolute path IDs and frozen history. Compare every row with independent
  scalar requests and verify `J^T w` against accepted weighted VJP. Retain the
  predeclared common-path central-difference step/tolerance; never tune them
  after an observed disagreement.
- Budget boundaries: minimum resident blocks, derivative/pointer/adjoint growth,
  skipped tails, reused larger capacities, concurrent reservations, transient
  overlap, failed submission, draining and retry counters. Assert no allocation
  exceeds its admitted capacity and no incomplete matrix is published.
- Lifetime/profiling OFF, individually ON and combined; focused ASan/UBSan and
  relevant TSan. Python detached/strict/GIL tests and actual Excel generated
  exports run on Windows. Regeneration has zero drift.
- Map changed hot paths before measuring. Preserve the original two-round,
  ten-alternating-process/minimum/4%-in-both-rounds old-entry policy. Retain raw
  outputs, environment, source/binary hashes and numerical oracles. Measure new
  block widths/replays/capacities separately as informational capability costs.
- Stable-head CI/Codacy/review gates pass before merge. Use focused local tests
  during implementation and consolidate platform/full checks at that boundary.

## Ordered implementation

1. Passive axes, block partition and overflow-safe result/scratch preflight.
2. Allocation-boundary tape accounting/admission and failure cleanup.
3. Native block root/seed and prefix/suffix tests.
4. Prepared common-path replay, owning matrix/provenance and independent oracles.
5. C++/Python/Excel consumers and actual generated-export parity.
6. Existing-entry performance, informational block costs, complete PR repair,
   current-head CI/Codacy/review acceptance and guarded merge.

## Open decisions

Resolve the exact scratch-capacity inventory and aggregate reservation API from
the prepared model/evaluator allocation sites before implementing those guards.
Do not substitute post-allocation sampling for enforcement. Document scalar
width-one versus vector-width-one execution and tail padding after the native
oracle establishes equivalence. Portfolio integration remains a subsequent PR.
