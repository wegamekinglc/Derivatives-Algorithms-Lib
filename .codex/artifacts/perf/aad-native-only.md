# Native-only AAD: frozen correctness and performance evidence

Status: fresh local functional and installed-consumer verification passes.
The unchanged nine-target performance gate passes. Supplemental production
acceptance passes after a predefined two-by-thirty confirmation of all five
profiles; the initial two-by-ten experiment contains two failures, retained below.
Exact publication-head CI and final D00/D03 requirement audits remain open.
This report does not complete Stage A or the full AAD development goal.

## Immutable scope and environment

- Original implementation baseline: `b5e3caca85bb1a5b1aa3acf7898b387832d443c8`.
- Measured native-only head: `8816951662fc6788f3e9876c45ac72f7e48c65f7`.
- Separate clean sources: `/tmp/dal-aad-baseline` and
  `/tmp/dal-aad-backend-adapter`. Separate build roots under the evidence root:
  `base-build` and `native-only-off-build`. Only build-tree binaries are used.
- Release, native AAD, Eigen, C++17, GCC 15.2.0, CMake 4.2.3, Unix Makefiles,
  static libraries, native-architecture opt-in OFF, diagnostics OFF, sanitizers OFF.
  Benchmarks are explicitly ON on both sides. Actual core definitions and flags
  match: `DAL_USE_EIGEN EIGEN_DONT_PARALLELIZE` and
  `-O3 -DNDEBUG -std=c++17 -fPIC -ffp-contract=fast -O3`.
- Five retained dependency gitlinks match the baseline and actual checkouts.
  Only the head removes XAD, CoDiPack and Adept; their directories are physically
  absent during fresh builds. The historical baseline remains intact.
- The baseline has Python OFF and the head Python ON. This component distinction
  is recorded; the compared core and benchmark compile commands have matching
  optimization/definitions. Python does not run during performance measurement.
- Shared WSL2 host, Intel i9-13900HX, 32 logical CPUs, approximately 31 GiB RAM.
  Desktop/hypervisor activity is uncontrolled. All pairing/resources use
  `DAL_NUM_THREADS=4` and affinity `4,6,8,10`. There are no concurrent DAL
  builds/tests or source, benchmark, Git or PR mutations during measurement.
- `native-only-environment-before.json`,
  `native-only-environment-after.json` and
  `native-only-confirmation-environment-after.json` retain full provenance.
  Before/after source SHAs, clean status, caches, five pins, 22 binary digests,
  compile commands and original helper digests are identical.
- All paths naming raw evidence below are relative to `/tmp/dal-aad-evidence`.
  Evidence is retained outside the repository; these paths are local provenance,
  not downloadable PR artifacts.

## Verification before measurement

- Final fresh native Release CTest: OFF 2,330/2,330; ON 2,359/2,359, including
  core/public, portable Excel, 34 examples and 793 Python tests in each build.
  Original ON vanilla-example lifetime failure and the include-order build
  failures remain in the evidence root. Scoped registration/reverse and an
  independent price/six-partial oracle repair the example; complete reruns pass.
- Final OFF/ON installed prefixes each pass 2/2 independent consumer tests:
  scalar weighted/repeated VJPs, vector channels, closed-scope rejection and
  exported diagnostic ABI. There is no old backend header or external export.
- Ten configuration/header cases pass: default and legacy OFF work; each legacy
  ON and enabled direct compile macro fails with an explicit migration error.
- All 60 focused native/lifecycle/lifetime ASan/UBSan cases pass with leak
  detection. Active tape/recording/test translation units are rebuilt with
  sanitizers; supporting non-active symbols use the fresh ON archive. This is
  not a claim that every library translation unit is locally sanitized.
- Generation/drift passes with zero generated files changed. CI helper tests
  pass 172 cases with six existing skips; workflow YAML/matrix/needs checks pass.
  Documentation integrity passes 75 files at the measured head. All 21 benchmark
  smoke targets pass serially; smoke does not establish paired performance.
- Default-OFF number/node/tape/scope sizes remain 16/40/368/72 bytes, each
  aligned to 8. Diagnostic-ON sizes are 64/56/424/72 bytes with matching ABI.
  Functional logs: `native-only-{off,on}-ctest-final.log`;
  installed logs: `native-only-{off,on}-consumer-reinstalled.log`;
  sanitizer log: `native-only-focused-final-on.log`.

## Unchanged nine-target gate

Two confirmation rounds of ten interleaved process samples per side, alternating
which side runs first, use minima and the existing +4% threshold in both rounds.
No case, executable, workload or threshold is removed from the formal verdict.
All 65 comparable cases pass; ten head-only cases are informational.
The head Sobol precise-opt-in/fast ratio is 9.37x, below the unchanged 10x ceiling.
Positive movements, including prepared PV at +4.14%/+1.68%, remain visible.

