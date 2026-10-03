# AAD recording phases and checkpoints: acceptance evidence

Status: active D01 acceptance. Corrected local correctness, unchanged nine-target
performance, supplemental MC/LSM and curve calibration, and memory observations
pass. Exact publication-head four-backend/Windows/binding CI remains pending.
No stage or the full AAD goal is declared complete.

## Scope and immutable sources

The increment adds checked input registration, recording, ready/reverse, failed,
and closed phases; opaque recording/checkpoint-generation/owner/mode tokens;
full native scalar/vector clearing; and per-worker-batch ordinary MC/LSM ownership.
It preserves suffix accumulation, one prefix sweep, path numbering, normalization,
root lifetime, explicit closure, and LSM policy-risk definitions. Raw active-number
lifetime diagnostics and the backend capability layer remain separate work.

- Baseline: `b5e3caca85bb1a5b1aa3acf7898b387832d443c8`, the controlling plan's
  original implementation baseline, held detached and unchanged.
- Initially measured lifecycle head: `8f4f09c2b9366d7c69f61b95454c4b8593e709ca`.
- Corrected measured C++ head: `71f41a81e4d939b192a986199104019bbbe9230c`.
- Sources: `/tmp/dal-aad-baseline` and `/tmp/dal-aad-implementation`.
- Separate native builds: `/tmp/dal-aad-evidence/base-build` and `head-build`;
  separate CoDiPack correctness build: `codipack-build`.
- Matching Release C++17, GCC 15.2, CMake 4.2.3, Unix Makefiles, Eigen, native AAD,
  pinned dependency gitlinks, benchmark-enabled configuration, and architecture
  opt-in disabled. Python remains an exact-head CI responsibility.
- Shared WSL2/i9-13900HX host, 32 logical CPUs, about 31 GB RAM; desktop and
  hypervisor activity are uncontrolled. All paired runs use `DAL_NUM_THREADS=4`
  and affinity `4,6,8,10`, with no concurrent DAL build/test or source changes.
- Ten interleaved process samples per side in each of two rounds; alternate
  base/head first; reduce each round with `min`; fail a common case only when
  both rounds exceed 4%. The nine-target allowlist and Sobol 10x ceiling are unchanged.

Source/configuration/hardware metadata and SHA-256 binary digests accompany each
evidence directory. Subsequent documentation packaging does not change measured
C++. Acceptance must still inspect CI for its actual publication head.

## Failure, correction, and confirmation

The first fully checked migration passes the nine-target gate, but fails
supplemental short vanilla AAD:

| Initial failed case | Round 1 | Round 2 |
|---|---:|---:|
| Tree, 20,000 paths x 1 event | +15.00% | +10.74% |
| Compiled, 20,000 paths x 1 event | +11.25% | +15.08% |

This head was held unpublished. Preserve `lifecycle-paired/` and
`lifecycle-mc-paired/`, including their environment records and every raw sample.

The correction keeps every owner/state/mode/token check, inlines the small
per-path operations, and constructs exception messages out of line. Full
clearing uses a typed callable so GCC's low-optimization sanitizer configuration
does not indirectly call an always-inline function. No unchecked API, additional
node field, per-expression release check, numerical approximation, new callback
contract, or changed benchmark workload is introduced.

All 35 common ordinary-MC cases and three LSM profiles pass the corrected
supplemental run. Tree vanilla records +3.25%/+8.83%, which passes the existing
rule but warrants confirmation; a second complete two-round run records
+3.65%/+0.98%. Compiled vanilla records +2.03%/-0.03%, then -3.23%/+1.87%.
Retain all four rounds; the second run does not replace or hide the first.

The LSM profiles are BS tree and compiled with 512 training/8,192 pricing paths
and weekly exercise, plus daily compiled local-vol with 256/4,096 paths. Every
paired PV and risk entry agrees at relative/absolute tolerance 1e-10 in both runs.
These are additional changed-workload checks, not new members of the nine-target gate.

## Correctness and resource observations

- Final native functional/regular/slow-example CTest: all 2,315 pass.
- Quiet, serial native benchmark smoke CTest: all 21 pass. Combined native
  discovered coverage is 2,336 cases; smoke timings are not paired evidence.
