# Native AAD production measurement acceptance

Status: **inconclusive overall; P01 remains open**. The unchanged formal nine-target gate passes;
the current original-baseline full MC confirmation fails one supplemental case.
Resource/scaling measurements are complete. Exact `8886c083` CI passes all 35 checks.

## Identities and protocol

- Original baseline: `b5e3caca85bb1a5b1aa3acf7898b387832d443c8`.
- Current implementation: `8886c083dcf80f00f09ea8e4c4c83c619507a786`.
- Retained core archives last built at `7ead7ecd`; their compiled inputs are unchanged at current head.
  The script-MC executable was rebuilt for the current MSVC-compatible product construction.
- GCC 15.2, CMake 4.2.3, static Release/O3, portable architecture, Eigen with its internal threading disabled.
- Native AAD, lifetime diagnostics OFF; profiler OFF and ON are separately identified.
- Shared WSL2 host with uncontrolled desktop load; no concurrent DAL builds/tests during sampling.
  Ordinary comparisons use CPUs 4/6/8/10 and four threads. Scaling uses CPUs 4/6/8/10/12/14/16/18.
- Min reduction, alternating interleaved base/head processes, two independent rounds.
  Formal gate: ten samples per side/round and unchanged 4%/Sobol policy.
  Supplemental confirmation/control: thirty samples per side/round, unchanged 4% threshold.
