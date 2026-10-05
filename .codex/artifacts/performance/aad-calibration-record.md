# F01 native curve record performance acceptance

Status: the incremental nine-target gate and default factory cost checks pass.
This accepts these measured workloads; common pullback implementation and exact
new-publication CI remain open. P01 production MC remains inconclusive.

## Inputs and environment

Baseline is the last accepted native publication
`533b6602f563dba78005fd9a63194b150d588af3`. The measured head is that parent plus native patch
SHA-256 `bbd83cefc78d8f7ab89ad04d001a45e477548cb781a824a89a5543565b5482f1`. The evidence manifest pins every
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
| tape_perf: Clear + re-record (100K nodes)                                            | 859601.00   | 856708.00   | -1.0878   | -0.3366   | No regression |
| tape_perf: PropagateToStart (100K nodes)                                             | 388563.00   | 388171.00   | -0.0229   | -0.1823   | No regression |
| tape_perf: PropagateToStart multi-mode (100K nodes, 10 results)                      | 484315.00   | 483480.00   | -0.1724   | +0.1737   | No regression |
| tape_perf: PropagateToStart multi-mode (50K steps, 1 result)                         | 381127.00   | 380254.00   | -0.9737   | +0.6998   | No regression |
| tape_perf: PropagateToStart multi-mode (50K steps, 16 results)                       | 638976.00   | 631142.00   | -0.7784   | -1.2260   | No regression |
| tape_perf: PropagateToStart multi-mode (50K steps, 4 results)                        | 380553.00   | 380695.00   | -0.7376   | +0.3784   | No regression |
| tape_perf: PropagateToStart multi-mode (50K steps, 64 results)                       | 6488000.00  | 6430000.00  | +0.2308   | -0.8940   | No regression |
| tape_perf: PropagateToStart passive constants (50K steps)                            | 147330.00   | 147471.00   | +0.0957   | +0.0271   | No regression |
| tape_perf: Rewind + passive-constant recording (50K steps)                           | 236346.00   | 236275.00   | +0.0317   | -0.0300   | No regression |
| tape_perf: Rewind + re-record (100K nodes)                                           | 566635.00   | 567545.00   | -0.1021   | +0.2903   | No regression |
| tape_perf: ZeroAdjoints sweep (100K nodes)                                           | 111218.00   | 111326.00   | +0.0971   | -0.0474   | No regression |
| jacobian_perf: AnalyticJacobian dense harvest (24 x 23)                              | 4709.00     | 4703.00     | +0.1274   | -0.1698   | No regression |
| jacobian_perf: AnalyticJacobian row-width harvest (24 x 23)                          | 4656.00     | 4667.00     | +0.2576   | +0.2363   | No regression |
| jacobian_perf: HarvestCurveJacobian dense (23 outputs x 24 parameters)               | 4321.00     | 4326.00     | +0.1157   | +0.3236   | No regression |
| jacobian_perf: HarvestCurveJacobian dense (95 outputs x 96 parameters)               | 62018.00    | 61993.00    | -0.0645   | +0.0113   | No regression |
| jacobian_perf: HarvestCurveJacobian proven prefix (23 outputs x 24 parameters)       | 3781.00     | 3777.00     | -0.1058   | +0.0264   | No regression |
| jacobian_perf: HarvestCurveJacobian proven prefix (95 outputs x 96 parameters)       | 58084.00    | 58115.00    | +0.0534   | -0.0825   | No regression |
| pde_perf: ThetaScheme rollback (200x200 CN)                                          | 222694.00   | 214748.00   | +0.0238   | -3.6521   | No regression |
| pde_perf: ThetaScheme rollback (200x200 implicit)                                    | 215161.00   | 214583.00   | +3.5220   | -2.9989   | No regression |
| pde_perf: ThetaScheme rollback (200x2000 explicit)                                   | 658735.00   | 661185.00   | +0.3901   | +0.3409   | No regression |
| rng_perf: BrownianBridge FillNormal (100K x 10D)                                     | 6329000.00  | 6318000.00  | -2.6559   | -0.1738   | No regression |
| rng_perf: IRN SkipNormalTo (100K x 10D)                                              | 7691000.00  | 7691000.00  | -0.1298   | +0.0000   | No regression |
| rng_perf: MRG32 SkipNormalTo (100K x 10D)                                            | 1292.00     | 1291.00     | -0.3089   | +0.0774   | No regression |
| rng_perf: MRG32k3a FillNormal (100K x 10D)                                           | 19260000.00 | 19206000.00 | -0.2737   | -0.2804   | No regression |
| rng_perf: ShuffledIRN FillNormal (100K x 10D)                                        | 10504000.00 | 10503000.00 | +0.0475   | -0.0095   | No regression |
| rng_perf: Sobol FillNormal fast (100K x 10D)                                         | 3985000.00  | 4004000.00  | -0.3732   | +0.5019   | No regression |
| rng_perf: Sobol FillNormal precise opt-in (100K x 10D)                               | 37408000.00 | 37227000.00 | +1.2324   | -0.7042   | No regression |
| rng_perf: Sobol FillUniform (100K x 10D)                                             | 752730.00   | 745317.00   | -0.8725   | -0.9848   | No regression |
| interp_perf: Cubic interp (50 knots, 10K queries)                                    | 50220.00    | 50196.00    | -0.1869   | -0.0478   | No regression |
| interp_perf: Inlined linear LV-style (1e5 paths x 200 steps, 200 knots)              | 97553000.00 | 97477000.00 | -0.0051   | -0.1598   | No regression |
| interp_perf: Linear interp (50 knots, 10K queries)                                   | 42172.00    | 42166.00    | +0.1873   | -0.0877   | No regression |
| krylov_perf: BCGSolve (500x500 tridiag)                                              | 60983.00    | 60968.00    | +0.0853   | -0.0459   | No regression |
| krylov_perf: CGSolve (500x500 tridiag)                                               | 51356.00    | 51636.00    | +0.2286   | +0.5452   | No regression |
| banded_perf: TriDecomp MultiplyLeft (10K)                                            | 4491.00     | 4493.00     | +0.0445   | +0.0445   | No regression |
| banded_perf: TriDiagonal Decompose (10K)                                             | 76730.00    | 76774.00    | +0.1447   | -0.1028   | No regression |
| banded_perf: TriDiagonal MultiplyLeft (10K)                                          | 4686.00     | 4685.00     | +0.0000   | -0.0213   | No regression |
| cholesky_perf: CholeskyDecompose (200x200)                                           | 225524.00   | 225319.00   | -0.0909   | -0.0168   | No regression |
| cholesky_perf: CholeskyDecompose+Multiply (200x200)                                  | 224987.00   | 224978.00   | -0.0378   | +0.0600   | No regression |
| rate_risk_perf: Quote risk aggregate (joint XCCY)                                    | 218650.00   | 221998.00   | +1.5312   | +4.4443   | No regression |
| rate_risk_perf: Quote risk aggregate (single curve)                                  | 8906.00     | 8918.00     | +0.1347   | -0.0324   | No regression |
| rate_risk_perf: Quote risk aggregate (staged XCCY basis)                             | 60191.00    | 60309.00    | +0.1960   | +2.7502   | No regression |
| rate_risk_perf: Quote risk generic joint (100 IRS x N=10)                            | 1121000.00  | 1123000.00  | +0.1784   | -2.0725   | No regression |
| rate_risk_perf: Quote risk generic joint (100 IRS x N=16)                            | 1698000.00  | 1698000.00  | +0.0000   | +0.7441   | No regression |
| rate_risk_perf: Quote risk generic joint (100 IRS x N=5)                             | 809793.00   | 807934.00   | -0.2296   | -0.8828   | No regression |
| rate_risk_perf: Quote risk generic joint (1000 IRS x N=10)                           | 10710000.00 | 10640000.00 | -0.6536   | -0.2897   | No regression |
| rate_risk_perf: Quote risk generic joint (1000 IRS x N=16)                           | 16562000.00 | 16569000.00 | +0.0423   | +0.4961   | No regression |
| rate_risk_perf: Quote risk generic joint (1000 IRS x N=5)                            | 7706000.00  | 7644000.00  | -0.8046   | +0.8657   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (100 IRS x N=10)             | 1063000.00  | 1059000.00  | -0.3763   | -3.2520   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (100 IRS x N=16)             | 1636000.00  | 1635000.00  | -0.0611   | +0.9074   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (100 IRS x N=5)              | 763213.00   | 759467.00   | -0.4908   | +1.8154   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (1000 IRS x N=10)            | 10518000.00 | 10541000.00 | +0.2187   | -2.7790   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (1000 IRS x N=16)            | 16358000.00 | 16410000.00 | +0.3179   | -0.5818   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (1000 IRS x N=5)             | 7543000.00  | 7501000.00  | -0.5568   | +0.5293   | No regression |
| rate_risk_perf: Quote risk portfolio joint ANALYTIC (24 XCCY x N=10/block)           | 4405000.00  | 4427000.00  | +0.4994   | +0.2455   | No regression |
| rate_risk_perf: Quote risk portfolio joint BUMPED (24 XCCY x N=10/block)             | 4429000.00  | 4432000.00  | +0.0677   | +1.1997   | No regression |
| rate_risk_perf: Quote risk portfolio single ANALYTIC (120 deposits x N=5)            | 121645.00   | 121923.00   | +0.2285   | +2.8951   | No regression |
| rate_risk_perf: Quote risk portfolio single BUMPED (120 deposits x N=16)             | 168497.00   | 169489.00   | +0.5887   | +0.4883   | No regression |
| rate_risk_perf: Quote risk portfolio staged ANALYTIC (24 XCCY x N=16)                | 3465000.00  | 3475000.00  | +0.2886   | -0.3619   | No regression |
| rate_risk_perf: Quote risk portfolio staged BUMPED (24 XCCY x N=5)                   | 1247000.00  | 1247000.00  | +0.0000   | +0.0772   | No regression |
| rate_risk_perf: Rate AAD 2 components (1024 IRS x 8 nodes, fixed maturities)         | 5018000.00  | 4981000.00  | -0.7373   | +0.4444   | No regression |
| rate_risk_perf: Rate AAD 2 components (256 IRS x 8 nodes, fixed maturities)          | 1238000.00  | 1235000.00  | -0.2423   | -0.6197   | No regression |
| rate_risk_perf: Rate AAD 2 components (32 IRS x 8 nodes, fixed maturities)           | 151469.00   | 150861.00   | -0.4014   | -1.7761   | No regression |
| rate_risk_perf: Rate OIS daily compounding sweep (5Y quarterly x daily)              | 209044.00   | 209457.00   | +0.1976   | -0.0478   | No regression |
| rate_risk_perf: Rate PV (1024 IRS x 8 nodes, fixed maturities)                       | 2440000.00  | 2419000.00  | -0.8607   | -4.0062   | No regression |
| rate_risk_perf: Rate PV (256 IRS x 8 nodes, fixed maturities)                        | 604071.00   | 603252.00   | -0.1356   | -0.4348   | No regression |
| rate_risk_perf: Rate PV (32 IRS x 8 nodes, fixed maturities)                         | 74737.00    | 74417.00    | -0.4282   | -0.7290   | No regression |
| rate_risk_perf: Rate XCCY batch serial (24 XCCY x 5 components)                      | 766593.00   | 764002.00   | -0.3380   | +0.1807   | No regression |
| rate_risk_perf: Rate batch serial (120 IRS x 2 components)                           | 1254000.00  | 1240000.00  | -1.1164   | +0.9456   | No regression |
| rate_risk_perf: Rate prepare geometry (256 IRS x 8 nodes, fixed maturities)          | 542147.00   | 533719.00   | -1.5546   | +0.7579   | No regression |
| rate_risk_perf: Rate prepare geometry (32 IRS x 8 nodes, fixed maturities)           | 64373.00    | 63986.00    | -0.6012   | -0.1618   | No regression |
| rate_risk_perf: Rate prepared AAD 2 components (256 IRS x 8 nodes, fixed maturities) | 706410.00   | 703507.00   | -0.4110   | +4.0733   | No regression |
| rate_risk_perf: Rate prepared AAD 2 components (32 IRS x 8 nodes, fixed maturities)  | 84080.00    | 84313.00    | +0.2771   | +0.3953   | No regression |
| rate_risk_perf: Rate prepared PV (256 IRS x 8 nodes, fixed maturities)               | 106514.00   | 105582.00   | -1.1747   | +2.4917   | No regression |
| rate_risk_perf: Rate prepared PV (32 IRS x 8 nodes, fixed maturities)                | 12751.00    | 12428.00    | -2.5331   | -0.8038   | No regression |
| rate_risk_perf: Rate single-trade sweeps (240 IRS calls)                             | 1957000.00  | 1970000.00  | +0.6643   | +0.1471   | No regression |

