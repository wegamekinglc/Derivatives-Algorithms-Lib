# DAL-504 scoped PDE risk performance

**No regression under frozen P1; all four P2 cases pass both 15% gates.**
Maximum P2 overhead is **0.944672%**. P3 adds zero native solves/work,
retains constant native payload and fixed peak detached capacities.
Product optimization was unnecessary.

Baseline `1216e03aaf71071e8e36aef1f24082a1255380f8` is the supplied S5
starting master. Executable/tool source is
`66702eb0dfa1077a960242e97f553401fa421d9d`. Documentation successors are
not new executable producers. See the [delivery map](../validation/dal-504-pde-risk-delivery.md),
[identity manifest](../validation/dal504/identity.json),
[all paired samples/minima](dal504/paired.json),
[all projection rows](dal504/projections.json) and
[P3 binary bracket](dal504/projection-binaries.json).

## Selection and exclusions

- `dal-cpp/benchmarks/pde_perf` dispatch: P1 existing Bermudan/American/NearZero;
  Daily also runs because the old driver has no case selector.
- `dal-cpp/benchmarks/pde_risk_perf`: P2 Bermudan17, American17, TwoCarry25 and
  boundary PARTIAL, with independent identity preflight.
- Python private probe and portable Excel probe: P3 pre-evaluated
  Bermudan17/TwoCarry25, R=1/10/100/1000 detached getter pairs and native
  compute plus explicit-copy controls.
- Component-owned examples: actual B8/B9 builds, CTest/install and installed
  consumers; no library algorithm rewrite.

P4 is excluded because common theta is untouched. Banded, tape, calibration,
unrelated meshes and the full benchmark matrix have no changed callers.
The formal nine-target allowlist is unchanged. The new selected modes use
frozen scoped gates; the default European verdict cannot replace them.

## Builds, machine and sampling

Separate detached sources/builds:
`../s5-perf-base-source` / `../s5-perf-base-build` and
`../s5-perf-head-source` / `../s5-perf-head-build`. Absolute paths and
immutable checkout SHAs are in the manifest. Both are fresh builds.
Initialized pinned dependency directories are shared through symlinks;
dependency commits and tracked content remain unchanged. No installed binary
is timed.

Both: Release, GCC **15.2.0**, CMake **4.2.3**, Unix Makefiles, C++17,
static libraries, `-O3 -DNDEBUG -ffp-contract=fast`, PIC, native CPU tuning
OFF, Eigen ON, AADET/Native. Public/Python/Excel/portable, core tests/examples
are OFF in both timing trees; benchmarks are ON. Python uses CPython
**3.13.9**, system pybind11 **3.0.4** (not the vendored submodule) and the
installed PIC public/core archives. All dependency SHAs, selected header,
cache and compile-command hashes are recorded.

Host: Intel Core i9-13900HX, x86_64, WSL2 VM; compiler baseline ISA, native
tuning OFF. Timed processes are pinned to **CPU 2**, `DAL_NUM_THREADS=4`;
native initialization logs confirm four pool threads. No run-owned builds
or other tests run during gate samples. VM host scheduling is outside task
control; raw right-skewed samples and both rounds are retained.

P1/P2 use **two rounds of ten interleaved process samples per side**, alternating
which side starts, and the **per-round minimum**. P1 reads the existing
driver's per-process median after two warmups and ten native repetitions.
P2 calibrates a 200 ms process target: 3/1/2/1 native repetitions,
identical for reference/risk and both rounds. Calibration is diagnostic.

Use identical options for each mapped SOURCE/BUILD:

```bash
cmake --preset Release-linux -S SOURCE -B BUILD -DCMAKE_CXX_COMPILER=g++-15 \
  -DBUILD_SHARED_LIBS=OFF -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_PYTHON=OFF \
  -DDAL_BUILD_EXCEL=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF \
  -DDAL_CPP_BUILD_TESTS=OFF -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_CPP_BUILD_BENCHMARKS=ON
cmake --build ../s5-perf-base-build --target pde_perf -j 6
cmake --build ../s5-perf-head-build --target pde_perf -j 6
DAL_NUM_THREADS=4 taskset -c 2 python3 .github/scripts/measure_pde_risk.py \
  --base-build ../s5-perf-base-build --head-build ../s5-perf-head-build \
  --base-sha 1216e03aaf71071e8e36aef1f24082a1255380f8 \
  --head-sha 66702eb0dfa1077a960242e97f553401fa421d9d \
  --output ../s5-performance
```