- Final complete CoDiPack CTest: all 2,255 pass.
- Twenty native and fifteen CoDiPack recording cases pass, covering phases,
  checkpoint suffix accumulation, rejected handles, owner boundaries,
  reverse/cleanup failure and recovery, and native mode/vector clearing.
- All twenty native recording cases pass ASan/UBSan with leak detection.
- Native full-block clearing checks retain capacity and verify scalar fields,
  vector leaves, specialized/fallback widths, and new seeds against analytic values.
- Nested-batch, phase, mode-selection, checkpoint and vector-clear regressions
  first fail or fail to compile before the corresponding required behavior exists.

An earlier combined CTest run overlapped builds and hit `rate_risk_perf`'s
within-process timing assertion. Keep `recording-inline-native-ctest.log`;
it is excluded from the quantitative performance verdict. Final functional
verification and all 21 quiet smoke checks pass separately.

Three interleaved `/usr/bin/time -v` observations per side use matching four-thread
settings. Maximum RSS samples in KiB are:

| Profile | Baseline | Corrected head |
|---|---|---|
| Ordinary default MC executable | 99,336 / 99,680 / 99,672 | 99,588 / 99,400 / 99,716 |
| Daily compiled local-vol LSM | 11,904 / 11,964 / 12,056 | 12,096 / 12,020 / 11,844 |

No material RSS increase is observed; major page faults are zero. These are
process observations, not a cumulative tape high-water metric, enforceable
memory budget, or proof for every workload. Default native number/node layouts
are unchanged; ownership/checkpoint metadata exists once per scope/thread.
The first RSS capture omitted the four-thread setting; its raw data remains
in `recording-inline-resource/` and is explicitly excluded. Accepted observations
are in `recording-inline-resource-verified/`.

## Verdict and coverage limits

No regression is found under the unchanged nine-target policy and the separately
applied supplemental criterion at the corrected C++ head. All 25 common
end-to-end curve-calibration cases also pass. Small positive movements remain
visible in the complete tables below and are not described as speedups.

Coverage includes existing tape/Jacobian/rate-risk kernels, short/long ordinary
AAD, two LSM engines, local-vol replay, and calibration workloads. New head-only
tape/harvest cases are informational. Large local-vol scaling, production phase
timings, cumulative storage measurements, workspace reuse, more output axes,
path-internal recomputation, and later plan features still require their own evidence.
Four-backend and binding CI for this increment is pending.

## Retained evidence and reproduction

Evidence root: `/tmp/dal-aad-evidence`.

- `recording-inline-paired/`: unchanged nine-target gate, all raw files, JSON,
  complete table and environment/binary metadata.
- `recording-inline-mc-paired/` and `recording-inline-mc-confirmation/`:
  all ordinary/LSM samples, numeric agreement, JSON and full tables.
- `recording-inline-curve-paired/`: all 25 calibration cases, samples and metadata.
- `recording-inline-resource-verified/`: RSS/page-fault raw observations.
- `recording-inline-final-{native,codipack}-ctest.log`,
  `recording-inline-final-native-smoke.log`, focused logs and
  `recording-inline-final-sanitized.log`: final correctness/smoke/sanitizer runs.
- `run_mc_pairing.py` and `run_curve_pairing.py`: supplemental runners;
  their SHA-256 digests are recorded in the respective environment files.

The formal command is the existing gate script with separate base/head roots,
`--samples 10 --confirmation-rounds 2 --threshold-percent 4`, under the same
four-thread environment and CPU affinity. Supplemental runners use unchanged
build-tree executables; they do not alter the allowlist.

See the [lifecycle contract](../specs/aad-recording-lifecycle.md),
[ownership acceptance](aad-recording-ownership.md), and
[full implementation ledger](../plans/aad-implementation.md).

## Complete nine-target results

## Paired benchmark regression gate

2 independent rounds of 10 interleaved process-level samples; failure requires every round to exceed +4.00%.

