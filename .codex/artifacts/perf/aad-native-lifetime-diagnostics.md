# Native AAD lifetime diagnostics: acceptance evidence

Status: active D02 acceptance. The initial `c83bcc9` mathematical, lifetime, build,
installed ABI and default-OFF performance checks pass. A subsequent assignment
failure audit adds three RED cases and a diagnostic-only correction. Corrected
full ON/OFF builds, CTest and fresh default-OFF pairing pass. The
[corrected-source acceptance report](aad-native-lifetime-diagnostics-corrected.md)
retains its final tables and a later calibration failure/confirmation trail.
Publication CI and the final requirement audit remain pending.
This increment and the bounded `BlockList_::Size` repair do not complete Stage A
or the full AAD implementation goal.

## Scope and immutable sources

`DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS` is native-only and defaults to OFF.
ON checks number ownership, recording epochs, live slots, allocation generations,
and scalar/vector layouts before active node access. Saved expressions and stale
assignments are checked before result allocation or destination mutation.
Full resets invalidate the graph; suffix restores preserve prefix bindings and
accumulated adjoints. Cached primal extraction remains passive, and explicit
registration/rebinding starts a fresh independent chain.

The definition propagates through public/installed targets and bindings.
Configuration rejects diagnostic selection with XAD, CoDiPack, or Adept.
Scoped owner/state/checkpoint checks remain enabled independently of this option.
Raw internal block-list mutation and arbitrary dangling C++ references remain
outside the diagnostic contract.

- Baseline: `b5e3caca85bb1a5b1aa3acf7898b387832d443c8`, the original goal baseline,
  retained detached and unchanged.
- Measured implementation: `c83bcc979ac294c0684c021f1191a2bb144e8fd0`.
- Assignment-corrected implementation: `4c2f0d57f61d788e1ec717fb11c5e1fa0b0bd005`;
  its full build checks pass; fresh pairing is recorded in the corrected report.
- Separate bounded-size repair: `5e6d0ae`, preceding the diagnostic commit.
- Sources: `/tmp/dal-aad-baseline` and `/tmp/dal-aad-implementation`.
- Default native builds: `/tmp/dal-aad-evidence/base-build` and `head-build`.
- Diagnostic correctness build: `native-diagnostics-build`; external correctness
  build: `codipack-build`, under the same evidence root.
- Matching Release/C++17, GCC 15.2, CMake 4.2.3, Unix Makefiles, Eigen, pinned
  dependency gitlinks, benchmark enablement and native-architecture opt-in OFF.
- Shared WSL2/i9-13900HX host. Desktop/hypervisor activity is uncontrolled;
  all comparisons use `DAL_NUM_THREADS=4` and affinity `4,6,8,10`.
  No concurrent DAL builds/tests or production/benchmark edits occur during pairing.
- Ten interleaved process samples per side in each of two rounds, alternating
  the first side; per-round reduction is `min`; failure requires both rounds
  above +4%. Existing nine targets and the Sobol 10x ceiling are unchanged.

`/tmp/dal-aad-evidence/lifetime-off-environment.json` records source/configuration,
CPU/toolchain, directly verified dependency SHAs and binary SHA-256 digests.
After all timing/resource observations, source cleanliness, SHAs and every
measured binary digest are checked again and remain unchanged.
These tables describe the initial frozen `c83bcc9` run. They must not be reused
as the final verdict for subsequent production changes.

## RED, GREEN and coverage

- The original configuration ignores the proposed option. The declared-BOOL/
  exported-definition oracle first fails, then passes with the CMake option.
  All three incompatible-backend configurations reject with the native-only error.
- Three stale-handle/assignment regressions first fail: full rewind, suffix
  restore before reuse, and assignment from an overwritten suffix.
- All 24 diagnostic cases pass ASan/UBSan with leak detection, including
  same-address reuse, foreign/exited threads, nested saved expressions,
  container/model copies, mode/width errors, counter exhaustion and reset recovery
  after partial allocation. The sanitizer executable recompiles diagnostic
  `tape.cpp`, recording and these tests; it uses the default archive for supporting
  non-active symbols. This is focused evidence, not a fully sanitized ON library.
- The complete native ON core/public/portable Excel CTest run passes 2,307 cases.
  The subsequently added model-copy case passes against the complete ON library.
  After include-order review/rebuild, all 71 lifetime/recording/tape/statistics/
  block-list cases pass against that library.
- Native ON Python passes 792 tests with one skip; the module receives the PUBLIC
  diagnostic definition. Installed ON and OFF external consumers both inherit
  their definition/package setting and verify analytic gradients; ON also verifies
  rejection of a closed recording's output.
