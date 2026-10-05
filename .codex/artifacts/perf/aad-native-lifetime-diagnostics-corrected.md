# Corrected native lifetime diagnostics: acceptance evidence

Status: corrected local functional, installed-consumer and paired performance checks pass.
Publication CI and the final D02 requirement/capability audit remain open.
This report does not complete D01, D03, Stage A or the full implementation goal.

The [initial diagnostic report](aad-native-lifetime-diagnostics.md) retains the
`c83bcc9` measurements and assignment failures. These are fresh measurements of
the assignment-corrected source; the earlier green performance/CI results are
not substituted for this increment's evidence.

## Scope and immutable environment

- Original baseline: `b5e3caca85bb1a5b1aa3acf7898b387832d443c8`.
- Measured corrected head: `8e1ef0941a949059858aa78cab46af4cb50b41e6`.
- Both sources were clean and detached during measurement: `/tmp/dal-aad-baseline`
  and `/tmp/dal-aad-implementation`.
- Separate Release build roots: `/tmp/dal-aad-evidence/base-build` and
  `/tmp/dal-aad-evidence/head-build`; build-tree binaries only.
- Native backend, Eigen, C++17, GCC 15.2, CMake 4.2.3, Unix Makefiles,
  benchmarks explicitly ON, native-architecture opt-in OFF.
- Dependency gitlinks were verified against actual checkout SHAs on each side
  and match. Source, cache, dependency, helper and binary identities were checked
  again after all measurements and remained unchanged.
- Shared WSL2/i9-13900HX host. Desktop/hypervisor activity is uncontrolled.
  Every paired/resource run uses `DAL_NUM_THREADS=4` and affinity `4,6,8,10`.
  No concurrent DAL builds/tests or production/benchmark/Git/PR mutations occur
  during the pure measurement phase.
- Nine-target policy unchanged: two rounds of ten interleaved process samples
  per side, alternating the first side, best-of-N process minima, failure only
  when both rounds exceed +4%. The Sobol 10x ceiling and all workloads remain.
- Supplemental MC/LSM and initial/confirmation calibration use the same
  two-by-ten pairing. The additional fixed calibration stability experiment
  uses two-by-thirty; it does not change the official nine-target gate.
- Environment records: `lifetime-assignment-off-environment.json` and
  `lifetime-assignment-off-environment-after.json` under the evidence root.
  Measurements started at `2026-10-04T03:03:00.497739+00:00`.

Subsequent result-extraction coverage changes only a test translation unit and
active evidence. Verify that the production/benchmark diff from the measured
head remains empty and the measured binary digests remain unchanged before
using this report for a publication head.

## Corrected functional and installed-consumer checks

- Three independently reproduced assignment RED cases remain in
  `lifetime-rebind-red.log` and `lifetime-expression-rebind-red.log`.
  Diagnostic double/expression assignment commits the cached primal and binding
  only after successful allocation/materialization. Default OFF bodies remain.
- All 28 lifetime cases pass focused ASan/UBSan with leak detection:
  `lifetime-assignment-sanitized.log`. This recompiles diagnostic tape/recording/
  tests and uses the default archive for supporting non-active symbols; it is
  not evidence of a completely sanitized ON library. A dedicated CI leg builds
  that complete configuration.
- Corrected full ON CTest: 2,313 cases, including Python, all pass.
  Corrected full OFF CTest: 2,318 functional/example cases, all pass.
  Retain `lifetime-assignment-{on,off}-{build,ctest}.log`.
- Fresh ON/OFF installs and independent external consumers pass. They check
  package setting, inherited ABI definition, analytic derivative and ON rejection
  of a closed recording's output. Retain
  `lifetime-assignment-install-{on,off}.log` and
  `lifetime-assignment-consumer-{on,off}{,-configure,-build}.log`.
- Existing layout checks remain: OFF number/node/tape size and alignment match
  the original baseline at 16/8, 40/8, 368/8 bytes; ON is 64/8, 56/8, 424/8.
  The assignment correction adds no fields. OFF tape symbols exclude diagnostic
  identity, validation and counter/reset functions.
- The separate full-final-block size repair keeps its timeout RED and eleven
  ASan/UBSan GREEN cases. Its final performance is included here.
- Later result-extraction exception coverage and exact-head CI must be recorded
  separately; they are not counted in the above frozen full-suite totals.

## Nine-target performance

All 65 comparable cases pass; all ten head-only cases remain informational.
Positive round movements, including prepared PV at +7.44%/+1.38%, remain visible.
Passing the unchanged policy does not mean every sample or minimum is identical.

