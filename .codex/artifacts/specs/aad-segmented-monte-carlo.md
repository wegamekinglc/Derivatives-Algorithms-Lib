# P05: explicit segmented Black–Scholes Monte Carlo

Active increment after merged #513 (`201339fd`). Source: the approved native AAD
implementation plan and its P05 complete-state/RNG acceptance requirement.

## Contract

1. Provide an explicit C++ mean-value/mean-gradient request over the trusted,
   immutable BlackScholesSegmentedPath_. Keep existing MC entry points unchanged.
   Return owning labels, value, gradient and execution metadata.
2. Require positive path count, fresh finite model/script parameters, valid RNG
   settings, representable absolute path ranges and valid per-path budgets.
   Admit numeric inputs, nesting, dimensions and checkpoint planning before
   allocating task or result storage. Do not execute a validation path.
3. Use existing sobol, mrg32 and irn generators and optional BrownianBridge.
   SkipNormalTo(firstPath) establishes the absolute zero-based path offset;
   do not add the factory's initial Sobol 2048 offset. FillNormal once per path,
   retain its transformed Gaussian vector through reverse replay, and never draw
   during replay. Zero-dimensional requests allocate no RNG and impose only the
   size_t path-range check, not generator-specific direction/draw limits. For
   zero drivers, the inclusive last path must fit: firstPath + pathCount - 1.
   A single path at SIZE_MAX is valid; two paths from that offset are invalid.
   Nonzero drivers retain representable exclusive-end admission for RNG seeking.
4. Use fixed 32-path batches and ordered individual batch reduction. A request
   owns at most min(pool thread count, 64, batch count) exclusive lanes. Each lane
   retains its RNG/Gaussian vector across drained waves and seeks monotonically.
   Clone the factory prototype for lanes one onward, then move it into lane zero.
   Do not retain an extra prototype or index scratch by ThreadNum().
5. Reuse SimulationTaskGroup_ and drain every accepted task on submission/worker
   failure. Snapshot numerical parameters and settings before submission. Declare
   task groups after captured owners. Return detached results
   only after cleanup, and recover on a later request using the same kernel.
6. Sum each batch in path order and fold individual batch sums in global batch
   order. Normalize once. Require finite sums and final results. Mean value and
   gradient are bitwise invariant across worker counts on the same build/platform
   and fixed batching policy. Resource maxima have no such guarantee.
7. Retain fixed-path history, persistent vectors/scalars, old fixings, cumulative
   and delayed payments, time zero, fuzzy semantics and fresh constant dependence.
   Keep the trusted one-factor compiled/no-exercise admission boundary.
8. Metadata identifies count, offset, RNG/bridge, batch/segment settings and maximum
   per-path tape/checkpoint/cleanup payload. Per-path budgets and maxima are not
   whole-process limits or simultaneous aggregate memory peaks.

## Acceptance

Start RED with 259 common paths at offset 7 (eight full batches plus three paths).
Compare every column with the existing complete native AAD batch processor.
Cover six RNG/bridge pairs, analytic finite-path risks, fresh numeric reuse,
history/vector/payment state, h=1/uneven/h>L, time-zero and historical-only cases,
one/four workers, reused lanes, concurrent callers and detached results. Cover
invalid settings/count/ranges/axes, nested callers, checkpoint/tape limits,
submission/worker failure and recovery. Reuse unchanged #512/#513 evidence.

Scope cost acceptance to complete short/long MC requests and one/four workers.
Freeze shapes and batching before timing; include RNG, replay, reduction and
cleanup. Untimed heap probes include preparation, adapter, lanes, coordinator,
retained tapes and results. Report the actual latency/memory tradeoff; fixed-path
figures alone cannot accept MC. Do not repeat unrelated performance matrices.

Publication requires focused correctness, strict compilation, installed use,
local review, complete current-head CI/Codacy/review inspection, actual new-case
runtime and guarded merge. Whole P05 remains open until this acceptance closes.
No Python/Excel surface or implicit strategy selection enters this increment.

See [API decisions](../api-notes/aad-segmented-monte-carlo.md) and
[design critique](../critiques/aad-segmented-monte-carlo.md).
