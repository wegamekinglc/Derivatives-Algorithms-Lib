# AAD independent recording ownership acceptance

Status: active acceptance evidence. Local correctness, paired performance, and all
46 exact-head CI checks at `bf89c6b66cf0dd2f0253543cf5fc3fcbe02af5d7` pass for the
ownership increment. D01 and the full AAD plan remain open.

## Measured sources and policy

- Immutable baseline: `b5e3caca85bb1a5b1aa3acf7898b387832d443c8`.
- Measured ownership head: `080d16e0047798766116d1dba34b03ff8d860815`.
- Sources/builds: `/tmp/dal-aad-baseline` and `/tmp/dal-aad-implementation`, with
  separate `base-build` and `head-build` under `/tmp/dal-aad-evidence`.
- Matching native AAD, Eigen, Release C++17, GCC 15.2, CMake 4.2.3, Unix Makefiles,
  dependency gitlinks, and benchmark-enabled configuration. Native architecture
  opt-in is disabled. The default build excludes Python; CI covers that binding.
- Both sides use `DAL_NUM_THREADS=4` and affinity `4,6,8,10` on the shared WSL2
  i9-13900HX workstation. No DAL build or test runs during pairing. Desktop and
  host activity are not controlled.
- Unchanged nine-target gate: two rounds, ten interleaved process samples per
  side per round, minimum reduction, failure only when both rounds exceed 4%.
  The Sobol precise/fast ceiling remains 10x.

The source remains fixed throughout each measurement. A later documentation
update specifies future state/checkpoint APIs without changing measured C++.
Each evidence directory contains source/cache/hardware metadata and binary
SHA-256 digests. Supplemental curve calibration runs after the nine-target gate;
it does not add a target to that gate.

## Correctness and storage

The nested legacy-guard test first failed because the inner guard silently
discarded the outer recording. The reset-reentrancy test first failed because
entry claimed ownership after rewinding. The implemented boundary rejects both
before mutation; the outer scalar graph still returns its independent gradient.

Nine native and eight CoDiPack ownership cases pass. They cover normal and
exceptional ownership release, explicit cleanup failure, failure while preserving
a primary business exception, rejected rebuilds, successful recovery, an older
closed scope's interaction with a newer one, foreign-thread rejection, concurrent
independent threads, reset callback nesting, and native vector-capacity retention.
ASan/UBSan passes all nine native cases with leak detection enabled.

Full native CTest passes 2,324 cases: core/public/portable Excel, 33 regular
examples, one slow example, and 21 benchmark smoke tests. Full CoDiPack passes
2,247 cases. Smoke timings are excluded from performance evidence. Documentation
integrity checks pass. All 46 exact-head CI checks pass at documentation head
`bf89c6b66cf0dd2f0253543cf5fc3fcbe02af5d7`, including XAD, Adept, Windows,
the extended Python/binding matrix, sanitizers, generated-source integrity, and
required gates. Later lifecycle increments need their own exact-head acceptance.

The native capacity test records a vector graph spanning several node blocks,
then closes it. Nodes, logical bytes, and occupied bytes return to zero while
allocated block capacity is unchanged. Scope/context metadata exists once per
independent recording/thread; no node fields or expression/propagation checks
are added. Successful close rewinds; rebuilding is reserved for failed cleanup.

## Performance verdict and coverage

Every existing case passes the nine-target policy. Selected affected/reference
rows are shown below; all process outputs and the full table remain in
`recording-owner-paired/`.

| Case | Round 1 change | Round 2 change | Verdict |
|---|---:|---:|---|
| Scalar propagation | -0.81% | -1.03% | pass |
| Vector propagation, width 10 | -0.90% | -0.57% | pass |
| Scalar full adjoint clearing | +0.17% | -0.18% | pass |
| Dense reference Jacobian, 24 x 23 | -5.14% | -4.53% | pass |
| Proven-prefix reference Jacobian, 24 x 23 | -4.43% | -4.52% | pass |
| Prepared rate AAD, 32 IRS x 8 nodes | +1.28% | +1.28% | pass |
| Prepared rate AAD, 256 IRS x 8 nodes | +0.71% | +2.18% | pass |
| Single-trade rate sweeps, 240 calls | -0.40% | -0.70% | pass |
| Single-curve quote-risk aggregation | -0.44% | -0.91% | pass |

The other gated targets—PDE, RNG, interpolation, Krylov, banded, and Cholesky—also
pass. Sobol's precise/fast ratio is 9.33x. An unchanged prepared-PV row exceeds 4%
in one round (+4.27%) but remains below it in the other (+1.73%); it passes the
established policy and is not claimed as an improvement. Small movements are
not proof of a library-wide speedup.

