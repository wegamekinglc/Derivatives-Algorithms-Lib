# Primal operation replay study

Complete primal replay is feasible in the tested scalar BS domain, but this
prototype does **not** justify production adoption. Retain native AAD defaults.
The stable, single-step vanilla is a candidate for further restricted work;
the single cached graph with guarded re-recording is a no-go for the measured
weekly barriers. These conclusions do not cover other models or callers.

The isolated program in `dal-cpp/experimental/primalreplay/` is not included in
the normal library build, installed headers, public bindings or backend
capabilities. Its code and [machine-readable results](data/primal-replay-summary.json)
are the reproducible research deliverable for DAL-358 / GitHub #577, WP-12.

## Representation and execution

Native `TapNode_` stores destination adjoint pointers and local derivatives.
`repeatedFixedGraphReverse_` permits another reverse with those derivatives;
it does not update primal values or reconstruct derivatives for new paths.

The prototype records ordered opcodes, indexed operands, passive double
immediates and separately bound model/script/path inputs. Every operation has
a primal value, two refreshed local derivatives and a scalar adjoint slot.
EXP, LOG, arithmetic and selector derivatives are recomputed during replay.
Forward guards appear before operations that depend on the guarded branch.
Reverse is permitted only after the whole forward pass succeeds.

Model coefficients and numeraires form a retained prefix, just as in ordinary
native MC. Each path reverses only its suffix and accumulates prefix seeds;
one final prefix sweep produces the six named risks. Thus the comparator
does not rebuild the production model prefix for every path. A single cache
retains the preceding path's suffix; a failed guard discards it, re-records
the suffix and then reverses the new graph. Failed partial replay and recording
never publish a usable graph. Input, timeline, payoff-kind or width changes
invalidate the prefix and suffix together.

The experiment hand-lowers one BS formula and one barrier contract. It does
not automatically lower arbitrary DAL expressions or compiled script bytecode.
Copies alias their existing leaf; equal-valued active constants occupy distinct
slots. Normals are rebound per path, while literals remain immediates. There is
no passive sink assumption or change to `Number_(double)`, explicit registration,
native adjoint addresses, scalar/multi layout or tape lifecycle.

## Path-dependent structure

| Structure                                | Representation or invalidation boundary                                                                                                                                    |
|------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Native mathematical `max/min`            | Fixed opcode; strict comparison derivatives give both operands zero derivative at equality.                                                                                |
| Script `MAX/MIN`                         | Separate fixed selector opcode; equality preserves the left operand's derivative, matching evaluator alias selection.                                                      |
| Fuzzy comparison                         | Guard the strict outside/ramp regions. Exact lower and upper endpoints retain active ramp arithmetic.                                                                      |
| Fuzzy IF                                 | Guard degree below EPSILON, above 1-EPSILON or between them. Record only the executed branch or blend; never evaluate skipped unsafe expressions.                          |
| `PayoffRoot`                             | Explicit terminal root also handles prefix aliases and constants. Native prefix-alias rooting is checked independently.                                                    |
| LV interpolation                         | Outside this prototype. Guard clamping/`upper_bound` cell and stencil, preserve exact knot conventions, recompute active interpolation weights, re-record on cell changes. |
| SLV leverage and variance                | Outside this prototype. Guard interpolation cells, clamps and variance/square-root branches; preserve their exact zero and knot conventions.                               |
| Dynamic vectors, EXERCISE, other outputs | Unsupported. Reject or introduce a separately validated structural key/guard before extension.                                                                             |

LV/SLV boundaries follow the actual searches and branches in
`dal-cpp/dal/model/surface/lvmodel.hpp` and `dal-cpp/dal/model/gsrslv.hpp`.
This is an integration boundary design, not a claim that those models replay.
Grid, contract, date/fixing snapshot, model type and output layout would also
belong in a production cache key. This executable freezes dates and admits only
its own finite, positive-spot, nonnegative-volatility BS workload.

## Correctness evidence

The standalone CTest passes on measured commit
`ff7222023e6eda4451bbb85324c3964d7ae3ff18`. Its checks remain active in Release:

- 214 individual paths compare PV and every named risk (`spot`, `vol`, `rate`,
  `div`, `STRIKE`, `BARRIER`) against fresh native AAD recording and the actual
  `BlackScholes_` path generator plus compiled fuzzy script evaluator.
- Paths include random inputs, perturbations of all six risk coordinates,
  strike ties and adjacent representable strikes, exact smoothing endpoints
  and adjacent representable barriers. Fixed-input paths also cross both
  smoothing endpoints by changing only the normal, requiring guard recovery.
