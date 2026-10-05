# F01 native curve record performance acceptance

Status: the incremental nine-target gate and default factory cost checks pass.
This accepts these measured workloads; exact native publication `a3833be9`
passes all 35 checks. Shared pullback acceptance remains open, and P01
production MC remains inconclusive.

## Inputs and environment

Baseline is the last accepted native publication
`533b6602f563dba78005fd9a63194b150d588af3`. The measured corrected head is
`a3833be93e64a2e427debbcf787fd99a7ff37c22`. Cumulative native patch relative to the
accepted baseline has SHA-256 `2ac992f0d5368d5602bc953bfb3949b3a117cae4837d3899ccdb775cd9485f3a`. The evidence manifest pins every
native/header, archive and executable input. Compiler: c++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0.
Release, native AAD, Eigen ON, native-architecture OFF, lifetime/profiling OFF,
static core/public; both factory consumers use equivalent CMake Release flags.
All processes use four DAL workers and CPU affinity 4 on the same shared WSL2
host: x86-64 Intel Core i9-13900HX, 32 visible logical CPUs, CMake 4.2.3,
Unix Makefiles. This host can be noisy; no failing or borderline row is discarded.

This is an incremental comparison with frozen outputs from the accepted build,
not a fresh clean-build comparison with master. The nine baseline build-tree
executables were frozen before any native edits and verified against the accepted
manifest. The head build was rebuilt and relinked before sampling. Eight gate
executables remain byte-identical; `rate_risk_perf` and the native archive change.
The older installed baseline has the same accepted core archive. Head consumers
use a separate fresh installed prefix, not stale stage binaries.

No build, correctness test, source edit or second benchmark runs during samples.
Every input hash is checked again afterwards. The script retains raw command
outputs, parsed durations, original complete workloads and all 75 comparable
cases. Both sides receive twenty alternating process samples: two independent
best-of-ten confirmation rounds, reported process minima, unchanged +4% threshold
in both rounds to fail. The Sobol precise/fast ratio rule is also unchanged.
Factory costs use the same two-by-ten policy, alternating first position, three
warmups and twenty internal calls per process. They are a separate acceptance
supplement, not an added executable in the nine-target allowlist.

## Nine-target results

All 75 comparable cases pass; no names disappear and no case lacks a baseline.
Base/head columns are overall best-of-twenty minima in ns; round columns retain
each independent comparison. A single round above 4% is not a two-round failure.