The supplemental unchanged `curve_calibration_perf` executable compares all 25
common cases with matching options/affinity and the same two-round/ten-sample
criterion. All pass. Representative analytic solve changes are LOG_LINEAR
+0.25%/+0.82%, PWC -0.45%/-2.88%, and PWL -0.02%/-6.65%.
LOG_CUBIC_NATURAL solve records +2.16%/+5.53%, so its supplemental acceptance
also relies on both rounds rather than a single timing. Raw data covers analytic
and bumped solves, diagnostics on/off, approximate calibration, and PWL queries.

## Evidence and remaining work

Evidence root: `/tmp/dal-aad-evidence`.

- `recording-owner-paired/`: all nine targets, full table, samples, environment.
- `recording-curve-paired/`: supplemental raw calibration samples and metadata.
- `recording-native-ctest.log` and `recording-codipack-ctest.log`: fresh full runs.
- `recording-owner-red.log` and `recording-reset-red.log`: failing regressions.
- `recording-native-focused.log`, `recording-codipack-focused.log`, and
  `recording-owner-sanitized.log`: successful ownership and sanitizer checks.

Keep prior failed performance evidence described in
[the core repair report](aad-native-stage-a.md). No threshold, case, sample count,
or result check was weakened.

This increment implements independent scope ownership and curve migration only.
The [lifecycle contract](../specs/aad-recording-lifecycle.md) still controls scoped
states, validated checkpoint tokens, native vector clearing, mode boundaries,
and MC/LSM migration. Per-path scope API costs, long-path checkpoint recomputation,
new risk APIs, and later stages require their own measurement and verification.

## Paired benchmark regression gate

2 independent rounds of 10 interleaved process-level samples; failure requires every round to exceed +4.00%.