- Native OFF passes 2,318 functional/example CTest cases, including 33 regular
  examples and the slow European MC example. All 21 quiet serial benchmark smoke
  checks pass separately. Final include-order rebuild passes 47 focused OFF cases.
- CoDiPack OFF passes 2,257 complete CTest cases. No upstream active-number
  diagnostic support is claimed; incompatible diagnostic selection is tested.
- Bounded-size repair: a full-final-block RED times out after five seconds;
  all eleven block-list cases pass ASan/UBSan after the occupied-slot count fix.
- Documentation integrity passes for all 70 Markdown files. CI classifier tests
  and YAML matrix/configuration checks pass. Reviewed new functions stay within
  the existing cyclomatic-complexity limit of eight.

The include-order review does not change runtime behavior. Final release binaries
are rebuilt before pairing. New CI legs explicitly build diagnostic native Python,
Windows/installed consumers, and a complete diagnostic ASan/UBSan library/test
configuration; their publication results must still be inspected.

Post-pairing review identifies two independent-registration/rebinding failures:
counter exhaustion changes a destination's cached value from 6 to 99 while keeping
its old node. A separate expression-assignment RED changes 6 to 10 under the same
allocation rejection. Preserve `lifetime-rebind-red.log` and
`lifetime-expression-rebind-red.log`. Diagnostic double assignment now binds
before changing its cached primal; diagnostic expression assignment materializes
a replacement before committing it. The default OFF assignment bodies are retained.
An added scoped-registration case verifies failure retention and recovery by the
next independent scope. All 28 diagnostic cases now pass focused ASan/UBSan
with leak detection. Corrected full ON CTest passes all 2,313 cases, including
Python; default OFF passes all 2,318 functional/example cases. Both complete
rebuilds succeed. Retain `lifetime-assignment-{on,off}-{build,ctest}.log`.
Corrected default performance is verified in the linked report; publication CI
still requires exact-head verification.
Fresh corrected ON/OFF installs and external consumers also pass. Retain
`lifetime-assignment-install-{on,off}.log` and
`lifetime-assignment-consumer-{on,off}{,-configure,-build}.log`; these consumers
verify the exported setting, inherited definition, analytic derivative and ON
closed-recording rejection against the newly installed libraries.

## Layout and resource observations

On this 64-bit host, OFF size/alignment match the original baseline exactly:
`Number_` 16/8, `TapNode_` 40/8, and `Tape_` 368/8 bytes. Fresh OFF tape object
symbols contain none of the diagnostic identity, validity, reset, or allocation
functions. All optional fields/checks/counters are conditionally compiled out;
there is no release registry or per-node atomic update.

ON size/alignment are 64/8, 56/8 and 424/8 respectively. This is opt-in diagnostic
storage overhead, not a release optimization. The lifetime identity uses an atomic
claim once per diagnostic tape; allocation generations are tape-local.

Three alternating four-thread RSS pairs for ordinary production workloads and
daily local-vol LSM are retained below. All observations have zero major faults;
daily LSM PV and every risk entry agree at relative/absolute tolerance 1e-10.
RSS ranges overlap; no material increase is observed in these cases. This is not
a proof of a process-wide budget or of all ON diagnostic workloads.

| Profile      | Pair | Base RSS KiB | Head RSS KiB | Delta  | Major faults base/head |
|--------------|------|--------------|--------------|--------|------------------------|
| ordinary     | 1    | 99720        | 99880        | +0.16% | 0/0                    |
| ordinary     | 2    | 99804        | 99468        | -0.34% | 0/0                    |
| ordinary     | 3    | 99444        | 99664        | +0.22% | 0/0                    |
| lsm-lv-daily | 1    | 11640        | 11944        | +2.61% | 0/0                    |
| lsm-lv-daily | 2    | 11852        | 11924        | +0.61% | 0/0                    |
| lsm-lv-daily | 3    | 12008        | 11952        | -0.47% | 0/0                    |

## Default nine-target performance

All 65 existing comparable cases pass; ten head-only cases remain informational.
Native 10-channel propagation records -4.25%/-2.94%, clear/re-record +1.38%/+0.31%,
and the small dense reference Jacobian -12.03%/-12.31% in the two rounds.
These are whole-branch comparisons, including prior P0/D01 changes; they are not
speedup claims for the diagnostic option itself.

Prepared 32-trade and 256-trade PV cases move +6.14%/+1.74% and +4.69%/+3.12%.
Both pass the unchanged two-round policy, and all positive movements remain in
the table. The precise-opt-in/fast Sobol ratio is 9.33x, within the 10x ceiling.

