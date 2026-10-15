# LSMC checkpoint and recomputation study

The isolated C++ executable in `dal-cpp/experimental/lsmccheckpoint/` compares
native materialized training rows with time-block checkpoint/recomputation.
Production storage, budgets and public bindings are unchanged. The controlling
production admission design is
[`lsmc-checkpoint-design.md`](../../.codex/artifacts/lsmc-checkpoint-design.md).

## Reproduce

Initialize the pinned Eigen, RapidJSON and Machinist submodules, then configure
the standalone project. Its target depends only on `DAL::cpp`; it disables the
public, Python, Excel, unit-test, example and normal benchmark targets.

```bash
git submodule update --init dal-cpp/externals/eigen dal-cpp/externals/rapidjson dal-cpp/externals/machinist
cmake -S dal-cpp/experimental/lsmccheckpoint -B build/checkpoint \
  -DCMAKE_BUILD_TYPE=Release -DDAL_ENABLE_NATIVE_ARCH=OFF
cmake --build build/checkpoint --target lsmc_checkpoint_study -j4
ctest --test-dir build/checkpoint --output-on-failure
python3 dal-cpp/experimental/lsmccheckpoint/runstudy.py \
  --binary build/checkpoint/lsmc_checkpoint_study --output build/checkpoint-samples
```

On the shared #486 machine, wrap each build/test command in
`flock /home/wegamekinglc/multica_workspaces/dal-4ee34dfed184/issue486-measurements.lock`.
Hold that same lock for the complete `runstudy.py` invocation. Check external
compiler/test/benchmark load as well; it can invalidate timing conclusions even
while this lock is held. The runner records load snapshots and never cancels
other work.

`--schedule materialized|checkpoint` emits one JSON sample. `--mode
hard|frozen|retrained` selects valuation. `--schedule verify` also checks native
rows, model samples, restored state, policy and all three valuation modes, and
must not be used for memory/timing comparison because its references coexist
with checkpoints. The five CTest cases cover block lengths 1, 4, exactly E,
and greater than E, an uneven last block, held-out paths and a Brownian-bridge
case with growing vector history. Invalid options exit unsuccessfully.

## Workload and comparisons

The contract has a historical scalar/vector seed depending on K; future events
update a running sum/count, maximum and average. Each event pays a coupon;
every event except the terminal one has conditional put exercise. A
three-coordinate quadratic regression uses spot, average and peak. The terminal
event includes both coupon and put payment. The growing-history variant appends
every spot and uses the full vector average.

Compiled recording and native regression/held-out degree selection are reused.
The materialized control directly generates native BS paths and records the
entire population, with the native backward sweep. Checkpoint capture uses a
study-only BS restart adapter with the same coefficient and log-spot arithmetic.
It saves complete script scalar/vector states and exact model log accumulators
before events, regenerates one time block, and uses the same native sweep/fit.
The block's last pending exercise is applied before its rows are released.

Verification compares every payment, feature, exercise value and condition row;
every regenerated spot, numeraire and model observation; and every scalar/vector
value at internal boundaries. Policy coefficients, normalization, degree, rank,
fallback and validation loss must match. The control policy is also compared
with unmodified `TrainFrozenPolicy` in the same source translation unit.
Hard exercise counts and prices, native frozen-AAD prices and all five risks,
and retrained policy secants including historical K are compared bit for bit.
Negative controls reset script state, reset model log state and change the
Sobol address; each must produce a detected difference. A changed model point
must be rejected before restoration.

The executable includes the native `lsmc.cpp` in a separate static research
target to access its private types without extending the library's public API.
This requires a static build; the normal library and benchmark targets do not
compile or call the research code. It is intentionally single-worker and
compiled-only. Its replay uses the existing persistent native workers and
per-path tape; it does not implement AAD tape segmentation.

## Measurement interpretation

