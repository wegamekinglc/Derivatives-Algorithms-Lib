# DAL-504 targeted Codacy repair

Repair base: `2cca0bf6ae6c986bf2770ede78afec8cebab40aa`.
Repair source: `6497255a96e92de626a0fcad96e2c927ba759d5e`.
The accepted main review, comment `01a1bc22-b6d7-7f2b-95ab-9a3eeb0dd117`,
continues to cover its original `1216e03a..2cca0bf6` scope. This increment
awaits focused review. PR #688 remains ready, without auto-merge.
The [raw archive manifest](dal504-codacy-repair/raw-archive.json) binds the
initial annotations, repair build/test logs, raw sampling and new native probes.

The initial Codacy check `128797789933` was ACTION_REQUIRED: nine complexity
findings, two subprocess findings and ten Python assert findings (21 total).
The twelve other runner annotations are platform notices, outside this repair.
The original responses are retained in the attached raw archive.

## Changes and local checks

Only the six flagged S5 files and two directly necessary contract tests change.
No numerical product, frozen fixture, budget, threshold or rule configuration
changes. [Identity](dal504-codacy-repair/identity.json) binds all source files,
rebuilt binaries and unchanged product subtrees.

- Sampling now separates recording, calibration, interleaving and reduction.
  It resolves executable `pde_perf` paths through the existing repository
  containment/executable validator. Only fixed price/risk arguments, canonical
  cases/modes and real integer counts 1–10000 are admitted. Output labels cannot
  traverse directories. The audited subprocess annotation follows the repository
  pattern, after validation, with explicit `shell=False`. This is not suppression
  in place of input checks.
- C++ helpers separate boundary/contact logic, axis descriptors, scenario
  execution and plan/scenario comparison. Input/scenario copies, one common
  preview/build plan, retained base/plan, all diagnostic fields, work and failures
  remain. Axis IDs/bumps still resolve once per axis; full bumps release per
  iteration. The wrapper splits parsing, identity output and timed execution.
- Excel separates case preparation, explicit spill-template copies, detached
  getter loops and measurement. Values, allocation accounting and destruction
  scopes remain equivalent.
- Python example/projection invariants use explicit RuntimeError checks, active
  under `-O`. Availability/value consistency, smooth COMPLETE, control success,
  unclipped boundary, PARTIAL/boundary Delta and counters/payload checks remain.

[Complexity receipt](dal504-codacy-repair/complexity.json): **66 current
functions, maximum CCN 8**. The original nine flagged methods measure
46/12/9/12/10/9/27/18/10; their replacements measure
3/6/7/3/4/6/1/3/3 respectively. The local engine/version is recorded.

RED: optimized Python accepted an AVAILABLE row with an absent value;
the new focused test failed with “RuntimeError not raised”. After explicit
checks, ordinary and optimized Python contracts each pass **5/5**. They also
cover counter/record drift, malformed arguments, executable symlink escape,
and retention of nonzero subprocess status/stdout/stderr.

C++ selection passes **4/4**: full reference identity, unsupported inputs,
counterfactual plan/diagnostic drift, and D-profile preflight. Optimized Python
runs the complete example; its output matches the original receipt exactly.
[Replay](dal504-codacy-repair/sampling-replay.json) consumes the original 212
raw outputs through the refactored sampler: identical command arguments/order
and final P1/P2/preflight/calibration reductions.

```bash
python3 -m unittest discover -s .github/scripts/tests -p test_pde_risk_delivery.py
python3 -O -m unittest discover -s .github/scripts/tests -p test_pde_risk_delivery.py
cmake --build build/s5-core --target dal_cpp_tests pde_perf -j 4
DAL_NUM_THREADS=4 ctest --test-dir build/s5-core \
  -R '^(PdeRiskBenchmarkTest\.|pde_risk_perf_identity$)' --output-on-failure
cmake --build build/s5-excel-projection --target pde_risk_excel_projection -j 2
DAL_NUM_THREADS=4 PYTHONPATH="$PWD/build/s5-python" \
  python3 -O -S dal-python/examples/019.early_exercise_pde_risk.py
```

## Affected performance