| Target         | Case                                                                           | Base min ms | Head min ms | Round 1 | Round 2 | Result   |
|----------------|--------------------------------------------------------------------------------|-------------|-------------|---------|---------|----------|
| tape_perf      | Clear + re-record (100K nodes)                                                 | 0.854687    | 0.866473    | +1.38%  | +0.31%  | pass     |
| tape_perf      | PropagateToStart (100K nodes)                                                  | 0.389796    | 0.387594    | -0.41%  | -0.56%  | pass     |
| tape_perf      | PropagateToStart multi-mode (100K nodes, 10 results)                           | 0.499217    | 0.484546    | -4.25%  | -2.94%  | pass     |
| tape_perf      | Rewind + re-record (100K nodes)                                                | 0.567611    | 0.568413    | -0.02%  | +0.15%  | pass     |
| tape_perf      | ZeroAdjoints sweep (100K nodes)                                                | 0.110965    | 0.111790    | +0.61%  | +0.75%  | pass     |
| tape_perf      | PropagateToStart multi-mode (50K steps, 1 result) (new coverage)               | —           | 0.385160    | —       | —       | new/info |
| tape_perf      | PropagateToStart multi-mode (50K steps, 16 results) (new coverage)             | —           | 0.659382    | —       | —       | new/info |
| tape_perf      | PropagateToStart multi-mode (50K steps, 4 results) (new coverage)              | —           | 0.383333    | —       | —       | new/info |
| tape_perf      | PropagateToStart multi-mode (50K steps, 64 results) (new coverage)             | —           | 6.516000    | —       | —       | new/info |
| tape_perf      | PropagateToStart passive constants (50K steps) (new coverage)                  | —           | 0.147351    | —       | —       | new/info |
| tape_perf      | Rewind + passive-constant recording (50K steps) (new coverage)                 | —           | 0.243415    | —       | —       | new/info |
| jacobian_perf  | AnalyticJacobian dense harvest (24 x 23)                                       | 0.005352    | 0.004702    | -12.03% | -12.31% | pass     |
| jacobian_perf  | AnalyticJacobian row-width harvest (24 x 23)                                   | 0.005378    | 0.004654    | -13.25% | -13.46% | pass     |
| jacobian_perf  | HarvestCurveJacobian dense (23 outputs x 24 parameters) (new coverage)         | —           | 0.004323    | —       | —       | new/info |
| jacobian_perf  | HarvestCurveJacobian dense (95 outputs x 96 parameters) (new coverage)         | —           | 0.061861    | —       | —       | new/info |
| jacobian_perf  | HarvestCurveJacobian proven prefix (23 outputs x 24 parameters) (new coverage) | —           | 0.003775    | —       | —       | new/info |
| jacobian_perf  | HarvestCurveJacobian proven prefix (95 outputs x 96 parameters) (new coverage) | —           | 0.057714    | —       | —       | new/info |
| pde_perf       | ThetaScheme rollback (200x200 CN)                                              | 0.214261    | 0.214906    | +1.09%  | +0.17%  | pass     |
| pde_perf       | ThetaScheme rollback (200x200 implicit)                                        | 0.214677    | 0.214519    | -2.52%  | -0.07%  | pass     |
| pde_perf       | ThetaScheme rollback (200x2000 explicit)                                       | 0.657053    | 0.659822    | +0.55%  | +0.28%  | pass     |
| rng_perf       | BrownianBridge FillNormal (100K x 10D)                                         | 6.399000    | 6.377000    | -0.34%  | -1.21%  | pass     |
| rng_perf       | IRN SkipNormalTo (100K x 10D)                                                  | 7.760000    | 7.760000    | +0.19%  | -0.50%  | pass     |
| rng_perf       | MRG32 SkipNormalTo (100K x 10D)                                                | 0.001293    | 0.001292    | -0.08%  | +0.15%  | pass     |
| rng_perf       | MRG32k3a FillNormal (100K x 10D)                                               | 19.556000   | 19.521000   | +0.57%  | -0.49%  | pass     |
| rng_perf       | ShuffledIRN FillNormal (100K x 10D)                                            | 10.656000   | 10.542000   | +0.56%  | -1.07%  | pass     |
| rng_perf       | Sobol FillNormal fast (100K x 10D)                                             | 4.005000    | 4.042000    | +1.17%  | +0.92%  | pass     |
| rng_perf       | Sobol FillNormal precise opt-in (100K x 10D)                                   | 37.373000   | 37.696000   | +0.31%  | +0.86%  | pass     |
| rng_perf       | Sobol FillUniform (100K x 10D)                                                 | 0.746336    | 0.746252    | -1.02%  | -0.01%  | pass     |
| interp_perf    | Cubic interp (50 knots, 10K queries)                                           | 0.050287    | 0.050295    | -0.23%  | +0.05%  | pass     |
| interp_perf    | Inlined linear LV-style (1e5 paths x 200 steps, 200 knots)                     | 100.481000  | 100.503000  | +0.73%  | +0.02%  | pass     |
| interp_perf    | Linear interp (50 knots, 10K queries)                                          | 0.042041    | 0.042161    | -0.46%  | +0.58%  | pass     |
| krylov_perf    | BCGSolve (500x500 tridiag)                                                     | 0.061172    | 0.060996    | -0.26%  | -0.29%  | pass     |
| krylov_perf    | CGSolve (500x500 tridiag)                                                      | 0.051539    | 0.051407    | -0.38%  | -0.26%  | pass     |
| banded_perf    | TriDecomp MultiplyLeft (10K)                                                   | 0.004492    | 0.004493    | -0.02%  | +0.02%  | pass     |
| banded_perf    | TriDiagonal Decompose (10K)                                                    | 0.077004    | 0.077027    | +0.01%  | +0.03%  | pass     |
| banded_perf    | TriDiagonal MultiplyLeft (10K)                                                 | 0.004685    | 0.004684    | +0.02%  | -0.06%  | pass     |
| cholesky_perf  | CholeskyDecompose (200x200)                                                    | 0.225521    | 0.225678    | +0.00%  | +0.07%  | pass     |
| cholesky_perf  | CholeskyDecompose+Multiply (200x200)                                           | 0.225100    | 0.225203    | -0.02%  | +0.06%  | pass     |
| rate_risk_perf | Quote risk aggregate (joint XCCY)                                              | 0.219618    | 0.219777    | +0.07%  | +0.01%  | pass     |
| rate_risk_perf | Quote risk aggregate (single curve)                                            | 0.008916    | 0.008938    | +2.88%  | +0.25%  | pass     |
| rate_risk_perf | Quote risk aggregate (staged XCCY basis)                                       | 0.060532    | 0.060122    | -0.68%  | -0.30%  | pass     |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=10)                                      | 1.147000    | 1.122000    | -1.74%  | -2.35%  | pass     |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=16)                                      | 1.723000    | 1.699000    | -2.02%  | -1.04%  | pass     |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=5)                                       | 0.821396    | 0.808798    | -1.00%  | -1.94%  | pass     |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=10)                                     | 11.070000   | 11.021000   | -0.87%  | +0.42%  | pass     |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=16)                                     | 17.292000   | 16.796000   | -2.87%  | -2.00%  | pass     |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=5)                                      | 7.904000    | 7.812000    | -1.20%  | -1.16%  | pass     |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=10)                       | 1.083000    | 1.060000    | -1.94%  | -2.12%  | pass     |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=16)                       | 1.653000    | 1.634000    | -2.04%  | -0.12%  | pass     |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=5)                        | 0.775794    | 0.761296    | -1.82%  | -1.87%  | pass     |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=10)                      | 10.932000   | 10.745000   | -1.23%  | -1.71%  | pass     |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=16)                      | 17.121000   | 17.015000   | -0.48%  | -0.97%  | pass     |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=5)                       | 7.722000    | 7.635000    | -1.13%  | -1.57%  | pass     |
| rate_risk_perf | Quote risk portfolio joint ANALYTIC (24 XCCY x N=10/block)                     | 4.510000    | 4.465000    | -0.53%  | -1.52%  | pass     |
| rate_risk_perf | Quote risk portfolio joint BUMPED (24 XCCY x N=10/block)                       | 4.516000    | 4.504000    | -0.24%  | -0.53%  | pass     |
| rate_risk_perf | Quote risk portfolio single ANALYTIC (120 deposits x N=5)                      | 0.121377    | 0.121558    | -0.29%  | +1.26%  | pass     |
| rate_risk_perf | Quote risk portfolio single BUMPED (120 deposits x N=16)                       | 0.170428    | 0.169857    | -0.50%  | -0.07%  | pass     |
| rate_risk_perf | Quote risk portfolio staged ANALYTIC (24 XCCY x N=16)                          | 3.529000    | 3.486000    | -1.22%  | -0.37%  | pass     |
| rate_risk_perf | Quote risk portfolio staged BUMPED (24 XCCY x N=5)                             | 1.268000    | 1.242000    | -1.87%  | -2.05%  | pass     |
| rate_risk_perf | Rate AAD 2 components (1024 IRS x 8 nodes, fixed maturities)                   | 5.123000    | 5.071000    | -1.02%  | -2.55%  | pass     |
| rate_risk_perf | Rate AAD 2 components (256 IRS x 8 nodes, fixed maturities)                    | 1.263000    | 1.253000    | -0.79%  | -0.87%  | pass     |
| rate_risk_perf | Rate AAD 2 components (32 IRS x 8 nodes, fixed maturities)                     | 0.153799    | 0.152009    | -1.16%  | -1.34%  | pass     |
| rate_risk_perf | Rate OIS daily compounding sweep (5Y quarterly x daily)                        | 0.214835    | 0.210078    | -2.02%  | -2.21%  | pass     |
| rate_risk_perf | Rate PV (1024 IRS x 8 nodes, fixed maturities)                                 | 2.468000    | 2.412000    | -1.94%  | -2.27%  | pass     |
| rate_risk_perf | Rate PV (256 IRS x 8 nodes, fixed maturities)                                  | 0.606255    | 0.598844    | +0.05%  | -1.91%  | pass     |
| rate_risk_perf | Rate PV (32 IRS x 8 nodes, fixed maturities)                                   | 0.074839    | 0.073859    | -0.96%  | -1.31%  | pass     |
| rate_risk_perf | Rate XCCY batch serial (24 XCCY x 5 components)                                | 0.781007    | 0.765059    | -2.72%  | -1.92%  | pass     |
| rate_risk_perf | Rate batch serial (120 IRS x 2 components)                                     | 1.274000    | 1.257000    | -1.17%  | -1.33%  | pass     |
| rate_risk_perf | Rate prepare geometry (256 IRS x 8 nodes, fixed maturities)                    | 0.534199    | 0.534430    | -1.08%  | +0.30%  | pass     |
| rate_risk_perf | Rate prepare geometry (32 IRS x 8 nodes, fixed maturities)                     | 0.064453    | 0.063895    | +2.55%  | -0.87%  | pass     |
| rate_risk_perf | Rate prepared AAD 2 components (256 IRS x 8 nodes, fixed maturities)           | 0.716642    | 0.716298    | -0.05%  | +0.53%  | pass     |
| rate_risk_perf | Rate prepared AAD 2 components (32 IRS x 8 nodes, fixed maturities)            | 0.085602    | 0.084140    | +2.06%  | -2.26%  | pass     |
| rate_risk_perf | Rate prepared PV (256 IRS x 8 nodes, fixed maturities)                         | 0.105703    | 0.109626    | +4.69%  | +3.12%  | pass     |
| rate_risk_perf | Rate prepared PV (32 IRS x 8 nodes, fixed maturities)                          | 0.012736    | 0.013017    | +6.14%  | +1.74%  | pass     |
| rate_risk_perf | Rate single-trade sweeps (240 IRS calls)                                       | 1.991000    | 1.966000    | -1.26%  | -1.35%  | pass     |