Raw evidence: `native-only-formal-paired/{results.json,summary.md}`,
all 360 command outputs and `native-only-formal-paired.log`; gate exit zero.

| Target         | Case                                                                           | Base min ms | Head min ms | Combined change | Round 1 | Round 2 | Result |
|----------------|--------------------------------------------------------------------------------|-------------|-------------|-----------------|---------|---------|--------|
| tape_perf      | Clear + re-record (100K nodes)                                                 | 0.854949    | 0.853760    | -0.14%          | -0.07%  | -0.14%  | pass   |
| tape_perf      | PropagateToStart (100K nodes)                                                  | 0.391366    | 0.387662    | -0.95%          | -1.15%  | -0.70%  | pass   |
| tape_perf      | PropagateToStart multi-mode (100K nodes, 10 results)                           | 0.491194    | 0.493120    | +0.39%          | -2.56%  | +0.67%  | pass   |
| tape_perf      | Rewind + re-record (100K nodes)                                                | 0.568191    | 0.568121    | -0.01%          | -0.18%  | -0.01%  | pass   |
| tape_perf      | ZeroAdjoints sweep (100K nodes)                                                | 0.108541    | 0.111698    | +2.91%          | +0.33%  | +2.91%  | pass   |
| tape_perf      | PropagateToStart multi-mode (50K steps, 1 result) (new coverage)               | —           | 0.382174    | —               | —       | —       | info   |
| tape_perf      | PropagateToStart multi-mode (50K steps, 16 results) (new coverage)             | —           | 0.644237    | —               | —       | —       | info   |
| tape_perf      | PropagateToStart multi-mode (50K steps, 4 results) (new coverage)              | —           | 0.380817    | —               | —       | —       | info   |
| tape_perf      | PropagateToStart multi-mode (50K steps, 64 results) (new coverage)             | —           | 6.589000    | —               | —       | —       | info   |
| tape_perf      | PropagateToStart passive constants (50K steps) (new coverage)                  | —           | 0.147671    | —               | —       | —       | info   |
| tape_perf      | Rewind + passive-constant recording (50K steps) (new coverage)                 | —           | 0.236471    | —               | —       | —       | info   |
| jacobian_perf  | AnalyticJacobian dense harvest (24 x 23)                                       | 0.005347    | 0.004702    | -12.06%         | -12.39% | -12.01% | pass   |
| jacobian_perf  | AnalyticJacobian row-width harvest (24 x 23)                                   | 0.005371    | 0.004662    | -13.20%         | -13.23% | -13.18% | pass   |
| jacobian_perf  | HarvestCurveJacobian dense (23 outputs x 24 parameters) (new coverage)         | —           | 0.004329    | —               | —       | —       | info   |
| jacobian_perf  | HarvestCurveJacobian dense (95 outputs x 96 parameters) (new coverage)         | —           | 0.061869    | —               | —       | —       | info   |
| jacobian_perf  | HarvestCurveJacobian proven prefix (23 outputs x 24 parameters) (new coverage) | —           | 0.003775    | —               | —       | —       | info   |
| jacobian_perf  | HarvestCurveJacobian proven prefix (95 outputs x 96 parameters) (new coverage) | —           | 0.058184    | —               | —       | —       | info   |
| pde_perf       | ThetaScheme rollback (200x200 CN)                                              | 0.214523    | 0.214256    | -0.12%          | -0.24%  | -0.12%  | pass   |
| pde_perf       | ThetaScheme rollback (200x200 implicit)                                        | 0.214329    | 0.214351    | +0.01%          | -0.18%  | +0.09%  | pass   |
| pde_perf       | ThetaScheme rollback (200x2000 explicit)                                       | 0.658877    | 0.657350    | -0.23%          | -0.40%  | -0.13%  | pass   |
| rng_perf       | BrownianBridge FillNormal (100K x 10D)                                         | 6.424000    | 6.412000    | -0.19%          | -0.19%  | +1.16%  | pass   |
| rng_perf       | IRN SkipNormalTo (100K x 10D)                                                  | 7.819000    | 7.738000    | -1.04%          | +0.19%  | -2.04%  | pass   |
| rng_perf       | MRG32 SkipNormalTo (100K x 10D)                                                | 0.001294    | 0.001293    | -0.08%          | +0.15%  | -0.08%  | pass   |
| rng_perf       | MRG32k3a FillNormal (100K x 10D)                                               | 19.804000   | 19.872000   | +0.34%          | +0.34%  | -1.72%  | pass   |
| rng_perf       | ShuffledIRN FillNormal (100K x 10D)                                            | 10.819000   | 10.757000   | -0.57%          | -0.45%  | -1.21%  | pass   |
| rng_perf       | Sobol FillNormal fast (100K x 10D)                                             | 4.059000    | 4.083000    | +0.59%          | +0.59%  | -1.43%  | pass   |
| rng_perf       | Sobol FillNormal precise opt-in (100K x 10D)                                   | 38.488000   | 38.241000   | -0.64%          | -0.64%  | -0.02%  | pass   |
| rng_perf       | Sobol FillUniform (100K x 10D)                                                 | 0.774029    | 0.757372    | -2.15%          | -2.15%  | +1.93%  | pass   |
| interp_perf    | Cubic interp (50 knots, 10K queries)                                           | 0.050246    | 0.050302    | +0.11%          | +0.09%  | +3.93%  | pass   |
| interp_perf    | Inlined linear LV-style (1e5 paths x 200 steps, 200 knots)                     | 104.721000  | 104.325000  | -0.38%          | -0.86%  | -0.24%  | pass   |
| interp_perf    | Linear interp (50 knots, 10K queries)                                          | 0.042160    | 0.042667    | +1.20%          | +0.58%  | +2.68%  | pass   |
| krylov_perf    | BCGSolve (500x500 tridiag)                                                     | 0.062666    | 0.061270    | -2.23%          | -1.91%  | -3.65%  | pass   |
| krylov_perf    | CGSolve (500x500 tridiag)                                                      | 0.051705    | 0.051973    | +0.52%          | +0.82%  | -0.14%  | pass   |
| banded_perf    | TriDecomp MultiplyLeft (10K)                                                   | 0.004494    | 0.004495    | +0.02%          | +0.00%  | +0.02%  | pass   |
| banded_perf    | TriDiagonal Decompose (10K)                                                    | 0.077163    | 0.077109    | -0.07%          | +0.02%  | -0.07%  | pass   |
| banded_perf    | TriDiagonal MultiplyLeft (10K)                                                 | 0.004686    | 0.004687    | +0.02%          | +0.02%  | +0.02%  | pass   |
| cholesky_perf  | CholeskyDecompose (200x200)                                                    | 0.225528    | 0.225333    | -0.09%          | -0.12%  | -0.09%  | pass   |
| cholesky_perf  | CholeskyDecompose+Multiply (200x200)                                           | 0.225085    | 0.225061    | -0.01%          | -0.01%  | -0.17%  | pass   |
| rate_risk_perf | Quote risk aggregate (joint XCCY)                                              | 0.220250    | 0.220076    | -0.08%          | -0.08%  | +2.65%  | pass   |
| rate_risk_perf | Quote risk aggregate (single curve)                                            | 0.008889    | 0.008873    | -0.18%          | +0.70%  | -0.18%  | pass   |
| rate_risk_perf | Quote risk aggregate (staged XCCY basis)                                       | 0.060456    | 0.060386    | -0.12%          | +0.22%  | -0.46%  | pass   |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=10)                                      | 1.148000    | 1.121000    | -2.35%          | -3.45%  | -1.31%  | pass   |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=16)                                      | 1.739000    | 1.717000    | -1.27%          | -1.64%  | -1.27%  | pass   |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=5)                                       | 0.821319    | 0.815412    | -0.72%          | +0.61%  | -0.72%  | pass   |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=10)                                     | 11.058000   | 10.994000   | -0.58%          | +2.81%  | -1.02%  | pass   |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=16)                                     | 17.585000   | 17.244000   | -1.94%          | -2.11%  | -1.94%  | pass   |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=5)                                      | 8.027000    | 7.862000    | -2.06%          | -2.06%  | -2.18%  | pass   |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=10)                       | 1.087000    | 1.073000    | -1.29%          | -1.29%  | -1.65%  | pass   |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=16)                       | 1.667000    | 1.627000    | -2.40%          | -1.97%  | -2.40%  | pass   |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=5)                        | 0.774135    | 0.764213    | -1.28%          | -1.68%  | -1.28%  | pass   |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=10)                      | 10.828000   | 10.863000   | +0.32%          | -0.24%  | +0.32%  | pass   |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=16)                      | 17.359000   | 17.230000   | -0.74%          | -0.74%  | -0.49%  | pass   |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=5)                       | 7.797000    | 7.653000    | -1.85%          | -1.85%  | -1.70%  | pass   |
| rate_risk_perf | Quote risk portfolio joint ANALYTIC (24 XCCY x N=10/block)                     | 4.513000    | 4.430000    | -1.84%          | -1.84%  | -2.16%  | pass   |
| rate_risk_perf | Quote risk portfolio joint BUMPED (24 XCCY x N=10/block)                       | 4.572000    | 4.489000    | -1.82%          | -1.92%  | -1.36%  | pass   |
| rate_risk_perf | Quote risk portfolio single ANALYTIC (120 deposits x N=5)                      | 0.121329    | 0.120858    | -0.39%          | -0.10%  | -1.95%  | pass   |
| rate_risk_perf | Quote risk portfolio single BUMPED (120 deposits x N=16)                       | 0.170510    | 0.168926    | -0.93%          | -1.15%  | -0.79%  | pass   |
| rate_risk_perf | Quote risk portfolio staged ANALYTIC (24 XCCY x N=16)                          | 3.551000    | 3.495000    | -1.58%          | -1.58%  | -0.28%  | pass   |
| rate_risk_perf | Quote risk portfolio staged BUMPED (24 XCCY x N=5)                             | 1.290000    | 1.262000    | -2.17%          | -0.23%  | -3.30%  | pass   |
| rate_risk_perf | Rate AAD 2 components (1024 IRS x 8 nodes, fixed maturities)                   | 5.149000    | 5.083000    | -1.28%          | -1.07%  | -1.68%  | pass   |
| rate_risk_perf | Rate AAD 2 components (256 IRS x 8 nodes, fixed maturities)                    | 1.268000    | 1.251000    | -1.34%          | -1.57%  | -0.32%  | pass   |
| rate_risk_perf | Rate AAD 2 components (32 IRS x 8 nodes, fixed maturities)                     | 0.153565    | 0.151687    | -1.22%          | -0.66%  | -4.00%  | pass   |
| rate_risk_perf | Rate OIS daily compounding sweep (5Y quarterly x daily)                        | 0.215196    | 0.209095    | -2.84%          | -3.45%  | +0.80%  | pass   |
| rate_risk_perf | Rate PV (1024 IRS x 8 nodes, fixed maturities)                                 | 2.457000    | 2.462000    | +0.20%          | -1.00%  | +0.20%  | pass   |
| rate_risk_perf | Rate PV (256 IRS x 8 nodes, fixed maturities)                                  | 0.607587    | 0.608711    | +0.18%          | -3.02%  | +2.23%  | pass   |
| rate_risk_perf | Rate PV (32 IRS x 8 nodes, fixed maturities)                                   | 0.074994    | 0.075120    | +0.17%          | +0.17%  | +1.19%  | pass   |
| rate_risk_perf | Rate XCCY batch serial (24 XCCY x 5 components)                                | 0.782017    | 0.768234    | -1.76%          | -1.76%  | -1.86%  | pass   |
| rate_risk_perf | Rate batch serial (120 IRS x 2 components)                                     | 1.271000    | 1.260000    | -0.87%          | -0.24%  | -1.18%  | pass   |
| rate_risk_perf | Rate prepare geometry (256 IRS x 8 nodes, fixed maturities)                    | 0.537403    | 0.546294    | +1.65%          | +1.77%  | +1.43%  | pass   |
| rate_risk_perf | Rate prepare geometry (32 IRS x 8 nodes, fixed maturities)                     | 0.064638    | 0.063799    | -1.30%          | +2.63%  | -1.35%  | pass   |
| rate_risk_perf | Rate prepared AAD 2 components (256 IRS x 8 nodes, fixed maturities)           | 0.717377    | 0.712067    | -0.74%          | +2.19%  | -0.77%  | pass   |
| rate_risk_perf | Rate prepared AAD 2 components (32 IRS x 8 nodes, fixed maturities)            | 0.086028    | 0.084266    | -2.05%          | +1.89%  | -2.06%  | pass   |
| rate_risk_perf | Rate prepared PV (256 IRS x 8 nodes, fixed maturities)                         | 0.107182    | 0.106647    | -0.50%          | +1.59%  | -0.63%  | pass   |
| rate_risk_perf | Rate prepared PV (32 IRS x 8 nodes, fixed maturities)                          | 0.012591    | 0.012803    | +1.68%          | +4.14%  | +1.68%  | pass   |
| rate_risk_perf | Rate single-trade sweeps (240 IRS calls)                                       | 2.013000    | 1.981000    | -1.59%          | -0.25%  | -2.61%  | pass   |