| Benchmark | Case | Base min | Head min | Change | Round changes | Result |
|---|---|---:|---:|---:|---:|:---:|
| tape_perf | Clear + re-record (100K nodes) | 0.852553 ms | 0.853180 ms | +0.07% | +0.48%, +0.07% | pass |
| tape_perf | PropagateToStart (100K nodes) | 0.389633 ms | 0.386557 ms | -0.79% | -1.09%, -0.74% | pass |
| tape_perf | PropagateToStart multi-mode (100K nodes, 10 results) | 0.484868 ms | 0.486365 ms | +0.31% | +0.91%, +0.31% | pass |
| tape_perf | Rewind + re-record (100K nodes) | 0.567826 ms | 0.567936 ms | +0.02% | +0.03%, +0.02% | pass |
| tape_perf | ZeroAdjoints sweep (100K nodes) | 0.110590 ms | 0.111449 ms | +0.78% | +0.78%, +0.66% | pass |
| tape_perf | PropagateToStart multi-mode (50K steps, 1 result) (new coverage) | — | 0.382962 ms | — | — | info |
| tape_perf | PropagateToStart multi-mode (50K steps, 16 results) (new coverage) | — | 0.648690 ms | — | — | info |
| tape_perf | PropagateToStart multi-mode (50K steps, 4 results) (new coverage) | — | 0.378746 ms | — | — | info |
| tape_perf | PropagateToStart multi-mode (50K steps, 64 results) (new coverage) | — | 6.538000 ms | — | — | info |
| tape_perf | PropagateToStart passive constants (50K steps) (new coverage) | — | 0.147231 ms | — | — | info |
| tape_perf | Rewind + passive-constant recording (50K steps) (new coverage) | — | 0.241275 ms | — | — | info |
| jacobian_perf | AnalyticJacobian dense harvest (24 x 23) | 0.005354 ms | 0.004702 ms | -12.18% | -12.18%, -12.04% | pass |
| jacobian_perf | AnalyticJacobian row-width harvest (24 x 23) | 0.005369 ms | 0.004656 ms | -13.28% | -13.28%, -13.14% | pass |
| jacobian_perf | HarvestCurveJacobian dense (23 outputs x 24 parameters) (new coverage) | — | 0.004324 ms | — | — | info |
| jacobian_perf | HarvestCurveJacobian dense (95 outputs x 96 parameters) (new coverage) | — | 0.061878 ms | — | — | info |
| jacobian_perf | HarvestCurveJacobian proven prefix (23 outputs x 24 parameters) (new coverage) | — | 0.003780 ms | — | — | info |
| jacobian_perf | HarvestCurveJacobian proven prefix (95 outputs x 96 parameters) (new coverage) | — | 0.058024 ms | — | — | info |
| pde_perf | ThetaScheme rollback (200x200 CN) | 0.217109 ms | 0.222738 ms | +2.59% | +1.34%, +2.68% | pass |
| pde_perf | ThetaScheme rollback (200x200 implicit) | 0.216744 ms | 0.216483 ms | -0.12% | +2.65%, -2.65% | pass |
| pde_perf | ThetaScheme rollback (200x2000 explicit) | 0.685812 ms | 0.687293 ms | +0.22% | +0.22%, -0.58% | pass |
| rng_perf | BrownianBridge FillNormal (100K x 10D) | 6.525000 ms | 6.559000 ms | +0.52% | +0.52%, +2.03% | pass |
| rng_perf | IRN SkipNormalTo (100K x 10D) | 7.866000 ms | 7.931000 ms | +0.83% | +0.72%, +1.14% | pass |
| rng_perf | MRG32 SkipNormalTo (100K x 10D) | 0.001295 ms | 0.001298 ms | +0.23% | +0.23%, +2.85% | pass |
| rng_perf | MRG32k3a FillNormal (100K x 10D) | 19.908000 ms | 20.075000 ms | +0.84% | +0.84%, -0.44% | pass |
| rng_perf | ShuffledIRN FillNormal (100K x 10D) | 10.835000 ms | 10.812000 ms | -0.21% | -0.21%, -0.80% | pass |
| rng_perf | Sobol FillNormal fast (100K x 10D) | 4.154000 ms | 4.213000 ms | +1.42% | +1.42%, +1.29% | pass |
| rng_perf | Sobol FillNormal precise opt-in (100K x 10D) | 39.196000 ms | 39.038000 ms | -0.40% | -0.40%, -1.25% | pass |
| rng_perf | Sobol FillUniform (100K x 10D) | 0.773320 ms | 0.789332 ms | +2.07% | +2.07%, +4.30% | pass |
| interp_perf | Cubic interp (50 knots, 10K queries) | 0.051975 ms | 0.050293 ms | -3.24% | -3.12%, -3.82% | pass |
| interp_perf | Inlined linear LV-style (1e5 paths x 200 steps, 200 knots) | 104.137000 ms | 104.149000 ms | +0.01% | +0.01%, -0.65% | pass |
| interp_perf | Linear interp (50 knots, 10K queries) | 0.043090 ms | 0.042452 ms | -1.48% | +1.78%, -3.20% | pass |
| krylov_perf | BCGSolve (500x500 tridiag) | 0.061091 ms | 0.061122 ms | +0.05% | +0.09%, -0.25% | pass |
| krylov_perf | CGSolve (500x500 tridiag) | 0.051622 ms | 0.051855 ms | +0.45% | +0.48%, +0.04% | pass |
| banded_perf | TriDecomp MultiplyLeft (10K) | 0.004496 ms | 0.004496 ms | +0.00% | +0.00%, -0.02% | pass |
| banded_perf | TriDiagonal Decompose (10K) | 0.076359 ms | 0.076461 ms | +0.13% | +0.13%, +0.03% | pass |
| banded_perf | TriDiagonal MultiplyLeft (10K) | 0.004687 ms | 0.004688 ms | +0.02% | +0.00%, +0.04% | pass |
| cholesky_perf | CholeskyDecompose (200x200) | 0.225445 ms | 0.225955 ms | +0.23% | +0.01%, +0.23% | pass |
| cholesky_perf | CholeskyDecompose+Multiply (200x200) | 0.225430 ms | 0.226033 ms | +0.27% | +0.27%, -1.54% | pass |
| rate_risk_perf | Quote risk aggregate (joint XCCY) | 0.222174 ms | 0.219706 ms | -1.11% | -1.11%, -1.41% | pass |
| rate_risk_perf | Quote risk aggregate (single curve) | 0.008928 ms | 0.008886 ms | -0.47% | +0.19%, -3.52% | pass |
| rate_risk_perf | Quote risk aggregate (staged XCCY basis) | 0.060410 ms | 0.060303 ms | -0.18% | +0.18%, -0.92% | pass |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=10) | 1.143000 ms | 1.139000 ms | -0.35% | +0.17%, -0.35% | pass |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=16) | 1.731000 ms | 1.737000 ms | +0.35% | +0.17%, +0.40% | pass |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=5) | 0.822798 ms | 0.823544 ms | +0.09% | +0.09%, +0.58% | pass |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=10) | 11.202000 ms | 11.008000 ms | -1.73% | -1.03%, -1.73% | pass |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=16) | 17.467000 ms | 17.362000 ms | -0.60% | -0.60%, +0.25% | pass |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=5) | 7.938000 ms | 7.858000 ms | -1.01% | +2.08%, -2.46% | pass |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=10) | 1.081000 ms | 1.075000 ms | -0.56% | -1.91%, -0.56% | pass |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=16) | 1.652000 ms | 1.669000 ms | +1.03% | +0.54%, +1.03% | pass |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=5) | 0.772721 ms | 0.773026 ms | +0.04% | +1.25%, +0.04% | pass |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=10) | 10.914000 ms | 11.047000 ms | +1.22% | +0.96%, +1.29% | pass |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=16) | 17.372000 ms | 17.200000 ms | -0.99% | -1.12%, -0.99% | pass |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=5) | 7.779000 ms | 7.812000 ms | +0.42% | +0.42%, -0.04% | pass |
| rate_risk_perf | Quote risk portfolio joint ANALYTIC (24 XCCY x N=10/block) | 4.516000 ms | 4.516000 ms | +0.00% | -0.92%, +1.28% | pass |
| rate_risk_perf | Quote risk portfolio joint BUMPED (24 XCCY x N=10/block) | 4.504000 ms | 4.511000 ms | +0.16% | -0.29%, +0.27% | pass |
| rate_risk_perf | Quote risk portfolio single ANALYTIC (120 deposits x N=5) | 0.121565 ms | 0.122347 ms | +0.64% | +0.64%, +0.48% | pass |
| rate_risk_perf | Quote risk portfolio single BUMPED (120 deposits x N=16) | 0.170568 ms | 0.171517 ms | +0.56% | +0.32%, +1.02% | pass |
| rate_risk_perf | Quote risk portfolio staged ANALYTIC (24 XCCY x N=16) | 3.571000 ms | 3.566000 ms | -0.14% | +0.17%, -0.14% | pass |
| rate_risk_perf | Quote risk portfolio staged BUMPED (24 XCCY x N=5) | 1.282000 ms | 1.274000 ms | -0.62% | -0.55%, -0.62% | pass |
| rate_risk_perf | Rate AAD 2 components (1024 IRS x 8 nodes, fixed maturities) | 5.164000 ms | 5.182000 ms | +0.35% | +0.52%, +0.29% | pass |
| rate_risk_perf | Rate AAD 2 components (256 IRS x 8 nodes, fixed maturities) | 1.277000 ms | 1.276000 ms | -0.08% | -0.85%, +0.55% | pass |
| rate_risk_perf | Rate AAD 2 components (32 IRS x 8 nodes, fixed maturities) | 0.153799 ms | 0.154081 ms | +0.18% | +0.18%, +0.20% | pass |
| rate_risk_perf | Rate OIS daily compounding sweep (5Y quarterly x daily) | 0.214572 ms | 0.212659 ms | -0.89% | -0.89%, -1.41% | pass |
| rate_risk_perf | Rate PV (1024 IRS x 8 nodes, fixed maturities) | 2.448000 ms | 2.424000 ms | -0.98% | -3.23%, -0.69% | pass |
| rate_risk_perf | Rate PV (256 IRS x 8 nodes, fixed maturities) | 0.606024 ms | 0.601830 ms | -0.69% | -4.21%, +1.72% | pass |
| rate_risk_perf | Rate PV (32 IRS x 8 nodes, fixed maturities) | 0.075131 ms | 0.074099 ms | -1.37% | -1.48%, -0.99% | pass |
| rate_risk_perf | Rate XCCY batch serial (24 XCCY x 5 components) | 0.779193 ms | 0.781628 ms | +0.31% | -0.63%, +0.61% | pass |
| rate_risk_perf | Rate batch serial (120 IRS x 2 components) | 1.277000 ms | 1.269000 ms | -0.63% | -0.16%, -1.32% | pass |
| rate_risk_perf | Rate prepare geometry (256 IRS x 8 nodes, fixed maturities) | 0.538641 ms | 0.535937 ms | -0.50% | -0.50%, +0.42% | pass |
| rate_risk_perf | Rate prepare geometry (32 IRS x 8 nodes, fixed maturities) | 0.064826 ms | 0.064147 ms | -1.05% | -1.05%, +0.92% | pass |
| rate_risk_perf | Rate prepared AAD 2 components (256 IRS x 8 nodes, fixed maturities) | 0.717893 ms | 0.730832 ms | +1.80% | +3.37%, +1.36% | pass |
| rate_risk_perf | Rate prepared AAD 2 components (32 IRS x 8 nodes, fixed maturities) | 0.085738 ms | 0.086995 ms | +1.47% | +1.18%, +2.03% | pass |
| rate_risk_perf | Rate prepared PV (256 IRS x 8 nodes, fixed maturities) | 0.106280 ms | 0.109587 ms | +3.11% | +3.63%, +3.11% | pass |
| rate_risk_perf | Rate prepared PV (32 IRS x 8 nodes, fixed maturities) | 0.012563 ms | 0.012988 ms | +3.38% | +3.38%, +3.65% | pass |
| rate_risk_perf | Rate single-trade sweeps (240 IRS calls) | 1.995000 ms | 2.010000 ms | +0.75% | +0.75%, -0.45% | pass |