| Case                                                                                 | Base ns     | Head ns     | Round 1 % | Round 2 % | Verdict       |
|--------------------------------------------------------------------------------------|-------------|-------------|-----------|-----------|---------------|
| tape_perf: Clear + re-record (100K nodes)                                            | 848560.00   | 846010.00   | -0.3005   | +0.5067   | No regression |
| tape_perf: PropagateToStart (100K nodes)                                             | 385551.00   | 385065.00   | -0.2204   | -0.1261   | No regression |
| tape_perf: PropagateToStart multi-mode (100K nodes, 10 results)                      | 469508.00   | 475638.00   | +1.0109   | +1.3056   | No regression |
| tape_perf: PropagateToStart multi-mode (50K steps, 1 result)                         | 381512.00   | 380334.00   | +0.3320   | -0.3088   | No regression |
| tape_perf: PropagateToStart multi-mode (50K steps, 16 results)                       | 599375.00   | 592571.00   | -1.1352   | -0.5326   | No regression |
| tape_perf: PropagateToStart multi-mode (50K steps, 4 results)                        | 374813.00   | 379579.00   | +0.4278   | +1.5007   | No regression |
| tape_perf: PropagateToStart multi-mode (50K steps, 64 results)                       | 6166000.00  | 6204000.00  | +0.6476   | +0.6163   | No regression |
| tape_perf: PropagateToStart passive constants (50K steps)                            | 146906.00   | 146718.00   | +0.0768   | -0.1280   | No regression |
| tape_perf: Rewind + passive-constant recording (50K steps)                           | 236325.00   | 236269.00   | -0.0761   | -0.0127   | No regression |
| tape_perf: Rewind + re-record (100K nodes)                                           | 567162.00   | 566943.00   | -0.0386   | -0.0261   | No regression |
| tape_perf: ZeroAdjoints sweep (100K nodes)                                           | 111198.00   | 111013.00   | -0.2274   | -0.1043   | No regression |
| jacobian_perf: AnalyticJacobian dense harvest (24 x 23)                              | 4701.00     | 4701.00     | +0.0000   | +0.0000   | No regression |
| jacobian_perf: AnalyticJacobian row-width harvest (24 x 23)                          | 4660.00     | 4660.00     | +0.0000   | -0.0215   | No regression |
| jacobian_perf: HarvestCurveJacobian dense (23 outputs x 24 parameters)               | 4328.00     | 4326.00     | -0.0693   | +0.0462   | No regression |
| jacobian_perf: HarvestCurveJacobian dense (95 outputs x 96 parameters)               | 61876.00    | 61869.00    | +0.0081   | -0.0162   | No regression |
| jacobian_perf: HarvestCurveJacobian proven prefix (23 outputs x 24 parameters)       | 3775.00     | 3780.00     | +0.1589   | -0.0529   | No regression |
| jacobian_perf: HarvestCurveJacobian proven prefix (95 outputs x 96 parameters)       | 57934.00    | 57957.00    | +0.0397   | +0.0742   | No regression |
| pde_perf: ThetaScheme rollback (200x200 CN)                                          | 214150.00   | 214361.00   | +0.0985   | +0.0326   | No regression |
| pde_perf: ThetaScheme rollback (200x200 implicit)                                    | 214227.00   | 214150.00   | -0.0359   | +0.0853   | No regression |
| pde_perf: ThetaScheme rollback (200x2000 explicit)                                   | 657668.00   | 659047.00   | +0.3693   | +0.1118   | No regression |
| rng_perf: BrownianBridge FillNormal (100K x 10D)                                     | 6309000.00  | 6387000.00  | +2.9323   | +0.5985   | No regression |
| rng_perf: IRN SkipNormalTo (100K x 10D)                                              | 7688000.00  | 7686000.00  | +0.2598   | -0.0260   | No regression |
| rng_perf: MRG32 SkipNormalTo (100K x 10D)                                            | 1294.00     | 1294.00     | -0.2313   | +0.0000   | No regression |
| rng_perf: MRG32k3a FillNormal (100K x 10D)                                           | 19393000.00 | 19286000.00 | -0.3670   | -0.5517   | No regression |
| rng_perf: ShuffledIRN FillNormal (100K x 10D)                                        | 10510000.00 | 10490000.00 | +0.2658   | -0.1903   | No regression |
| rng_perf: Sobol FillNormal fast (100K x 10D)                                         | 4020000.00  | 4003000.00  | +1.9403   | -2.5085   | No regression |
| rng_perf: Sobol FillNormal precise opt-in (100K x 10D)                               | 37336000.00 | 37667000.00 | -0.0290   | +0.8865   | No regression |
| rng_perf: Sobol FillUniform (100K x 10D)                                             | 758970.00   | 778247.00   | +2.5399   | +3.0774   | No regression |
| interp_perf: Cubic interp (50 knots, 10K queries)                                    | 50276.00    | 50267.00    | +0.0477   | -0.0179   | No regression |
| interp_perf: Inlined linear LV-style (1e5 paths x 200 steps, 200 knots)              | 97327000.00 | 97355000.00 | +0.7369   | +0.0288   | No regression |
| interp_perf: Linear interp (50 knots, 10K queries)                                   | 42238.00    | 42210.00    | -0.0663   | +0.1065   | No regression |
| krylov_perf: BCGSolve (500x500 tridiag)                                              | 60912.00    | 60981.00    | +0.0016   | +0.2380   | No regression |
| krylov_perf: CGSolve (500x500 tridiag)                                               | 51525.00    | 51721.00    | +0.3804   | +0.4539   | No regression |
| banded_perf: TriDecomp MultiplyLeft (10K)                                            | 4492.00     | 4491.00     | +0.0223   | -0.0223   | No regression |
| banded_perf: TriDiagonal Decompose (10K)                                             | 76682.00    | 76602.00    | +0.0248   | -0.1043   | No regression |
| banded_perf: TriDiagonal MultiplyLeft (10K)                                          | 4685.00     | 4685.00     | +0.0213   | +0.0000   | No regression |
| cholesky_perf: CholeskyDecompose (200x200)                                           | 225485.00   | 225500.00   | -0.0160   | +0.0067   | No regression |
| cholesky_perf: CholeskyDecompose+Multiply (200x200)                                  | 225061.00   | 224903.00   | -0.1350   | -0.0338   | No regression |
| rate_risk_perf: Quote risk aggregate (joint XCCY)                                    | 219598.00   | 220124.00   | -0.3450   | +0.3934   | No regression |
| rate_risk_perf: Quote risk aggregate (single curve)                                  | 8891.00     | 8863.00     | -0.1798   | -0.3149   | No regression |
| rate_risk_perf: Quote risk aggregate (staged XCCY basis)                             | 60197.00    | 60175.00    | -0.4615   | +0.0083   | No regression |
| rate_risk_perf: Quote risk generic joint (100 IRS x N=10)                            | 1120000.00  | 1121000.00  | +0.3571   | -0.1781   | No regression |
| rate_risk_perf: Quote risk generic joint (100 IRS x N=16)                            | 1687000.00  | 1697000.00  | +0.8892   | +0.0590   | No regression |
| rate_risk_perf: Quote risk generic joint (100 IRS x N=5)                             | 807005.00   | 809493.00   | +0.6570   | +0.2279   | No regression |
| rate_risk_perf: Quote risk generic joint (1000 IRS x N=10)                           | 10670000.00 | 10651000.00 | -0.7455   | +0.4686   | No regression |
| rate_risk_perf: Quote risk generic joint (1000 IRS x N=16)                           | 16459000.00 | 16497000.00 | -0.1755   | +1.4156   | No regression |
| rate_risk_perf: Quote risk generic joint (1000 IRS x N=5)                            | 7725000.00  | 7675000.00  | -0.9166   | -0.5049   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (100 IRS x N=10)             | 1064000.00  | 1063000.00  | -0.0939   | -0.0940   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (100 IRS x N=16)             | 1624000.00  | 1627000.00  | -0.0614   | +0.3079   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (100 IRS x N=5)              | 760943.00   | 764000.00   | +0.4017   | +0.4094   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (1000 IRS x N=10)            | 10496000.00 | 10498000.00 | +0.0191   | +0.9425   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (1000 IRS x N=16)            | 16348000.00 | 16451000.00 | +0.6300   | +0.5301   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (1000 IRS x N=5)             | 7576000.00  | 7571000.00  | -0.0660   | -0.5386   | No regression |
| rate_risk_perf: Quote risk portfolio joint ANALYTIC (24 XCCY x N=10/block)           | 4405000.00  | 4402000.00  | -0.0681   | +0.1585   | No regression |
| rate_risk_perf: Quote risk portfolio joint BUMPED (24 XCCY x N=10/block)             | 4412000.00  | 4423000.00  | +0.2493   | +0.0226   | No regression |
| rate_risk_perf: Quote risk portfolio single ANALYTIC (120 deposits x N=5)            | 121237.00   | 121772.00   | +0.0025   | +0.4413   | No regression |
| rate_risk_perf: Quote risk portfolio single BUMPED (120 deposits x N=16)             | 168767.00   | 168632.00   | +0.0503   | -0.0800   | No regression |
| rate_risk_perf: Quote risk portfolio staged ANALYTIC (24 XCCY x N=16)                | 3457000.00  | 3474000.00  | +0.6075   | -0.1437   | No regression |
| rate_risk_perf: Quote risk portfolio staged BUMPED (24 XCCY x N=5)                   | 1243000.00  | 1247000.00  | +0.3218   | +0.1603   | No regression |
| rate_risk_perf: Rate AAD 2 components (1024 IRS x 8 nodes, fixed maturities)         | 4987000.00  | 4977000.00  | +0.4211   | -0.8171   | No regression |
| rate_risk_perf: Rate AAD 2 components (256 IRS x 8 nodes, fixed maturities)          | 1242000.00  | 1247000.00  | +0.4831   | +0.4026   | No regression |
| rate_risk_perf: Rate AAD 2 components (32 IRS x 8 nodes, fixed maturities)           | 151186.00   | 150281.00   | -1.0417   | -0.3975   | No regression |
| rate_risk_perf: Rate OIS daily compounding sweep (5Y quarterly x daily)              | 209119.00   | 209246.00   | +0.0727   | -0.0554   | No regression |
| rate_risk_perf: Rate PV (1024 IRS x 8 nodes, fixed maturities)                       | 2430000.00  | 2434000.00  | +0.5350   | -0.8958   | No regression |
| rate_risk_perf: Rate PV (256 IRS x 8 nodes, fixed maturities)                        | 603107.00   | 604245.00   | +0.4943   | -0.7653   | No regression |
| rate_risk_perf: Rate PV (32 IRS x 8 nodes, fixed maturities)                         | 74540.00    | 74408.00    | -0.0362   | -0.1771   | No regression |
| rate_risk_perf: Rate XCCY batch serial (24 XCCY x 5 components)                      | 764027.00   | 765250.00   | -0.1571   | +0.1840   | No regression |
| rate_risk_perf: Rate batch serial (120 IRS x 2 components)                           | 1246000.00  | 1248000.00  | +0.3210   | +0.0000   | No regression |
| rate_risk_perf: Rate prepare geometry (256 IRS x 8 nodes, fixed maturities)          | 543371.00   | 536050.00   | -1.5175   | -1.1311   | No regression |
| rate_risk_perf: Rate prepare geometry (32 IRS x 8 nodes, fixed maturities)           | 63871.00    | 64234.00    | +0.2059   | +0.9488   | No regression |
| rate_risk_perf: Rate prepared AAD 2 components (256 IRS x 8 nodes, fixed maturities) | 702288.00   | 706088.00   | +0.5686   | +0.0575   | No regression |
| rate_risk_perf: Rate prepared AAD 2 components (32 IRS x 8 nodes, fixed maturities)  | 84200.00    | 84648.00    | -0.4352   | +0.7423   | No regression |
| rate_risk_perf: Rate prepared PV (256 IRS x 8 nodes, fixed maturities)               | 106515.00   | 105756.00   | -0.7126   | -0.8436   | No regression |
| rate_risk_perf: Rate prepared PV (32 IRS x 8 nodes, fixed maturities)                | 12652.00    | 12606.00    | -1.2379   | -0.2924   | No regression |
| rate_risk_perf: Rate single-trade sweeps (240 IRS calls)                             | 1963000.00  | 1966000.00  | +0.3566   | +0.1018   | No regression |