## Production experiments: failures, control and fixed confirmation

The initial original-baseline two-by-ten run exits one. It passes 34/35 ordinary
cases, 5/6 GSR market cases and all three LSM profiles. Two cases exceed +4% in
both rounds. Their failure evidence is preserved; it is not replaced by a pass.

The first investigation compares the exact same new head against the previously
published native implementation. All five profiles are measured with two-by-ten
samples: all 44 cases pass. The control's baseline binary was built at
`8e1ef0941a949059858aa78cab46af4cb50b41e6` and is production/benchmark-equivalent
to the published `9b5febcca79643e817999e76e8f5dda44922d618`. The original recorded
binary digest still matches. This control bounds the removal increment's cost;
it does not substitute the old publication for the original full-goal baseline.

The initial control metadata mistakenly annotated its extra built-at-SHA field
with the publication SHA after mutating a copied dictionary. The source-equivalence
check used the correct built-at SHA and binary identity; timings are unaffected.
The original metadata is retained. Use
`native-only-prior-head-control-environment-corrected.json`, which records
both SHAs and the annotation correction explicitly.

Before starting the confirmation, the schedule is fixed to two rounds of thirty
samples per side for every one of the five profiles, with identical workloads,
paths, threads, affinity, parsing, reduction, numeric tolerance and +4% rule.
All 44 cases pass this larger experiment. It does not alter or rerun the formal
nine-target gate. It is an additional borderline-result investigation, allowed
by the benchmark workflow, with no selective case stopping or threshold change.