Build-tree `pde_perf` SHA256, unchanged before/after all gate samples:

- Base: `e974ca3c3ae781513a2b5aa7b718e0d75d09fd31aa570fe7962059d477e3492f`.
- Head: `139894a0a336cd0152fbadcb718162cf0533bed687819e5e8ced4f9290264e9b`.

## P1 old-price comparison

Persistent >4% regression in both rounds is the failure rule.
Every price/status/step/solve/work/capacity row is identical across all
base/head samples. No failed or missing sample is removed.

| Case     | R1 base ms | R1 head ms | R1 delta % | R2 base ms | R2 head ms | R2 delta % | Result |
|----------|------------|------------|------------|------------|------------|------------|--------|
| Bermudan | 4.363000   | 4.302000   | -1.398     | 4.415000   | 4.447000   | 0.725      | PASS   |
| American | 12.553000  | 12.536000  | -0.135     | 12.767000  | 12.631000  | -1.065     | PASS   |
| Daily    | 42.178000  | 42.029000  | -0.353     | 43.016000  | 43.097000  | 0.188      | PASS   |
| NearZero | 0.669361   | 0.663624   | -0.857     | 0.694101   | 0.673129   | -3.021     | PASS   |

## P2 owning-risk overhead

D meshes: N=401, ordinary steps=500, upper=400, theta=1, default bumps/screens.
Fixed Q work/byte caps avoid unrelated admission: valuation 1e9 work and
1 GiB payload/workspace; risk 1e10 work and 2 GiB each. These are benchmark
overrides, not changed product defaults.

Frozen P2 inputs: K=100, sigma=.2, one-segment PUT S=100,r=.05,q=0,
Bermudan [2027-03-04,maturity] or American empty dates. TwoCarry25 is N4's
noncoincident events/carry. Boundary17 uses the first evaluation contact
run's right-edge adjacent-node midpoint in the D American control.
The request function and source hashes are retained; no fixture or threshold
is selected after measuring.

The independent loop validates inputs, makes matching owned copies,
previews/builds the common plan and calls private rollback per independently
constructed market. It retains the full base and releases each full bump
after extracting the same diagnostics, without calling risk difference/status
assembly. All four identity preflights pass before P2 timing.

`100*(risk_min/reference_min-1) <= 15%` is required **in each round**:

| Case       | Repeats | R1 ref ms  | R1 risk ms | R1 overhead % | R2 ref ms  | R2 risk ms | R2 overhead % | Result |
|------------|---------|------------|------------|---------------|------------|------------|---------------|--------|
| Bermudan17 | 3       | 81.133331  | 81.137118  | 0.0047        | 80.580031  | 80.568283  | -0.0146       | PASS   |
| American17 | 1       | 216.694768 | 217.825149 | 0.5216        | 215.504688 | 214.901578 | -0.2799       | PASS   |
| TwoCarry25 | 2       | 117.281067 | 117.662902 | 0.3256        | 117.123401 | 118.229833 | 0.9447        | PASS   |
| Boundary17 | 1       | 216.362566 | 216.894929 | 0.2461        | 217.594268 | 215.708381 | -0.8667       | PASS   |

Native counts/capacities are distinct from RSS; actual capacities are within
reported frozen envelopes:

| Case / status         | Steps / M | Solves / work    | Retained B | Native peak / envelope B |
|-----------------------|-----------|------------------|------------|--------------------------|
| Bermudan17 / COMPLETE | 501 / 17  | 8517 / 3398283   | 155511     | 342473 / 783712          |
| American17 / COMPLETE | 500 / 17  | 26315 / 10499685 | 2181187    | 4393813 / 7265656        |
| TwoCarry25 / COMPLETE | 501 / 25  | 12525 / 4997475  | 160963     | 349318 / 830952          |
| Boundary17 / PARTIAL  | 500 / 17  | 26315 / 10499685 | 2181341    | 4393813 / 7265656        |