| Target         | Case                                                                           | Base min ms | Head min ms | Combined change | Round 1 | Round 2 | Result |
|----------------|--------------------------------------------------------------------------------|-------------|-------------|-----------------|---------|---------|--------|
| tape_perf      | Clear + re-record (100K nodes)                                                 | 0.864247    | 0.875558    | +1.31%          | -1.72%  | +1.31%  | pass   |
| tape_perf      | PropagateToStart (100K nodes)                                                  | 0.390588    | 0.388423    | -0.55%          | -1.12%  | -0.53%  | pass   |
| tape_perf      | PropagateToStart multi-mode (100K nodes, 10 results)                           | 0.512413    | 0.491450    | -4.09%          | -3.31%  | -6.68%  | pass   |
| tape_perf      | Rewind + re-record (100K nodes)                                                | 0.568387    | 0.569005    | +0.11%          | +0.62%  | +0.11%  | pass   |
| tape_perf      | ZeroAdjoints sweep (100K nodes)                                                | 0.111011    | 0.111384    | +0.34%          | +0.62%  | +0.34%  | pass   |
| tape_perf      | PropagateToStart multi-mode (50K steps, 1 result) (new coverage)               | —           | 0.386847    | —               | —       | —       | info   |
| tape_perf      | PropagateToStart multi-mode (50K steps, 16 results) (new coverage)             | —           | 0.638635    | —               | —       | —       | info   |
| tape_perf      | PropagateToStart multi-mode (50K steps, 4 results) (new coverage)              | —           | 0.385286    | —               | —       | —       | info   |
| tape_perf      | PropagateToStart multi-mode (50K steps, 64 results) (new coverage)             | —           | 6.692000    | —               | —       | —       | info   |
| tape_perf      | PropagateToStart passive constants (50K steps) (new coverage)                  | —           | 0.147756    | —               | —       | —       | info   |
| tape_perf      | Rewind + passive-constant recording (50K steps) (new coverage)                 | —           | 0.243427    | —               | —       | —       | info   |
| jacobian_perf  | AnalyticJacobian dense harvest (24 x 23)                                       | 0.005383    | 0.004702    | -12.65%         | -12.60% | -12.65% | pass   |
| jacobian_perf  | AnalyticJacobian row-width harvest (24 x 23)                                   | 0.005386    | 0.004663    | -13.42%         | -13.32% | -13.42% | pass   |
| jacobian_perf  | HarvestCurveJacobian dense (23 outputs x 24 parameters) (new coverage)         | —           | 0.004339    | —               | —       | —       | info   |
| jacobian_perf  | HarvestCurveJacobian dense (95 outputs x 96 parameters) (new coverage)         | —           | 0.061956    | —               | —       | —       | info   |
| jacobian_perf  | HarvestCurveJacobian proven prefix (23 outputs x 24 parameters) (new coverage) | —           | 0.003783    | —               | —       | —       | info   |
| jacobian_perf  | HarvestCurveJacobian proven prefix (95 outputs x 96 parameters) (new coverage) | —           | 0.058077    | —               | —       | —       | info   |
| pde_perf       | ThetaScheme rollback (200x200 CN)                                              | 0.214638    | 0.214706    | +0.03%          | -4.18%  | +0.09%  | pass   |
| pde_perf       | ThetaScheme rollback (200x200 implicit)                                        | 0.214630    | 0.214213    | -0.19%          | -5.68%  | +0.03%  | pass   |
| pde_perf       | ThetaScheme rollback (200x2000 explicit)                                       | 0.663634    | 0.669235    | +0.84%          | +0.74%  | +0.84%  | pass   |
| rng_perf       | BrownianBridge FillNormal (100K x 10D)                                         | 6.607000    | 6.496000    | -1.68%          | -2.32%  | +1.44%  | pass   |
| rng_perf       | IRN SkipNormalTo (100K x 10D)                                                  | 7.810000    | 7.850000    | +0.51%          | +0.51%  | +1.10%  | pass   |
| rng_perf       | MRG32 SkipNormalTo (100K x 10D)                                                | 0.001300    | 0.001298    | -0.15%          | -0.15%  | -0.15%  | pass   |
| rng_perf       | MRG32k3a FillNormal (100K x 10D)                                               | 19.732000   | 19.841000   | +0.55%          | +0.55%  | -0.84%  | pass   |
| rng_perf       | ShuffledIRN FillNormal (100K x 10D)                                            | 10.767000   | 10.794000   | +0.25%          | +0.25%  | +1.12%  | pass   |
| rng_perf       | Sobol FillNormal fast (100K x 10D)                                             | 4.074000    | 4.064000    | -0.25%          | -2.19%  | +4.91%  | pass   |
| rng_perf       | Sobol FillNormal precise opt-in (100K x 10D)                                   | 38.518000   | 38.163000   | -0.92%          | -0.92%  | -0.21%  | pass   |
| rng_perf       | Sobol FillUniform (100K x 10D)                                                 | 0.791208    | 0.790610    | -0.08%          | +1.81%  | -2.48%  | pass   |
| interp_perf    | Cubic interp (50 knots, 10K queries)                                           | 0.050282    | 0.051599    | +2.62%          | +4.04%  | +1.96%  | pass   |
| interp_perf    | Inlined linear LV-style (1e5 paths x 200 steps, 200 knots)                     | 104.662000  | 104.556000  | -0.10%          | -0.20%  | -0.10%  | pass   |
| interp_perf    | Linear interp (50 knots, 10K queries)                                          | 0.042436    | 0.042526    | +0.21%          | +3.16%  | -2.13%  | pass   |
| krylov_perf    | BCGSolve (500x500 tridiag)                                                     | 0.061156    | 0.061314    | +0.26%          | +3.16%  | +0.26%  | pass   |
| krylov_perf    | CGSolve (500x500 tridiag)                                                      | 0.051696    | 0.051680    | -0.03%          | +1.04%  | -0.03%  | pass   |
| banded_perf    | TriDecomp MultiplyLeft (10K)                                                   | 0.004503    | 0.004497    | -0.13%          | -0.29%  | +0.16%  | pass   |
| banded_perf    | TriDiagonal Decompose (10K)                                                    | 0.076429    | 0.076890    | +0.60%          | -3.23%  | +0.83%  | pass   |
| banded_perf    | TriDiagonal MultiplyLeft (10K)                                                 | 0.004768    | 0.004690    | -1.64%          | -2.39%  | -0.52%  | pass   |
| cholesky_perf  | CholeskyDecompose (200x200)                                                    | 0.226147    | 0.225545    | -0.27%          | -0.27%  | -1.44%  | pass   |
| cholesky_perf  | CholeskyDecompose+Multiply (200x200)                                           | 0.225585    | 0.225202    | -0.17%          | -0.17%  | +1.87%  | pass   |
| rate_risk_perf | Quote risk aggregate (joint XCCY)                                              | 0.227090    | 0.229057    | +0.87%          | +0.87%  | -0.31%  | pass   |
| rate_risk_perf | Quote risk aggregate (single curve)                                            | 0.009279    | 0.009338    | +0.64%          | +1.36%  | +0.64%  | pass   |
| rate_risk_perf | Quote risk aggregate (staged XCCY basis)                                       | 0.063139    | 0.061750    | -2.20%          | -3.04%  | -0.52%  | pass   |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=10)                                      | 1.162000    | 1.162000    | +0.00%          | +0.00%  | -0.59%  | pass   |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=16)                                      | 1.807000    | 1.786000    | -1.16%          | -1.38%  | +0.22%  | pass   |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=5)                                       | 0.854334    | 0.839980    | -1.68%          | -0.96%  | -2.33%  | pass   |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=10)                                     | 11.690000   | 11.542000   | -1.27%          | +0.08%  | -1.27%  | pass   |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=16)                                     | 18.089000   | 17.826000   | -1.45%          | -1.02%  | -1.71%  | pass   |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=5)                                      | 8.293000    | 8.265000    | -0.34%          | -1.12%  | +0.24%  | pass   |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=10)                       | 1.115000    | 1.108000    | -0.63%          | -0.63%  | -2.63%  | pass   |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=16)                       | 1.740000    | 1.705000    | -2.01%          | -2.63%  | -1.49%  | pass   |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=5)                        | 0.804009    | 0.795425    | -1.07%          | -0.93%  | -2.24%  | pass   |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=10)                      | 11.535000   | 11.352000   | -1.59%          | -1.59%  | -1.93%  | pass   |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=16)                      | 17.855000   | 17.522000   | -1.87%          | +0.26%  | -2.65%  | pass   |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=5)                       | 8.146000    | 8.120000    | -0.32%          | -0.32%  | -1.62%  | pass   |
| rate_risk_perf | Quote risk portfolio joint ANALYTIC (24 XCCY x N=10/block)                     | 4.715000    | 4.591000    | -2.63%          | -1.61%  | -2.79%  | pass   |
| rate_risk_perf | Quote risk portfolio joint BUMPED (24 XCCY x N=10/block)                       | 4.661000    | 4.634000    | -0.58%          | -0.04%  | -0.58%  | pass   |
| rate_risk_perf | Quote risk portfolio single ANALYTIC (120 deposits x N=5)                      | 0.126620    | 0.126367    | -0.20%          | -0.20%  | +0.83%  | pass   |
| rate_risk_perf | Quote risk portfolio single BUMPED (120 deposits x N=16)                       | 0.176746    | 0.174287    | -1.39%          | -1.39%  | -2.13%  | pass   |
| rate_risk_perf | Quote risk portfolio staged ANALYTIC (24 XCCY x N=16)                          | 3.665000    | 3.631000    | -0.93%          | -1.14%  | +0.16%  | pass   |
| rate_risk_perf | Quote risk portfolio staged BUMPED (24 XCCY x N=5)                             | 1.306000    | 1.309000    | +0.23%          | +1.84%  | -1.28%  | pass   |
| rate_risk_perf | Rate AAD 2 components (1024 IRS x 8 nodes, fixed maturities)                   | 5.374000    | 5.274000    | -1.86%          | -2.94%  | -0.69%  | pass   |
| rate_risk_perf | Rate AAD 2 components (256 IRS x 8 nodes, fixed maturities)                    | 1.311000    | 1.326000    | +1.14%          | -1.12%  | +1.14%  | pass   |
| rate_risk_perf | Rate AAD 2 components (32 IRS x 8 nodes, fixed maturities)                     | 0.154670    | 0.157687    | +1.95%          | +3.18%  | -1.41%  | pass   |
| rate_risk_perf | Rate OIS daily compounding sweep (5Y quarterly x daily)                        | 0.223846    | 0.220749    | -1.38%          | -0.78%  | -3.47%  | pass   |
| rate_risk_perf | Rate PV (1024 IRS x 8 nodes, fixed maturities)                                 | 2.556000    | 2.566000    | +0.39%          | -0.58%  | +1.84%  | pass   |
| rate_risk_perf | Rate PV (256 IRS x 8 nodes, fixed maturities)                                  | 0.609458    | 0.609354    | -0.02%          | -0.02%  | -2.25%  | pass   |
| rate_risk_perf | Rate PV (32 IRS x 8 nodes, fixed maturities)                                   | 0.075466    | 0.076933    | +1.94%          | +2.10%  | -1.46%  | pass   |
| rate_risk_perf | Rate XCCY batch serial (24 XCCY x 5 components)                                | 0.810060    | 0.795404    | -1.81%          | -1.80%  | -1.81%  | pass   |
| rate_risk_perf | Rate batch serial (120 IRS x 2 components)                                     | 1.281000    | 1.303000    | +1.72%          | -3.19%  | +2.50%  | pass   |
| rate_risk_perf | Rate prepare geometry (256 IRS x 8 nodes, fixed maturities)                    | 0.544929    | 0.540008    | -0.90%          | +4.41%  | -5.91%  | pass   |
| rate_risk_perf | Rate prepare geometry (32 IRS x 8 nodes, fixed maturities)                     | 0.068107    | 0.066133    | -2.90%          | -5.10%  | -2.46%  | pass   |
| rate_risk_perf | Rate prepared AAD 2 components (256 IRS x 8 nodes, fixed maturities)           | 0.730399    | 0.712572    | -2.44%          | +3.92%  | -6.17%  | pass   |
| rate_risk_perf | Rate prepared AAD 2 components (32 IRS x 8 nodes, fixed maturities)            | 0.089526    | 0.084602    | -5.50%          | -5.50%  | -4.31%  | pass   |
| rate_risk_perf | Rate prepared PV (256 IRS x 8 nodes, fixed maturities)                         | 0.105811    | 0.113686    | +7.44%          | +7.44%  | +1.38%  | pass   |
| rate_risk_perf | Rate prepared PV (32 IRS x 8 nodes, fixed maturities)                          | 0.013689    | 0.013517    | -1.26%          | -1.58%  | +0.21%  | pass   |
| rate_risk_perf | Rate single-trade sweeps (240 IRS calls)                                       | 2.094000    | 2.056000    | -1.81%          | -1.88%  | -1.81%  | pass   |