- Separate graph checks cover mathematical and script selector ties and
  changed-side derivatives, aliasing, distinct active leaves, prefix roots,
  suffix/full invalidation, and exact/adjacent IF cutoff guards.
- Forced outside-branch crossings produce exactly two guard failures; stable
  reuse then hits. Negative spot, nonfinite inputs and overflowing path
  arithmetic reject; the following valid path agrees with fresh recording.
- A 257-path prefix-accumulation comparison covers native, replay,
  forced re-recording and the real compiled caller. Every measured 32,768-path
  result and diagnostic run also compares all seven output coordinates.

The numerical comparison is `abs(a-b) <= 2e-10 * max(1,abs(a),abs(b))`, with
finite checks, not a bitwise identity claim or a business Greek tolerance.
Backend-specific tie rules have exact assertions. There is no validation claim
for multi adjoints, weighted outputs, threading, lifecycle diagnostics ON,
LV/SLV, calibration or LSMC.

## Measurement

Baseline production source is master
`01dcadace1180629e3b4467f5334c021c530054a`, including the accepted #567/#572
evidence and retained constant policy. Measured research source is the commit
above; its production tree is unchanged. All modes use the same executable,
so separate production baseline/head builds are unnecessary here. This is an
informational experiment outside the nine-target regression gate.

GCC 15.2.0, CMake 4.2.3, Release static C++17, `-O3 -DNDEBUG
-ffp-contract=fast`, native arch OFF, Eigen ON, native scalar AAD, profiling
and lifetime diagnostics OFF; WSL2 i9-13900HX, logical CPU 4, one worker.
The shared measurement flock covers each build/test and the entire formal
sampling invocation. The host is shared; load snapshots are retained and
the two independently placed native A/A samples differ by less than 4% in
every case and round. This supports the scoped comparison, not an idle-host claim.

Each case has two independent rounds of ten process samples per mode, alternating
mode order, minimum reduction and a sustained 4% materiality threshold. There
are 300 timed process runs plus 12 phase/memory diagnostics in this main matrix. Default Fast Sobol,
offset 1, no bridge, 32,768 paths, spot 100, vol 0.2, rate 0.05, div 0.02 and
strike 100 are shared. Vanilla has one observation after 7/365 years; barriers
have 52 weekly observations through 364/365 years, width 0.1, barrier 150 or 110.

`native` re-records the common lowered formula on the real native tape.
`record` rebuilds the prototype's own opcodes every path. `replay` reuses them
with guards and fallback. `caller` uses the real BS model and compiled script,
including product preparation. All timings include RNG construction/draws,
model prefix, forward/recording or replay, reverse and risk reduction inside
one valuation; phase diagnostics are separate and not used as timing samples.

| Case                | Native minima ms, R1/R2 | Replay minima ms, R1/R2 | Replay delta R1/R2  | Replay vs caller R1/R2 |
|---------------------|-------------------------|-------------------------|---------------------|------------------------|
| Single-step vanilla | 3.547 / 3.589           | 2.556 / 2.566           | -27.94% / -28.52%   | -34.86% / -34.41%      |
| Barrier 150         | 46.595 / 47.202         | 68.686 / 69.348         | +47.41% / +46.92%   | +5.40% / +5.88%        |
| Barrier 110         | 47.175 / 46.683         | 187.845 / 184.221       | +298.18% / +294.62% | +177.20% / +167.28%    |

The common lowered native comparator separates replay from avoiding script
interpretation. Comparing replay only with its own more expensive recorder
would substantially overstate the opportunity against current DAL. Replay is
faster than that recorder in every case, but the barrier implementations still
lose to both native comparators. No 80% saving is inferred from forward share.

To check the influence of initialization on the short vanilla, a further
100 interleaved process runs and four diagnostics use **262,144 vanilla paths
only**, the same executable hash and the same two-round best-of-ten policy.
Native minima are 27.184 / 27.411 ms; replay minima are 22.398 / 22.706 ms,
or **-17.60% / -17.16%**. Native A/A is -1.25% / -3.93%. The benefit persists
after more initialization amortization, but this does not measure reuse of
an already allocated worker tape across separate valuations. The 32,768-path
gain is not a universal steady-state speedup. Both captures use identical
source and executable identities, with separate retained manifests.

| Case        | Hits  | Initial recordings | Guard re-recordings | Hit rate | Re-record rate |
|-------------|-------|--------------------|---------------------|----------|----------------|
| Vanilla     | 32767 | 1                  | 0                   | 99.9969% | 0%             |
| Barrier 150 | 30245 | 1                  | 2522                | 92.3004% | 7.6965%        |
| Barrier 110 | 4649  | 1                  | 28118               | 14.1876% | 85.8093%       |

