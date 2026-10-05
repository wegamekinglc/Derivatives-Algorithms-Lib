# F01 common C++ calibration performance acceptance

Status: incremental performance and all 35 exact `12b3d7d` CI checks accepted
(`aad-calibration-pullback-ci-05.jsonl`).
Full F01 and P01 production MC acceptance remain open.

## Inputs and protocol

Baseline is accepted native record capture
`a3833be93e64a2e427debbcf787fd99a7ff37c22`. The head is its uncommitted
common C++ increment, frozen in `aad-calibration-pullback-gate-inputs-01.json`.
The recorded tracked-patch hash is
`bd7f66873fc756ad2d0b7c7f8c1d75928a3156865c0891296ad53bad75cc8706`;
new production files are covered by the separate 654-file source manifest.
Subsequent acceptance-document edits do not change these compiled inputs.

Writable source:
`/home/wegamekinglc/.cache/dal-aad-workspaces-20261004/head-source`
(`/tmp/dal-aad-backend-adapter`). The prior accepted build is copied before
editing into `aad-calibration-pullback-baseline-01`; its eleven binary/archive
hashes match the accepted native-capture publication inputs. Freshly rebuilt
head binaries come from `aad-risk-results-off-build`.
Isolated measured copies are `aad-calibration-pullback-gate-base-01` and
`aad-calibration-pullback-gate-head-01`, using the original build-tree layout.
Neither side is an old installed stage binary.

This is a bounded incremental comparison against accepted capture, using
retained baseline outputs and an incrementally rebuilt head. It is not a fresh
merge-base/master build or the final whole-plan performance audit. Eight of
nine executables are byte-identical; rate_risk_perf exercises the changed
aggregation helper. The provenance-constructor object is byte-identical:
`924ce1213e79c8a85af960304c33b89fbf3889240a3b57408666ad245faff9a9`.
Construction has no new default-path work; the accepted eight native-capture
factory comparisons remain separately scoped in the
[record report](aad-calibration-record.md).

Host: WSL2 Linux 5.15.167.4, x86-64 Intel i9-13900HX, 32 visible logical CPUs.
GCC 15.2.0, CMake 4.2.3, Unix Makefiles, C++17 Release `-O3 -DNDEBUG -fPIC`,
Eigen ON, native architecture OFF, native AAD, lifetime/profiling OFF,
benchmarks explicitly ON. Pin every process to CPU 4 and set DAL_NUM_THREADS=4.
No other local build, correctness test or benchmark runs during measurement;
no production/benchmark/Git/PR mutation occurs. WSL2 remains a shared noisy host.

Dependency pointers remain:
Eigen `3147391d946bb4b6c68edd901f2add6ac1f31f8c`,
GoogleTest `d72f9c8aea6817cdf1ca0ac10887f328de7f3da2`,
Machinist `e76b0ef243ca9bd506bb2f3743d8c9b52ee8d01d`,
pybind11 `8a099e44b3d5f85b20f05828d919d2332a8de841`,
RapidJSON `ea152ded01708ff1657b275ed0bcbf0681d64519`.

Correctness precedes timing: full OFF/combined pass 2,412/2,426;
fully instrumented ASan/UBSan passes 186 relevant cases plus twelve
leak-enabled common cases. Installed consumers and actual six-unit/mode
MSVC syntax checks pass. These do not replace remote runtime CI.

The unchanged executable gate runs two independent rounds of ten interleaved
processes per side per target (360 total), alternating the first side.
Reduce by min. A comparable case fails only above +4% in both rounds.
Preserve all nine targets and the precise Sobol ratio ceiling of ten.
All 677 measured source/configuration/library/executable inputs retain their
hashes afterward.

## Complete nine-target results

All 75 comparable cases pass. Base/head and combined columns use minima over
both rounds; independent round movements remain visible.