The GSR four-node AAD fit remains borderline: +3.52%/+5.24% in the confirmation.
Only one round exceeds +4%, so it passes the existing two-round rule. Do not
interpret acceptance as zero runtime cost or prove that the initial gap was only
noise. The shared-host minima and the published-head control do not identify a
causal source change. A future increment must keep this case in acceptance.

Raw evidence and all samples are retained in
`native-only-production-paired`,
`native-only-prior-head-production-control` and
`native-only-production-confirmation-30`, each with `results.json`,
`summary.md`, command outputs and its sibling `.log`.
The original helper is unchanged. The new fixed-count helper digest is
`d15d83d4a08f22dd05ee13fd881bb72f56ca582d92c78c45f24df093376c15ae`;
its schedule is recorded in `native-only-confirmation-environment.json`.

| Profile                | Initially failed case                                     | Initial 2x10  | Published-head control 2x10 | Original-baseline confirmation 2x30 |
|------------------------|-----------------------------------------------------------|---------------|-----------------------------|-------------------------------------|
| ordinary               | GSR 1F 5Y swaption (1 price, order 16/32)                 | +5.75%/+6.84% | +0.21%/+1.13%               | -1.30%/-3.63%                       |
| gsr-market-calibration | GSR market 4-node fit, 12 expiry/tenor/strike quotes, AAD | +4.46%/+5.13% | +0.69%/+1.45%               | +3.52%/+5.24%                       |

