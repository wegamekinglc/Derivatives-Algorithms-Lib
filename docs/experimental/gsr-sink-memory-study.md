# GSR passive sink memory study

Keep the sink as an isolated experiment. Its GSR memory benefit depends on
crossing a tape allocation boundary. The selected three-factor swap fixture
avoids one 640 KiB node block per worker; one-, two- and four-factor variants
reduce occupied payload without reducing allocated capacity. These results
support a focused memory-constrained GSR caller evaluation, with no public
`Number_(double)` contract change or production adoption.

## Scope and measurement

The [prototype](passive-sink-study.md) and its native overlay are unchanged.
Both standalone static Release executables are byte-identical to the completed
study: baseline source `3be8dab9997646c040db209eaa999a459d556d59`, with the sink
overlay only on the experimental side. GCC 15.2, CMake 4.2.3, Native/AADET, Eigen
enabled, native-arch, profiling and timed lifetime diagnostics disabled are
unchanged. Full hashes, configuration, dependency identities and prior evidence
digest are in the [JSON report](data/gsr-memory-summary.json).

The requested extension maps passive leaf allocation to the existing compiled
`gsr-swap-*f` production-profile caller. Four factor counts cover model/graph
size and allocation thresholds. Three-factor one/four/eight-worker cases cover
replicated tapes; 8192 versus 32768 paths with four workers checks path-count
scaling. All fixtures retain 17 observation events, scalar AAD, one output,
fixed-model named risks and unscrambled Sobol without a Brownian bridge.
Other products, event schedules, GSR-SLV, tree evaluation, calibration, LSMC,
public bindings, vector widths and hard memory limits are outside this extension.

Every case has two rounds of ten independent processes per side, interleaved
base/head first position. The complete cohort holds the shared workspace flock.
CPU affinity is 0 for one worker, 0–3 for four and 0–7 for eight. The shared WSL2
i9-13900HX host has substantial unrelated CPU and memory load; no other work is
stopped. Host memory, swap/pressure and process-name/load snapshots are retained.

GNU time measures whole-process peak RSS in KiB, including startup, numerical
admission/oracles, reference valuation and the request. RSS uses medians per
round and across all twenty processes, with full ranges retained. It is not a
request-only allocation measure or a calibrated RSS acceptance gate. Tape
occupied bytes and allocated array capacity come from prefix/eight-path graph
scans using the same native worker initialization/evaluation. Neither is added
to RSS. Multiplying that single-worker capacity by configured worker count is a
planning projection, not an observation of simultaneously live process memory.

## Allocation boundary

The single-worker graph has 3813 fewer nodes in every factor variant, saving
152520 occupied bytes (148.945 KiB). Edges and their storage are unchanged.
`TapNode_` is 40 bytes and a node block holds 16384 nodes: 655360 bytes, or
640 KiB. Only the three-factor fixture crosses that boundary, from 19588 to
15775 nodes. Its total tape capacity drops from 2560 to 1920 KiB, or 25%.
Four factors still need two node blocks and two blocks in each edge array.
The one/two-factor graphs already fit in one node block. This explains why
occupied payload savings alone do not imply a smaller allocation or RSS.

For the three-factor graph the projected tape capacity saving is 0.625 MiB at
one worker, 2.5 MiB at four and 5 MiB at eight. These projections exclude model,
path/RNG/workspace arrays, allocator retention and process startup. A
long-lived service with an already enlarged tape can behave differently.

| Factors | Base nodes | Sink nodes | Base occupied KiB | Sink occupied KiB | Base capacity KiB | Sink capacity KiB | Edges |
|---------|------------|------------|-------------------|-------------------|-------------------|-------------------|-------|
| 1F      | 12246      | 8433       | 807.828           | 658.883           | 1920              | 1920              | 21086 |
| 2F      | 15261      | 11448      | 1097.898          | 948.953           | 1920              | 1920              | 32113 |
| 3F      | 19588      | 15775      | 1520.219          | 1371.273          | 2560              | 1920              | 48324 |
| 4F      | 26979      | 23166      | 2171.117          | 2022.172          | 3584              | 3584              | 71503 |

## Process peak RSS

The table uses medians across twenty fresh processes per side. Negative changes
mean less memory. Round deltas use each round's median independently.

| Factors | Workers | Paths | Base RSS KiB | Sink RSS KiB | Round 1 delta | Round 2 delta | Combined delta |
|---------|---------|-------|--------------|--------------|---------------|---------------|----------------|
| 1F      | 1       | 8192  | 11832        | 11878        | +0.56%        | +0.03%        | +0.39%         |
| 2F      | 1       | 8192  | 12082        | 12140        | +0.35%        | +1.18%        | +0.48%         |
| 3F      | 1       | 8192  | 12744        | 11618        | -9.11%        | -8.44%        | -8.84%         |
| 4F      | 1       | 8192  | 13824        | 13806        | 0.00%         | -0.13%        | -0.13%         |
| 3F      | 4       | 8192  | 20750        | 17970        | -13.27%       | -13.63%       | -13.40%        |
| 3F      | 8       | 8192  | 31380        | 26340        | -16.19%       | -15.97%       | -16.06%        |
| 3F      | 4       | 32768 | 20816        | 17860        | -13.75%       | -14.32%       | -14.20%        |