Head Sobol precise-opt-in/fast ratio is 9.39x, below the unchanged 10x ceiling.
Raw data: `lifetime-assignment-off-paired/{results.json,summary.md}` and every
retained command output; process exit is zero.

## Supplemental Monte Carlo and LSM

All 35 ordinary cases and three LSM profiles pass the fixed two-round policy.
Every LSM sample's PV and risk vector agree across sides at relative/absolute
tolerance 1e-10. Ordinary executable result checks pass, but the runner does not
extract a complete cross-side numeric matrix for every ordinary case; do not
expand that evidence into a broader numeric claim.

| Profile         | Case                                                                             | Base min ms | Head min ms | Round 1 | Round 2 | Result |
|-----------------|----------------------------------------------------------------------------------|-------------|-------------|---------|---------|--------|
| ordinary        | GSR 1F 5Y swaption (1 price, order 16/32)                                        | 0.013240    | 0.013172    | -4.55%  | +2.34%  | pass   |
| ordinary        | GSR 1F bond (100K paths x 4 steps)                                               | 6.458000    | 6.717000    | +0.76%  | +4.01%  | pass   |
| ordinary        | GSR 1F bond option (1000 prices)                                                 | 1.677000    | 1.703000    | +1.55%  | -2.57%  | pass   |
| ordinary        | GSR 1F swap and Libor (10K paths x 4 steps)                                      | 15.347000   | 15.197000   | -1.02%  | +1.45%  | pass   |
| ordinary        | GSR 2F 5Y swaption (1 price, order 16/32)                                        | 0.262332    | 0.260350    | -3.56%  | +3.26%  | pass   |
| ordinary        | GSR 2F bond (100K paths x 4 steps)                                               | 9.198000    | 9.396000    | +3.55%  | +2.15%  | pass   |
| ordinary        | GSR 2F bond option (1000 prices)                                                 | 2.462000    | 2.445000    | -3.78%  | +2.15%  | pass   |
| ordinary        | GSR 2F swap and Libor (10K paths x 4 steps)                                      | 15.245000   | 15.641000   | +2.60%  | +2.32%  | pass   |
| ordinary        | GSR 3F 5Y swaption (1 price, order 16/32)                                        | 0.257294    | 0.267116    | +4.23%  | +1.37%  | pass   |
| ordinary        | GSR 3F bond (100K paths x 4 steps)                                               | 10.559000   | 10.203000   | -3.37%  | -1.50%  | pass   |
| ordinary        | GSR 3F bond option (1000 prices)                                                 | 3.356000    | 3.441000    | +0.26%  | +3.64%  | pass   |
| ordinary        | GSR 3F swap and Libor (10K paths x 4 steps)                                      | 15.294000   | 15.894000   | +3.92%  | +0.65%  | pass   |
| ordinary        | GSR g calibration (3 quotes x 3 buckets)                                         | 0.196585    | 0.203866    | +0.66%  | +3.82%  | pass   |
| ordinary        | LSMC regression degree=3 (100000 paths)                                          | 0.359167    | 0.352444    | -6.46%  | +0.73%  | pass   |
| ordinary        | LSMC regression degree=3 ITM mask (100000 paths)                                 | 0.346620    | 0.353093    | -2.33%  | +2.06%  | pass   |
| ordinary        | LSMC regression degree=8 (100000 paths)                                          | 0.660008    | 0.648030    | -6.28%  | +0.23%  | pass   |
| ordinary        | LSMC regression degree=8 ITM mask (100000 paths)                                 | 0.659585    | 0.660189    | -2.01%  | +0.11%  | pass   |
| ordinary        | LSMC regression features=2 degree=2 ITM mask (100000 paths)                      | 7.746000    | 7.908000    | +0.85%  | +2.09%  | pass   |
| ordinary        | LSMC regression features=2 degree=3 ITM mask (100000 paths)                      | 13.690000   | 13.762000   | +0.53%  | -0.80%  | pass   |
| ordinary        | LSMC regression features=3 degree=3 ITM mask (100000 paths)                      | 35.031000   | 34.517000   | -2.52%  | -1.47%  | pass   |
| ordinary        | correlated BS path (100K x 12 steps x 1 assets)                                  | 10.264000   | 9.898000    | -3.57%  | -3.21%  | pass   |
| ordinary        | correlated BS path (100K x 12 steps x 2 assets)                                  | 15.233000   | 15.209000   | -0.01%  | -0.96%  | pass   |
| ordinary        | correlated BS path (100K x 12 steps x 3 assets)                                  | 20.370000   | 19.557000   | -3.99%  | -1.62%  | pass   |
| ordinary        | hybrid flat-rate path (100K x 12 steps x 2 assets)                               | 38.153000   | 37.651000   | -1.32%  | -0.09%  | pass   |
| ordinary        | hybrid logDF path (100K x 12 steps x 2 assets)                                   | 37.935000   | 37.720000   | -0.57%  | +0.49%  | pass   |
| ordinary        | script engine bermudan exercise double compiled=false (100000 paths x 54 events) | 117.361000  | 118.859000  | -0.54%  | +1.40%  | pass   |
| ordinary        | script engine bermudan exercise double compiled=true (100000 paths x 54 events)  | 106.996000  | 107.426000  | +0.40%  | +0.45%  | pass   |
| ordinary        | script engine vanilla Number_ compiled=false (20000 paths x 1 events)            | 0.687489    | 0.704197    | +5.44%  | +1.35%  | pass   |
| ordinary        | script engine vanilla Number_ compiled=true (20000 paths x 1 events)             | 0.634186    | 0.634965    | -0.38%  | +0.12%  | pass   |
| ordinary        | script engine vanilla double compiled=false (200000 paths x 1 events)            | 1.911000    | 1.911000    | +0.00%  | +0.36%  | pass   |
| ordinary        | script engine vanilla double compiled=true (200000 paths x 1 events)             | 1.685000    | 1.736000    | +3.56%  | -0.12%  | pass   |
| ordinary        | script engine weekly barrier Number_ compiled=false (10000 paths x 52 events)    | 9.054000    | 9.062000    | +0.09%  | +0.23%  | pass   |
| ordinary        | script engine weekly barrier Number_ compiled=true (10000 paths x 52 events)     | 7.436000    | 7.383000    | -0.26%  | -1.26%  | pass   |
| ordinary        | script engine weekly barrier double compiled=false (100000 paths x 52 events)    | 35.073000   | 34.802000   | -0.77%  | -1.79%  | pass   |
| ordinary        | script engine weekly barrier double compiled=true (100000 paths x 52 events)     | 20.601000   | 20.564000   | -1.19%  | -0.18%  | pass   |
| lsm-bs-tree     | lsm-bs-tree                                                                      | 103.693686  | 103.292399  | -0.39%  | +0.36%  | pass   |
| lsm-bs-compiled | lsm-bs-compiled                                                                  | 100.942574  | 100.469000  | -0.22%  | -0.47%  | pass   |
| lsm-lv-daily    | lsm-lv-daily                                                                     | 497.984675  | 492.943260  | -1.45%  | -1.01%  | pass   |

