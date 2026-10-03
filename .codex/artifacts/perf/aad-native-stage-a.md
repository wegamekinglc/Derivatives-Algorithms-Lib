# Native AAD correctness and measurement increment

Status: active acceptance evidence. The nine-target performance gate and all 46
CI checks at documentation head `aec6689` pass for this measurement increment. This report does
not complete Stage A or the full AAD plan.

## Source and configuration

- Baseline: `b5e3caca85bb1a5b1aa3acf7898b387832d443c8`, the initial implementation
  merge base with `master`, retained immutably for the full goal.
- Measured head: `13b0964b870af232ff4bed745835ac076249ded8`.
- Separate clean sources: `/tmp/dal-aad-baseline` and `/tmp/dal-aad-implementation`.
- Separate build roots: `/tmp/dal-aad-evidence/base-build` and
  `/tmp/dal-aad-evidence/head-build`; build-tree binaries only.
- Matching Release C++17, GCC 15.2, CMake 4.2.3, Unix Makefiles, native AAD, Eigen,
  `-O3 -DNDEBUG -ffp-contract=fast`, benchmarks enabled, native-architecture
  opt-in disabled. Dependency gitlinks match the baseline on both sides.
- WSL2/Microsoft hypervisor on Intel Core i9-13900HX, 32 logical CPUs. This is a
  shared workstation; host activity is not controlled. No DAL build or test ran
  during pairing. Both sides inherited `DAL_NUM_THREADS=4` and CPU affinity
  `4,6,8,10`.
- Two independent rounds, ten interleaved process samples per side per round,
  alternating which side runs first. Reduction is the minimum; a comparable
  case fails only if every round exceeds the unchanged 4% threshold.

Compiler identity, cache options, dependency SHAs, CPU identity, and binary SHA-256
digests are retained in `p0-paired-verified/environment.json` under the evidence root.

## Correctness and CI evidence

The measurement increment's full native CTest passed 2,293 cases, including core,
public, portable Excel, and 33 non-slow examples. Full CoDiPack CTest passed
2,239 cases. Eleven statistics/block-list tests passed with ASan/UBSan. The last
change after those full runs only refactored the benchmark fixture; its independent
entry-by-entry Jacobian checks pass. No core or binding code changed in that refactor.

All 46 exact-head CI checks passed for the preceding core repair at
`83399b5a31389563a26b7e69d82bfc2b5bd4b68e` and the measurement increment's
documentation head `aec6689ae6d029f9a7eb4f810e3e074fd4ed6030`. That audit covers
compiler/backend, binding, sanitizer, and required gates. Subsequent ownership
and other increments need their own exact-head audit. Codacy's two complexity
findings in the expanded Jacobian benchmark were corrected by factoring recording
and isolating the established timed fixture; the limits were not relaxed.

Retained local logs include `measurement-native-ctest.log`,
`measurement-codipack-ctest.log`, `tape-statistics-sanitized.log`, and the focused
RED/GREEN propagation/public-result/statistics logs. The full-block diagnostic
scan first crashed at a full final node block; its corrected bounded traversal
passes at that boundary and after rollover without allocating a block to inspect it.

## Resource observations

These are explicit diagnostic snapshots, outside default timing runs. The chain
has 50,000 steps. Active constants allocate two constant leaves and one fused
expression node per step; the historical `100K nodes` labels are preserved for
regression continuity. The scalar reference includes an additional registration
leaf. Multi-channel cases seed one output in several independent channels, rather
than representing distinct portfolio payoffs.