The three-factor case saves 1126, 2780 and 5040 KiB at one, four and eight
workers: about 1.10, 2.71 and 4.92 MiB per process. The larger path count saves
2956 KiB (2.89 MiB) at four workers. The two rounds confirm these changes, and
the observed base/sink ranges do not overlap for these four cases. The
one/two/four-factor control ranges overlap and their median changes are small.

| Factors/workers/paths | Base RSS range KiB | Sink RSS range KiB |
|-----------------------|--------------------|--------------------|
| 1F / 1 / 8192         | 11628–12016        | 11616–12020        |
| 2F / 1 / 8192         | 11592–12288        | 11620–12272        |
| 3F / 1 / 8192         | 12516–12968        | 11280–11808        |
| 4F / 1 / 8192         | 13568–13940        | 13648–13976        |
| 3F / 4 / 8192         | 20548–21388        | 17640–18112        |
| 3F / 8 / 8192         | 31104–31564        | 26132–26488        |
| 3F / 4 / 32768        | 20580–21204        | 17708–18036        |

Increasing paths fourfold at four workers does not materially increase RSS
within these ranges. Each worker restores its prefix checkpoint for each path;
do not multiply the per-worker tape saving by path count. Absolute process RSS
also reflects allocator and reference/admission history, so it need not equal
the projected tape capacity saving.

## Request latency

Latency remains informational. Values are the minimum across both rounds;
deltas use each round's own minimum. The existing strict four-percent rule
classifies every case as `no-change`: no case exceeds four percent in the same
direction in both rounds.

| Factors/workers/paths | Base min ms | Sink min ms | Round 1 delta | Round 2 delta |
|-----------------------|-------------|-------------|---------------|---------------|
| 1F / 1 / 8192         | 268.345     | 284.906     | +10.74%       | -0.53%        |
| 2F / 1 / 8192         | 351.889     | 345.860     | -3.13%        | +0.06%        |
| 3F / 1 / 8192         | 436.098     | 429.233     | -1.57%        | -8.82%        |
| 4F / 1 / 8192         | 533.227     | 565.856     | +6.12%        | -1.78%        |
| 3F / 4 / 8192         | 125.167     | 119.486     | -3.67%        | -9.01%        |
| 3F / 8 / 8192         | 78.065      | 79.518      | +1.86%        | -2.99%        |
| 3F / 4 / 32768        | 516.785     | 533.522     | +3.24%        | +1.23%        |

The +10.74% one-factor and +6.12% four-factor first-round movements, and the
second-round three-factor improvements, remain visible. This is a contended
host, with system-wide swap-in activity in both rounds. Its latency assessment
is inconclusive for adoption; these samples do not establish a speedup or a
sustained regression. Absolute times are not compared across the prior and
current cohorts. These experimental cases are outside the nine-target
production performance gate.

## Decision

The isolated experiment has a repeatable memory benefit for this three-factor
graph at an allocation threshold. A specific memory-constrained GSR caller
that values roughly 2.7–4.9 MiB of process headroom at four/eight workers is a
reasonable next evaluation target. Priority should depend on that absolute
budget benefit, rather than the percentage alone. The one/two/four-factor
controls do not justify adoption for this fixture, and three factors alone
do not guarantee a benefit for other event schedules or observation products.

Keep the production constructor/registration contract and bindings unchanged.
A real caller would still need its own graph-size, allocator-retention and
memory-budget evidence plus the separate compatibility design before adoption.
No hard-limit capacity, larger concurrency or OOM-avoidance claim is made.


## Verification and reproducibility

Run the [focused profile](../../dal-cpp/experimental/passivesink/README.md#gsr-memory-follow-up)
with `--study gsr-memory` against matching isolated builds. The profile rejects
changed executable/overlay identities and reuses the completed native 5/5 checks
on each side and sink lifetime-diagnostic 7/7 checks. It adds structural admission
for all four factor counts and verifies the PV and every named risk by double
bits for every paired request, preserving signed zero. Each CLI invocation also
retains its existing numeric admission/oracle checks.

The cohort contains 288 successful commands: 280 timed requests and eight
structural requests, all with bit-identical base/sink PV and complete named
risks. Normal and optimized Python collector checks both pass 9/9. Sampled and
delivered collector hashes are retained separately; the delivered collector
reproduces every raw timing/RSS reduction with unchanged native executables. The
[CSV](data/gsr-memory-results.csv) combines tape, RSS and latency reductions;
the JSON retains per-round medians, full ranges and identities.

The raw evidence retains commands, stdout/stderr, resources, fingerprints,
structure scans, environment and before/after executable identities. Its offline
audit rebuilds all reductions from raw files and rejects missing/duplicate
sample slots, wrong workload/affinity or missing risk axes. Collector tests cover
round completeness, median versus minimum reduction, invalid/nonfinite RSS,
changed binaries, wrong production modes and existing double-bit rejection.
No native source changes, full regression or original full-cohort resampling
are required for this collector/report extension.