## Existing factory cost and record size

Eight cases cover single, joint-XCCY, staged-XCCY and generic joint, including
layered bases. These existing default calls keep record capture OFF. Base/head
columns below use first-round minima; both independent round deltas are retained.
All eight default cases pass the unchanged two-round 4% criterion.

| Case                                         | Base ns    | Head ns    | Round 1 % | Round 2 % | Capture bytes | Verdict       |
|----------------------------------------------|------------|------------|-----------|-----------|---------------|---------------|
| Record provenance single (8 quotes)          | 219469.00  | 221574.00  | +0.9591   | +1.2792   | 9275          | No regression |
| Record provenance joint (3x8 quotes)         | 2297000.00 | 2322000.00 | +1.0884   | +0.4413   | 115434        | No regression |
| Record provenance staged (8 quotes)          | 741478.00  | 734263.00  | -0.9731   | +1.5365   | 23665         | No regression |
| Record provenance single (16 quotes)         | 447761.00  | 442116.00  | -1.2607   | -0.9753   | 22719         | No regression |
| Record provenance joint (3x16 quotes)        | 5930000.00 | 5940000.00 | +0.1686   | +2.5738   | 368141        | No regression |
| Record provenance staged (16 quotes)         | 1313000.00 | 1324000.00 | +0.8378   | +0.6107   | 48810         | No regression |
| Record provenance generic plain (5 quotes)   | 249880.00  | 256648.00  | +2.7085   | -1.1982   | 9115          | No regression |
| Record provenance generic layered (5 quotes) | 304352.00  | 301437.00  | -0.9578   | -0.1175   | 9872          | No regression |

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
| Record provenance single (8 quotes)          | 223008.00  | 221783.00   | -0.5493   | -0.0592   |
| Record provenance joint (3x8 quotes)         | 2321000.00 | 2266000.00  | -2.3697   | -0.9206   |
| Record provenance staged (8 quotes)          | 741625.00  | 743765.00   | +0.2886   | +0.2765   |
| Record provenance single (16 quotes)         | 451192.00  | 445994.00   | -1.1521   | -0.0600   |
| Record provenance joint (3x16 quotes)        | 5863000.00 | 5919000.00  | +0.9551   | +0.4652   |
| Record provenance staged (16 quotes)         | 1299000.00 | 1299000.00  | +0.0000   | -0.3831   |
| Record provenance generic plain (5 quotes)   | 249525.00  | 249013.00   | -0.2052   | -0.4934   |
| Record provenance generic layered (5 quotes) | 299317.00  | 307922.00   | +2.8749   | -0.5338   |

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
  `aad-calibration-record-gate-inputs-01.json` and
  `aad-calibration-record-relinked-identity-01.json` pin the source/binary inputs.
- `aad-calibration-record-gate-paired-01/results.json` and `summary.md` retain
  every raw sample, both round deltas and the unchanged executable verdict.
- `aad-calibration-record-cost-pairs.py`, `aad-calibration-record-cost-source/`
  and `aad-calibration-record-cost-paired-01/` retain the complete supplementary
  consumer, protocol, eighty raw outputs and before/after manifests.
- `aad-calibration-record-installed-parity-01.json` records four consumer runs
  against old, new OFF/default, OFF/captured and combined/captured prefixes.

Verdict: no regression in the measured incremental default workloads under the
unchanged policy. This does not resolve P01 production MC throughput, measure
future common-result/binding overhead, or accept the entire F01/AAD plan. Keep
its original failed production rows and investigation requirements. The
[common specification](../specs/aad-calibration-pullback.md) still controls the
remaining implementation and acceptance.