Head Sobol precise opt-in / fast ratio: 9.27x (limit 10.00x).

Rows marked (new coverage) are head-only benchmark cases added by the PR; they are reported for information and are not gated.

All performance acceptance checks passed.

## Supplemental MC/LSM: initial corrected pairing

# Supplemental Monte Carlo paired measurement

| Profile | Case | Round 1 | Round 2 | Result |
|---|---|---:|---:|---|
| ordinary | GSR 1F 5Y swaption (1 price, order 16/32) | -1.93% | -2.72% | pass |
| ordinary | GSR 1F bond (100K paths x 4 steps) | +0.27% | +4.78% | pass |
| ordinary | GSR 1F bond option (1000 prices) | +1.94% | -4.84% | pass |
| ordinary | GSR 1F swap and Libor (10K paths x 4 steps) | -2.33% | -4.72% | pass |
| ordinary | GSR 2F 5Y swaption (1 price, order 16/32) | +1.57% | -2.75% | pass |
| ordinary | GSR 2F bond (100K paths x 4 steps) | -0.03% | +0.87% | pass |
| ordinary | GSR 2F bond option (1000 prices) | -0.64% | +0.82% | pass |
| ordinary | GSR 2F swap and Libor (10K paths x 4 steps) | +5.79% | +2.08% | pass |
| ordinary | GSR 3F 5Y swaption (1 price, order 16/32) | -3.18% | -0.45% | pass |
| ordinary | GSR 3F bond (100K paths x 4 steps) | -2.25% | -0.95% | pass |
| ordinary | GSR 3F bond option (1000 prices) | -1.75% | +2.93% | pass |
| ordinary | GSR 3F swap and Libor (10K paths x 4 steps) | -1.21% | -3.79% | pass |
| ordinary | GSR g calibration (3 quotes x 3 buckets) | -6.01% | +0.32% | pass |
| ordinary | LSMC regression degree=3 (100000 paths) | +2.74% | +2.17% | pass |
| ordinary | LSMC regression degree=3 ITM mask (100000 paths) | +7.39% | -0.56% | pass |
| ordinary | LSMC regression degree=8 (100000 paths) | +4.01% | -1.13% | pass |
| ordinary | LSMC regression degree=8 ITM mask (100000 paths) | -2.46% | -0.02% | pass |
| ordinary | LSMC regression features=2 degree=2 ITM mask (100000 paths) | -1.27% | +1.33% | pass |
| ordinary | LSMC regression features=2 degree=3 ITM mask (100000 paths) | -0.80% | +2.34% | pass |
| ordinary | LSMC regression features=3 degree=3 ITM mask (100000 paths) | +0.77% | +1.81% | pass |
| ordinary | correlated BS path (100K x 12 steps x 1 assets) | +3.68% | -0.62% | pass |
| ordinary | correlated BS path (100K x 12 steps x 2 assets) | +4.39% | -0.42% | pass |
| ordinary | correlated BS path (100K x 12 steps x 3 assets) | +0.63% | -0.09% | pass |
| ordinary | hybrid flat-rate path (100K x 12 steps x 2 assets) | +1.09% | -1.05% | pass |
| ordinary | hybrid logDF path (100K x 12 steps x 2 assets) | -0.68% | -0.06% | pass |
| ordinary | script engine bermudan exercise double compiled=false (100000 paths x 54 events) | +0.25% | +0.47% | pass |
| ordinary | script engine bermudan exercise double compiled=true (100000 paths x 54 events) | +0.85% | +0.85% | pass |
| ordinary | script engine vanilla Number_ compiled=false (20000 paths x 1 events) | +3.25% | +8.83% | pass |
| ordinary | script engine vanilla Number_ compiled=true (20000 paths x 1 events) | +2.03% | -0.03% | pass |
| ordinary | script engine vanilla double compiled=false (200000 paths x 1 events) | -2.26% | +2.31% | pass |
| ordinary | script engine vanilla double compiled=true (200000 paths x 1 events) | -2.25% | -1.13% | pass |
| ordinary | script engine weekly barrier Number_ compiled=false (10000 paths x 52 events) | -2.74% | -2.51% | pass |
| ordinary | script engine weekly barrier Number_ compiled=true (10000 paths x 52 events) | -1.70% | +1.66% | pass |
| ordinary | script engine weekly barrier double compiled=false (100000 paths x 52 events) | -1.20% | +0.54% | pass |
| ordinary | script engine weekly barrier double compiled=true (100000 paths x 52 events) | +0.81% | +0.61% | pass |
| lsm-bs-tree | lsm-bs-tree | +0.63% | -1.90% | pass |
| lsm-bs-compiled | lsm-bs-compiled | +0.21% | +0.42% | pass |
| lsm-lv-daily | lsm-lv-daily | -0.44% | -0.58% | pass |