The runner collects two rounds of ten fresh processes per schedule and case,
alternating materialized/checkpoint first within every sample. It retains all
stdout/stderr, `/usr/bin/time -v` records, commands, per-round minima, CSV samples,
binary/source SHA-256, source commit, dependencies, CMake cache, compile commands,
toolchain and CPU identity. Each paired process must produce identical prices
and risks. These are informational research cases outside the nine-target
regression gate; no production performance verdict follows from them.

`peak_owned_storage_bytes` counts actual checkpoint container capacities,
block/full row payload and container headers, inclusion masks and holding-value
vectors. It excludes immutable preparation/policies, block plan mappings, worker
scratch and QR transient matrices. RSS includes those, the allocator, native tape capacity
and process/runtime overhead. It is measured in a new process for each sample,
after training storage has been released before pricing. High-water RSS still
includes the training peak.

Work counters cover passive training and each retrained fit. They count full
normal vectors/coordinates, model integration steps including separately listed
workspace bootstrap steps, and executed future recording events. Preparation, native
oracle checks and pricing/replay work are excluded from those counters;
`seconds` includes training and the selected pricing/risk mode after preparation.
Native counts use the completed forward phases' path counts and simulation
dimension, including the numeraire probe; checkpoint transition/event loops
increment their counters directly.
History seed replay and checkpoint copying are included in elapsed time rather
than in the future-event counter.
The supplied executable always regenerates a full normal vector per block,
including Brownian-bridge transforms when selected. Consequently twice the
model/script event work does not imply twice the forward runtime.

The restart adapter admits exact BS, one positive-time sample per event,
deterministic numeraires and the supplied feature schema. Local-vol and hybrid
models, stochastic interval discounts, retained future fixing samples, tree
execution, concurrent checkpoint restoration, persistence and active AAD
segmentation are outside its validation scope. Current repository CI does not
build this opt-in standalone target; the recorded local CTest run is its direct
verification evidence.

## Measured results and admission

Measured source: `e83674d660883b28395a3c188b3c1feb33fe58da`, on baseline
`e6f57c7237db603256a8d5f99656fb66a639de9a`. The only dirty files during sampling
were the design and report Markdown; executable sources were unchanged.
The executable SHA-256 is
`4227c3cc77cde6c3828cc22917f9cd544d45b312fdb311a6f4e213901e733169`.
Configuration: GNU 15.2.0, CMake 4.2.3, Release, static native AAD, Eigen enabled,
native architecture disabled, one worker, Intel i9-13900HX on WSL2. Both
schedules run in the same immutable binary. This is a schedule comparison,
rather than a branch-versus-baseline production regression benchmark.

The five correctness cases pass on that binary. Together they compare 152,130
row values, 53,460 sample values and 61,050 boundary state values, with twelve
negative-control detections/rejections. Every coefficient/normalization,
validation loss, hard exercise count, price and five-input frozen/retrained
risk comparison passes. The 200 measured processes also have identical paired
prices and risks. Pricing uses 257 paths per replicate, two replicates,
training/pricing seeds 17/29, default normals and no bridge in these samples.

| Case            | E / B   | Training / validation | Owned MiB full / checkpoint | RSS MiB full  | RSS MiB checkpoint |
|-----------------|---------|-----------------------|-----------------------------|---------------|--------------------|
| hard_blocks     | 65 / 8  | 8193 / 257            | 21.286 / 12.645             | 30.086–30.383 | 24.938–25.297      |
| hard_one_block  | 65 / 65 | 8193 / 257            | 21.286 / 22.447             | 30.004–30.320 | 31.613–31.938      |
| growing_history | 65 / 8  | 2049 / 73             | 5.348 / 7.839               | 13.570–14.031 | 16.934–17.355      |
| frozen          | 17 / 4  | 2049 / 73             | 1.363 / 1.743               | 10.203–10.367 | 10.324–10.723      |
| retrained       | 17 / 4  | 513 / 73              | 0.377 / 0.482               | 10.438–10.738 | 10.469–10.805      |