Raw data: `lifetime-assignment-off-mc-paired/{results.json,summary.md}`,
including all process outputs and LSM numeric vectors; process exit is zero.

## Calibration failure, confirmation and stability

The initial calibration run exits one: PWL 4096-by-8-node queries slow by
+8.73%/+5.61%. This is a real failed acceptance observation and is retained.
Its per-round base/head minima are 41,562/45,190 ns and 43,612/46,060 ns.
No case, calculation, benchmark code, threshold or binary is changed afterward.

An independent two-by-ten confirmation passes all 25 cases; the failed query
changes by -0.75%/-2.39%. A fixed further two-by-thirty experiment also passes
all 25 cases; the query changes by +1.54%/-1.32%.
A two-by-ten same-baseline-binary control measures +1.15%/+3.08% for that query;
this control is noise evidence, not a branch-versus-baseline acceptance run.

The query fixture and its double PWL implementation are unchanged from baseline.
The short timings, changing minima and reversal without source/binary changes
support an inference of measurement variation; no hardware trace establishes a
specific cause. The failure is not erased or described as an initial pass.
Additional independent and larger-sample evidence finds no sustained regression
under the unchanged threshold. The full formal gate remains its original two-by-ten run.

The table retains all initial and subsequent round changes. Confirm minima are
over the confirmation run's twenty samples per side. Every row passes in both
the confirmation and extended experiments.