LSM PV and every risk entry agree in all paired samples at rel/abs tolerance 1e-10.

Performance acceptance: PASS.

## Supplemental MC/LSM: additional confirmation

# Supplemental Monte Carlo paired measurement

| Profile | Case | Round 1 | Round 2 | Result |
|---|---|---:|---:|---|
| ordinary | GSR 1F 5Y swaption (1 price, order 16/32) | -3.64% | +1.45% | pass |
| ordinary | GSR 1F bond (100K paths x 4 steps) | +1.59% | -0.88% | pass |
| ordinary | GSR 1F bond option (1000 prices) | -2.67% | +0.47% | pass |
| ordinary | GSR 1F swap and Libor (10K paths x 4 steps) | +2.26% | -0.59% | pass |
| ordinary | GSR 2F 5Y swaption (1 price, order 16/32) | +1.39% | +1.34% | pass |
| ordinary | GSR 2F bond (100K paths x 4 steps) | +0.00% | +0.26% | pass |
| ordinary | GSR 2F bond option (1000 prices) | -1.98% | +1.77% | pass |
| ordinary | GSR 2F swap and Libor (10K paths x 4 steps) | +3.30% | +0.50% | pass |
| ordinary | GSR 3F 5Y swaption (1 price, order 16/32) | -0.14% | -1.09% | pass |
| ordinary | GSR 3F bond (100K paths x 4 steps) | -0.87% | +3.92% | pass |
| ordinary | GSR 3F bond option (1000 prices) | +0.15% | -3.37% | pass |
| ordinary | GSR 3F swap and Libor (10K paths x 4 steps) | -2.05% | +2.29% | pass |
| ordinary | GSR g calibration (3 quotes x 3 buckets) | -4.54% | -2.88% | pass |
| ordinary | LSMC regression degree=3 (100000 paths) | +2.81% | -1.54% | pass |
| ordinary | LSMC regression degree=3 ITM mask (100000 paths) | +3.82% | +0.88% | pass |
| ordinary | LSMC regression degree=8 (100000 paths) | +2.21% | -1.75% | pass |
| ordinary | LSMC regression degree=8 ITM mask (100000 paths) | +1.78% | +4.34% | pass |
| ordinary | LSMC regression features=2 degree=2 ITM mask (100000 paths) | -0.91% | +1.05% | pass |
| ordinary | LSMC regression features=2 degree=3 ITM mask (100000 paths) | +1.89% | +1.50% | pass |
| ordinary | LSMC regression features=3 degree=3 ITM mask (100000 paths) | +0.87% | +0.34% | pass |
| ordinary | correlated BS path (100K x 12 steps x 1 assets) | -0.92% | -2.13% | pass |
| ordinary | correlated BS path (100K x 12 steps x 2 assets) | -5.47% | -0.94% | pass |
| ordinary | correlated BS path (100K x 12 steps x 3 assets) | +0.88% | -0.55% | pass |
| ordinary | hybrid flat-rate path (100K x 12 steps x 2 assets) | -0.23% | +2.58% | pass |
| ordinary | hybrid logDF path (100K x 12 steps x 2 assets) | -1.21% | +2.59% | pass |
| ordinary | script engine bermudan exercise double compiled=false (100000 paths x 54 events) | -6.38% | -0.51% | pass |
| ordinary | script engine bermudan exercise double compiled=true (100000 paths x 54 events) | -0.29% | -2.95% | pass |
| ordinary | script engine vanilla Number_ compiled=false (20000 paths x 1 events) | +3.65% | +0.98% | pass |
| ordinary | script engine vanilla Number_ compiled=true (20000 paths x 1 events) | -3.23% | +1.87% | pass |
| ordinary | script engine vanilla double compiled=false (200000 paths x 1 events) | +2.11% | -1.15% | pass |
| ordinary | script engine vanilla double compiled=true (200000 paths x 1 events) | +1.98% | -1.80% | pass |
| ordinary | script engine weekly barrier Number_ compiled=false (10000 paths x 52 events) | -2.87% | -0.65% | pass |
| ordinary | script engine weekly barrier Number_ compiled=true (10000 paths x 52 events) | -0.44% | -1.11% | pass |
| ordinary | script engine weekly barrier double compiled=false (100000 paths x 52 events) | +0.25% | +0.72% | pass |
| ordinary | script engine weekly barrier double compiled=true (100000 paths x 52 events) | -3.23% | +1.00% | pass |
| lsm-bs-tree | lsm-bs-tree | -0.44% | -0.62% | pass |
| lsm-bs-compiled | lsm-bs-compiled | -0.23% | +0.09% | pass |
| lsm-lv-daily | lsm-lv-daily | +0.96% | -0.81% | pass |

