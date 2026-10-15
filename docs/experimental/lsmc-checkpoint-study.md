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
workspace bootstrap steps, and executed recording events. Preparation, native
oracle checks and pricing/replay work are excluded from those counters;
`seconds` includes training and the selected pricing/risk mode after preparation.
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