## Existing factory cost and record size

Eight cases cover single, joint-XCCY, staged-XCCY and generic joint, including
layered bases. These existing default calls keep record capture OFF. Base/head
columns below use first-round minima; both independent round deltas are retained.
All eight default cases pass the unchanged two-round 4% criterion.

| Case                                         | Base ns    | Head ns    | Round 1 % | Round 2 % | Capture bytes | Verdict       |
|----------------------------------------------|------------|------------|-----------|-----------|---------------|---------------|
| Record provenance single (8 quotes)          | 220292.00  | 221940.00  | +0.7481   | +0.8731   | 9275          | No regression |
| Record provenance joint (3x8 quotes)         | 2256000.00 | 2244000.00 | -0.5319   | -1.3687   | 115434        | No regression |
| Record provenance staged (8 quotes)          | 727396.00  | 729074.00  | +0.2307   | +0.1306   | 23665         | No regression |
| Record provenance single (16 quotes)         | 441501.00  | 442253.00  | +0.1703   | +0.8545   | 22719         | No regression |
| Record provenance joint (3x16 quotes)        | 5811000.00 | 5758000.00 | -0.9121   | +0.6608   | 368141        | No regression |
| Record provenance staged (16 quotes)         | 1290000.00 | 1295000.00 | +0.3876   | +0.6197   | 48810         | No regression |
| Record provenance generic plain (5 quotes)   | 245008.00  | 247323.00  | +0.9449   | -0.1666   | 9115          | No regression |
| Record provenance generic layered (5 quotes) | 295283.00  | 298403.00  | +1.0566   | +1.0182   | 9872          | No regression |