LSM PV and every risk entry agree in all paired samples at rel/abs tolerance 1e-10.

Performance acceptance: PASS.

## Supplemental end-to-end calibration

# Supplemental curve calibration pairing

| Case | Round 1 | Round 2 | Result |
|---|---:|---:|---|
| CalibrateYieldCurve LOG_CUBIC_NATURAL ANALYTIC +DIAG (23 swaps) | +2.03% | +0.07% | pass |
| CalibrateYieldCurve LOG_CUBIC_NATURAL ANALYTIC SOLVE (23 swaps) | +0.23% | -0.30% | pass |
| CalibrateYieldCurve LOG_CUBIC_NATURAL BUMPED +DIAG (23 swaps) | -0.36% | -0.95% | pass |
| CalibrateYieldCurve LOG_CUBIC_NATURAL BUMPED SOLVE (23 swaps) | -0.84% | +0.74% | pass |
| CalibrateYieldCurve LOG_LINEAR ANALYTIC +DIAG (23 swaps) | +0.21% | -1.51% | pass |
| CalibrateYieldCurve LOG_LINEAR ANALYTIC SOLVE (23 swaps) | -1.66% | -2.53% | pass |
| CalibrateYieldCurve LOG_LINEAR APPROXIMATE (23 swaps) | +0.02% | -1.35% | pass |
| CalibrateYieldCurve LOG_LINEAR BUMPED +DIAG (23 swaps) | -1.91% | +1.56% | pass |
| CalibrateYieldCurve LOG_LINEAR BUMPED SOLVE (23 swaps) | -2.69% | -0.41% | pass |
| CalibrateYieldCurve MIXED ANALYTIC +DIAG (23 swaps) | -2.00% | +1.15% | pass |
| CalibrateYieldCurve MIXED ANALYTIC SOLVE (23 swaps) | -1.93% | +0.11% | pass |
| CalibrateYieldCurve MIXED BUMPED +DIAG (23 swaps) | -3.27% | -1.38% | pass |
| CalibrateYieldCurve MIXED BUMPED SOLVE (23 swaps) | -1.03% | +1.58% | pass |
| CalibrateYieldCurve PWC ANALYTIC +DIAG (23 swaps) | -4.36% | -3.14% | pass |
| CalibrateYieldCurve PWC ANALYTIC SOLVE (23 swaps) | -4.49% | -1.38% | pass |
| CalibrateYieldCurve PWC BUMPED +DIAG (23 swaps) | +3.48% | +0.45% | pass |
| CalibrateYieldCurve PWC BUMPED SOLVE (23 swaps) | +0.92% | +0.02% | pass |
| CalibrateYieldCurve PWL ANALYTIC +DIAG (23 swaps) | -3.67% | -2.58% | pass |
| CalibrateYieldCurve PWL ANALYTIC SOLVE (23 swaps) | -2.91% | -1.34% | pass |
| CalibrateYieldCurve PWL BUMPED +DIAG (23 swaps) | +0.69% | -0.32% | pass |
| CalibrateYieldCurve PWL BUMPED SOLVE (23 swaps) | -1.30% | +0.48% | pass |
| PWL DF queries (4096 x 24 nodes) | -0.88% | +0.83% | pass |
| PWL DF queries (4096 x 256 nodes) | -0.58% | -0.97% | pass |
| PWL DF queries (4096 x 64 nodes) | +0.18% | -2.52% | pass |
| PWL DF queries (4096 x 8 nodes) | +5.36% | +1.87% | pass |