- Evidence survives `/tmp` cleanup in `/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.
  `/tmp/dal-aad-evidence` is now an alias to that persistent directory.
  Exact sources and five dependency pins were restored in independent persistent checkouts.
  All 1,681 frozen source/helper/executable hashes match; compiled binaries were retained, not rebuilt.
  Storage changed from tmpfs to an ext4-backed cache; new diagnostic metadata records that boundary.

## Formal nine-target gate

Frozen `5a6382b` default-OFF gate executables, configuration and gate code remain byte-identical
at `7ead7ecd` and `8886c083`. All 65 comparable cases pass; ten new cases are informational.
Sobol precise/fast ratio is 9.38x, below the existing 10x ceiling. Minima below are in microseconds.

| Target         | Case                                                                           | Base min us | Head min us | Round 1  | Round 2  | Result |
|----------------|--------------------------------------------------------------------------------|-------------|-------------|----------|----------|--------|
| tape_perf      | Clear + re-record (100K nodes)                                                 | 869.318     | 868.238     | +0.300%  | -0.345%  | PASS   |
| tape_perf      | PropagateToStart (100K nodes)                                                  | 391.097     | 387.973     | -0.672%  | -0.799%  | PASS   |
| tape_perf      | PropagateToStart multi-mode (100K nodes, 10 results)                           | 491.941     | 485.127     | -1.809%  | -1.373%  | PASS   |
| tape_perf      | Rewind + re-record (100K nodes)                                                | 568.194     | 568.257     | +0.050%  | -0.111%  | PASS   |
| tape_perf      | ZeroAdjoints sweep (100K nodes)                                                | 111.251     | 111.896     | +0.742%  | +0.444%  | PASS   |
| tape_perf      | PropagateToStart multi-mode (50K steps, 1 result) (new coverage)               | -           | 383.457     | -        | -        | INFO   |
| tape_perf      | PropagateToStart multi-mode (50K steps, 16 results) (new coverage)             | -           | 633.882     | -        | -        | INFO   |
| tape_perf      | PropagateToStart multi-mode (50K steps, 4 results) (new coverage)              | -           | 383.297     | -        | -        | INFO   |
| tape_perf      | PropagateToStart multi-mode (50K steps, 64 results) (new coverage)             | -           | 6792.000    | -        | -        | INFO   |
| tape_perf      | PropagateToStart passive constants (50K steps) (new coverage)                  | -           | 147.657     | -        | -        | INFO   |
| tape_perf      | Rewind + passive-constant recording (50K steps) (new coverage)                 | -           | 236.492     | -        | -        | INFO   |
| jacobian_perf  | AnalyticJacobian dense harvest (24 x 23)                                       | 5.385       | 4.705       | -12.706% | -12.628% | PASS   |
| jacobian_perf  | AnalyticJacobian row-width harvest (24 x 23)                                   | 5.400       | 4.665       | -13.614% | -13.611% | PASS   |
| jacobian_perf  | HarvestCurveJacobian dense (23 outputs x 24 parameters) (new coverage)         | -           | 4.367       | -        | -        | INFO   |
| jacobian_perf  | HarvestCurveJacobian dense (95 outputs x 96 parameters) (new coverage)         | -           | 61.934      | -        | -        | INFO   |
| jacobian_perf  | HarvestCurveJacobian proven prefix (23 outputs x 24 parameters) (new coverage) | -           | 3.784       | -        | -        | INFO   |
| jacobian_perf  | HarvestCurveJacobian proven prefix (95 outputs x 96 parameters) (new coverage) | -           | 58.137      | -        | -        | INFO   |
| pde_perf       | ThetaScheme rollback (200x200 CN)                                              | 215.226     | 215.183     | +0.519%  | -3.512%  | PASS   |
| pde_perf       | ThetaScheme rollback (200x200 implicit)                                        | 214.652     | 214.829     | -0.100%  | +2.500%  | PASS   |
| pde_perf       | ThetaScheme rollback (200x2000 explicit)                                       | 669.310     | 665.930     | -0.211%  | -0.700%  | PASS   |
| rng_perf       | BrownianBridge FillNormal (100K x 10D)                                         | 6680.000    | 6559.000    | -3.643%  | +1.213%  | PASS   |
| rng_perf       | IRN SkipNormalTo (100K x 10D)                                                  | 7928.000    | 7871.000    | +0.832%  | -1.600%  | PASS   |
| rng_perf       | MRG32 SkipNormalTo (100K x 10D)                                                | 1.295       | 1.294       | +0.000%  | -0.154%  | PASS   |
| rng_perf       | MRG32k3a FillNormal (100K x 10D)                                               | 20438.000   | 20116.000   | -1.575%  | -0.718%  | PASS   |
| rng_perf       | ShuffledIRN FillNormal (100K x 10D)                                            | 10841.000   | 10910.000   | +0.119%  | +1.052%  | PASS   |
| rng_perf       | Sobol FillNormal fast (100K x 10D)                                             | 4216.000    | 4220.000    | +1.945%  | -1.906%  | PASS   |
| rng_perf       | Sobol FillNormal precise opt-in (100K x 10D)                                   | 39439.000   | 39565.000   | +0.635%  | +0.319%  | PASS   |
| rng_perf       | Sobol FillUniform (100K x 10D)                                                 | 773.477     | 788.681     | -0.890%  | +3.413%  | PASS   |
| interp_perf    | Cubic interp (50 knots, 10K queries)                                           | 52.260      | 52.244      | -0.590%  | +0.264%  | PASS   |
| interp_perf    | Inlined linear LV-style (1e5 paths x 200 steps, 200 knots)                     | 106282.000  | 106072.000  | -0.040%  | -0.322%  | PASS   |
| interp_perf    | Linear interp (50 knots, 10K queries)                                          | 43.902      | 44.146      | -0.708%  | +0.556%  | PASS   |
| krylov_perf    | BCGSolve (500x500 tridiag)                                                     | 61.197      | 61.117      | -0.304%  | -0.131%  | PASS   |
| krylov_perf    | CGSolve (500x500 tridiag)                                                      | 51.776      | 51.859      | +0.160%  | -0.378%  | PASS   |
| banded_perf    | TriDecomp MultiplyLeft (10K)                                                   | 4.497       | 4.495       | -0.443%  | -0.044%  | PASS   |
| banded_perf    | TriDiagonal Decompose (10K)                                                    | 77.125      | 76.953      | +1.872%  | -0.598%  | PASS   |
| banded_perf    | TriDiagonal MultiplyLeft (10K)                                                 | 4.687       | 4.688       | +0.021%  | +0.085%  | PASS   |
| cholesky_perf  | CholeskyDecompose (200x200)                                                    | 234.813     | 235.330     | +0.062%  | +0.287%  | PASS   |
| cholesky_perf  | CholeskyDecompose+Multiply (200x200)                                           | 234.131     | 234.634     | +0.227%  | +0.067%  | PASS   |
| rate_risk_perf | Quote risk aggregate (joint XCCY)                                              | 220.208     | 226.845     | +3.014%  | +4.770%  | PASS   |
| rate_risk_perf | Quote risk aggregate (single curve)                                            | 8.895       | 8.876       | -2.302%  | -0.214%  | PASS   |
| rate_risk_perf | Quote risk aggregate (staged XCCY basis)                                       | 60.369      | 61.031      | -3.093%  | +1.307%  | PASS   |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=10)                                      | 1154.000    | 1182.000    | -0.838%  | +2.426%  | PASS   |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=16)                                      | 1773.000    | 1733.000    | -3.400%  | -0.451%  | PASS   |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=5)                                       | 849.426     | 825.525     | -3.144%  | -2.218%  | PASS   |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=10)                                     | 11486.000   | 11388.000   | +1.468%  | -0.853%  | PASS   |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=16)                                     | 18441.000   | 17672.000   | -1.616%  | -4.486%  | PASS   |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=5)                                      | 8376.000    | 8048.000    | +0.310%  | -4.645%  | PASS   |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=10)                       | 1106.000    | 1080.000    | -4.594%  | -1.356%  | PASS   |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=16)                       | 1677.000    | 1662.000    | -0.894%  | -0.766%  | PASS   |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=5)                        | 805.697     | 763.099     | -5.287%  | -2.228%  | PASS   |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=10)                      | 11641.000   | 11419.000   | -1.967%  | -1.907%  | PASS   |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=16)                      | 18154.000   | 17836.000   | -0.011%  | -1.752%  | PASS   |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=5)                       | 8039.000    | 7996.000    | -1.793%  | -0.535%  | PASS   |
| rate_risk_perf | Quote risk portfolio joint ANALYTIC (24 XCCY x N=10/block)                     | 4678.000    | 4625.000    | -1.133%  | -0.149%  | PASS   |
| rate_risk_perf | Quote risk portfolio joint BUMPED (24 XCCY x N=10/block)                       | 4654.000    | 4491.000    | -4.142%  | -3.502%  | PASS   |
| rate_risk_perf | Quote risk portfolio single ANALYTIC (120 deposits x N=5)                      | 123.002     | 126.054     | +1.929%  | +6.878%  | PASS   |
| rate_risk_perf | Quote risk portfolio single BUMPED (120 deposits x N=16)                       | 169.855     | 169.662     | -0.671%  | +0.583%  | PASS   |
| rate_risk_perf | Quote risk portfolio staged ANALYTIC (24 XCCY x N=16)                          | 3648.000    | 3501.000    | -6.590%  | -2.111%  | PASS   |
| rate_risk_perf | Quote risk portfolio staged BUMPED (24 XCCY x N=5)                             | 1307.000    | 1283.000    | -0.918%  | -2.508%  | PASS   |
| rate_risk_perf | Rate AAD 2 components (1024 IRS x 8 nodes, fixed maturities)                   | 5406.000    | 5278.000    | -2.494%  | -1.165%  | PASS   |
| rate_risk_perf | Rate AAD 2 components (256 IRS x 8 nodes, fixed maturities)                    | 1320.000    | 1276.000    | -3.333%  | -0.530%  | PASS   |
| rate_risk_perf | Rate AAD 2 components (32 IRS x 8 nodes, fixed maturities)                     | 154.585     | 151.564     | -0.023%  | -6.002%  | PASS   |
| rate_risk_perf | Rate OIS daily compounding sweep (5Y quarterly x daily)                        | 224.907     | 212.982     | -4.637%  | -5.302%  | PASS   |
| rate_risk_perf | Rate PV (1024 IRS x 8 nodes, fixed maturities)                                 | 2582.000    | 2563.000    | -0.736%  | -0.770%  | PASS   |
| rate_risk_perf | Rate PV (256 IRS x 8 nodes, fixed maturities)                                  | 630.879     | 608.195     | -3.596%  | -2.435%  | PASS   |
| rate_risk_perf | Rate PV (32 IRS x 8 nodes, fixed maturities)                                   | 76.643      | 76.316      | +0.724%  | -0.427%  | PASS   |
| rate_risk_perf | Rate XCCY batch serial (24 XCCY x 5 components)                                | 790.173     | 782.896     | -3.602%  | +0.502%  | PASS   |
| rate_risk_perf | Rate batch serial (120 IRS x 2 components)                                     | 1295.000    | 1295.000    | -0.303%  | +0.000%  | PASS   |
| rate_risk_perf | Rate prepare geometry (256 IRS x 8 nodes, fixed maturities)                    | 570.225     | 554.585     | -2.743%  | -1.099%  | PASS   |
| rate_risk_perf | Rate prepare geometry (32 IRS x 8 nodes, fixed maturities)                     | 64.905      | 65.128      | -3.402%  | +4.758%  | PASS   |
| rate_risk_perf | Rate prepared AAD 2 components (256 IRS x 8 nodes, fixed maturities)           | 741.376     | 725.950     | -5.594%  | +0.456%  | PASS   |
| rate_risk_perf | Rate prepared AAD 2 components (32 IRS x 8 nodes, fixed maturities)            | 85.698      | 85.114      | -1.164%  | +3.797%  | PASS   |
| rate_risk_perf | Rate prepared PV (256 IRS x 8 nodes, fixed maturities)                         | 109.186     | 111.263     | -0.462%  | +3.912%  | PASS   |
| rate_risk_perf | Rate prepared PV (32 IRS x 8 nodes, fixed maturities)                          | 12.655      | 12.972      | +1.741%  | +7.728%  | PASS   |
| rate_risk_perf | Rate single-trade sweeps (240 IRS calls)                                       | 2063.000    | 2018.000    | -0.388%  | -4.405%  | PASS   |

## Complete Monte Carlo confirmation

`production-profiling-mc-paired-02/` initially passes 42/44; the failing 3F swaption and
4-node AAD calibration cases are retained. The prior-native `8816951` control passes 44/44.
The predeclared full two-by-thirty original-baseline confirmation passes 43/44:
**GSR 1F bond option (1000 prices) fails at +5.0028%/+8.0347%.**
The following table retains every case; a passing CI does not override this result.

Minima are microseconds across both rounds; both independent round deltas are shown.

| Profile                | Case                                                                             | Base min us | Head min us | Round 1 | Round 2 | Result |
|------------------------|----------------------------------------------------------------------------------|-------------|-------------|---------|---------|--------|
| ordinary               | GSR 1F 5Y swaption (1 price, order 16/32)                                        | 13.401      | 14.303      | +2.604% | +9.223% | PASS   |
| ordinary               | GSR 1F bond (100K paths x 4 steps)                                               | 6876.000    | 7275.000    | +6.559% | +0.762% | PASS   |
| ordinary               | GSR 1F bond option (1000 prices)                                                 | 1730.000    | 1847.000    | +5.003% | +8.035% | FAIL   |
| ordinary               | GSR 1F swap and Libor (10K paths x 4 steps)                                      | 16289.000   | 16305.000   | +0.098% | -1.102% | PASS   |
| ordinary               | GSR 2F 5Y swaption (1 price, order 16/32)                                        | 260.273     | 271.171     | -0.075% | +4.187% | PASS   |
| ordinary               | GSR 2F bond (100K paths x 4 steps)                                               | 9963.000    | 9968.000    | +0.050% | +1.442% | PASS   |
| ordinary               | GSR 2F bond option (1000 prices)                                                 | 2548.000    | 2589.000    | +1.568% | +1.609% | PASS   |
| ordinary               | GSR 2F swap and Libor (10K paths x 4 steps)                                      | 16802.000   | 16769.000   | -0.256% | +0.536% | PASS   |
| ordinary               | GSR 3F 5Y swaption (1 price, order 16/32)                                        | 267.880     | 264.510     | -5.576% | +0.535% | PASS   |
| ordinary               | GSR 3F bond (100K paths x 4 steps)                                               | 11060.000   | 10828.000   | -2.098% | -1.029% | PASS   |
| ordinary               | GSR 3F bond option (1000 prices)                                                 | 3512.000    | 3370.000    | -4.982% | -4.043% | PASS   |
| ordinary               | GSR 3F swap and Libor (10K paths x 4 steps)                                      | 16897.000   | 16579.000   | +0.941% | -2.350% | PASS   |
| ordinary               | GSR g calibration (3 quotes x 3 buckets)                                         | 205.417     | 202.293     | -3.963% | +1.516% | PASS   |
| ordinary               | LSMC regression degree=3 (100000 paths)                                          | 395.652     | 387.375     | -0.370% | -3.780% | PASS   |
| ordinary               | LSMC regression degree=3 ITM mask (100000 paths)                                 | 368.509     | 369.025     | +1.616% | +0.140% | PASS   |
| ordinary               | LSMC regression degree=8 (100000 paths)                                          | 719.649     | 699.230     | -1.940% | -2.837% | PASS   |
| ordinary               | LSMC regression degree=8 ITM mask (100000 paths)                                 | 688.310     | 688.331     | +1.689% | +0.003% | PASS   |
| ordinary               | LSMC regression features=2 degree=2 ITM mask (100000 paths)                      | 8288.000    | 8066.000    | -3.667% | +0.603% | PASS   |
| ordinary               | LSMC regression features=2 degree=3 ITM mask (100000 paths)                      | 14496.000   | 14759.000   | -1.092% | +2.201% | PASS   |
| ordinary               | LSMC regression features=3 degree=3 ITM mask (100000 paths)                      | 36775.000   | 37410.000   | -1.999% | +3.630% | PASS   |
| ordinary               | correlated BS path (100K x 12 steps x 1 assets)                                  | 10533.000   | 10669.000   | +2.934% | +1.118% | PASS   |
| ordinary               | correlated BS path (100K x 12 steps x 2 assets)                                  | 16108.000   | 15745.000   | -2.610% | -0.851% | PASS   |
| ordinary               | correlated BS path (100K x 12 steps x 3 assets)                                  | 21553.000   | 21588.000   | -0.507% | +0.696% | PASS   |
| ordinary               | hybrid flat-rate path (100K x 12 steps x 2 assets)                               | 40301.000   | 40031.000   | -0.670% | -0.357% | PASS   |
| ordinary               | hybrid logDF path (100K x 12 steps x 2 assets)                                   | 39337.000   | 39723.000   | +2.000% | +0.981% | PASS   |
| ordinary               | script engine bermudan exercise double compiled=false (100000 paths x 54 events) | 128595.000  | 127245.000  | -1.050% | +0.906% | PASS   |
| ordinary               | script engine bermudan exercise double compiled=true (100000 paths x 54 events)  | 114263.000  | 116692.000  | +2.126% | +0.497% | PASS   |
| ordinary               | script engine vanilla Number_ compiled=false (20000 paths x 1 events)            | 700.113     | 678.542     | -0.065% | -3.081% | PASS   |
| ordinary               | script engine vanilla Number_ compiled=true (20000 paths x 1 events)             | 671.552     | 667.998     | -1.694% | +1.788% | PASS   |
| ordinary               | script engine vanilla double compiled=false (200000 paths x 1 events)            | 1893.000    | 1934.000    | +1.605% | +2.166% | PASS   |
| ordinary               | script engine vanilla double compiled=true (200000 paths x 1 events)             | 1752.000    | 1766.000    | +0.799% | +1.136% | PASS   |
| ordinary               | script engine weekly barrier Number_ compiled=false (10000 paths x 52 events)    | 9550.000    | 9520.000    | -0.230% | -3.673% | PASS   |
| ordinary               | script engine weekly barrier Number_ compiled=true (10000 paths x 52 events)     | 8099.000    | 7897.000    | -2.734% | -1.667% | PASS   |
| ordinary               | script engine weekly barrier double compiled=false (100000 paths x 52 events)    | 37304.000   | 37224.000   | -1.218% | +2.472% | PASS   |
| ordinary               | script engine weekly barrier double compiled=true (100000 paths x 52 events)     | 22229.000   | 22315.000   | +3.046% | +0.387% | PASS   |
| gsr-market-calibration | GSR lagged swaption, 512 outer x 32 inner paths                                  | 6019.000    | 6194.000    | +2.907% | +0.351% | PASS   |
| gsr-market-calibration | GSR market 4-node fit, 12 expiry/tenor/strike quotes, AAD                        | 77106.000   | 80042.000   | +3.808% | +3.300% | PASS   |
| gsr-market-calibration | GSR market 4-node fit, 12 expiry/tenor/strike quotes, FD                         | 21916.000   | 22316.000   | +1.825% | +2.037% | PASS   |
| gsr-market-calibration | GSR market 48-node price Jacobian, 12 quotes, AAD                                | 19683.000   | 20300.000   | +3.135% | +2.071% | PASS   |
| gsr-market-calibration | GSR market 48-node price Jacobian, 12 quotes, FD                                 | 48778.000   | 48759.000   | -0.039% | -3.896% | PASS   |
| gsr-market-calibration | GSR market vol + native curve quote risk, one fitted node                        | 22263.000   | 21739.000   | -2.354% | -1.453% | PASS   |
| lsm-bs-tree            | lsm-bs-tree                                                                      | 107686.482  | 108168.441  | -0.174% | +0.680% | PASS   |
| lsm-bs-compiled        | lsm-bs-compiled                                                                  | 104570.671  | 103603.401  | -0.501% | -1.018% | PASS   |
| lsm-lv-daily           | lsm-lv-daily                                                                     | 503144.694  | 500842.245  | +0.617% | -1.436% | PASS   |

Post-cleanup same-binary full control passes 44/44, using the exact same retained head executable
on both sides. The 1F bond-option case still varies by -4.7235%/+2.4390%.
Original-baseline/head GSR European isolation on CPU 4 passes all seven cases:
1F bond option moves +2.0630%/-1.7194%. These controls are diagnostic and do not replace
the failed complete-suite confirmation. The GSR pricing source and benchmark are unchanged;
all 108 text sections and instruction/relocation text in its compiled object match.
Different source-root diagnostic strings remain in five rodata sections.
A predeclared fresh matched-prefix build follows the current scheduled-CI methodology
to test this confound. Its GSR European object is byte-identical across base/head.
The full 44-case two-by-thirty comparison passes 42/44; bond option is now -2.76%/-1.64%,
but vanilla native AAD tree is +6.3848%/+7.7373% and 4-node GSR AAD calibration
is +4.2605%/+5.0575%. All three LSM numeric comparisons pass. This controlled build
is a new explicitly matched configuration and does not rewrite the earlier result.
Failure identities and best-case timings have not stabilized on this WSL2 host.
The overall verdict remains inconclusive; no additional full-suite rerun is chosen to obtain a pass.

The section-extraction command accidentally rewrote intermediate-object metadata.
Both objects were restored from unchanged retained static archives to their exact original SHA-256.
No measured executable or library archive changed. The restoration record and transformed copies survive.

## Complete curve confirmation

The initial 24-node PWL query failure (+6.58%/+4.29%) is retained.
The prior-native control passes 25/25; the predeclared full two-by-thirty original-baseline
confirmation passes 25/25. The 24-node query moves -0.0848%/-0.6082% in confirmation.

| Case                                                            | Base min us | Head min us | Round 1 | Round 2 | Result |
|-----------------------------------------------------------------|-------------|-------------|---------|---------|--------|
| CalibrateYieldCurve LOG_CUBIC_NATURAL ANALYTIC +DIAG (23 swaps) | 1431.000    | 1394.000    | -2.789% | -1.258% | PASS   |
| CalibrateYieldCurve LOG_CUBIC_NATURAL ANALYTIC SOLVE (23 swaps) | 851.344     | 827.405     | -3.075% | -2.812% | PASS   |
| CalibrateYieldCurve LOG_CUBIC_NATURAL BUMPED +DIAG (23 swaps)   | 2108.000    | 2122.000    | +1.471% | -0.469% | PASS   |
| CalibrateYieldCurve LOG_CUBIC_NATURAL BUMPED SOLVE (23 swaps)   | 1322.000    | 1332.000    | +1.740% | -0.075% | PASS   |
| CalibrateYieldCurve LOG_LINEAR ANALYTIC +DIAG (23 swaps)        | 1346.000    | 1328.000    | -2.476% | -1.337% | PASS   |
| CalibrateYieldCurve LOG_LINEAR ANALYTIC SOLVE (23 swaps)        | 778.065     | 774.475     | -0.926% | -0.228% | PASS   |
| CalibrateYieldCurve LOG_LINEAR APPROXIMATE (23 swaps)           | 10064.000   | 10115.000   | -1.208% | +0.507% | PASS   |
| CalibrateYieldCurve LOG_LINEAR BUMPED +DIAG (23 swaps)          | 1849.000    | 1807.000    | -2.271% | -2.685% | PASS   |
| CalibrateYieldCurve LOG_LINEAR BUMPED SOLVE (23 swaps)          | 1149.000    | 1133.000    | -2.627% | -1.393% | PASS   |
| CalibrateYieldCurve MIXED ANALYTIC +DIAG (23 swaps)             | 1354.000    | 1352.000    | -0.515% | +0.000% | PASS   |
| CalibrateYieldCurve MIXED ANALYTIC SOLVE (23 swaps)             | 781.416     | 780.446     | -0.822% | -0.124% | PASS   |
| CalibrateYieldCurve MIXED BUMPED +DIAG (23 swaps)               | 1858.000    | 1869.000    | +1.884% | -1.476% | PASS   |
| CalibrateYieldCurve MIXED BUMPED SOLVE (23 swaps)               | 1175.000    | 1183.000    | +2.367% | +0.681% | PASS   |
| CalibrateYieldCurve PWC ANALYTIC +DIAG (23 swaps)               | 1080.000    | 1051.000    | -2.683% | -2.685% | PASS   |
| CalibrateYieldCurve PWC ANALYTIC SOLVE (23 swaps)               | 588.377     | 583.313     | -0.913% | -0.861% | PASS   |
| CalibrateYieldCurve PWC BUMPED +DIAG (23 swaps)                 | 924.062     | 935.928     | +0.386% | +1.284% | PASS   |
| CalibrateYieldCurve PWC BUMPED SOLVE (23 swaps)                 | 631.142     | 636.708     | +1.331% | +0.066% | PASS   |
| CalibrateYieldCurve PWL ANALYTIC +DIAG (23 swaps)               | 1230.000    | 1199.000    | -2.520% | -2.195% | PASS   |
| CalibrateYieldCurve PWL ANALYTIC SOLVE (23 swaps)               | 649.463     | 644.206     | -0.809% | +0.335% | PASS   |
| CalibrateYieldCurve PWL BUMPED +DIAG (23 swaps)                 | 1510.000    | 1521.000    | +0.262% | +0.728% | PASS   |
| CalibrateYieldCurve PWL BUMPED SOLVE (23 swaps)                 | 939.798     | 934.621     | -1.322% | -0.551% | PASS   |
| PWL DF queries (4096 x 24 nodes)                                | 47.161      | 47.066      | -0.085% | -0.608% | PASS   |
| PWL DF queries (4096 x 256 nodes)                               | 80.367      | 80.070      | -0.370% | -0.251% | PASS   |
| PWL DF queries (4096 x 64 nodes)                                | 53.065      | 53.600      | -0.279% | +1.008% | PASS   |
| PWL DF queries (4096 x 8 nodes)                                 | 41.975      | 42.252      | +0.660% | +3.423% | PASS   |

## Matched-prefix full Monte Carlo confirmation

All minima are microseconds. Compiler/options and five common pins match;
source roots map to `/dal-aad-source` on both sides.

| Profile                | Case                                                                             | Base min us | Head min us | Round 1  | Round 2 | Result |
|------------------------|----------------------------------------------------------------------------------|-------------|-------------|----------|---------|--------|
| ordinary               | GSR 1F 5Y swaption (1 price, order 16/32)                                        | 13.466      | 13.747      | +0.022%  | +2.243% | PASS   |
| ordinary               | GSR 1F bond (100K paths x 4 steps)                                               | 6501.000    | 6775.000    | +1.483%  | +4.215% | PASS   |
| ordinary               | GSR 1F bond option (1000 prices)                                                 | 1709.000    | 1681.000    | -2.762%  | -1.638% | PASS   |
| ordinary               | GSR 1F swap and Libor (10K paths x 4 steps)                                      | 15369.000   | 15223.000   | -1.940%  | -0.950% | PASS   |
| ordinary               | GSR 2F 5Y swaption (1 price, order 16/32)                                        | 261.464     | 262.155     | +1.995%  | -1.034% | PASS   |
| ordinary               | GSR 2F bond (100K paths x 4 steps)                                               | 9551.000    | 9479.000    | -0.555%  | -0.754% | PASS   |
| ordinary               | GSR 2F bond option (1000 prices)                                                 | 2404.000    | 2437.000    | -1.974%  | +1.373% | PASS   |
| ordinary               | GSR 2F swap and Libor (10K paths x 4 steps)                                      | 15688.000   | 15313.000   | +0.402%  | -2.390% | PASS   |
| ordinary               | GSR 3F 5Y swaption (1 price, order 16/32)                                        | 254.052     | 256.340     | -2.258%  | +3.356% | PASS   |
| ordinary               | GSR 3F bond (100K paths x 4 steps)                                               | 10556.000   | 10514.000   | -0.398%  | -1.259% | PASS   |
| ordinary               | GSR 3F bond option (1000 prices)                                                 | 3331.000    | 3349.000    | +2.522%  | -0.416% | PASS   |
| ordinary               | GSR 3F swap and Libor (10K paths x 4 steps)                                      | 15909.000   | 15659.000   | +0.871%  | -1.571% | PASS   |
| ordinary               | GSR g calibration (3 quotes x 3 buckets)                                         | 194.894     | 196.658     | +3.782%  | +0.905% | PASS   |
| ordinary               | LSMC regression degree=3 (100000 paths)                                          | 375.632     | 384.941     | +2.616%  | +1.735% | PASS   |
| ordinary               | LSMC regression degree=3 ITM mask (100000 paths)                                 | 353.408     | 361.790     | +3.001%  | +2.372% | PASS   |
| ordinary               | LSMC regression degree=8 (100000 paths)                                          | 661.577     | 673.015     | -0.302%  | +4.119% | PASS   |
| ordinary               | LSMC regression degree=8 ITM mask (100000 paths)                                 | 661.121     | 664.717     | +0.448%  | +0.544% | PASS   |
| ordinary               | LSMC regression features=2 degree=2 ITM mask (100000 paths)                      | 7598.000    | 7749.000    | -2.379%  | +1.987% | PASS   |
| ordinary               | LSMC regression features=2 degree=3 ITM mask (100000 paths)                      | 13732.000   | 14085.000   | +0.178%  | +2.571% | PASS   |
| ordinary               | LSMC regression features=3 degree=3 ITM mask (100000 paths)                      | 34881.000   | 34869.000   | -0.440%  | -0.034% | PASS   |
| ordinary               | correlated BS path (100K x 12 steps x 1 assets)                                  | 10106.000   | 9907.000    | -3.186%  | -0.960% | PASS   |
| ordinary               | correlated BS path (100K x 12 steps x 2 assets)                                  | 14521.000   | 15127.000   | -0.611%  | +4.173% | PASS   |
| ordinary               | correlated BS path (100K x 12 steps x 3 assets)                                  | 20212.000   | 20150.000   | +1.038%  | -0.307% | PASS   |
| ordinary               | hybrid flat-rate path (100K x 12 steps x 2 assets)                               | 38143.000   | 37879.000   | -0.668%  | -0.692% | PASS   |
| ordinary               | hybrid logDF path (100K x 12 steps x 2 assets)                                   | 37512.000   | 37514.000   | -1.542%  | +0.005% | PASS   |
| ordinary               | script engine bermudan exercise double compiled=false (100000 paths x 54 events) | 117694.000  | 117796.000  | +0.773%  | +0.087% | PASS   |
| ordinary               | script engine bermudan exercise double compiled=true (100000 paths x 54 events)  | 108234.000  | 108459.000  | -1.488%  | +0.208% | PASS   |
| ordinary               | script engine vanilla Number_ compiled=false (20000 paths x 1 events)            | 654.129     | 695.894     | +6.385%  | +7.737% | FAIL   |
| ordinary               | script engine vanilla Number_ compiled=true (20000 paths x 1 events)             | 594.477     | 633.453     | +10.153% | -0.080% | PASS   |
| ordinary               | script engine vanilla double compiled=false (200000 paths x 1 events)            | 1860.000    | 1910.000    | -0.624%  | +2.742% | PASS   |
| ordinary               | script engine vanilla double compiled=true (200000 paths x 1 events)             | 1669.000    | 1690.000    | +0.862%  | +1.258% | PASS   |
| ordinary               | script engine weekly barrier Number_ compiled=false (10000 paths x 52 events)    | 9158.000    | 9100.000    | -2.921%  | -0.633% | PASS   |
| ordinary               | script engine weekly barrier Number_ compiled=true (10000 paths x 52 events)     | 7405.000    | 7501.000    | -1.471%  | +2.215% | PASS   |
| ordinary               | script engine weekly barrier double compiled=false (100000 paths x 52 events)    | 34425.000   | 35380.000   | -1.162%  | +3.335% | PASS   |
| ordinary               | script engine weekly barrier double compiled=true (100000 paths x 52 events)     | 21384.000   | 21217.000   | -1.624%  | -0.781% | PASS   |
| gsr-market-calibration | GSR lagged swaption, 512 outer x 32 inner paths                                  | 6082.000    | 6074.000    | -2.971%  | +1.102% | PASS   |
| gsr-market-calibration | GSR market 4-node fit, 12 expiry/tenor/strike quotes, AAD                        | 75828.000   | 79663.000   | +4.260%  | +5.057% | FAIL   |
| gsr-market-calibration | GSR market 4-node fit, 12 expiry/tenor/strike quotes, FD                         | 21452.000   | 21248.000   | -0.142%  | -0.951% | PASS   |
| gsr-market-calibration | GSR market 48-node price Jacobian, 12 quotes, AAD                                | 19486.000   | 20064.000   | +2.966%  | +2.532% | PASS   |
| gsr-market-calibration | GSR market 48-node price Jacobian, 12 quotes, FD                                 | 47503.000   | 47816.000   | -0.924%  | +0.659% | PASS   |
| gsr-market-calibration | GSR market vol + native curve quote risk, one fitted node                        | 21717.000   | 21007.000   | -0.497%  | -3.318% | PASS   |
| lsm-bs-tree            | lsm-bs-tree                                                                      | 101401.418  | 101188.507  | +0.451%  | -0.509% | PASS   |
| lsm-bs-compiled        | lsm-bs-compiled                                                                  | 97883.783   | 97742.380   | -0.062%  | -0.796% | PASS   |
| lsm-lv-daily           | lsm-lv-daily                                                                     | 480721.273  | 484622.613  | -0.600%  | +0.871% | PASS   |

## Fixed-total-path scaling

All 2,496 measured processes across 24 profiles validate price and every requested risk
at rel/abs 1e-10 against a full-path untimed reference. Of these, 882 are bitwise equal;
ordinary parallel reduction is not required to be bitwise invariant. Every LSM sample is bitwise equal.
Each thread count has ten default-OFF cold-request process samples and one separate ON phase sample.
Cold includes fresh model/product/preparation after untimed validation; it is not a process-first
or cold-OS-cache measurement. Warm reuses preparation, not worker state.
Outputs 4/16/64 are distinct strikes executed sequentially as scalar requests, not a VJP.
Grid 32/64 has 256 paths; grids 2/4/8/16 have 1,024. Cross-grid latency is not equal-work speedup.

| Profile          | Paths/output | Outputs | 1T ms    | 2T ms    | 4T ms    | 8T ms    | 4T/8T speedup | 4T/8T efficiency | 4T paths/s |
|------------------|--------------|---------|----------|----------|----------|----------|---------------|------------------|------------|
| short-tree       | 8192         | 1       | 1.1302   | 0.6488   | 0.3778   | 0.3873   | 2.99/2.92     | 0.748/0.365      | 21684636   |
| short-compiled   | 8192         | 1       | 1.0126   | 0.5431   | 0.3302   | 0.3789   | 3.07/2.67     | 0.767/0.334      | 24812513   |
| short-passive    | 8192         | 1       | 0.3030   | 0.2034   | 0.1658   | 0.1979   | 1.83/1.53     | 0.457/0.191      | 49423831   |
| short-64-paths   | 64           | 1       | 0.0206   | 0.0426   | 0.0961   | 0.1176   | 0.21/0.17     | 0.054/0.022      | 665862     |
| short-256-paths  | 256          | 1       | 0.0449   | 0.0459   | 0.0678   | 0.1574   | 0.66/0.29     | 0.166/0.036      | 3778040    |
| short-4096-paths | 4096         | 1       | 0.5197   | 0.3252   | 0.2278   | 0.2023   | 2.28/2.57     | 0.570/0.321      | 17983606   |
| long-tree        | 2048         | 1       | 78.7095  | 39.9749  | 23.6431  | 15.7341  | 3.33/5.00     | 0.832/0.625      | 86622      |
| long-compiled    | 2048         | 1       | 59.8424  | 31.8712  | 18.1097  | 11.6025  | 3.30/5.16     | 0.826/0.645      | 113088     |
| long-passive     | 2048         | 1       | 15.3494  | 9.7642   | 6.2746   | 4.9030   | 2.45/3.13     | 0.612/0.391      | 326395     |
| lv-grid-2        | 1024         | 1       | 65.0703  | 35.3958  | 19.6068  | 12.6228  | 3.32/5.15     | 0.830/0.644      | 52227      |
| lv-grid-4        | 1024         | 1       | 66.5227  | 34.3731  | 18.9486  | 13.4946  | 3.51/4.93     | 0.878/0.616      | 54041      |
| lv-grid-8        | 1024         | 1       | 66.2978  | 36.5891  | 20.0748  | 11.8015  | 3.30/5.62     | 0.826/0.702      | 51009      |
| lv-grid-16       | 1024         | 1       | 68.4170  | 35.6039  | 20.6746  | 13.4483  | 3.31/5.09     | 0.827/0.636      | 49529      |
| lv-grid-32       | 256          | 1       | 19.6480  | 11.5724  | 8.0662   | 7.8831   | 2.44/2.49     | 0.609/0.312      | 31737      |
| lv-grid-64       | 256          | 1       | 25.1356  | 18.7983  | 14.8109  | 15.1289  | 1.70/1.66     | 0.424/0.208      | 17285      |
| lv-passive       | 1024         | 1       | 22.3535  | 12.3691  | 6.8449   | 4.9719   | 3.27/4.50     | 0.816/0.562      | 149600     |
| lsm-bs-tree      | 2048         | 1       | 55.0073  | 53.1442  | 52.1894  | 54.9654  | 1.05/1.00     | 0.263/0.125      | 39242      |
| lsm-bs-compiled  | 2048         | 1       | 51.2948  | 50.9796  | 50.9119  | 51.0847  | 1.01/1.00     | 0.252/0.126      | 40226      |
| lsm-bs-passive   | 2048         | 1       | 26.6877  | 26.3810  | 26.2732  | 25.9260  | 1.02/1.03     | 0.254/0.129      | 77950      |
| lsm-lv-daily     | 1024         | 1       | 282.2183 | 279.0367 | 278.3658 | 282.2286 | 1.01/1.00     | 0.253/0.125      | 3679       |
| lsm-lv-passive   | 1024         | 1       | 161.4770 | 156.2144 | 156.9417 | 164.6812 | 1.03/0.98     | 0.257/0.123      | 6525       |
| outputs-4        | 8192         | 4       | 4.1971   | 2.4047   | 1.6387   | 1.4970   | 2.56/2.80     | 0.640/0.350      | 19996070   |
| outputs-16       | 8192         | 16      | 16.7746  | 9.8844   | 6.1508   | 6.1403   | 2.73/2.73     | 0.682/0.341      | 21309624   |
| outputs-64       | 8192         | 64      | 66.5806  | 40.3275  | 25.7462  | 25.7045  | 2.59/2.59     | 0.647/0.324      | 20363678   |

## Diagnostic resources

One explicit phase sample at four threads per profile, separate from default-OFF timing.
RSS is process max including untimed validation; tape is SUM(per-thread MAX(capacity))
and is a proxy, not simultaneous total peak. CPU is the sum of actual available self thread CPU.
Selected arrays exclude private evaluator/model/solver/diagnostic storage.
REDUCE inclusive wall can overlap inner spans; phase and worker wall totals cannot be added to request latency.

| Profile          | Parameters | Events | Timeline samples | Diag wall ms | RSS MiB | Tape proxy MiB | Self CPU ms | Reduce ms | New blocks |
|------------------|------------|--------|------------------|--------------|---------|----------------|-------------|-----------|------------|
| short-tree       | 4          | 1      | 1                | 3.246        | 15.66   | 7.50           | 12.326      | 0.0038    | 0          |
| short-compiled   | 4          | 1      | 1                | 3.027        | 15.66   | 7.50           | 11.650      | 0.0049    | 0          |
| short-passive    | 4          | 1      | 1                | 2.084        | 15.73   | 0.00           | 6.803       | 0.0002    | 0          |
| short-64-paths   | 4          | 1      | 1                | 0.133        | 15.60   | 3.75           | 0.186       | 0.0007    | 0          |
| short-256-paths  | 4          | 1      | 1                | 0.293        | 15.57   | 7.50           | 0.646       | 0.0046    | 0          |
| short-4096-paths | 4          | 1      | 1                | 2.469        | 15.74   | 7.50           | 7.259       | 0.0048    | 0          |
| long-tree        | 4          | 366    | 366              | 28.144       | 17.94   | 7.50           | 97.847      | 0.0035    | 0          |
| long-compiled    | 4          | 366    | 366              | 21.369       | 18.17   | 7.50           | 73.133      | 0.0045    | 0          |
| long-passive     | 4          | 366    | 366              | 8.132        | 18.19   | 0.00           | 21.825      | 0.0003    | 0          |
| lv-grid-2        | 7          | 366    | 366              | 22.267       | 17.98   | 7.50           | 78.091      | 0.0066    | 0          |
| lv-grid-4        | 19         | 366    | 366              | 22.543       | 17.65   | 7.50           | 77.767      | 0.0086    | 0          |
| lv-grid-8        | 67         | 366    | 366              | 23.730       | 17.92   | 7.50           | 83.379      | 0.0236    | 0          |
| lv-grid-16       | 259        | 366    | 366              | 24.209       | 18.28   | 7.50           | 86.081      | 0.1010    | 0          |
| lv-grid-32       | 1027       | 366    | 366              | 9.055        | 20.17   | 7.50           | 24.623      | 0.4179    | 0          |
| lv-grid-64       | 4099       | 366    | 366              | 18.103       | 27.23   | 7.50           | 37.445      | 1.9924    | 0          |
| lv-passive       | 259        | 366    | 366              | 10.083       | 17.93   | 0.00           | 30.034      | 0.0003    | 0          |
| lsm-bs-tree      | 5          | 52     | 52               | 65.705       | 17.40   | 1.88           | 65.692      | 0.0015    | 0          |
| lsm-bs-compiled  | 5          | 52     | 52               | 64.934       | 17.31   | 1.88           | 64.921      | 0.0002    | 0          |
| lsm-bs-passive   | 5          | 52     | 52               | 38.948       | 15.55   | 0.00           | 38.846      | 0.0004    | 0          |
| lsm-lv-daily     | 20         | 359    | 359              | 326.733      | 35.67   | 1.88           | 326.696     | 0.0003    | 0          |
| lsm-lv-passive   | 20         | 359    | 359              | 235.156      | 32.53   | 0.00           | 235.128     | 0.0026    | 0          |
| outputs-4        | 4          | 1      | 1                | 13.949       | 15.60   | 7.50           | 51.385      | 0.0119    | 0          |
| outputs-16       | 4          | 1      | 1                | 65.915       | 15.46   | 7.50           | 226.851     | 0.0593    | 0          |
| outputs-64       | 4          | 1      | 1                | 244.460      | 16.59   | 7.50           | 863.779     | 0.3107    | 0          |

## Enabled unsampled and explicit diagnostic cost

Ten alternating paired processes per side and mode, four threads; min reduction.
ON/OFF below is a single-round diagnostic comparison, not the formal two-round regression gate.
Only explicit phases collect statistics. ON unsampled still executes compiled-in scope/TLS checks.
Positive/negative short-request differences can include code layout and host noise; no threshold is weakened.

| Profile          | ON unsampled cold | ON unsampled warm | Explicit phases vs OFF cold |
|------------------|-------------------|-------------------|-----------------------------|
| short-tree       | +5.09%            | +22.89%           | +689.31%                    |
| short-compiled   | +4.69%            | +15.14%           | +723.55%                    |
| short-passive    | +8.21%            | +15.69%           | +805.91%                    |
| short-64-paths   | +30.48%           | +0.86%            | +108.39%                    |
| short-256-paths  | +9.91%            | +6.55%            | +166.98%                    |
| short-4096-paths | +7.20%            | +49.26%           | +521.87%                    |
| long-tree        | +3.79%            | -7.51%            | +25.26%                     |
| long-compiled    | -3.99%            | -8.63%            | +27.12%                     |
| long-passive     | -0.62%            | +1.59%            | +28.98%                     |
| lv-grid-2        | +1.23%            | -2.51%            | +4.74%                      |
| lv-grid-4        | +5.47%            | +1.05%            | +8.60%                      |
| lv-grid-8        | +3.87%            | +6.96%            | +9.19%                      |
| lv-grid-16       | -4.77%            | -8.53%            | +11.74%                     |
| lv-grid-32       | +5.45%            | +1.59%            | +18.88%                     |
| lv-grid-64       | -4.34%            | -1.69%            | +4.17%                      |
| lv-passive       | -9.33%            | -12.42%           | +4.58%                      |
| lsm-bs-tree      | +3.97%            | +3.53%            | +18.68%                     |
| lsm-bs-compiled  | -0.85%            | -0.32%            | +30.23%                     |
| lsm-bs-passive   | +1.05%            | +3.56%            | +43.73%                     |
| lsm-lv-daily     | +0.50%            | +0.24%            | +7.54%                      |
| lsm-lv-passive   | +0.77%            | +2.43%            | +9.07%                      |
| outputs-4        | +6.28%            | +23.43%           | +815.72%                    |
| outputs-16       | +3.70%            | +18.45%           | +804.26%                    |
| outputs-64       | +14.02%           | +8.06%            | +851.73%                    |

## Consequences and open acceptance

- Long compiled paths scale from 59.842 ms at one thread to 11.603 ms at eight threads (5.16x).
- Short 64-path requests slow from 0.0206 ms at one thread to 0.0961 ms at four threads;
  task/initialization cost is material. Future optimization must preserve paths and reduction semantics.
- LSM BS compiled remains near 51 ms for 1/2/4/8 threads; current training/replay scheduling limits scaling.
- Grid 64 has 4,099 parameters; diagnostic reduction and setup are material, so extraction/workspace
  experiments should be measured separately from path-generation kernels.
- Explicit short-path phase instrumentation costs roughly an order of magnitude; it is an opt-in
  diagnostic and cannot be used as production throughput evidence.
- R01–R14 implementation/correctness, resources and scaling are supported; R15 exact current CI passes.
  R16 full supplemental default-OFF acceptance and final R17 reconciliation remain open.
  No P01/Stage A/full-plan completion is claimed and no PR merge is performed.

## Raw evidence

All paths below are relative to the persistent evidence root named above:

- `production-profiling-8886c083-environment-01.json`, `production-profiling-post-cleanup-identity-01.json`.
- `production-profiling-paired-01/`: formal samples/results/summary.
- `production-profiling-mc-paired-02/`, `production-profiling-mc-prior-control-01/`, `production-profiling-mc-confirmation-01/`.
- `production-profiling-mc-same-binary-control-01/`, `production-profiling-gsr-isolation-01/`.
- `production-profiling-gsr-code-comparison-01.json`, `production-profiling-gsr-object-restoration-01.json`.
- `production-profiling-curve-paired-01/`, `production-profiling-curve-prior-control-01/`, `production-profiling-curve-confirmation-01/`.
- `production-profiling-expanded-environment-01.json`, `production-profiling-expanded-measurements-01/`
  (profiles, all per-process commands, stdout/stderr, resources, scope/phase records, scaling tables).
- `production-profiling-post-cleanup-pr480-ci-01.json`: exact 35-check success audit.
- `production-prefix-map-build-protocol-01.json`, `production-prefix-map-measurement-environment-01.json`:
  frozen follow-up source/build/compiler/binary identities and sampling design.
- `production-prefix-map-mc-confirmation-01/`: all matched-prefix samples, numeric checks and failed results.
