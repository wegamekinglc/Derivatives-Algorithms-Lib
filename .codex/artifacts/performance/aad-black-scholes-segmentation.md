# P05 Black–Scholes fixed-path performance acceptance

Status: local acceptance complete; publication gates remain open.

## Frozen comparison and scope

Base commit: `235a5c114e311cc589b8fa27a033c4a748911cbe`.
Measured head tree: `9d910cee935a7b51a828f3d9a618ca3adba4579d`.
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
- Head: `4637807865a8c9b49601b84abb78e9bde74afa4779b2b6a033a7117e52e60205`.
- Base executable: `e53722fb96a101d1bc662282683584b2f4422496ff68ce21eba4b19b8143532d`.
- Head executable: `f82ac714e945d6518596347da27b739fcf0d8cee4d55837a35c45d051d5c258f`.

No curve, PDE, solver, RNG arithmetic, unrelated portfolio matrix or binding
performance suite repeats. Selected rows are two affected ordinary callers,
two same-head calibration controls and six new strategy comparisons. Segment
length 64 is fixed before sampling. Each process verifies against full native
AAD, warms once and measures three complete requests, retaining its minimum.
Each row uses two rounds of ten interleaved processes per side. All 400 processes
finish in 6.883 seconds; source and publication state remain frozen during timing.

## Existing callers and calibration

The comparable existing-caller gate rejects a sustained increase above 4% in
both rounds. Both calibration controls are within 4% in each final round.

| Row               | Round 1 change | Round 2 change | Result        |
|-------------------|----------------|----------------|---------------|
| Same-head short   | −2.51%         | −1.20%         | Stable        |
| Same-head running | −0.04%         | −2.12%         | Stable        |
| Existing short    | −1.29%         | 0.00%          | No regression |
| Existing running  | +3.23%         | +2.87%         | No regression |

The first running calibration has a noisy +6.91% round. Only that control is
repeated: forty processes in 1.025 seconds produce the stable confirmation above.
All original observations remain retained. Negative changes are observed minima,
not claims of a general speedup.

## Explicit strategy latency

Values are best process minima, in microseconds. Short has 16 samples; running
and live each have 2,048. Live retains older fixings and cumulative delayed
payments. All cases use the same frozen Gaussian drivers and compare the complete
parameter gradient. Cold clears retained tape capacity before each request;
warm retains the worker tape between requests.

| Case    | Regime | Full graph, μs | Segmented, μs | Change   |
|---------|--------|----------------|---------------|----------|
| Short   | Cold   | 38.885         | 43.926        | +12.96%  |
| Short   | Warm   | 3.674          | 8.466         | +130.43% |
| Running | Cold   | 674.857        | 1,141.514     | +69.15%  |
| Running | Warm   | 405.404        | 1,039.568     | +156.43% |
| Live    | Cold   | 672.011        | 1,138.095     | +69.36%  |
| Live    | Warm   | 403.597        | 1,077.408     | +166.95% |

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
|---------|--------------------|-----------------|---------------|-------------------|------------------------|
| Short   | 28,139             | 6,576           | 160           | 1,971,464         | 1,967,232              |
| Running | 3,348,427          | 819,376         | 16,416        | 3,212,056         | 1,985,472              |
| Live    | 3,356,783          | 819,584         | 16,416        | 3,212,120         | 1,985,824              |

Full totals include preparation, caller inputs and full request peak. Segmented
totals additionally include the adapter. The comparator does not charge the full
graph for the adapter constructed only for probe diagnostics.

| Case    | Full total bytes | Segmented total bytes | Total reduction | Request peak reduction |
|---------|------------------|-----------------------|-----------------|------------------------|
| Short   | 1,999,763        | 2,002,107             | −0.12%          | 0.21%                  |
| Running | 6,576,899        | 6,169,691             | 6.19%           | 38.19%                 |
| Live    | 6,585,319        | 6,178,607             | 6.18%           | 38.18%                 |

Core tape falls from 2,621,440 to 1,966,080 bytes on both long cases. Adding
2,104/2,368 checkpoint bytes gives 24.92%/24.91% reduction for those components.
The 655,360-byte cleanup reserve is an admission reservation already represented
in capacity reporting; it is not another allocated payload to add to heap totals.
Short uses the same minimum tape allocation and has no total-memory benefit.
Whole-request budgets cannot be inferred from core tape/checkpoint statistics.

## Additional LSMC dispatch repair acceptance

Codacy requires splitting the existing sharp/fuzzy LSMC dispatch and the compiled
instruction tail. Four archive members change relative to the first publication,
including LSMC; 172 members still retain baseline bytes. Four affected existing
LSMC tests pass with independent tree/compiled, payment and fuzzy-risk references.

Only two existing compiled Black–Scholes requests are added: weekly exercise,
64 training and 257 pricing paths, hard and native-AAD modes, four workers.
The existing `script_mc_perf --lsmc-replay` request and timing boundaries are
retained, covering preparation, training, pricing/replay, reverse and results.
The same original source is compiled against each side's frozen headers/archive;
a wrapper selects that entry and linker garbage collection removes unused cases.

Cold-process controls are noisy; all 160 observations are retained as inconclusive.
The predeclared quiet confirmation runs one warmup and eight identical complete
requests per process, retaining the best measured request. It checks compiled
execution with worker reuse and makes no cold-start claim. Unchanged worker
startup objects retain accepted bytes. Price and every gradient match per pair.

| Row                         | Round 1 change | Round 2 change | Result        |
|-----------------------------|----------------|----------------|---------------|
| Same-head hard confirmation | −0.82%         | +1.41%         | Stable        |
| Same-head AAD               | −2.36%         | −1.55%         | Stable        |
| Existing compiled hard      | −2.56%         | +1.42%         | No regression |
| Existing compiled AAD       | +0.27%         | −0.53%         | No regression |

The 160 warm processes take 5.017 seconds. The first hard calibration has a noisy
+8.96% round; only that control repeats, forty processes in 0.728 seconds, giving
the stable confirmation above. `lsmc-scope.md`, build commands, all cold/warm raw
logs, `lsmc-results.json`, `lsmc-warm-results.json` and
`lsmc-hard-calibration-confirmation.json` retain the complete audit trail.

## Evidence and limits

Session evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261008`.
`financial-path-performance-codacy-repair/` retains frozen source, harness, scope,
environment, all 400 raw timing logs, `results.json`, all twelve heap logs and
`heap-results.json`. `financial-path-library-codacy-repair/verification.json` proves
archive membership and reuse. Initial failing, noisy and superseded measurements
remain separately retained in `financial-path-performance/`; final acceptance
uses the trace-refactored sources above.

Correctness evidence includes 120 distinct affected tests before the helper-only
refactor, 63 compiler/state/path/model/LSMC cases after Codacy repair, ten final strict
OFF/combined checks and an installed CMake consumer. This slice establishes
fixed-path behavior and a measured long-path memory tradeoff. Monte Carlo/RNG
integration, other models, optimal segment-length selection and whole-P05
acceptance remain open. Exact-head CI and full external review precede merge.