| Case                                                                                 | Base ns     | Head ns     | Combined % | Round 1 % | Round 2 % | Verdict       |
|--------------------------------------------------------------------------------------|-------------|-------------|------------|-----------|-----------|---------------|
| tape_perf: Clear + re-record (100K nodes)                                            | 853078.00   | 845580.00   | -0.8789    | -1.9811   | 1.5938    | No regression |
| tape_perf: PropagateToStart (100K nodes)                                             | 386250.00   | 386787.00   | 0.1390     | 0.0553    | 0.1390    | No regression |
| tape_perf: PropagateToStart multi-mode (100K nodes, 10 results)                      | 481861.00   | 484931.00   | 0.6371     | 0.9409    | -0.0190   | No regression |
| tape_perf: PropagateToStart multi-mode (50K steps, 1 result)                         | 379190.00   | 381122.00   | 0.5095     | 0.1489    | 0.5095    | No regression |
| tape_perf: PropagateToStart multi-mode (50K steps, 16 results)                       | 622394.00   | 625292.00   | 0.4656     | 0.4656    | 0.0889    | No regression |
| tape_perf: PropagateToStart multi-mode (50K steps, 4 results)                        | 381486.00   | 381095.00   | -0.1025    | -0.1394   | 0.4629    | No regression |
| tape_perf: PropagateToStart multi-mode (50K steps, 64 results)                       | 6366000.00  | 6400000.00  | 0.5341     | -0.3979   | 0.5341    | No regression |
| tape_perf: PropagateToStart passive constants (50K steps)                            | 147457.00   | 147451.00   | -0.0041    | -0.4785   | -0.0041   | No regression |
| tape_perf: Rewind + passive-constant recording (50K steps)                           | 236162.00   | 236314.00   | 0.0644     | 0.0512    | 0.0644    | No regression |
| tape_perf: Rewind + re-record (100K nodes)                                           | 566950.00   | 566955.00   | 0.0009     | 0.0009    | 0.0337    | No regression |
| tape_perf: ZeroAdjoints sweep (100K nodes)                                           | 111443.00   | 108424.00   | -2.7090    | -2.7090   | -0.4339   | No regression |
| jacobian_perf: AnalyticJacobian dense harvest (24 x 23)                              | 4698.00     | 4702.00     | 0.0851     | 0.0000    | 0.0851    | No regression |
| jacobian_perf: AnalyticJacobian row-width harvest (24 x 23)                          | 4659.00     | 4658.00     | -0.0215    | -0.0215   | -0.0644   | No regression |
| jacobian_perf: HarvestCurveJacobian dense (23 outputs x 24 parameters)               | 4318.00     | 4322.00     | 0.0926     | 0.3011    | -0.0231   | No regression |
| jacobian_perf: HarvestCurveJacobian dense (95 outputs x 96 parameters)               | 61879.00    | 61862.00    | -0.0275    | -0.0275   | -0.0307   | No regression |
| jacobian_perf: HarvestCurveJacobian proven prefix (23 outputs x 24 parameters)       | 3773.00     | 3780.00     | 0.1855     | 0.2915    | -0.0793   | No regression |
| jacobian_perf: HarvestCurveJacobian proven prefix (95 outputs x 96 parameters)       | 57939.00    | 57990.00    | 0.0880     | 0.0880    | 0.0293    | No regression |
| pde_perf: ThetaScheme rollback (200x200 CN)                                          | 214240.00   | 214014.00   | -0.1055    | -0.1055   | -0.0238   | No regression |
| pde_perf: ThetaScheme rollback (200x200 implicit)                                    | 214310.00   | 214295.00   | -0.0070    | -0.0070   | -0.0532   | No regression |
| pde_perf: ThetaScheme rollback (200x2000 explicit)                                   | 654144.00   | 656436.00   | 0.3504     | 0.3504    | -0.0202   | No regression |
| rng_perf: BrownianBridge FillNormal (100K x 10D)                                     | 6465000.00  | 6328000.00  | -2.1191    | -4.8850   | -1.6396   | No regression |
| rng_perf: IRN SkipNormalTo (100K x 10D)                                              | 7698000.00  | 7699000.00  | 0.0130     | 0.0130    | 0.1038    | No regression |
| rng_perf: MRG32 SkipNormalTo (100K x 10D)                                            | 1294.00     | 1293.00     | -0.0773    | 0.0000    | -0.0773   | No regression |
| rng_perf: MRG32k3a FillNormal (100K x 10D)                                           | 19320000.00 | 19350000.00 | 0.1553     | 0.1655    | 0.1553    | No regression |
| rng_perf: ShuffledIRN FillNormal (100K x 10D)                                        | 10529000.00 | 10484000.00 | -0.4274    | -0.4274   | -0.1612   | No regression |
| rng_perf: Sobol FillNormal fast (100K x 10D)                                         | 3978000.00  | 4011000.00  | 0.8296     | 2.1116    | 0.4256    | No regression |
| rng_perf: Sobol FillNormal precise opt-in (100K x 10D)                               | 37279000.00 | 37104000.00 | -0.4694    | -1.2866   | -0.4694   | No regression |
| rng_perf: Sobol FillUniform (100K x 10D)                                             | 763661.00   | 749322.00   | -1.8777    | -5.2452   | 1.3533    | No regression |
| interp_perf: Cubic interp (50 knots, 10K queries)                                    | 50170.00    | 50212.00    | 0.0837     | 0.0837    | 0.0617    | No regression |
| interp_perf: Inlined linear LV-style (1e5 paths x 200 steps, 200 knots)              | 97296000.00 | 97794000.00 | 0.5118     | -0.2652   | 0.5786    | No regression |
| interp_perf: Linear interp (50 knots, 10K queries)                                   | 42113.00    | 42110.00    | -0.0071    | -0.0404   | -0.0071   | No regression |
| krylov_perf: BCGSolve (500x500 tridiag)                                              | 60856.00    | 60959.00    | 0.1693     | 0.1693    | 0.2903    | No regression |
| krylov_perf: CGSolve (500x500 tridiag)                                               | 51548.00    | 51331.00    | -0.4210    | -0.4210   | -0.3188   | No regression |
| banded_perf: TriDecomp MultiplyLeft (10K)                                            | 4494.00     | 4496.00     | 0.0445     | 0.0222    | 0.6676    | No regression |
| banded_perf: TriDiagonal Decompose (10K)                                             | 76917.00    | 76767.00    | -0.1950    | -0.1950   | 1.0335    | No regression |
| banded_perf: TriDiagonal MultiplyLeft (10K)                                          | 4690.00     | 4687.00     | -0.0640    | -0.1917   | 1.6205    | No regression |
| cholesky_perf: CholeskyDecompose (200x200)                                           | 225533.00   | 225853.00   | 0.1419     | 0.1419    | 0.0558    | No regression |
| cholesky_perf: CholeskyDecompose+Multiply (200x200)                                  | 225180.00   | 225044.00   | -0.0604    | -0.0604   | -0.0062   | No regression |
| rate_risk_perf: Quote risk aggregate (joint XCCY)                                    | 221339.00   | 220073.00   | -0.5720    | 0.9317    | -0.5720   | No regression |
| rate_risk_perf: Quote risk aggregate (single curve)                                  | 8854.00     | 8844.00     | -0.1129    | -0.4054   | 0.2824    | No regression |
| rate_risk_perf: Quote risk aggregate (staged XCCY basis)                             | 60190.00    | 60119.00    | -0.1180    | 0.3000    | -0.1180   | No regression |
| rate_risk_perf: Quote risk generic joint (100 IRS x N=10)                            | 1118000.00  | 1119000.00  | 0.0894     | 0.7073    | 0.0894    | No regression |
| rate_risk_perf: Quote risk generic joint (100 IRS x N=16)                            | 1702000.00  | 1703000.00  | 0.0588     | 0.5282    | 0.0588    | No regression |
| rate_risk_perf: Quote risk generic joint (100 IRS x N=5)                             | 805392.00   | 804696.00   | -0.0864    | 2.1628    | -0.0864   | No regression |
| rate_risk_perf: Quote risk generic joint (1000 IRS x N=10)                           | 10647000.00 | 10759000.00 | 1.0519     | 0.0279    | 1.1553    | No regression |
| rate_risk_perf: Quote risk generic joint (1000 IRS x N=16)                           | 16512000.00 | 16602000.00 | 0.5451     | 0.4532    | 0.5451    | No regression |
| rate_risk_perf: Quote risk generic joint (1000 IRS x N=5)                            | 7721000.00  | 7685000.00  | -0.4663    | -1.6977   | -0.4663   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (100 IRS x N=10)             | 1061000.00  | 1063000.00  | 0.1885     | 0.2825    | 0.1885    | No regression |
| rate_risk_perf: Quote risk generic joint node reference (100 IRS x N=16)             | 1636000.00  | 1637000.00  | 0.0611     | 0.7326    | 0.0611    | No regression |
| rate_risk_perf: Quote risk generic joint node reference (100 IRS x N=5)              | 761066.00   | 759355.00   | -0.2248    | 2.9643    | -0.2484   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (1000 IRS x N=10)            | 10589000.00 | 10523000.00 | -0.6233    | 0.0940    | -0.6233   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (1000 IRS x N=16)            | 16334000.00 | 16319000.00 | -0.0918    | -0.8211   | -0.0918   | No regression |
| rate_risk_perf: Quote risk generic joint node reference (1000 IRS x N=5)             | 7558000.00  | 7560000.00  | 0.0265     | -1.1400   | 0.0265    | No regression |
| rate_risk_perf: Quote risk portfolio joint ANALYTIC (24 XCCY x N=10/block)           | 4402000.00  | 4424000.00  | 0.4998     | -0.4234   | 0.4998    | No regression |
| rate_risk_perf: Quote risk portfolio joint BUMPED (24 XCCY x N=10/block)             | 4425000.00  | 4422000.00  | -0.0678    | 0.4044    | -0.0678   | No regression |
| rate_risk_perf: Quote risk portfolio single ANALYTIC (120 deposits x N=5)            | 121611.00   | 121278.00   | -0.2738    | -0.1227   | -0.2738   | No regression |
| rate_risk_perf: Quote risk portfolio single BUMPED (120 deposits x N=16)             | 169375.00   | 169130.00   | -0.1446    | 1.0021    | -0.1446   | No regression |
| rate_risk_perf: Quote risk portfolio staged ANALYTIC (24 XCCY x N=16)                | 3452000.00  | 3462000.00  | 0.2897     | -0.9130   | 0.2897    | No regression |
| rate_risk_perf: Quote risk portfolio staged BUMPED (24 XCCY x N=5)                   | 1247000.00  | 1246000.00  | -0.0802    | 0.9585    | -0.0802   | No regression |
| rate_risk_perf: Rate AAD 2 components (1024 IRS x 8 nodes, fixed maturities)         | 5034000.00  | 4986000.00  | -0.9535    | -2.4230   | -0.9535   | No regression |
| rate_risk_perf: Rate AAD 2 components (256 IRS x 8 nodes, fixed maturities)          | 1244000.00  | 1242000.00  | -0.1608    | 1.5273    | -0.3210   | No regression |
| rate_risk_perf: Rate AAD 2 components (32 IRS x 8 nodes, fixed maturities)           | 150288.00   | 150989.00   | 0.4664     | -0.1351   | 0.4664    | No regression |
| rate_risk_perf: Rate OIS daily compounding sweep (5Y quarterly x daily)              | 209210.00   | 209552.00   | 0.1635     | 0.0940    | 0.1635    | No regression |
| rate_risk_perf: Rate PV (1024 IRS x 8 nodes, fixed maturities)                       | 2446000.00  | 2435000.00  | -0.4497    | 3.1071    | -1.1769   | No regression |
| rate_risk_perf: Rate PV (256 IRS x 8 nodes, fixed maturities)                        | 604436.00   | 605536.00   | 0.1820     | 0.5906    | 0.1820    | No regression |
| rate_risk_perf: Rate PV (32 IRS x 8 nodes, fixed maturities)                         | 74651.00    | 74897.00    | 0.3295     | -0.1679   | 0.6604    | No regression |
| rate_risk_perf: Rate XCCY batch serial (24 XCCY x 5 components)                      | 766891.00   | 765323.00   | -0.2045    | 0.0487    | -0.2045   | No regression |
| rate_risk_perf: Rate batch serial (120 IRS x 2 components)                           | 1247000.00  | 1253000.00  | 0.4812     | -0.5556   | 0.8019    | No regression |
| rate_risk_perf: Rate prepare geometry (256 IRS x 8 nodes, fixed maturities)          | 536121.00   | 535774.00   | -0.0647    | -0.6236   | 0.2216    | No regression |
| rate_risk_perf: Rate prepare geometry (32 IRS x 8 nodes, fixed maturities)           | 64523.00    | 64276.00    | -0.3828    | -0.4456   | -0.3828   | No regression |
| rate_risk_perf: Rate prepared AAD 2 components (256 IRS x 8 nodes, fixed maturities) | 704963.00   | 706560.00   | 0.2265     | 1.7183    | 0.2265    | No regression |
| rate_risk_perf: Rate prepared AAD 2 components (32 IRS x 8 nodes, fixed maturities)  | 84807.00    | 84523.00    | -0.3349    | 1.1834    | -0.3349   | No regression |
| rate_risk_perf: Rate prepared PV (256 IRS x 8 nodes, fixed maturities)               | 105113.00   | 106363.00   | 1.1892     | 0.2867    | 1.1892    | No regression |
| rate_risk_perf: Rate prepared PV (32 IRS x 8 nodes, fixed maturities)                | 12652.00    | 12693.00    | 0.3241     | 1.1698    | 0.1815    | No regression |
| rate_risk_perf: Rate single-trade sweeps (240 IRS calls)                             | 1972000.00  | 1972000.00  | 0.0000     | -0.2518   | 0.0000    | No regression |