Every LSM PV and every risk entry agrees across sides in every initial,
control and confirmation sample at relative/absolute tolerance 1e-10. Ordinary
and GSR market executable assertions pass, but the runner does not extract every
ordinary/calibration numeric result into a complete cross-side matrix. Do not
extend the LSM numeric-equality claim to all ordinary/calibration results.

The next table lists every original-baseline production case: combined minimum
durations from the fixed confirmation, both confirmation-round deltas, both
initial-round deltas and the larger experiment's verdict.

| Profile                | Case                                                                             | Base min ms | Head min ms | Confirm round 1 | Confirm round 2 | Initial rounds | Result |
|------------------------|----------------------------------------------------------------------------------|-------------|-------------|-----------------|-----------------|----------------|--------|
| ordinary               | GSR 1F 5Y swaption (1 price, order 16/32)                                        | 0.013605    | 0.013428    | -1.30%          | -3.63%          | +5.75%/+6.84%  | pass   |
| ordinary               | GSR 1F bond (100K paths x 4 steps)                                               | 6.843000    | 6.825000    | +0.36%          | -0.26%          | +6.57%/+0.26%  | pass   |
| ordinary               | GSR 1F bond option (1000 prices)                                                 | 1.757000    | 1.717000    | -2.28%          | -1.39%          | -2.73%/+5.81%  | pass   |
| ordinary               | GSR 1F swap and Libor (10K paths x 4 steps)                                      | 15.780000   | 16.196000   | +2.88%          | +2.49%          | -2.51%/-0.78%  | pass   |
| ordinary               | GSR 2F 5Y swaption (1 price, order 16/32)                                        | 0.260715    | 0.269130    | +4.07%          | -0.16%          | +1.73%/-2.23%  | pass   |
| ordinary               | GSR 2F bond (100K paths x 4 steps)                                               | 9.658000    | 9.822000    | +0.09%          | +1.70%          | -2.49%/+1.53%  | pass   |
| ordinary               | GSR 2F bond option (1000 prices)                                                 | 2.512000    | 2.525000    | +0.52%          | -0.51%          | +0.76%/-0.08%  | pass   |
| ordinary               | GSR 2F swap and Libor (10K paths x 4 steps)                                      | 15.679000   | 15.814000   | -1.82%          | +0.86%          | -1.40%/+0.69%  | pass   |
| ordinary               | GSR 3F 5Y swaption (1 price, order 16/32)                                        | 0.268070    | 0.257750    | -3.94%          | -2.14%          | -0.84%/+0.08%  | pass   |
| ordinary               | GSR 3F bond (100K paths x 4 steps)                                               | 10.848000   | 10.838000   | -0.09%          | -0.74%          | +1.45%/+1.22%  | pass   |
| ordinary               | GSR 3F bond option (1000 prices)                                                 | 3.386000    | 3.302000    | -3.49%          | -2.48%          | -1.67%/-0.60%  | pass   |
| ordinary               | GSR 3F swap and Libor (10K paths x 4 steps)                                      | 15.732000   | 15.793000   | +0.39%          | -3.72%          | +0.48%/+1.73%  | pass   |
| ordinary               | GSR g calibration (3 quotes x 3 buckets)                                         | 0.198888    | 0.201215    | +1.17%          | +0.87%          | -0.05%/+2.85%  | pass   |
| ordinary               | LSMC regression degree=3 (100000 paths)                                          | 0.393268    | 0.369161    | -6.13%          | -3.18%          | +2.32%/-0.21%  | pass   |
| ordinary               | LSMC regression degree=3 ITM mask (100000 paths)                                 | 0.360496    | 0.367519    | -2.37%          | +2.25%          | +1.79%/+2.01%  | pass   |
| ordinary               | LSMC regression degree=8 (100000 paths)                                          | 0.688295    | 0.688521    | -4.08%          | +2.44%          | +2.05%/+0.21%  | pass   |
| ordinary               | LSMC regression degree=8 ITM mask (100000 paths)                                 | 0.674320    | 0.667071    | -3.97%          | +1.27%          | +0.80%/-0.16%  | pass   |
| ordinary               | LSMC regression features=2 degree=2 ITM mask (100000 paths)                      | 7.892000    | 7.886000    | -2.26%          | +0.90%          | +0.05%/+5.06%  | pass   |
| ordinary               | LSMC regression features=2 degree=3 ITM mask (100000 paths)                      | 13.605000   | 13.947000   | +2.51%          | -2.35%          | +1.71%/-3.47%  | pass   |
| ordinary               | LSMC regression features=3 degree=3 ITM mask (100000 paths)                      | 35.812000   | 35.773000   | -0.10%          | -1.12%          | +1.28%/-1.78%  | pass   |
| ordinary               | correlated BS path (100K x 12 steps x 1 assets)                                  | 10.319000   | 10.204000   | -3.29%          | -1.11%          | +2.03%/-3.68%  | pass   |
| ordinary               | correlated BS path (100K x 12 steps x 2 assets)                                  | 15.436000   | 15.438000   | -0.93%          | +0.01%          | -3.22%/-0.23%  | pass   |
| ordinary               | correlated BS path (100K x 12 steps x 3 assets)                                  | 20.691000   | 20.642000   | -0.24%          | -0.05%          | +1.60%/+0.11%  | pass   |
| ordinary               | hybrid flat-rate path (100K x 12 steps x 2 assets)                               | 38.693000   | 38.887000   | +1.16%          | -0.60%          | +0.45%/+0.38%  | pass   |
| ordinary               | hybrid logDF path (100K x 12 steps x 2 assets)                                   | 38.612000   | 38.680000   | -0.99%          | +1.05%          | +0.49%/-1.68%  | pass   |
| ordinary               | script engine bermudan exercise double compiled=false (100000 paths x 54 events) | 123.686000  | 119.002000  | -3.79%          | -1.18%          | -2.34%/+0.84%  | pass   |
| ordinary               | script engine bermudan exercise double compiled=true (100000 paths x 54 events)  | 112.611000  | 111.491000  | -0.64%          | -0.99%          | +3.42%/+0.64%  | pass   |
| ordinary               | script engine vanilla Number_ compiled=false (20000 paths x 1 events)            | 0.692159    | 0.702339    | +1.19%          | +2.41%          | -6.12%/-0.25%  | pass   |
| ordinary               | script engine vanilla Number_ compiled=true (20000 paths x 1 events)             | 0.643409    | 0.650927    | +2.26%          | +0.18%          | -3.41%/-2.60%  | pass   |
| ordinary               | script engine vanilla double compiled=false (200000 paths x 1 events)            | 1.896000    | 1.907000    | +0.21%          | +0.58%          | +0.63%/+0.90%  | pass   |
| ordinary               | script engine vanilla double compiled=true (200000 paths x 1 events)             | 1.751000    | 1.749000    | +0.57%          | -0.11%          | +1.66%/+6.10%  | pass   |
| ordinary               | script engine weekly barrier Number_ compiled=false (10000 paths x 52 events)    | 9.238000    | 8.971000    | -2.89%          | -0.21%          | +0.19%/+4.59%  | pass   |
| ordinary               | script engine weekly barrier Number_ compiled=true (10000 paths x 52 events)     | 7.682000    | 7.593000    | -0.87%          | -2.78%          | -0.06%/+0.19%  | pass   |
| ordinary               | script engine weekly barrier double compiled=false (100000 paths x 52 events)    | 35.922000   | 36.593000   | +0.83%          | +1.87%          | +0.82%/-1.09%  | pass   |
| ordinary               | script engine weekly barrier double compiled=true (100000 paths x 52 events)     | 21.886000   | 21.719000   | -1.36%          | +0.86%          | -1.15%/-4.09%  | pass   |
| gsr-market-calibration | GSR lagged swaption, 512 outer x 32 inner paths                                  | 6.505000    | 6.350000    | -2.38%          | -4.69%          | -0.62%/-3.11%  | pass   |
| gsr-market-calibration | GSR market 4-node fit, 12 expiry/tenor/strike quotes, AAD                        | 80.940000   | 83.939000   | +3.52%          | +5.24%          | +4.46%/+5.13%  | pass   |
| gsr-market-calibration | GSR market 4-node fit, 12 expiry/tenor/strike quotes, FD                         | 23.076000   | 23.343000   | +1.16%          | +0.31%          | -1.88%/-0.38%  | pass   |
| gsr-market-calibration | GSR market 48-node price Jacobian, 12 quotes, AAD                                | 20.463000   | 20.919000   | +2.67%          | +1.47%          | +2.47%/+3.36%  | pass   |
| gsr-market-calibration | GSR market 48-node price Jacobian, 12 quotes, FD                                 | 51.115000   | 50.991000   | -0.24%          | +0.18%          | -0.50%/+1.08%  | pass   |
| gsr-market-calibration | GSR market vol + native curve quote risk, one fitted node                        | 22.868000   | 22.660000   | -0.91%          | -1.76%          | +1.00%/-2.85%  | pass   |
| lsm-bs-tree            | lsm-bs-tree                                                                      | 109.126784  | 109.196012  | +0.59%          | +0.06%          | +0.68%/+0.44%  | pass   |
| lsm-bs-compiled        | lsm-bs-compiled                                                                  | 107.596163  | 107.700820  | +0.10%          | -0.07%          | -1.96%/-2.00%  | pass   |
| lsm-lv-daily           | lsm-lv-daily                                                                     | 530.434667  | 527.265665  | -0.60%          | +0.24%          | -1.92%/-0.17%  | pass   |