| Case                                                            | Initial R1 | Initial R2 | Confirm base min ms | Confirm head min ms | Confirm R1 | Confirm R2 | Extended R1 | Extended R2 |
|-----------------------------------------------------------------|------------|------------|---------------------|---------------------|------------|------------|-------------|-------------|
| CalibrateYieldCurve LOG_CUBIC_NATURAL ANALYTIC +DIAG (23 swaps) | -0.93%     | +0.27%     | 1.492000            | 1.467000            | -2.85%     | -0.94%     | +3.13%      | -2.08%      |
| CalibrateYieldCurve LOG_CUBIC_NATURAL ANALYTIC SOLVE (23 swaps) | -1.16%     | +3.91%     | 0.864075            | 0.868415            | -2.02%     | +1.76%     | +3.94%      | -4.18%      |
| CalibrateYieldCurve LOG_CUBIC_NATURAL BUMPED +DIAG (23 swaps)   | -0.67%     | +0.36%     | 2.150000            | 2.256000            | -1.22%     | +4.93%     | +2.44%      | -0.37%      |
| CalibrateYieldCurve LOG_CUBIC_NATURAL BUMPED SOLVE (23 swaps)   | +1.61%     | -1.69%     | 1.359000            | 1.396000            | +1.08%     | +2.72%     | -2.98%      | +2.67%      |
| CalibrateYieldCurve LOG_LINEAR ANALYTIC +DIAG (23 swaps)        | +5.35%     | -3.41%     | 1.421000            | 1.399000            | -0.56%     | -2.78%     | +0.00%      | -0.44%      |
| CalibrateYieldCurve LOG_LINEAR ANALYTIC SOLVE (23 swaps)        | +1.95%     | -2.82%     | 0.804726            | 0.815492            | -0.16%     | +1.71%     | -1.14%      | -1.41%      |
| CalibrateYieldCurve LOG_LINEAR APPROXIMATE (23 swaps)           | -3.55%     | -2.38%     | 10.491000           | 10.317000           | -4.89%     | -1.59%     | -0.41%      | -0.01%      |
| CalibrateYieldCurve LOG_LINEAR BUMPED +DIAG (23 swaps)          | -2.55%     | -0.63%     | 1.908000            | 1.888000            | -1.05%     | -1.45%     | -1.89%      | +3.30%      |
| CalibrateYieldCurve LOG_LINEAR BUMPED SOLVE (23 swaps)          | -1.40%     | -8.25%     | 1.198000            | 1.228000            | +3.09%     | -1.29%     | +1.60%      | -1.02%      |
| CalibrateYieldCurve MIXED ANALYTIC +DIAG (23 swaps)             | -0.86%     | -2.09%     | 1.434000            | 1.415000            | -2.41%     | -1.05%     | +0.07%      | -1.28%      |
| CalibrateYieldCurve MIXED ANALYTIC SOLVE (23 swaps)             | +1.75%     | -2.37%     | 0.834196            | 0.822429            | -1.42%     | -1.41%     | +0.34%      | -3.05%      |
| CalibrateYieldCurve MIXED BUMPED +DIAG (23 swaps)               | -3.78%     | -2.32%     | 1.951000            | 1.925000            | -1.33%     | +0.20%     | +2.93%      | +1.26%      |
| CalibrateYieldCurve MIXED BUMPED SOLVE (23 swaps)               | +2.86%     | +7.54%     | 1.282000            | 1.218000            | -1.64%     | -6.45%     | -4.82%      | -1.21%      |
| CalibrateYieldCurve PWC ANALYTIC +DIAG (23 swaps)               | -0.64%     | -2.10%     | 1.091000            | 1.061000            | -4.81%     | -2.75%     | -1.28%      | -2.34%      |
| CalibrateYieldCurve PWC ANALYTIC SOLVE (23 swaps)               | +1.12%     | -3.62%     | 0.608374            | 0.599103            | -4.42%     | -0.88%     | -4.44%      | -4.72%      |
| CalibrateYieldCurve PWC BUMPED +DIAG (23 swaps)                 | -0.46%     | -2.19%     | 0.959157            | 0.942438            | -2.63%     | +0.52%     | -2.04%      | -0.70%      |
| CalibrateYieldCurve PWC BUMPED SOLVE (23 swaps)                 | +1.99%     | +0.33%     | 0.658284            | 0.659871            | +0.36%     | -0.34%     | +0.28%      | +2.27%      |
| CalibrateYieldCurve PWL ANALYTIC +DIAG (23 swaps)               | +2.27%     | -6.55%     | 1.279000            | 1.250000            | -2.27%     | -0.86%     | +0.00%      | -2.13%      |
| CalibrateYieldCurve PWL ANALYTIC SOLVE (23 swaps)               | +0.75%     | -1.91%     | 0.674563            | 0.650193            | -2.50%     | -3.61%     | -0.56%      | +0.41%      |
| CalibrateYieldCurve PWL BUMPED +DIAG (23 swaps)                 | -0.19%     | -1.64%     | 1.547000            | 1.615000            | +3.35%     | +4.40%     | +1.24%      | +0.83%      |
| CalibrateYieldCurve PWL BUMPED SOLVE (23 swaps)                 | +7.87%     | -1.43%     | 0.975934            | 0.991606            | +1.26%     | +1.61%     | -0.77%      | +0.94%      |
| PWL DF queries (4096 x 24 nodes)                                | +4.40%     | +0.26%     | 0.050435            | 0.049256            | -2.10%     | -2.84%     | -2.68%      | -2.74%      |
| PWL DF queries (4096 x 256 nodes)                               | -1.74%     | +2.39%     | 0.084379            | 0.080430            | -3.07%     | -5.04%     | -6.25%      | -8.14%      |
| PWL DF queries (4096 x 64 nodes)                                | +3.75%     | +1.62%     | 0.056001            | 0.055467            | -0.95%     | -2.49%     | -2.69%      | -1.08%      |
| PWL DF queries (4096 x 8 nodes)                                 | +8.73%     | +5.61%     | 0.043684            | 0.043358            | -0.75%     | -2.39%     | +1.54%      | -1.32%      |