## Supplemental ordinary MC and LSM

All 35 common ordinary cases and all three LSM replay profiles pass.
Short vanilla AAD tree records -0.97%/+1.32%; compiled records +3.33%/+1.60%.
The earlier D01 short-path failure data remain in the lifecycle report and are
not replaced by this successful increment.

LSM profiles use BS tree/compiled with 512 training and 8,192 weekly pricing
paths, and compiled daily local vol with 256/4,096 paths. Every paired PV and
risk element agrees at relative/absolute tolerance 1e-10. Ordinary benchmark
cases retain their built-in numeric checks; this runner does not additionally
extract every ordinary case's price/risk into a cross-binary matrix.

| Profile         | Case                                                                             | Base r1 ms | Head r1 ms | Base r2 ms | Head r2 ms | Delta r1 | Delta r2 | Result |
|-----------------|----------------------------------------------------------------------------------|------------|------------|------------|------------|----------|----------|--------|
| ordinary        | GSR 1F 5Y swaption (1 price, order 16/32)                                        | 0.013066   | 0.013550   | 0.012941   | 0.012373   | +3.70%   | -4.39%   | pass   |
| ordinary        | GSR 1F bond (100K paths x 4 steps)                                               | 6.431000   | 6.420000   | 6.448000   | 6.504000   | -0.17%   | +0.87%   | pass   |
| ordinary        | GSR 1F bond option (1000 prices)                                                 | 1.676000   | 1.651000   | 1.629000   | 1.613000   | -1.49%   | -0.98%   | pass   |
| ordinary        | GSR 1F swap and Libor (10K paths x 4 steps)                                      | 14.530000  | 14.689000  | 14.687000  | 14.881000  | +1.09%   | +1.32%   | pass   |
| ordinary        | GSR 2F 5Y swaption (1 price, order 16/32)                                        | 0.256538   | 0.253303   | 0.250862   | 0.262346   | -1.26%   | +4.58%   | pass   |
| ordinary        | GSR 2F bond (100K paths x 4 steps)                                               | 9.032000   | 8.971000   | 9.011000   | 9.082000   | -0.68%   | +0.79%   | pass   |
| ordinary        | GSR 2F bond option (1000 prices)                                                 | 2.423000   | 2.335000   | 2.315000   | 2.370000   | -3.63%   | +2.38%   | pass   |
| ordinary        | GSR 2F swap and Libor (10K paths x 4 steps)                                      | 14.609000  | 15.050000  | 14.630000  | 14.745000  | +3.02%   | +0.79%   | pass   |
| ordinary        | GSR 3F 5Y swaption (1 price, order 16/32)                                        | 0.251477   | 0.249669   | 0.252401   | 0.249112   | -0.72%   | -1.30%   | pass   |
| ordinary        | GSR 3F bond (100K paths x 4 steps)                                               | 9.978000   | 9.891000   | 9.827000   | 10.358000  | -0.87%   | +5.40%   | pass   |
| ordinary        | GSR 3F bond option (1000 prices)                                                 | 3.337000   | 3.224000   | 3.210000   | 3.370000   | -3.39%   | +4.98%   | pass   |
| ordinary        | GSR 3F swap and Libor (10K paths x 4 steps)                                      | 15.045000  | 14.947000  | 14.887000  | 15.091000  | -0.65%   | +1.37%   | pass   |
| ordinary        | GSR g calibration (3 quotes x 3 buckets)                                         | 0.194288   | 0.195260   | 0.195275   | 0.196688   | +0.50%   | +0.72%   | pass   |
| ordinary        | LSMC regression degree=3 (100000 paths)                                          | 0.361772   | 0.359072   | 0.362580   | 0.361235   | -0.75%   | -0.37%   | pass   |
| ordinary        | LSMC regression degree=3 ITM mask (100000 paths)                                 | 0.339728   | 0.326692   | 0.345731   | 0.340838   | -3.84%   | -1.42%   | pass   |
| ordinary        | LSMC regression degree=8 (100000 paths)                                          | 0.633382   | 0.618118   | 0.674170   | 0.634650   | -2.41%   | -5.86%   | pass   |
| ordinary        | LSMC regression degree=8 ITM mask (100000 paths)                                 | 0.610373   | 0.600263   | 0.623453   | 0.636853   | -1.66%   | +2.15%   | pass   |
| ordinary        | LSMC regression features=2 degree=2 ITM mask (100000 paths)                      | 7.598000   | 7.622000   | 7.647000   | 7.671000   | +0.32%   | +0.31%   | pass   |
| ordinary        | LSMC regression features=2 degree=3 ITM mask (100000 paths)                      | 13.724000  | 13.552000  | 13.602000  | 13.494000  | -1.25%   | -0.79%   | pass   |
| ordinary        | LSMC regression features=3 degree=3 ITM mask (100000 paths)                      | 34.532000  | 34.329000  | 33.999000  | 34.481000  | -0.59%   | +1.42%   | pass   |
| ordinary        | correlated BS path (100K x 12 steps x 1 assets)                                  | 9.703000   | 9.382000   | 9.636000   | 9.689000   | -3.31%   | +0.55%   | pass   |
| ordinary        | correlated BS path (100K x 12 steps x 2 assets)                                  | 14.594000  | 14.069000  | 14.671000  | 14.548000  | -3.60%   | -0.84%   | pass   |
| ordinary        | correlated BS path (100K x 12 steps x 3 assets)                                  | 19.225000  | 19.051000  | 19.575000  | 19.367000  | -0.91%   | -1.06%   | pass   |
| ordinary        | hybrid flat-rate path (100K x 12 steps x 2 assets)                               | 36.331000  | 35.902000  | 36.260000  | 36.413000  | -1.18%   | +0.42%   | pass   |
| ordinary        | hybrid logDF path (100K x 12 steps x 2 assets)                                   | 35.958000  | 35.838000  | 36.617000  | 36.727000  | -0.33%   | +0.30%   | pass   |
| ordinary        | script engine bermudan exercise double compiled=false (100000 paths x 54 events) | 113.073000 | 112.943000 | 114.369000 | 113.931000 | -0.11%   | -0.38%   | pass   |
| ordinary        | script engine bermudan exercise double compiled=true (100000 paths x 54 events)  | 103.816000 | 102.318000 | 103.612000 | 105.407000 | -1.44%   | +1.73%   | pass   |
| ordinary        | script engine vanilla Number_ compiled=false (20000 paths x 1 events)            | 0.681471   | 0.674866   | 0.709352   | 0.718700   | -0.97%   | +1.32%   | pass   |
| ordinary        | script engine vanilla Number_ compiled=true (20000 paths x 1 events)             | 0.633424   | 0.654489   | 0.633560   | 0.643706   | +3.33%   | +1.60%   | pass   |
| ordinary        | script engine vanilla double compiled=false (200000 paths x 1 events)            | 1.926000   | 1.884000   | 1.911000   | 1.864000   | -2.18%   | -2.46%   | pass   |
| ordinary        | script engine vanilla double compiled=true (200000 paths x 1 events)             | 1.709000   | 1.700000   | 1.684000   | 1.693000   | -0.53%   | +0.53%   | pass   |
| ordinary        | script engine weekly barrier Number_ compiled=false (10000 paths x 52 events)    | 8.997000   | 8.863000   | 8.922000   | 8.724000   | -1.49%   | -2.22%   | pass   |
| ordinary        | script engine weekly barrier Number_ compiled=true (10000 paths x 52 events)     | 7.342000   | 7.251000   | 7.406000   | 7.145000   | -1.24%   | -3.52%   | pass   |
| ordinary        | script engine weekly barrier double compiled=false (100000 paths x 52 events)    | 34.021000  | 33.552000  | 34.441000  | 33.902000  | -1.38%   | -1.56%   | pass   |
| ordinary        | script engine weekly barrier double compiled=true (100000 paths x 52 events)     | 19.781000  | 19.833000  | 20.211000  | 20.189000  | +0.26%   | -0.11%   | pass   |
| lsm-bs-tree     | lsm-bs-tree                                                                      | 100.462914 | 100.999522 | 100.848404 | 100.309081 | +0.53%   | -0.53%   | pass   |
| lsm-bs-compiled | lsm-bs-compiled                                                                  | 97.643497  | 96.556824  | 97.965993  | 97.864787  | -1.11%   | -0.10%   | pass   |
| lsm-lv-daily    | lsm-lv-daily                                                                     | 486.589823 | 479.692316 | 482.739776 | 485.175796 | -1.42%   | +0.50%   | pass   |

