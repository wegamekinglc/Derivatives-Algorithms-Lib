# P05 Black–Scholes fixed-path performance acceptance

Status: local acceptance complete; publication gates remain open.

## Frozen comparison and scope

Base commit: `235a5c114e311cc589b8fa27a033c4a748911cbe`.
Measured head tree: `09d13dfe7b980ee04af668a4fbe1e27d9f298bd5`.
Subsequent edits cover CI selectors and documentation only.

Both executables use the same financial harness, Release C++17, GCC 15.2.0,
`-O3 -ffp-contract=fast`, four declared threads and CPU affinity 0–3 on x86-64
WSL2. Baseline compilation excludes only the unavailable segmented adapter.
Full requests include model/evaluator initialization, input copies, execution,
reverse, cleanup and an owning result. Prepared immutable data is outside timed
requests on both sides and accounted for separately below.

The include graph identifies the affected script translation units. Sixteen are
freshly compiled after the final trace-helper refactor. Seven of the 179 archive
members change; 172 retain accepted bytes. Archive SHA-256 values:

- Base: `e7030e040b5d99c1b8e05aae62023c1505b9d7600273b97abb14ca2253b95840`.
- Head: `b62c2c1e41d510511351045e7370155404a326bd3d6d39b53917c83c62b3e6c1`.
- Base executable: `40c335862a84e7920f368bfbe16ea23d22fb8c99db4224730eb8b046210dbd47`.
- Head executable: `3df8f742eca3a5c9600bf8be19f27e153242ca9cab662c0d018b495942e75387`.

No curve, PDE, solver, RNG arithmetic, unrelated portfolio matrix or binding
performance suite repeats. Selected rows are two affected ordinary callers,
two same-head calibration controls and six new strategy comparisons. Segment
length 64 is fixed before sampling. Each process verifies against full native
AAD, warms once and measures three complete requests, retaining its minimum.
Each row uses two rounds of ten interleaved processes per side. All 400 processes
finish in 7.855 seconds; source and publication state remain frozen during timing.

## Existing callers and calibration

The comparable existing-caller gate rejects a sustained increase above 4% in
both rounds. Both calibration controls are within 4% in each final round.

| Row               | Round 1 change | Round 2 change | Result        |
| ----------------- | -------------- | -------------- | ------------- |
| Same-head short   | +0.24%         | −1.04%         | Stable        |
| Same-head running | +2.46%         | −2.79%         | Stable        |
| Existing short    | −1.69%         | −0.85%         | No regression |
| Existing running  | −5.97%         | −1.25%         | No regression |

Negative changes are observed minima, not claims of a general speedup.

## Explicit strategy latency

Values are best process minima, in microseconds. Short has 16 samples; running
and live each have 2,048. Live retains older fixings and cumulative delayed
payments. All cases use the same frozen Gaussian drivers and compare the complete
parameter gradient. Cold clears retained tape capacity before each request;
warm retains the worker tape between requests.

| Case    | Regime | Full graph, μs | Segmented, μs | Change   |
| ------- | ------ | -------------- | ------------- | -------- |
| Short   | Cold   | 40.539         | 42.753        | +5.46%   |
| Short   | Warm   | 3.667          | 8.511         | +132.10% |
| Running | Cold   | 714.379        | 1,217.182     | +70.38%  |
| Running | Warm   | 429.474        | 1,110.380     | +158.54% |
| Live    | Cold   | 716.039        | 1,185.498     | +65.56%  |
| Live    | Warm   | 427.936        | 1,114.509     | +160.44% |

This prototype trades latency for bounded segment activity. It remains explicit;
these results do not justify automatic selection or a latency improvement claim.

## Complete incremental C++ heap accounting

A separate untimed executable intercepts ordinary and aligned C++ allocation.
It records requested heap payload, excluding allocator headers, stacks, code and
unrelated process globals. Timing executables contain no interception. The
request peak includes resident minimum tape capacity, active model/evaluator
scratch, input copies, checkpoint/trace storage, cleanup and detached results.
Warm retained tape is charged relative to the pre-warm passive baseline.
Cold and warm heap counts agree in each case.

| Case    | Shared preparation | Adapter storage | Caller inputs | Full request peak | Segmented request peak |
| ------- | ------------------ | --------------- | ------------- | ----------------- | ---------------------- |
| Short   | 28,139             | 6,576           | 160           | 1,971,464         | 1,967,232              |
| Running | 3,348,427          | 819,376         | 16,416        | 3,212,056         | 1,985,472              |
| Live    | 3,356,783          | 819,584         | 16,416        | 3,212,120         | 1,985,824              |

Full totals include preparation, caller inputs and full request peak. Segmented
totals additionally include the adapter. The comparator does not charge the full
graph for the adapter constructed only for probe diagnostics.

| Case    | Full total bytes | Segmented total bytes | Total reduction | Request peak reduction |
| ------- | ---------------- | --------------------- | --------------- | ---------------------- |
| Short   | 1,999,763        | 2,002,107             | −0.12%          | 0.21%                  |
| Running | 6,576,899        | 6,169,691             | 6.19%           | 38.19%                 |
| Live    | 6,585,319        | 6,178,607             | 6.18%           | 38.18%                 |

Core tape falls from 2,621,440 to 1,966,080 bytes on both long cases. Adding
2,104/2,368 checkpoint bytes gives 24.92%/24.91% reduction for those components.
The 655,360-byte cleanup reserve is an admission reservation already represented
in capacity reporting; it is not another allocated payload to add to heap totals.
Short uses the same minimum tape allocation and has no total-memory benefit.
Whole-request budgets cannot be inferred from core tape/checkpoint statistics.

## Evidence and limits

Session evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261008`.
`financial-path-performance-final/` retains frozen source, harness, scope,
environment, all 400 raw timing logs, `results.json`, all twelve heap logs and
`heap-results.json`. `financial-path-library-final/verification.json` proves
archive membership and reuse. Initial failing, noisy and superseded measurements
remain separately retained in `financial-path-performance/`; final acceptance
uses the trace-refactored sources above.

Correctness evidence includes 120 distinct affected tests before the helper-only
refactor, 53 affected compiler/state/path cases afterward, ten final strict
OFF/combined checks and an installed CMake consumer. This slice establishes
fixed-path behavior and a measured long-path memory tradeoff. Monte Carlo/RNG
integration, other models, optimal segment-length selection and whole-P05
acceptance remain open. Exact-head CI and full external review precede merge.