Retain all of:
`lifetime-assignment-off-curve-paired/` (initial FAIL),
`lifetime-assignment-off-curve-confirmation/` (two-by-ten PASS),
`lifetime-assignment-off-curve-extended/` (two-by-thirty PASS), and
`lifetime-assignment-curve-noise-control/` (same-binary control).
Each directory contains full sample outputs and JSON; paired runs also retain
Markdown summaries.

## Resource observations

Three alternating pairs per profile use verbose process resource records.
All daily local-vol LSM prices/risks agree in every resource pair.
Ranges overlap and no material RSS increase is observed; these measurements do
not establish a strict request/process memory budget or diagnostic-ON cost.

| Profile      | Pair | Base RSS KiB | Head RSS KiB | Major faults base/head |
|--------------|------|--------------|--------------|------------------------|
| ordinary     | 1    | 99780        | 99516        | 0/0                    |
| ordinary     | 2    | 99508        | 99804        | 0/0                    |
| ordinary     | 3    | 99476        | 99780        | 0/0                    |
| lsm-lv-daily | 1    | 11992        | 11760        | 0/0                    |
| lsm-lv-daily | 2    | 11864        | 11732        | 0/0                    |
| lsm-lv-daily | 3    | 11812        | 12004        | 0/0                    |

All major page-fault counts are zero. Raw outputs and resource records are under
`lifetime-assignment-off-resource/`, including `results.json`.

