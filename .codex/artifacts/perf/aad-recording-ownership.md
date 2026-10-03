# AAD independent recording ownership acceptance

Status: active acceptance evidence. Local correctness and paired performance pass;
the ownership increment still requires its exact-head compiler/backend and binding
CI audit. D01 and the full AAD plan remain open.

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
integrity checks pass. XAD, Adept, Windows, and the extended Python/binding matrix
remain part of the exact-head CI audit.

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
