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
Its eight regime/strategy pairs are informational cases within the existing target;
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

Eight strict checks, twelve focused cases, installed-only consumption and
default/selected benchmark oracles pass before sampling. Pure sampling completes
320 process observations (960 timed requests) in 1.592 seconds. No old timing
matrix is repeated. All 179 native archive members retain accepted bytes.

| Case      | Tape regime | Full min (us) | Segmented min (us) | Round 1 delta | Round 2 delta | Tape + checkpoint reduction |
|-----------|-------------|---------------|--------------------|---------------|---------------|-----------------------------|
| short     | cold        | 34.567        | 35.507             | +4.74%        | +2.72%        | -0.010%                     |
| short     | warm        | 1.161         | 2.281              | +96.30%       | +97.50%       | -0.010%                     |
| long64    | cold        | 1138.641      | 777.854            | -31.80%       | -31.60%       | 60.32%                      |
| long64    | warm        | 489.848       | 741.800            | +51.28%       | +51.81%       | 60.32%                      |
| long256   | cold        | 1121.691      | 757.938            | -33.04%       | -32.38%       | 60.47%                      |
| long256   | warm        | 489.565       | 717.527            | +46.51%       | +46.56%       | 60.47%                      |
| cashflows | cold        | 1249.992      | 920.528            | -26.35%       | -27.41%       | 60.42%                      |
| cashflows | warm        | 626.627       | 878.882            | +40.09%       | +40.87%       | 60.42%                      |

The long full-graph tape peak is 4,980,736 bytes. Segmented tape peak is
1,966,080 bytes, plus checkpoint capacities 10,272 / 2,592 / 5,152 bytes
for long64 / long256 / cashflows. Both strategies reserve 655,360 bytes for
cleanup, separately from allocated peak payload. Short paths retain the same
tape peak and add 192 checkpoint bytes; there is no short-path storage win.

Under the preserved two-round 4% rule, warm segmentation costs more for every
selected case. Cold long requests cost less; the short cold movement is not
sustained. These are strategy costs, not newly measured legacy regressions.
Keep the opt-in API and ordinary full-graph default. No universal AUTO cutoff,
financial speedup, full-process memory saving or whole-plan P05 acceptance is
claimed from this component study.

## Immutable provenance

Backend baseline: accepted #511 merge
`ddea35911c8e5f3b759e70b7fe3c12a9c6dd0594`.
Measured source: `b12662de994cf889adb871367d1dfaa2b32a79ed`.
Both strategies run in the same frozen Release driver against the accepted
native archive; the only production source-tree addition is the opt-in header.
This is an algorithm comparison, not a moving-version baseline comparison.

The detached source is `path-segmentation-performance/head-source` under the
external evidence root; its isolated `head-build` contains the measured driver.
Compiler: GCC 15.2.0; CMake 4.2.3; C++17 Release, native scalar, diagnostics and
profiling OFF, `DAL_NUM_THREADS=4`. Host: i9-13900HX, WSL2 Linux x86-64,
32 logical CPUs; initial load 0.61 / 0.39 / 0.29. The shared/WSL host is noisy
in principle, so every verdict retains both independent minima rounds.

Archive SHA-256:
`e7030e040b5d99c1b8e05aae62023c1505b9d7600273b97abb14ca2253b95840`.
Driver SHA-256:
`45a83cabea4185bc45c986f67f0b37de047fe4803bbd707c14f3a33996afc3e6`.
Results SHA-256:
`8f1fd20ca64c7bae9c4a40ac6fc3726f5a306cfcc249fef8709303239cb6a8ac`.
Source, binary and archive hashes are unchanged after sampling.

Raw outputs, full source/configuration identity and all observations are retained
under `/home/wegamekinglc/.cache/dal-aad-evidence-20261008/path-segmentation-performance/`
as `raw/`, `environment.json`, `results.json` and `measurement.log`.
The exact sampler is `path-segmentation-measure.py` in that evidence root.
Run one selected mode with
`tape_perf --segmented-path --case long64 --mode segmented-warm`;
the other mode names are `full`, `segmented` and `full-warm`.