Full plan IDs, prices, workspace and envelopes are in paired JSON.
A separate GNU-time American17 diagnostic records process peak RSS
**7,844 KiB**, including runtime/image/allocator memory. Its command,
output and resource record are archived; it is not a native capacity counter.

## P3 getter projection

The native control computes once and explicitly copies the same native
risk/scenario values. Python measures factory plus detached getters; Excel
measures typed factory/spill construction against native compute plus copying
identical captured spill-cell templates. Each pair is destroyed before the
next. Native/public C++ share the aliased types and copy path.

All R retain the same solves/work/payload: Bermudan17
**8517 / 3398283 / 155511 B**; TwoCarry25 **12525 / 4997475 / 160963 B**.
Record/cell counts grow exactly with R, without retained per-read history.

| Case       | R values            | Python records          | Excel cells                     | Native copy peak B | Excel copy peak B | Python traced peak B |
|------------|---------------------|-------------------------|---------------------------------|--------------------|-------------------|----------------------|
| Bermudan17 | 1 / 10 / 100 / 1000 | 22 / 220 / 2200 / 22000 | 816 / 8160 / 81600 / 816000     | 11670              | 34934             | 1736                 |
| TwoCarry25 | 1 / 10 / 100 / 1000 | 32 / 320 / 3200 / 32000 | 1164 / 11640 / 116400 / 1164000 | 17042              | 49826             | 2360                 |

Native/Excel columns count buffer capacities, external strings and stencils,
excluding allocator metadata. Python tracing measures only its Python
allocator component; native backing capacity is shown separately. Bounded
loop bookkeeping is not valuation history. None of these columns is RSS.

Representative diagnostic compute+copy totals, without latency gates:

| Projection / case, R=1000 | Native compute + copy ms | Binding compute + copy ms | Increment ms |
|---------------------------|--------------------------|---------------------------|--------------|
| python / Bermudan17       | 91.542                   | 100.299                   | 8.757        |
| python / TwoCarry25       | 125.221                  | 134.855                   | 9.634        |
| excel / Bermudan17        | 98.343                   | 88.602                    | -9.741       |
| excel / TwoCarry25        | 132.739                  | 135.384                   | 2.644        |

All R timings, incremental costs and copied byte counts are in projection
JSON. Initial unbracketed diagnostic captures remain in the raw archive;
one complete final P3 replay records equal before/after module/probe hashes
without row selection. P1/P2 gate sampling is not repeated.

```bash
cmake -S dal-python -B build/s5-python -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++-15 -DDAL_INSTALL_PREFIX="$PWD/../s5-stage" \
  -DPython3_EXECUTABLE=/home/wegamekinglc/anaconda3/bin/python3 \
  -DDAL_PYTHON_BUILD_TESTS=ON -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF
cmake --build build/s5-python --target _dal -j 2
DAL_NUM_THREADS=4 PYTHONPATH="$PWD/build/s5-python" taskset -c 2 \
  python3 -S dal-python/benchmarks/pde_risk_projection.py
cmake -S dal-excel/benchmarks/pde_risk_projection -B build/s5-excel-projection \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-15 -DCMAKE_PREFIX_PATH="$PWD/../s5-stage"
cmake --build build/s5-excel-projection --target pde_risk_excel_projection -j 2
DAL_NUM_THREADS=4 taskset -c 2 build/s5-excel-projection/pde_risk_excel_projection
```

The [delivery receipt](../validation/dal-504-pde-risk-delivery.md) retains
RED/GREEN and the initial missing-PIC link failure, corrected by configuration.
Numerical qualifications, binding matrices and real Windows B10 keep their
original accepted producer identities. No Windows COM timing or unrelated
benchmark pass is claimed.