| Fixture | Nodes | Edges | Width | Logical bytes | Cursor bytes | Block capacity bytes |
|---|---:|---:|---:|---:|---:|---:|
| Active constants, scalar | 150,002 | 150,000 | 1 | 8,400,080 | 8,400,112 | 9,961,472 |
| Passive constants, scalar | 50,001 | 50,000 | 1 | 2,800,040 | 2,800,040 | 3,932,160 |
| Active constants, vector | 150,001 | 150,000 | 1 | 9,600,048 | 9,600,080 | 11,010,048 |
| Active constants, vector | 150,001 | 150,000 | 4 | 13,200,072 | 13,200,104 | 14,680,064 |
| Active constants, vector | 150,001 | 150,000 | 10 | 20,400,120 | 20,403,032 | 21,757,952 |
| Active constants, vector | 150,001 | 150,000 | 16 | 27,600,168 | 27,600,200 | 29,097,984 |
| Active constants, vector | 150,001 | 150,000 | 64 | 85,200,552 | 85,200,584 | 86,507,520 |
| Production harvest, 23 outputs / 24 parameters | 137 | 180 | 1 | 8,360 | 8,360 | 1,966,080 |
| Production harvest, 95 outputs / 96 parameters | 569 | 756 | 1 | 34,856 | 34,856 | 1,966,080 |

The tape diagnostic process's peak RSS was 88,892 kbytes as reported by
`/usr/bin/time -v`. It includes all its sequential fixtures, so it is not a
per-case peak. The Jacobian diagnostic process reported 7,720 kbytes. RSS does
not identify live tape bytes. Capacity excludes list/allocator bookkeeping;
current block counts are not cumulative allocation counts. Snapshots do not
automatically capture a high-water mark. Diagnostic timings are not used below.

## Regression verdict and coverage

Overall verdict: **no regression under the existing nine-target policy**.
The scalar and vector tape cases are effectively unchanged. Both small reference
Jacobian cases improve consistently by approximately 4%–5%. The other targets
pass; small mixed-round movements are not claimed as improvements. The Sobol
precise-opt-in/fast ratio is 9.33x, within the unchanged 10x ceiling.

The initial correctness-only kernel failed vector/Jacobian performance acceptance;
the subsequent optimized kernel corrected that. Adding coverage exposed a dense
fixture regression at `e64a2690e7d4ee2ce6cc857aad5929325ea7a409` (+6.52%/+6.80%).
That result remains in `p0-paired-final/`. The same current core library with the
original fixture passed; separating the extended fixture then passed diagnostics
and this fresh full gate. Neither failed run was waived or discarded.

All existing case names and workloads remain. Head-only passive/vector and actual
`HarvestCurveJacobian` cases are informational, with independent result oracles;
they have no shared historical baseline and are not included in the verdict.
The unchanged gate retains tape, Jacobian, PDE, RNG, interpolation, Krylov,
banded, Cholesky, and production rate/quote-risk coverage.

MC phase attribution, repeated small requests, long paths, large local-vol grids,
worker scaling, distinct-output valuation, and future structured reverse operators
still need the planned P01/feature-specific evidence. These measurements do not
claim that the whole library or full plan has been covered.

All raw process outputs, parsed samples, and the exact summary are retained in
`/tmp/dal-aad-evidence/p0-paired-verified/` (`results.json`, `summary.md`,
`environment.json`, and per-target directories). Reproduce using the repository's
paired regression script with the two isolated build roots and the settings above.


## Paired benchmark regression gate

2 independent rounds of 10 interleaved process-level samples; failure requires every round to exceed +4.00%.