## Supplemental calibration

All 25 common calibration cases pass under the same sampling/confirmation rule.
The benchmark's independent analytic/bump result checks remain enabled.

| Case                                                            | Base r1 ms | Head r1 ms | Base r2 ms | Head r2 ms | Delta r1 | Delta r2 | Result |
|-----------------------------------------------------------------|------------|------------|------------|------------|----------|----------|--------|
| CalibrateYieldCurve LOG_CUBIC_NATURAL ANALYTIC +DIAG (23 swaps) | 1.420000   | 1.397000   | 1.412000   | 1.392000   | -1.62%   | -1.42%   | pass   |
| CalibrateYieldCurve LOG_CUBIC_NATURAL ANALYTIC SOLVE (23 swaps) | 0.837945   | 0.830931   | 0.832635   | 0.834009   | -0.84%   | +0.17%   | pass   |
| CalibrateYieldCurve LOG_CUBIC_NATURAL BUMPED +DIAG (23 swaps)   | 2.140000   | 2.113000   | 2.117000   | 2.105000   | -1.26%   | -0.57%   | pass   |
| CalibrateYieldCurve LOG_CUBIC_NATURAL BUMPED SOLVE (23 swaps)   | 1.307000   | 1.322000   | 1.316000   | 1.361000   | +1.15%   | +3.42%   | pass   |
| CalibrateYieldCurve LOG_LINEAR ANALYTIC +DIAG (23 swaps)        | 1.329000   | 1.320000   | 1.343000   | 1.327000   | -0.68%   | -1.19%   | pass   |
| CalibrateYieldCurve LOG_LINEAR ANALYTIC SOLVE (23 swaps)        | 0.774787   | 0.765401   | 0.782653   | 0.763365   | -1.21%   | -2.46%   | pass   |
| CalibrateYieldCurve LOG_LINEAR APPROXIMATE (23 swaps)           | 10.171000  | 9.930000   | 10.110000  | 9.978000   | -2.37%   | -1.31%   | pass   |
| CalibrateYieldCurve LOG_LINEAR BUMPED +DIAG (23 swaps)          | 1.819000   | 1.797000   | 1.823000   | 1.819000   | -1.21%   | -0.22%   | pass   |
| CalibrateYieldCurve LOG_LINEAR BUMPED SOLVE (23 swaps)          | 1.157000   | 1.138000   | 1.147000   | 1.151000   | -1.64%   | +0.35%   | pass   |
| CalibrateYieldCurve MIXED ANALYTIC +DIAG (23 swaps)             | 1.355000   | 1.341000   | 1.348000   | 1.354000   | -1.03%   | +0.45%   | pass   |
| CalibrateYieldCurve MIXED ANALYTIC SOLVE (23 swaps)             | 0.784370   | 0.777679   | 0.777582   | 0.786403   | -0.85%   | +1.13%   | pass   |
| CalibrateYieldCurve MIXED BUMPED +DIAG (23 swaps)               | 1.850000   | 1.887000   | 1.857000   | 1.865000   | +2.00%   | +0.43%   | pass   |
| CalibrateYieldCurve MIXED BUMPED SOLVE (23 swaps)               | 1.164000   | 1.180000   | 1.172000   | 1.188000   | +1.37%   | +1.37%   | pass   |
| CalibrateYieldCurve PWC ANALYTIC +DIAG (23 swaps)               | 1.069000   | 1.052000   | 1.068000   | 1.052000   | -1.59%   | -1.50%   | pass   |
| CalibrateYieldCurve PWC ANALYTIC SOLVE (23 swaps)               | 0.590286   | 0.581578   | 0.589189   | 0.578686   | -1.48%   | -1.78%   | pass   |
| CalibrateYieldCurve PWC BUMPED +DIAG (23 swaps)                 | 0.932056   | 0.933451   | 0.920228   | 0.932188   | +0.15%   | +1.30%   | pass   |
| CalibrateYieldCurve PWC BUMPED SOLVE (23 swaps)                 | 0.629691   | 0.637089   | 0.630240   | 0.640061   | +1.17%   | +1.56%   | pass   |
| CalibrateYieldCurve PWL ANALYTIC +DIAG (23 swaps)               | 1.231000   | 1.187000   | 1.224000   | 1.201000   | -3.57%   | -1.88%   | pass   |
| CalibrateYieldCurve PWL ANALYTIC SOLVE (23 swaps)               | 0.650275   | 0.643678   | 0.648335   | 0.640098   | -1.01%   | -1.27%   | pass   |
| CalibrateYieldCurve PWL BUMPED +DIAG (23 swaps)                 | 1.516000   | 1.528000   | 1.522000   | 1.510000   | +0.79%   | -0.79%   | pass   |
| CalibrateYieldCurve PWL BUMPED SOLVE (23 swaps)                 | 0.933502   | 0.937376   | 0.929311   | 0.938686   | +0.41%   | +1.01%   | pass   |
| PWL DF queries (4096 x 24 nodes)                                | 0.047053   | 0.047255   | 0.047055   | 0.047552   | +0.43%   | +1.06%   | pass   |
| PWL DF queries (4096 x 256 nodes)                               | 0.079767   | 0.074047   | 0.080961   | 0.078493   | -7.17%   | -3.05%   | pass   |
| PWL DF queries (4096 x 64 nodes)                                | 0.053520   | 0.052300   | 0.053915   | 0.052268   | -2.28%   | -3.05%   | pass   |
| PWL DF queries (4096 x 8 nodes)                                 | 0.041790   | 0.043137   | 0.041973   | 0.042198   | +3.22%   | +0.54%   | pass   |

