# Fixed-grid European PDE caller cost

Local result: no regression on existing paths established by immutable identity;
two new complete-request costs are informational. Publication acceptance remains
open. Scope was frozen before timing, after all six financial cases passed.

## Changed paths and selected evidence

The new support caller, example, tests and documentation do not change library
production sources/headers or existing numerical/recording call paths. Accepted
baseline is #505 final `17e67ea4536a537eb8e47f96a2ba72209a274602`, merged as
`72cac784020c106a3e748d76441540e538cfc973`, with tested/merged tree
`9d1586a0498f06c815660c91b9a45d54385756e3`.

The freshly verified core archive retains SHA-256
`4be4ca7405f83b77b73e25a652d4320cbb5842530686956d53a40f693e59919b`,
covering all 176 accepted objects. Seven accepted existing caller executables
retain their hashes. The complete production headers and relevant library build
inputs match baseline. Reuse the immutable
[native-step report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/17e67ea4536a537eb8e47f96a2ba72209a274602/.codex/artifacts/performance/aad-native-sampled-theta-step.md)
and its original prior-caller evidence: zero old rows are retimed or relinked.

Only the new two-layer/two-channel financial caller is timed: small n = 9,
ordinarySteps = 8, and example n = 61, ordinarySteps = 120. Large refinement
n = 181 is functional convergence coverage and adds no performance row.
No full size/mode/mesh/performance matrix is justified by this change.

## Environment and timing boundary

Linux WSL2, Intel i9-13900HX, GCC 15.2, C++17 `-O3 -DNDEBUG`,
`-ffp-contract=fast`, accepted native static library, lifetime/profiling OFF,
caller pinned to CPU 0 and `DAL_NUM_THREADS=4`; no worker tasks. Each process
has one unmeasured warmup per row and 32 full requests per observation. Twenty
processes yield two rounds of ten observations per row, reduced by round minimum.

The warm-thread complete request includes mode/scope setup, registration,
financial expressions, every step capture, forward checks, two-channel reverse,
actual report extraction, all price/risk extraction, validation and scope/tape
release. Registry startup and initial thread-tape construction are excluded.
The result arrays and report counts are checked for deterministic equality inside
the boundary; independently frozen numerical acceptance is checked per sample.
There is no paired speedup claim or comparison threshold for the new entry.

| Nodes / ordinary steps | Reports | Round 1 minimum (ms) | Round 2 minimum (ms) | Event bytes | Reverse scratch bytes |
|------------------------|---------|----------------------|----------------------|-------------|-----------------------|
| 9 / 8                  | 10      | 0.204212             | 0.206735             | 29016       | 648                   |
| 61 / 120               | 122     | 7.895385             | 7.851827             | 1947576     | 4392                  |

All forty samples match independent prices within `1e-9` and all six risks within
`1e-8`, validate checksums, report counts and physical forward/transpose limits
`1e-12`, retain identical event/scratch capacities and refund event storage to
zero after Close. These capacities describe event-owned tape state and reverse
scratch, not total caller/process memory. Full step retention has an explicit
grid-times-time-step storage cost; this example does not implement recomputation.
Sampling wall time is 5.5947 seconds. No compilation or source edits overlap it.

Raw evidence is retained in session directory `pde-financial-performance/`:
`raw-00.jsonl` through `raw-19.jsonl`, corresponding stderr, `results.json`
and `identity-proof.json`. The latter binds production/input identity and all
four new code hashes; `results.json` binds the measured helper, cost source and
executable hashes, environment, command and reduction. Failed compile capture
before timing remains retained; no failed timing was discarded.

Final exact-head checks, Codacy annotations, full review-body inspection and
guarded tested/merged tree verification remain mandatory publication gates.