## Curve calibration supplement

All 25 comparable cases pass two-by-ten pairing under the same policy. The
8-node DF query's +7.42%/+0.48% movement remains visible. No output/case is lost.
This is affected-path information outside the nine-target formal verdict.
Raw evidence: `native-only-curve-paired/{results.json,summary.md}`, all command
outputs and `native-only-curve-paired.log`; process exit zero.

| Case                                                            | Base min ms | Head min ms | Round 1 | Round 2 | Result |
|-----------------------------------------------------------------|-------------|-------------|---------|---------|--------|
| CalibrateYieldCurve LOG_CUBIC_NATURAL ANALYTIC +DIAG (23 swaps) | 1.453000    | 1.454000    | +0.07%  | +0.41%  | pass   |
| CalibrateYieldCurve LOG_CUBIC_NATURAL ANALYTIC SOLVE (23 swaps) | 0.854891    | 0.844037    | -1.22%  | -1.27%  | pass   |
| CalibrateYieldCurve LOG_CUBIC_NATURAL BUMPED +DIAG (23 swaps)   | 2.165000    | 2.140000    | -0.96%  | -1.15%  | pass   |
| CalibrateYieldCurve LOG_CUBIC_NATURAL BUMPED SOLVE (23 swaps)   | 1.368000    | 1.360000    | -2.30%  | +0.29%  | pass   |
| CalibrateYieldCurve LOG_LINEAR ANALYTIC +DIAG (23 swaps)        | 1.367000    | 1.341000    | -0.36%  | -1.90%  | pass   |
| CalibrateYieldCurve LOG_LINEAR ANALYTIC SOLVE (23 swaps)        | 0.778487    | 0.777844    | -3.86%  | +0.90%  | pass   |
| CalibrateYieldCurve LOG_LINEAR APPROXIMATE (23 swaps)           | 10.377000   | 10.156000   | -3.19%  | -1.03%  | pass   |
| CalibrateYieldCurve LOG_LINEAR BUMPED +DIAG (23 swaps)          | 1.805000    | 1.807000    | +1.71%  | +0.11%  | pass   |
| CalibrateYieldCurve LOG_LINEAR BUMPED SOLVE (23 swaps)          | 1.163000    | 1.181000    | +1.55%  | -0.50%  | pass   |
| CalibrateYieldCurve MIXED ANALYTIC +DIAG (23 swaps)             | 1.399000    | 1.382000    | -1.22%  | -1.98%  | pass   |
| CalibrateYieldCurve MIXED ANALYTIC SOLVE (23 swaps)             | 0.793771    | 0.786777    | -2.24%  | -0.88%  | pass   |
| CalibrateYieldCurve MIXED BUMPED +DIAG (23 swaps)               | 1.901000    | 1.889000    | +1.53%  | -0.84%  | pass   |
| CalibrateYieldCurve MIXED BUMPED SOLVE (23 swaps)               | 1.206000    | 1.212000    | +1.00%  | +0.50%  | pass   |
| CalibrateYieldCurve PWC ANALYTIC +DIAG (23 swaps)               | 1.082000    | 1.074000    | -0.18%  | -2.19%  | pass   |
| CalibrateYieldCurve PWC ANALYTIC SOLVE (23 swaps)               | 0.598421    | 0.583468    | +0.77%  | -5.99%  | pass   |
| CalibrateYieldCurve PWC BUMPED +DIAG (23 swaps)                 | 0.936752    | 0.940800    | +1.54%  | -0.95%  | pass   |
| CalibrateYieldCurve PWC BUMPED SOLVE (23 swaps)                 | 0.641032    | 0.641499    | +0.07%  | +0.94%  | pass   |
| CalibrateYieldCurve PWL ANALYTIC +DIAG (23 swaps)               | 1.264000    | 1.222000    | -1.66%  | -4.46%  | pass   |
| CalibrateYieldCurve PWL ANALYTIC SOLVE (23 swaps)               | 0.665615    | 0.644846    | -3.13%  | -0.81%  | pass   |
| CalibrateYieldCurve PWL BUMPED +DIAG (23 swaps)                 | 1.535000    | 1.556000    | +2.87%  | +0.78%  | pass   |
| CalibrateYieldCurve PWL BUMPED SOLVE (23 swaps)                 | 0.941572    | 0.951637    | +2.10%  | +1.07%  | pass   |
| PWL DF queries (4096 x 24 nodes)                                | 0.047328    | 0.047572    | +0.52%  | -0.62%  | pass   |
| PWL DF queries (4096 x 256 nodes)                               | 0.081703    | 0.079976    | +1.42%  | -2.36%  | pass   |
| PWL DF queries (4096 x 64 nodes)                                | 0.054122    | 0.054472    | +0.65%  | +0.48%  | pass   |
| PWL DF queries (4096 x 8 nodes)                                 | 0.042322    | 0.043601    | +7.42%  | +0.48%  | pass   |