Precise-opt-in/fast Sobol ratio is 9.25x, within the unchanged 10x ceiling.
No cases are removed, renamed or silently excluded.

## Informational common-entry cost

A separate installed DAL::public consumer uses prepared captured boundaries
and owning parameter/direct seeds. Time PullbackCalibration and passive result
construction only: no calibration, record capture, boundary construction,
seed construction, repricing, worker submission or numeric projection.
Dupire still performs its own independent reverse; curves use the retained
inverse. Three internal warmups precede twenty internal repetitions.

Two rounds of ten fresh processes (twenty total) on CPU 4 retain all twelve
timing rows. The existing gate parser reads per-process minima; each table
column is the minimum of its round. Typed/common Dupire checksums agree
exactly, and all twelve checksum strings remain identical in every process.
All 408 installed/header/library/fixture/consumer/binary inputs retain their
before/after hashes.

| Case                                    | Round 1 ns | Round 2 ns |
|-----------------------------------------|------------|------------|
| Typed Dupire (9x2 surface)              | 17610.00   | 17713.00   |
| Common Dupire (9x2 surface)             | 17753.00   | 17762.00   |
| Typed Dupire (21x8 surface)             | 175151.00  | 175443.00  |
| Common Dupire (21x8 surface)            | 175368.00  | 175228.00  |
| Common curve single (8 quotes)          | 141.00     | 142.00     |
| Common curve joint (3x8 quotes)         | 467.00     | 475.00     |
| Common curve staged (8 quotes)          | 135.00     | 135.00     |
| Common curve single (16 quotes)         | 209.00     | 209.00     |
| Common curve joint (3x16 quotes)        | 1278.00    | 1421.00    |
| Common curve staged (16 quotes)         | 207.00     | 206.00     |
| Common curve generic plain (5 quotes)   | 119.00     | 119.00     |
| Common curve generic layered (5 quotes) | 119.00     | 119.00     |