| Benchmark | Case | Base min | Head min | Change | Round changes | Result |
|---|---|---:|---:|---:|---:|:---:|
| tape_perf | Clear + re-record (100K nodes) | 0.852514 ms | 0.852463 ms | -0.01% | -0.01%, -0.70% | pass |
| tape_perf | PropagateToStart (100K nodes) | 0.389029 ms | 0.386851 ms | -0.56% | -0.83%, -0.56% | pass |
| tape_perf | PropagateToStart multi-mode (100K nodes, 10 results) | 0.483168 ms | 0.482273 ms | -0.19% | -1.15%, +1.24% | pass |
| tape_perf | Rewind + re-record (100K nodes) | 0.567994 ms | 0.567454 ms | -0.10% | -0.10%, -0.07% | pass |
| tape_perf | ZeroAdjoints sweep (100K nodes) | 0.110406 ms | 0.110504 ms | +0.09% | +0.02%, +0.15% | pass |
| tape_perf | PropagateToStart multi-mode (50K steps, 1 result) (new coverage) | — | 0.383636 ms | — | — | info |
| tape_perf | PropagateToStart multi-mode (50K steps, 16 results) (new coverage) | — | 0.642524 ms | — | — | info |
| tape_perf | PropagateToStart multi-mode (50K steps, 4 results) (new coverage) | — | 0.380706 ms | — | — | info |
| tape_perf | PropagateToStart multi-mode (50K steps, 64 results) (new coverage) | — | 6.649000 ms | — | — | info |
| tape_perf | PropagateToStart passive constants (50K steps) (new coverage) | — | 0.146933 ms | — | — | info |
| tape_perf | Rewind + passive-constant recording (50K steps) (new coverage) | — | 0.236510 ms | — | — | info |
| jacobian_perf | AnalyticJacobian dense harvest (24 x 23) | 0.005351 ms | 0.005098 ms | -4.73% | -4.89%, -4.60% | pass |
| jacobian_perf | AnalyticJacobian row-width harvest (24 x 23) | 0.005371 ms | 0.005131 ms | -4.47% | -4.63%, -4.47% | pass |
| jacobian_perf | HarvestCurveJacobian dense (23 outputs x 24 parameters) (new coverage) | — | 0.004329 ms | — | — | info |
| jacobian_perf | HarvestCurveJacobian dense (95 outputs x 96 parameters) (new coverage) | — | 0.061881 ms | — | — | info |
| jacobian_perf | HarvestCurveJacobian proven prefix (23 outputs x 24 parameters) (new coverage) | — | 0.003753 ms | — | — | info |
| jacobian_perf | HarvestCurveJacobian proven prefix (95 outputs x 96 parameters) (new coverage) | — | 0.058121 ms | — | — | info |
| pde_perf | ThetaScheme rollback (200x200 CN) | 0.215056 ms | 0.219723 ms | +2.17% | +2.17%, -0.03% | pass |
| pde_perf | ThetaScheme rollback (200x200 implicit) | 0.222636 ms | 0.216493 ms | -2.76% | -2.76%, +0.20% | pass |
| pde_perf | ThetaScheme rollback (200x2000 explicit) | 0.680720 ms | 0.684596 ms | +0.57% | +0.57%, -0.58% | pass |
| rng_perf | BrownianBridge FillNormal (100K x 10D) | 6.364000 ms | 6.365000 ms | +0.02% | +0.33%, +0.02% | pass |
| rng_perf | IRN SkipNormalTo (100K x 10D) | 7.750000 ms | 7.737000 ms | -0.17% | +0.22%, -0.17% | pass |
| rng_perf | MRG32 SkipNormalTo (100K x 10D) | 0.001293 ms | 0.001292 ms | -0.08% | -0.31%, +0.15% | pass |
| rng_perf | MRG32k3a FillNormal (100K x 10D) | 19.467000 ms | 19.485000 ms | +0.09% | -0.32%, +0.32% | pass |
| rng_perf | ShuffledIRN FillNormal (100K x 10D) | 10.618000 ms | 10.600000 ms | -0.17% | -0.15%, -0.17% | pass |
| rng_perf | Sobol FillNormal fast (100K x 10D) | 4.012000 ms | 4.018000 ms | +0.15% | -0.12%, +0.27% | pass |
| rng_perf | Sobol FillNormal precise opt-in (100K x 10D) | 37.584000 ms | 37.469000 ms | -0.31% | -0.53%, -0.31% | pass |
| rng_perf | Sobol FillUniform (100K x 10D) | 0.743818 ms | 0.740578 ms | -0.44% | -0.64%, -0.44% | pass |
| interp_perf | Cubic interp (50 knots, 10K queries) | 0.050267 ms | 0.050314 ms | +0.09% | +0.22%, -0.03% | pass |
| interp_perf | Inlined linear LV-style (1e5 paths x 200 steps, 200 knots) | 99.999000 ms | 100.117000 ms | +0.12% | -0.57%, +0.20% | pass |
| interp_perf | Linear interp (50 knots, 10K queries) | 0.042259 ms | 0.042258 ms | -0.00% | -0.00%, -0.19% | pass |
| krylov_perf | BCGSolve (500x500 tridiag) | 0.061018 ms | 0.061499 ms | +0.79% | +0.79%, -0.07% | pass |
| krylov_perf | CGSolve (500x500 tridiag) | 0.051857 ms | 0.051802 ms | -0.11% | +2.78%, -0.11% | pass |
| banded_perf | TriDecomp MultiplyLeft (10K) | 0.004493 ms | 0.004495 ms | +0.04% | +0.04%, +0.07% | pass |
| banded_perf | TriDiagonal Decompose (10K) | 0.076705 ms | 0.076559 ms | -0.19% | -0.19%, -1.22% | pass |
| banded_perf | TriDiagonal MultiplyLeft (10K) | 0.004686 ms | 0.004685 ms | -0.02% | -0.02%, +0.00% | pass |
| cholesky_perf | CholeskyDecompose (200x200) | 0.226059 ms | 0.225611 ms | -0.20% | -0.22%, +0.28% | pass |
| cholesky_perf | CholeskyDecompose+Multiply (200x200) | 0.225254 ms | 0.225419 ms | +0.07% | +0.07%, +1.02% | pass |
| rate_risk_perf | Quote risk aggregate (joint XCCY) | 0.221320 ms | 0.220597 ms | -0.33% | +0.08%, -2.70% | pass |
| rate_risk_perf | Quote risk aggregate (single curve) | 0.008919 ms | 0.008905 ms | -0.16% | -0.16%, -3.54% | pass |
| rate_risk_perf | Quote risk aggregate (staged XCCY basis) | 0.060540 ms | 0.060238 ms | -0.50% | -0.51%, -0.22% | pass |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=10) | 1.150000 ms | 1.140000 ms | -0.87% | -0.87%, +1.13% | pass |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=16) | 1.747000 ms | 1.712000 ms | -2.00% | -2.12%, -0.57% | pass |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=5) | 0.823274 ms | 0.824554 ms | +0.16% | +0.16%, -3.70% | pass |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=10) | 11.115000 ms | 10.859000 ms | -2.30% | -0.46%, -3.32% | pass |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=16) | 17.330000 ms | 17.216000 ms | -0.66% | -0.66%, -0.92% | pass |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=5) | 7.922000 ms | 7.931000 ms | +0.11% | +0.11%, -0.16% | pass |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=10) | 1.086000 ms | 1.085000 ms | -0.09% | +0.09%, -1.36% | pass |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=16) | 1.662000 ms | 1.643000 ms | -1.14% | -1.26%, -0.42% | pass |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=5) | 0.777265 ms | 0.771188 ms | -0.78% | -0.55%, -4.74% | pass |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=10) | 10.801000 ms | 10.810000 ms | +0.08% | +0.14%, -0.37% | pass |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=16) | 17.258000 ms | 17.061000 ms | -1.14% | -1.32%, -0.87% | pass |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=5) | 7.810000 ms | 7.757000 ms | -0.68% | -0.68%, +0.04% | pass |
| rate_risk_perf | Quote risk portfolio joint ANALYTIC (24 XCCY x N=10/block) | 4.535000 ms | 4.511000 ms | -0.53% | -0.53%, -0.81% | pass |
| rate_risk_perf | Quote risk portfolio joint BUMPED (24 XCCY x N=10/block) | 4.555000 ms | 4.520000 ms | -0.77% | -0.77%, -0.72% | pass |
| rate_risk_perf | Quote risk portfolio single ANALYTIC (120 deposits x N=5) | 0.121862 ms | 0.121134 ms | -0.60% | -0.43%, -1.88% | pass |
| rate_risk_perf | Quote risk portfolio single BUMPED (120 deposits x N=16) | 0.171375 ms | 0.170132 ms | -0.73% | -0.73%, -0.09% | pass |
| rate_risk_perf | Quote risk portfolio staged ANALYTIC (24 XCCY x N=16) | 3.540000 ms | 3.519000 ms | -0.59% | -0.34%, -1.15% | pass |
| rate_risk_perf | Quote risk portfolio staged BUMPED (24 XCCY x N=5) | 1.267000 ms | 1.267000 ms | +0.00% | +0.00%, -1.48% | pass |
| rate_risk_perf | Rate AAD 2 components (1024 IRS x 8 nodes, fixed maturities) | 5.142000 ms | 5.072000 ms | -1.36% | -1.93%, -0.78% | pass |
| rate_risk_perf | Rate AAD 2 components (256 IRS x 8 nodes, fixed maturities) | 1.287000 ms | 1.275000 ms | -0.93% | -0.23%, -2.75% | pass |
| rate_risk_perf | Rate AAD 2 components (32 IRS x 8 nodes, fixed maturities) | 0.154065 ms | 0.152986 ms | -0.70% | -0.70%, -0.89% | pass |
| rate_risk_perf | Rate OIS daily compounding sweep (5Y quarterly x daily) | 0.214859 ms | 0.212578 ms | -1.06% | -1.08%, -1.06% | pass |
| rate_risk_perf | Rate PV (1024 IRS x 8 nodes, fixed maturities) | 2.455000 ms | 2.450000 ms | -0.20% | -0.20%, -1.89% | pass |
| rate_risk_perf | Rate PV (256 IRS x 8 nodes, fixed maturities) | 0.608614 ms | 0.604893 ms | -0.61% | -0.61%, +0.12% | pass |
| rate_risk_perf | Rate PV (32 IRS x 8 nodes, fixed maturities) | 0.075172 ms | 0.074302 ms | -1.16% | -0.80%, -1.16% | pass |
| rate_risk_perf | Rate XCCY batch serial (24 XCCY x 5 components) | 0.779578 ms | 0.778207 ms | -0.18% | -0.18%, -3.37% | pass |
| rate_risk_perf | Rate batch serial (120 IRS x 2 components) | 1.268000 ms | 1.264000 ms | -0.32% | -0.32%, +0.47% | pass |
| rate_risk_perf | Rate prepare geometry (256 IRS x 8 nodes, fixed maturities) | 0.538066 ms | 0.544453 ms | +1.19% | +0.52%, +1.39% | pass |
| rate_risk_perf | Rate prepare geometry (32 IRS x 8 nodes, fixed maturities) | 0.064679 ms | 0.064756 ms | +0.12% | +0.12%, -0.09% | pass |
| rate_risk_perf | Rate prepared AAD 2 components (256 IRS x 8 nodes, fixed maturities) | 0.712973 ms | 0.708416 ms | -0.64% | +1.63%, -2.44% | pass |
| rate_risk_perf | Rate prepared AAD 2 components (32 IRS x 8 nodes, fixed maturities) | 0.085816 ms | 0.086017 ms | +0.23% | +0.23%, -0.50% | pass |
| rate_risk_perf | Rate prepared PV (256 IRS x 8 nodes, fixed maturities) | 0.106071 ms | 0.104656 ms | -1.33% | -1.33%, -1.61% | pass |
| rate_risk_perf | Rate prepared PV (32 IRS x 8 nodes, fixed maturities) | 0.012633 ms | 0.012639 ms | +0.05% | -0.50%, +1.14% | pass |
| rate_risk_perf | Rate single-trade sweeps (240 IRS calls) | 2.004000 ms | 1.996000 ms | -0.40% | -0.40%, +0.05% | pass |

Head Sobol precise opt-in / fast ratio: 9.33x (limit 10.00x).

Rows marked (new coverage) are head-only benchmark cases added by the PR; they are reported for information and are not gated.

All performance acceptance checks passed.