## Resource observations

Three alternating baseline/head process pairs are retained for ordinary MC and
daily local-volatility LSM. This is resource observation, not a memory-budget
implementation or a paired performance gate. Every process has zero major page
faults. Every daily LSM numeric result agrees at relative/absolute tolerance
1e-10. Process-wide peak RSS is not tape-only memory or allocated-capacity detail.

| Profile      | Side | RSS samples KiB     | RSS range KiB | Major faults |
|--------------|------|---------------------|---------------|--------------|
| ordinary     | base | 99812, 99836, 99856 | 99812–99856   | 0, 0, 0      |
| ordinary     | head | 99944, 99864, 99696 | 99696–99944   | 0, 0, 0      |
| lsm-lv-daily | base | 11928, 11872, 11612 | 11612–11928   | 0, 0, 0      |
| lsm-lv-daily | head | 11688, 11980, 12000 | 11688–12000   | 0, 0, 0      |

Ordinary RSS ranges overlap; daily LSM ranges also overlap, while the head's
largest observation is 72 KiB above the baseline's largest. These three samples
cannot establish an exact memory delta or a hard peak-memory guarantee.
Raw records: `native-only-resources/results.json`, all process `.time.txt`
and numeric output files, and `native-only-resources.log`.

## Coverage and final boundary