These are new-entry costs, with no legacy acceptance threshold or regression
verdict. Typed/common cases have fixed within-process order, so the table does
not claim a statistically established wrapper-overhead difference.
Curves cost about 0.12–1.42 microseconds and Dupire about 18/175 microseconds in
these prepared small/large workloads. Larger content comparisons, detached
binding projections, calibration and portfolio execution require later
coverage. The result owns three quote matrices and one immutable source;
no new dense Jacobian is constructed.

The first measurement runner uses an incorrect private-fixture path and fails
during hashing before any sample. Retain the failed script/log and empty
`aad-calibration-pullback-cost-samples-01` directory. The corrected `-02`
runner/output uses the actual fixture path; no binary, workload, parser,
sample count or acceptance policy changes.

## Evidence, coverage and verdict

All paths below are relative to permanent evidence root
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`:

- `aad-calibration-pullback-baseline-01/manifest.json`,
  `aad-calibration-pullback-gate-inputs-01.json` and
  `aad-calibration-pullback-gate-acceptance-01.json` pin the inputs and
  constructor-object identity.
- `aad-calibration-pullback-gate-paired-01/results.json`, `summary.md`
  and raw logs retain all 360 gate outputs and unchanged executable verdict.
- `aad-calibration-pullback-cost-source/`,
  `aad-calibration-pullback-cost-build-01/`,
  `aad-calibration-pullback-cost-run-{01,02}.py` and
  `aad-calibration-pullback-cost-samples-02/` retain the full consumer,
  failed setup, all twenty outputs, checksums and before/after hashes.
- `aad-calibration-pullback-installed-parity-01.json` retains sixteen
  numeric and three metadata rows, identical across fresh OFF/combined installs.

Changed old aggregation is covered by existing rate-risk aggregate and
portfolio ANALYTIC/BUMPED cases, including generic joint N=5/10/16 shapes.
New prepared common entry is covered by the installed consumer, all four
providers, layered/plain generic sources and both Dupire sizes.
Production MC, Python/Excel common projections, multi-output and large
content-identity costs are not accepted by this increment.

Verdict: **no regression in the measured incremental old workloads** under
the unchanged policy. Keep P01's production MC failures and inconclusive
investigation open; this result does not accept F01 or the complete AAD plan.