P2's four cases pass both frozen ≤15% rounds; maximum new overhead
**3.563310%**. Workloads, two best-of-ten interleaved rounds, alternating
first side, calibration and native repetition equality are unchanged.
[All samples/minima](dal504-codacy-repair/paired.json) retain the new producer,
raw commands and unchanged before/after executable hashes.

| Case       | R1 ref ms  | R1 risk ms | R1 overhead % | R2 ref ms  | R2 risk ms | R2 overhead % |
|------------|------------|------------|---------------|------------|------------|---------------|
| Bermudan17 | 84.027436  | 83.603629  | -0.5044       | 83.919021  | 83.512367  | -0.4846       |
| American17 | 222.795707 | 226.499283 | 1.6623        | 225.254649 | 223.452524 | -0.8000       |
| TwoCarry25 | 123.260619 | 123.919420 | 0.5345        | 128.148148 | 129.694538 | 1.2067        |
| Boundary17 | 232.775904 | 241.070430 | 3.5633        | 230.041503 | 232.878038 | 1.2331        |

All four preflight JSON records are exactly equal to the original capture:
plans, prices, statuses, scenario/solve/work counts and native capacities/
envelopes. Boundary17 remains PARTIAL; no failed/missing sample is removed.

Fresh timing executable SHA256:
`ef61af8a0781f797bde5b87a2c47dc7ff71d68c7dc206246967ff4edd5e17aae`.
Fresh Excel probe SHA256:
`17a2b9612050a2eabe71b08e96e4f571ddc1a50895dae3050bb3f4205d716537`.

[P3 rows](dal504-codacy-repair/projections.json): eight Python and eight
Excel measurements, R=1/10/100/1000. Python output remains 22/32 records
per pair, Excel 816/1164 cells. Copies release per iteration; native
work/solves/payload and detached capacities stay fixed per case.
[Binary brackets](dal504-codacy-repair/projection-binaries.json) show no drift.
Python executes with optimization enabled; its two native modules are
unchanged original producers, not rebuilt or relabeled. P3 timings remain
diagnostic, without a new ratio or latency threshold.

Same existing isolated source/build directories, unchanged native archive,
dependency pins and configuration: Release/GCC 15.2/CMake 4.2.3/AADET/Eigen/
PIC/static/native tuning OFF; CPython 3.13.9/system pybind11 3.0.4,
CPU 2 and DAL_NUM_THREADS=4.

```bash
git -C ../s5-perf-head-source checkout --detach 6497255a96e92de626a0fcad96e2c927ba759d5e
cmake --build ../s5-perf-head-build --target pde_perf -j 4
DAL_NUM_THREADS=4 taskset -c 2 python3 -O .github/scripts/measure_pde_risk.py \
  --group P2 --base-build ../s5-perf-base-build --head-build ../s5-perf-head-build \
  --base-sha 1216e03aaf71071e8e36aef1f24082a1255380f8 \
  --head-sha 6497255a96e92de626a0fcad96e2c927ba759d5e --output ../s5-codacy-performance
DAL_NUM_THREADS=4 PYTHONPATH="$PWD/build/s5-python" taskset -c 2 \
  python3 -O -S dal-python/benchmarks/pde_risk_projection.py
DAL_NUM_THREADS=4 taskset -c 2 build/s5-excel-projection/pde_risk_excel_projection
```

## Reuse and handoff

The [original delivery](dal-504-pde-risk-delivery.md) and
[original performance](../performance/dal-504-pde-risk.md) retain producer
`66702eb0` and all original samples. Affected build paths now contain new
binaries. The original archive `e50d31ca...` preserves the old timing executable
`139894a0...`; it must not be verified against the replaced build-tree file.

P1's old-price caller and native kernel, B8/B9 examples/installed consumers,
S3/S4 numerical/binding/resource evidence and real Windows B10 are unchanged
and reused. No new P1 verdict or full matrix is claimed. Windows B10 still
belongs to `20401a29`. The original main Approve is preserved; only this
increment needs review. Final-head Codacy/CI is reported as one actual snapshot
in the issue handoff, with unfinished work explicit and no polling.