- Native scalar/vector propagation, clearing and rewinding: `tape_perf`.
- Curve harvesting and calibration: `jacobian_perf` plus the 25-case supplement.
- Existing PDE/RNG/interpolation/linear algebra: their seven formal targets.
- Native rate aggregation, quote calibration semantics and high-water observations:
  `rate_risk_perf`. Native observations formerly protected by the removed macro
  remain unconditional; no timed workload is changed.
- Scoped ordinary/LSM workers and GSR native Jacobian wrapper: all five unchanged
  `script_mc_perf` profiles, including the added existing market-calibration mode.
- All other benchmark targets are smoke only. No additional target is admitted
  into the formal verdict. Thread-count scaling, phase-level production telemetry,
  total-memory budgets and new multi-output/budgeted execution remain separate
  P01/P02/P03/F02/D04 work; this increment does not claim to deliver them.

Overall performance verdict: **no regression under the existing acceptance
policy**, with the retained initial failures and borderline GSR confirmation
described above. This statement is limited to the measured cases/configuration;
it is not proof of identical runtime or a universal no-regression guarantee.

Next: publish the reviewed native-only increment, inspect exact new-head CI
(including complete sanitizer libraries, Windows and wheels), and finish the
D00/D03 audits. Verify that any subsequent evidence-only publication commit has
an empty production/benchmark diff from the measured head and unchanged binary
digests before extending this evidence to it. D01/D02 final audits and remaining
P01 work still precede full Stage A completion; Stages B/C/D remain active scope.