## Measured binary identities

These SHA-256 values are captured before and after measurement. Source and
dependency checks alone cannot prove that the executed binaries are current.

| Binary                 | Base SHA-256                                                     | Head SHA-256                                                     |
|------------------------|------------------------------------------------------------------|------------------------------------------------------------------|
| tape_perf              | 7cc1286b5ab5e869a6294c00963d2ef82c87c9ac85d77144b15a4df7bc914c20 | 55353aaa70b1f7e7fc290322e6548da1f010b31e4697e0577549133c090493c7 |
| jacobian_perf          | a9750d6a0c4beb5bd7858373bbeaea83bc7fecc39ac6193fb4bca4231fdf22f1 | a35682526d31c1b05d781de8d2c9c5649d3087543c21c91117af82c248fc7972 |
| pde_perf               | 3e4565e64df225de6d5f0bbb345bf1ace51b56a1c594cfcc50ed7a58bd766ac4 | f0468010c6bd5a8a1eaa136c7e4fad295c12130322290e578269aced34075c1d |
| rng_perf               | bae86ed42b0ea5119cf3fa22ea46638d8a3bbb56e503317190f82c468680bae8 | 5487647fbc86baebb0980ef788886bb7e5ab87cb669951f5875708760379ae47 |
| interp_perf            | cc15d16fd23c7fd2eaa05cc74f4e71a6adac6d2515f2ffcfaffc31e27201d172 | fbb90e0a249a743fb66eee9230585da94622a1e87d8f543a523ee0f93d481b34 |
| krylov_perf            | f531dda8d512aa403c3f1aaa35538f43b241e19f4398307706cd859a2b935658 | 316400b371783ba359e319aa07c787344b148632e2ee37e6e1d653f2a5d0bc23 |
| banded_perf            | 9083e58020e1cf0e5610a52ea1de9f20de2ffdc9ccd50431908c8feaeb61fcd0 | f9cef6b5415fe2dcb1bbd4b15a7509800b6dd8cca7c73a6e453ab5aa8b6edca4 |
| cholesky_perf          | a31addc1ad85b4654103f22617fd87c8310484b04ed179da51939db8488c92a1 | 818908751dca521278df276b334f0fc5f00dbc8b01f7227584d33a99f7760bfa |
| rate_risk_perf         | 0108c2fff1a84550af2bcbe86a2a7bb690563815554a0072c875c5a356211551 | b89b8e85defea2781128ae31f760b4b7e996b7416d612935e8ad654192d0481e |
| script_mc_perf         | d24378565d1484632f8fdb4b162e142e41a653f65246aeaa416a2b85a0d89f4c | 2d68b95beeadcdcf160f8b2cf0b7c80f480e984a1d59d8401254f93e7a2e8b50 |
| curve_calibration_perf | 1f19252efe0e9b6ce98399d88d218d996de91060825bde1ff081dc4216a4713a | fdf779ad9f1392bdcd2a9246a8e15cffa17d9f367c51f255afbbec60362a4649 |

## Remaining acceptance and scope

Local performance verdict: **no sustained regression under the unchanged policy**,
with the initial calibration failure and its independent confirmations retained.
Publication requires exact-head inspection of the new native diagnostic Linux/
Python, full ASan/UBSan and Windows/installed-consumer legs, as well as the
existing compiler/backend/wheel/static-analysis gates.

D01 still needs its final requirement audit. The new direct result-extraction
exception test passes with 76 ON and 48 OFF focused cases; CoDiPack and current
publication CI verification remain open. D02 needs the final requirement audit and separate D03 capability
reporting. P01 production phase/resource/scaling work and D03 remain active;
market/portfolio risks, structured operators, sparsity/checkpoint extensions and
second-order scope remain required parts of the original goal.