Capture retains 9,115–368,141 canonical bytes in these cases. It moves the already
canonicalized state string into immutable ownership; it does not canonicalize
again or retain a second JSON tree. Default provenance adds one ownership slot
and retains no canonical text. Captured storage is proportional to the original
record, including serialized inverse data.

## Opt-in cost, informational

Compare captured and default calls from the same new installed library. Negative
small movements are timing variation, not evidence that retaining data improves
construction. These rows describe new-entry cost; the legacy regression verdict
uses the separate old-versus-head default comparison above.

| Case                                         | Default ns | Captured ns | Round 1 % | Round 2 % |
|----------------------------------------------|------------|-------------|-----------|-----------|
| Record provenance single (8 quotes)          | 221473.00  | 221691.00   | +0.0984   | -0.9458   |
| Record provenance joint (3x8 quotes)         | 2250000.00 | 2255000.00  | +0.2222   | +0.6233   |
| Record provenance staged (8 quotes)          | 728822.00  | 734363.00   | +0.7603   | +0.2781   |
| Record provenance single (16 quotes)         | 440520.00  | 439975.00   | -0.1237   | +0.1776   |
| Record provenance joint (3x16 quotes)        | 5797000.00 | 5789000.00  | -0.1380   | +0.4839   |
| Record provenance staged (16 quotes)         | 1296000.00 | 1295000.00  | -0.0772   | +0.0773   |
| Record provenance generic plain (5 quotes)   | 248114.00  | 247026.00   | -0.4385   | +0.6006   |
| Record provenance generic layered (5 quotes) | 297756.00  | 298044.00   | +0.0967   | -0.9080   |