## Retained evidence and remaining acceptance

Under `/tmp/dal-aad-evidence/`:

- `diagnostics-option-red-oracle.log`, `diagnostics-option-green*.log`,
  `diagnostics-reject-*.log`: build/ABI configuration RED/GREEN.
- `lifetime-red.log`, `lifetime-final-sanitized.log`,
  `lifetime-on-review-focused.log`, `lifetime-off-review-focused.log`:
  stale-handle RED and final focused numerical/lifetime checks.
- `native-diagnostics-ctest.log`, `native-diagnostics-python-ctest.log`,
  `lifetime-off-ctest.log`, `lifetime-codipack-ctest.log`,
  `lifetime-off-smoke.log`: full functional/binding and quiet smoke results.
- `lifetime-consumer-{on,off}.log`, `lifetime-off-layout.log`,
  `lifetime-off-review-tape-symbols.txt`: installed ABI and default instrumentation.
- `block-size-red-timeout.log`, `block-size-green.log`: independent size repair.
- `lifetime-off-paired/`: every nine-target raw sample, `results.json`, `summary.md`.
- `lifetime-off-mc-paired/`, `lifetime-off-curve-paired/`: every supplemental raw
  sample, full comparisons and summaries.
- `lifetime-off-resource/`: all six pairs' output and verbose resource records,
  conditions and `results.json`.

Initial `c83bcc9` performance verdict: **no regression under the unchanged policy**.
Corrected local/performance acceptance is retained in the linked report.
Publication CI, a D02 requirement audit and backend capability reporting remain.
Passing the original D01 head's 46 checks is not evidence for
these new C++, CMake or workflow changes. Stage A still contains P01/D01/D03 work;
market/portfolio risk, structured operators and second-order scope remain active.
