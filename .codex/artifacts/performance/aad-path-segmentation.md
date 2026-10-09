# Native path segmentation cost and storage

Status: selected before measurement; correctness acceptance precedes timing.

## Scope

Only the new opt-in segmented-path header, test and benchmark consume the new
core. Ordinary pricing/tape headers and compiled production sources are unchanged.
Reuse accepted PR #511 archive bytes and old caller evidence after proving this
dependency boundary; do not rerun the nine-target matrix.

No existing benchmark exercises a complete standalone fixed-state segmented
request. `tape_perf --segmented-path` compares the new request with an ordinary native
full-graph request using the same typed transition formulas, independent value
and gradient oracles, scalar mode and retained driver/parameter copies.
Its four new comparison pairs are informational cases within the existing target;
the existing scheduled regression cases retain their accepted timing bodies.
It does not claim financial Monte Carlo or script integration acceptance.

## Selected cases and protocol

- `short`: 32 steps, segment length 8, terminal objective only.
- `long64`: 16,384 steps, segment length 64, terminal objective only.
- `long256`: the same long path with segment length 256.
- `cashflows`: 16,384 steps, segment length 128, direct contributions at every step.

Measure cold reset and steady-state tape reuse separately for each of the four
cases: eight comparison pairs, with no unrelated targets. This addition follows
the smoke probe's different full-graph allocation costs after sustained reuse;
cold-only evidence would not support an ordinary reusable-tape cost claim.
Collect two independent rounds of ten interleaved process samples per strategy,
regime and case, alternating which strategy starts. Retain raw outputs and reduce each round
with its minimum; keep the existing sustained 4% rule when describing cost
differences. Each process also checks its complete returned value and gradient.

The timed body includes copies, initialization, all forward
work, checkpoint storage, recomputation, fresh recording, reverse, reduction,
result ownership and recording cleanup. The segmented prepass is included.
Common fixture construction is outside both timings. Cold samples include tape
reset inside every timed request; warm samples retain each strategy's own tape
after its warmup. Final tape release is outside both timings. An initial reset
outside both regimes prevents a full-graph case from inflating the segmented tape.

Report peak allocated tape payload, retained checkpoint capacity and separate
cleanup reservation. Request input copies, result storage, transient kernel
vectors, allocator bookkeeping and arbitrary kernel data are not included in
these component counts; they are not total process memory. The full driver
copy remains present in both modes. No AUTO selection or speedup is assumed.

## Results

Pending final strict/installed correctness acceptance and read-only sampling.