An exact-path structural signature can change at any observation, including
after a previous knock-out. The single preceding-graph cache records this
behavior faithfully rather than assuming dead paths remove future computation.

Separate diagnostics include recomputation and guard decisions together in the
forward phase. Mean hit forward cost is approximately 68 ns/path for vanilla
and 1.05–1.06 microseconds/path for either barrier; misses including partial replay,
discard and recording cost about 5.8–6.4 microseconds/path for barriers.
The JSON retains total forward/reverse and hit/miss times for each mode.
These clock-instrumented figures are explanatory; they are not subtracted
from the uninstrumented end-to-end samples to manufacture a recording saving.

| Case        | Replay occupied / capacity bytes | Native lowered occupied bytes | Real caller occupied bytes |
|-------------|----------------------------------|-------------------------------|----------------------------|
| Vanilla     | 2704 / 3328                      | 992                           | 1928                       |
| Barrier 150 | 81328 / 106496                   | 20128                         | 33384                      |
| Barrier 110 | 84344 / 106496                   | 21144                         | 34400                      |

Graph payload includes opcodes/operands, primal values, local derivatives and
adjoints; native payload includes nodes, derivative and pointer arenas.
Both exclude model/evaluator containers and allocator overhead. Native tape
block capacity is 1,966,080 bytes in all these small scalar cases. Isolated
replay processes use roughly 1 MiB less minimum peak RSS, as retained in the
JSON, but this does not establish incremental savings in a production hybrid
that retains its native tape and adds a replay graph. Replay occupied payload
is larger; capacity and RSS must not be confused with live graph bytes.

## Reproduction and decision

Initialize the pinned Eigen, RapidJSON and Machinist submodules. Set
`DAL_REPLAY_LOCK` to the workspace's shared `issue486-measurements.lock`, inspect
external CPU load, then run from the repository root:

```bash
git submodule update --init dal-cpp/externals/eigen dal-cpp/externals/rapidjson dal-cpp/externals/machinist
flock "$DAL_REPLAY_LOCK" cmake -S dal-cpp/experimental/primalreplay -B build/primal-replay -DCMAKE_BUILD_TYPE=Release -DDAL_ENABLE_NATIVE_ARCH=OFF
flock "$DAL_REPLAY_LOCK" cmake --build build/primal-replay --target primal_replay_study --parallel 4
flock "$DAL_REPLAY_LOCK" ctest --test-dir build/primal-replay -R '^primal_replay_correctness$' --output-on-failure
flock "$DAL_REPLAY_LOCK" python3 dal-cpp/experimental/primalreplay/runstudy.py --binary build/primal-replay/primal_replay_study --output build/primal-replay-results --cpu 4 --paths 32768
flock "$DAL_REPLAY_LOCK" python3 dal-cpp/experimental/primalreplay/runstudy.py --binary build/primal-replay/primal_replay_study --output build/primal-replay-vanilla-scale --cpu 4 --paths 262144 --cases vanilla
```

The runner is Linux-specific and requires Python 3.9+, Git, `lscpu`, `ps` and
GNU time. It refuses an existing output directory and checks correctness
before sampling. It retains executable/library/source/cache identities,
compiler flags, dependency SHAs, affinity, load snapshots, every raw result and
process RSS, `results.json` and diagnostics. Its measurement summary is the
machine-readable companion to this note; the complete raw bundle is attached
to DAL-358. No full regression suite or nine-target gate was rerun: production
code and defaults are unchanged, and the standalone experiment has no callers
outside this study. Default CI does not build this opt-in experimental target.

The runner accepts the operator's executable local `primal_replay_study` build
and resolves a fixed set of metadata tools. Its process boundary rejects
executables outside that allowlist, restricts mode/case and path-count arguments,
and passes an argv list with explicit `shell=False`. The local Bandit B404/B603
and Semgrep variable-argv exceptions cover this intentional process boundary,
following the repository benchmark runner pattern; they do not exempt the file
or change repository analysis policy. `test_runstudy.py` checks literal execution
of a path containing shell characters and rejection of unlisted programs and
unsupported study requests:

```bash
python3 -m unittest discover -s dal-cpp/experimental/primalreplay -p 'test_runstudy.py' -v
```

Retain production behavior and seek independent focused review of this
research code. A later stable-graph opt-in would need real integration,
amortized capture cost, model/output/lifecycle admission, measured guard cost
and end-to-end validation at its actual public callers. More graph caches,
fused operations or a different selector representation are unmeasured ideas,
not evidence that either barrier result or other models would improve.