| Benchmark | Case | Base min | Head min | Change | Round changes | Result |
|---|---|---:|---:|---:|---:|:---:|
| tape_perf | Clear + re-record (100K nodes) | 0.853472 ms | 0.857531 ms | +0.48% | -1.22%, +0.77% | pass |
| tape_perf | PropagateToStart (100K nodes) | 0.389865 ms | 0.385851 ms | -1.03% | -0.81%, -1.03% | pass |
| tape_perf | PropagateToStart multi-mode (100K nodes, 10 results) | 0.490903 ms | 0.488103 ms | -0.57% | -0.90%, -0.57% | pass |
| tape_perf | Rewind + re-record (100K nodes) | 0.568112 ms | 0.567357 ms | -0.13% | -0.11%, -0.13% | pass |
| tape_perf | ZeroAdjoints sweep (100K nodes) | 0.110574 ms | 0.110530 ms | -0.04% | +0.17%, -0.18% | pass |
| tape_perf | PropagateToStart multi-mode (50K steps, 1 result) (new coverage) | — | 0.381346 ms | — | — | info |
| tape_perf | PropagateToStart multi-mode (50K steps, 16 results) (new coverage) | — | 0.647387 ms | — | — | info |
| tape_perf | PropagateToStart multi-mode (50K steps, 4 results) (new coverage) | — | 0.379869 ms | — | — | info |
| tape_perf | PropagateToStart multi-mode (50K steps, 64 results) (new coverage) | — | 6.664000 ms | — | — | info |
| tape_perf | PropagateToStart passive constants (50K steps) (new coverage) | — | 0.146857 ms | — | — | info |
| tape_perf | Rewind + passive-constant recording (50K steps) (new coverage) | — | 0.236443 ms | — | — | info |
| jacobian_perf | AnalyticJacobian dense harvest (24 x 23) | 0.005346 ms | 0.005095 ms | -4.70% | -5.14%, -4.53% | pass |
| jacobian_perf | AnalyticJacobian row-width harvest (24 x 23) | 0.005377 ms | 0.005134 ms | -4.52% | -4.43%, -4.52% | pass |
| jacobian_perf | HarvestCurveJacobian dense (23 outputs x 24 parameters) (new coverage) | — | 0.004322 ms | — | — | info |
| jacobian_perf | HarvestCurveJacobian dense (95 outputs x 96 parameters) (new coverage) | — | 0.061875 ms | — | — | info |
| jacobian_perf | HarvestCurveJacobian proven prefix (23 outputs x 24 parameters) (new coverage) | — | 0.003777 ms | — | — | info |
| jacobian_perf | HarvestCurveJacobian proven prefix (95 outputs x 96 parameters) (new coverage) | — | 0.058048 ms | — | — | info |
| pde_perf | ThetaScheme rollback (200x200 CN) | 0.214222 ms | 0.214396 ms | +0.08% | +0.08%, +0.43% | pass |
| pde_perf | ThetaScheme rollback (200x200 implicit) | 0.214647 ms | 0.214595 ms | -0.02% | -0.02%, -0.01% | pass |
| pde_perf | ThetaScheme rollback (200x2000 explicit) | 0.658409 ms | 0.657846 ms | -0.09% | -0.09%, +0.43% | pass |
| rng_perf | BrownianBridge FillNormal (100K x 10D) | 6.428000 ms | 6.410000 ms | -0.28% | -0.28%, +0.11% | pass |
| rng_perf | IRN SkipNormalTo (100K x 10D) | 7.766000 ms | 7.773000 ms | +0.09% | +0.09%, +0.57% | pass |
| rng_perf | MRG32 SkipNormalTo (100K x 10D) | 0.001294 ms | 0.001293 ms | -0.08% | -0.15%, +0.08% | pass |
| rng_perf | MRG32k3a FillNormal (100K x 10D) | 19.623000 ms | 19.610000 ms | -0.07% | -0.07%, +0.20% | pass |
| rng_perf | ShuffledIRN FillNormal (100K x 10D) | 10.630000 ms | 10.632000 ms | +0.02% | -0.39%, +0.09% | pass |
| rng_perf | Sobol FillNormal fast (100K x 10D) | 4.031000 ms | 4.031000 ms | +0.00% | +0.45%, -0.30% | pass |
| rng_perf | Sobol FillNormal precise opt-in (100K x 10D) | 37.812000 ms | 37.612000 ms | -0.53% | +0.19%, -0.70% | pass |
| rng_perf | Sobol FillUniform (100K x 10D) | 0.744179 ms | 0.750133 ms | +0.80% | +0.68%, +2.47% | pass |
| interp_perf | Cubic interp (50 knots, 10K queries) | 0.050232 ms | 0.050272 ms | +0.08% | -0.12%, +0.08% | pass |
| interp_perf | Inlined linear LV-style (1e5 paths x 200 steps, 200 knots) | 100.484000 ms | 100.959000 ms | +0.47% | -0.01%, +0.51% | pass |
| interp_perf | Linear interp (50 knots, 10K queries) | 0.042230 ms | 0.042065 ms | -0.39% | -0.70%, -0.19% | pass |
| krylov_perf | BCGSolve (500x500 tridiag) | 0.063555 ms | 0.062516 ms | -1.63% | +0.76%, -2.25% | pass |
| krylov_perf | CGSolve (500x500 tridiag) | 0.053915 ms | 0.052292 ms | -3.01% | -1.56%, -3.01% | pass |
| banded_perf | TriDecomp MultiplyLeft (10K) | 0.004492 ms | 0.004493 ms | +0.02% | +0.07%, -0.02% | pass |
| banded_perf | TriDiagonal Decompose (10K) | 0.076830 ms | 0.076731 ms | -0.13% | -0.13%, +0.04% | pass |
| banded_perf | TriDiagonal MultiplyLeft (10K) | 0.004684 ms | 0.004685 ms | +0.02% | -0.02%, +0.02% | pass |
| cholesky_perf | CholeskyDecompose (200x200) | 0.225620 ms | 0.225717 ms | +0.04% | +0.04%, +0.03% | pass |
| cholesky_perf | CholeskyDecompose+Multiply (200x200) | 0.225136 ms | 0.225220 ms | +0.04% | +0.04%, -0.02% | pass |
| rate_risk_perf | Quote risk aggregate (joint XCCY) | 0.220017 ms | 0.219348 ms | -0.30% | -0.30%, +0.02% | pass |
| rate_risk_perf | Quote risk aggregate (single curve) | 0.008928 ms | 0.008852 ms | -0.85% | -0.44%, -0.91% | pass |
| rate_risk_perf | Quote risk aggregate (staged XCCY basis) | 0.060275 ms | 0.060574 ms | +0.50% | +0.22%, +0.54% | pass |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=10) | 1.136000 ms | 1.138000 ms | +0.18% | +0.18%, +0.26% | pass |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=16) | 1.735000 ms | 1.736000 ms | +0.06% | -0.63%, +0.17% | pass |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=5) | 0.818794 ms | 0.816172 ms | -0.32% | -0.10%, -1.13% | pass |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=10) | 11.033000 ms | 11.159000 ms | +1.14% | +0.57%, +1.14% | pass |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=16) | 17.283000 ms | 17.029000 ms | -1.47% | -0.70%, -1.47% | pass |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=5) | 7.935000 ms | 7.898000 ms | -0.47% | -1.74%, -0.34% | pass |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=10) | 1.088000 ms | 1.077000 ms | -1.01% | -0.18%, -1.46% | pass |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=16) | 1.652000 ms | 1.664000 ms | +0.73% | -0.60%, +1.09% | pass |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=5) | 0.771049 ms | 0.769335 ms | -0.22% | -0.11%, -1.27% | pass |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=10) | 10.863000 ms | 10.877000 ms | +0.13% | +1.34%, -0.89% | pass |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=16) | 17.086000 ms | 17.063000 ms | -0.13% | -1.28%, -0.13% | pass |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=5) | 7.791000 ms | 7.764000 ms | -0.35% | +0.41%, -0.58% | pass |
| rate_risk_perf | Quote risk portfolio joint ANALYTIC (24 XCCY x N=10/block) | 4.529000 ms | 4.493000 ms | -0.79% | -0.79%, -0.71% | pass |
| rate_risk_perf | Quote risk portfolio joint BUMPED (24 XCCY x N=10/block) | 4.490000 ms | 4.491000 ms | +0.02% | -0.66%, +0.02% | pass |
| rate_risk_perf | Quote risk portfolio single ANALYTIC (120 deposits x N=5) | 0.121237 ms | 0.121638 ms | +0.33% | +0.81%, +0.33% | pass |
| rate_risk_perf | Quote risk portfolio single BUMPED (120 deposits x N=16) | 0.169794 ms | 0.171058 ms | +0.74% | +0.74%, +0.85% | pass |
| rate_risk_perf | Quote risk portfolio staged ANALYTIC (24 XCCY x N=16) | 3.507000 ms | 3.516000 ms | +0.26% | +0.26%, -0.11% | pass |
| rate_risk_perf | Quote risk portfolio staged BUMPED (24 XCCY x N=5) | 1.263000 ms | 1.258000 ms | -0.40% | -1.33%, -0.40% | pass |
| rate_risk_perf | Rate AAD 2 components (1024 IRS x 8 nodes, fixed maturities) | 5.169000 ms | 5.138000 ms | -0.60% | -0.50%, -0.60% | pass |
| rate_risk_perf | Rate AAD 2 components (256 IRS x 8 nodes, fixed maturities) | 1.264000 ms | 1.272000 ms | +0.63% | +0.63%, +1.42% | pass |
| rate_risk_perf | Rate AAD 2 components (32 IRS x 8 nodes, fixed maturities) | 0.153972 ms | 0.153416 ms | -0.36% | -0.02%, -0.36% | pass |
| rate_risk_perf | Rate OIS daily compounding sweep (5Y quarterly x daily) | 0.214726 ms | 0.212771 ms | -0.91% | -0.81%, -0.91% | pass |
| rate_risk_perf | Rate PV (1024 IRS x 8 nodes, fixed maturities) | 2.457000 ms | 2.433000 ms | -0.98% | -0.98%, -0.53% | pass |
| rate_risk_perf | Rate PV (256 IRS x 8 nodes, fixed maturities) | 0.608001 ms | 0.599826 ms | -1.34% | -1.06%, -1.34% | pass |
| rate_risk_perf | Rate PV (32 IRS x 8 nodes, fixed maturities) | 0.075044 ms | 0.073632 ms | -1.88% | -1.38%, -2.08% | pass |
| rate_risk_perf | Rate XCCY batch serial (24 XCCY x 5 components) | 0.780314 ms | 0.776260 ms | -0.52% | -0.53%, -0.52% | pass |
| rate_risk_perf | Rate batch serial (120 IRS x 2 components) | 1.273000 ms | 1.274000 ms | +0.08% | +0.24%, +0.00% | pass |
| rate_risk_perf | Rate prepare geometry (256 IRS x 8 nodes, fixed maturities) | 0.536211 ms | 0.534119 ms | -0.39% | -1.35%, -0.22% | pass |
| rate_risk_perf | Rate prepare geometry (32 IRS x 8 nodes, fixed maturities) | 0.064351 ms | 0.064053 ms | -0.46% | -1.14%, -0.46% | pass |
| rate_risk_perf | Rate prepared AAD 2 components (256 IRS x 8 nodes, fixed maturities) | 0.716514 ms | 0.728246 ms | +1.64% | +0.71%, +2.18% | pass |
| rate_risk_perf | Rate prepared AAD 2 components (32 IRS x 8 nodes, fixed maturities) | 0.085638 ms | 0.086732 ms | +1.28% | +1.28%, +1.28% | pass |
| rate_risk_perf | Rate prepared PV (256 IRS x 8 nodes, fixed maturities) | 0.105957 ms | 0.108669 ms | +2.56% | +4.27%, +1.73% | pass |
| rate_risk_perf | Rate prepared PV (32 IRS x 8 nodes, fixed maturities) | 0.012663 ms | 0.012749 ms | +0.68% | +0.68%, +2.27% | pass |
| rate_risk_perf | Rate single-trade sweeps (240 IRS calls) | 2.001000 ms | 1.993000 ms | -0.40% | -0.40%, -0.70% | pass |

Head Sobol precise opt-in / fast ratio: 9.33x (limit 10.00x).

Rows marked (new coverage) are head-only benchmark cases added by the PR; they are reported for information and are not gated.

All performance acceptance checks passed.