RSS ranges retain the minimum and maximum of all twenty processes per
schedule, not a selected pair. Owned storage is deterministic across samples.
`hard_blocks` saves 40.60% of tracked training storage and approximately 5 MiB
of process RSS. Single-block checkpointing adds 5.45% to training storage.
Growing history adds 46.58%; the shorter frozen/retrained cases add 27.90% and
27.79%, respectively. These are workload-specific results.

| Case            | Normal vectors full / checkpoint | Model steps full / checkpoint | Script events full / checkpoint |
|-----------------|----------------------------------|-------------------------------|---------------------------------|
| hard_blocks     | 8,451 / 84,500                   | 549,315 / 1,099,865           | 549,250 / 1,098,500             |
| hard_one_block  | 8,451 / 16,900                   | 549,315 / 1,098,825           | 549,250 / 1,098,500             |
| growing_history | 2,123 / 21,220                   | 137,995 / 277,225             | 137,930 / 275,860               |
| frozen          | 2,123 / 12,732                   | 36,091 / 72,369               | 36,074 / 72,148                 |
| retrained       | 6,457 / 38,676                   | 109,769 / 221,595             | 109,582 / 219,164               |

Retrained counts include eleven sequential fits (base plus two for each of
five inputs); frozen/hard counts include one fit. In `hard_blocks`, full
normal-vector generation increases almost tenfold, model steps increase
approximately twofold including 1,365 bootstrap steps, and script events
double. Regression and copy costs remain separate. This does not establish
a twofold forward-runtime law.

| Case            | Best seconds full / checkpoint | Round 1 overhead | Round 2 overhead |
|-----------------|--------------------------------|------------------|------------------|
| hard_blocks     | 0.204728 / 0.263568            | +21.68%          | +51.20%          |
| hard_one_block  | 0.192738 / 0.231871            | +30.29%          | +20.30%          |
| growing_history | 0.047759 / 0.072559            | +55.30%          | +51.93%          |
| frozen          | 0.015766 / 0.021555            | +35.13%          | +36.72%          |
| retrained       | 0.044644 / 0.063267            | +46.18%          | +41.71%          |

Each round uses the minimum of ten fresh processes on each side; the best
seconds column uses the minimum over both rounds. External Rust compiler/linker
load is present in both load snapshots despite holding the shared flock.
Timing is therefore informational and the production performance verdict is
**inconclusive**. No nine-target regression gate or full regression suite was
run for this isolated target. The selected cases cover its storage provider,
native rollback/QR, replay modes, path-chunk tail, and state-size boundary;
unmodified matrix, PDE, curve and public binding algorithms are excluded.

For reference, frozen PV is `3.8900300166278816`; its risks in
`[spot, vol, rate, dividend, K]` order are
`[-0.38121721626404603, 12.775869211005434, -7.22345252593874, 6.282154046368558, 0.16905959579573723]`.
The smaller retrained case has PV `3.908236555828068` and risks
`[-0.41311342159589415, 13.434159956253371, -7.969792684651049, 6.969817728153761, 0.45539303086469163]`.
The training populations differ, so these two rows are not a frozen-versus-
retrained estimator comparison. Each matches its own materialized control.

Admission: deterministic time-block recovery is demonstrated for this BS
adapter, including history and multivariate policy fits. Compact long-grid
state is a candidate for an opt-in production provider with a measured memory
budget. General replacement, growing-history products and the tested short-grid
shapes are **no-go** for a default switch. Model adapters, retained-sample state,
parallel restore, and peak accounting with retained AAD tape capacity remain
production admission requirements. Training defaults and persistent pricing
worker behavior stay as implemented in the native library.

The versioned evidence is [all samples](data/lsmc-checkpoint-samples.csv),
[summary including all risks](data/lsmc-checkpoint-summary.json), and
[provenance](data/lsmc-checkpoint-provenance.json). Full command stdout/stderr,
time resource records, configuration/build and verification logs are retained
in the evidence archive delivered with DAL-351.