All 80 factory process runs retain identical state/axis fingerprints, inverse
checksums and tolerance across old/default/captured modes. Record sizes are zero
for every default case and nonzero only for captured cases. Both consumer source
files, shared fixtures, installed headers/libraries and executables retain their
input hashes. Initial consumer compiles miss the public curvespec include;
keep the failed logs, add that declaration, then compile both consumers. A clean
old-header RED fails only because the capture flag/getter do not exist.

## Evidence and limits

Raw evidence persists under
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`:

- `aad-calibration-record-baseline-01.json`,
  `aad-calibration-record-publication-gate-inputs-02.json` and
  `aad-calibration-record-publication-identity-01.json` pin the source/binary inputs.
- `aad-calibration-record-gate-paired-02/results.json` and `summary.md` retain
  every raw sample, both round deltas and the unchanged executable verdict.
- `aad-calibration-record-cost-pairs-02.py`, `aad-calibration-record-cost-source/`
  and `aad-calibration-record-cost-paired-02/` retain the complete supplementary
  consumer, protocol, eighty raw outputs and before/after manifests.
- `aad-calibration-record-publication-installed-parity-01.json` records four consumer runs
  against old, new OFF/default, OFF/captured and combined/captured prefixes.

Verdict: no regression in the measured incremental default workloads under the
unchanged policy. This does not resolve P01 production MC throughput, measure
future common-result/binding overhead, or accept the entire F01/AAD plan. Keep
its original failed production rows and investigation requirements. The
[common specification](../specs/aad-calibration-pullback.md) still controls the
remaining implementation and acceptance.

The initial paired evidence remains under the `-01` paths and in the
`28b4e180` Git snapshot. It did not accept that failed publication. This
report uses fresh `-02` gate/cost runs and fresh installed prefixes after the
const-reference correction. Every corrected input retains its measured hash;
all 75 gate cases, eight default costs and eighty numeric process checks pass
again. All 35 exact corrective publication checks pass; the complete capture
is `aad-calibration-record-correction-ci-11.jsonl`. Subsequent shared-operation
source requires its own performance and publication checks.
