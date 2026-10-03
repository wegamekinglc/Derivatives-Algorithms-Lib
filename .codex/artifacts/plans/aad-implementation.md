# DAL AAD implementation ledger

Status: active implementation. No stage is complete until its correctness, compatibility,
performance, and applicable CI evidence has been inspected.

Controlling design: [detailed AAD plan](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/9dd9282bb2c517c838a6576a95c9b7a937e750af/.codex/artifacts/plans/aad-improvement-plan.md).
Initial implementation baseline: `b5e3caca85bb1a5b1aa3acf7898b387832d443c8`.

The user authorized full implementation on 2026-10-04 and requires existing functionality
to retain performance and CI compatibility. This ledger preserves the full scope across
incremental implementation turns and PRs; a green first stage does not complete the goal.

## Delivery rules

- Work in isolated writable sources; preserve the original workspace and unrelated changes.
- Establish a failing independent test before each behavioral change.
- Validate native, XAD, CoDiPack, and Adept contracts according to actual adapter capabilities.
- Preserve legacy single-output APIs, units, paths, normalization, and LSM policy semantics.
- Compare isolated Release baseline/head binaries with matching dependency SHAs and configuration.
- Use the existing paired regression policy: ten interleaved process samples per side in each
  of two confirmation rounds, best-of-N minima, and a 4% threshold. Keep all nine existing targets.
- Extend production-workload evidence without weakening or bypassing existing CI/gates.
- Performance failures require investigation and correction. Noisy evidence is inconclusive.
- Keep each change reviewable. Publish current-state documentation only after behavior exists.
- Do not mark the overall goal complete until every applicable requirement below is verified.

## Stage A: trustworthy differentiation and measurement

- [x] C01: propagate every nonzero scalar adjoint; exact zero is the only default skip.
- [x] C02: vector channels have independent propagation semantics and match scalar requests.
- [x] C03: non-finite derivatives/seeds remain observable; public results diagnose invalid risk.
- [x] C04: preserve consumed intermediate clearing, leaf accumulation, repeated sweeps, and checkpoints.
- [x] C05: no implicit approximate gradient truncation.
- [ ] P01: correct production-oriented tape/Jacobian/MC benchmarks and explanatory resource metrics.
- [ ] D01: explicit recording lifecycle, checkpoints, nested-use rejection, and exception recovery.
- [ ] D02: optional owner/slot-lifetime diagnostics without release per-node overhead.
- [ ] D03: compile-time backend adapter/capability contracts and verified fallbacks.

## Stage B: market and portfolio risk

- [ ] D04: structured requests/results with stable axes, methods, units, provenance, and budgets.
- [ ] D04: compatibility projections for existing `PV` and `d_...` outputs.
- [ ] F01: deterministic-rate Dupire spread-quote pullback connected to Hybrid valuation gradients.
- [ ] F01: direct quote dependencies and snapshot/axis mismatch validation.
- [ ] F01: calibration-only and full bump/recalibrate/common-path oracles.
- [ ] F01: common calibration pullback integration with existing curve quote-risk semantics.
- [ ] F02: fixed-weight VJP for multiple prepared-script outputs, including aliases/constants.
- [ ] F02: budgeted blocked Jacobian with explicit backend/rerecording behavior.
- [ ] F02: compatible portfolio observation/timeline integration.
- [ ] P02: per-worker capacity reuse and safe re-registration/reinitialization.
- [ ] P03: measured block-width selection and demand-driven result extraction.
- [ ] Bindings: C++ public API, Python keyword/result interfaces, and Excel immutable handles/getters.

## Stage C: structured reverse operators

- [ ] F03: independent linear-solve pullback with decomposition reuse and directional adjoint oracle.
- [ ] F03: recording integration, aliases, multiple seeds, repeated reverse, cache ownership, and failures.
- [ ] F03: structured matrix coordinates and singular/ill-conditioned solver diagnostics.
- [ ] F03: implicit calibration and PDE pullbacks derived and verified separately.
- [ ] P04: proven structural sparsity, safe invalidation, compressed seeds, and mode selection evidence.
- [ ] P05: measured long-path checkpoint with complete state/RNG restoration and recomputation.

## Stage D: second-order risk

- [ ] F04: specified Gamma, cross-Gamma, and Hessian-vector requests using bump-over-AAD.
- [ ] F04: quote-risk second order includes calibration curvature through full recalibration.
- [ ] F04: common-path, smoothing, Frozen/RetrainedBump, and nested-step semantics.
- [ ] F04: mixed-mode prototype on smooth kernels and backend capability validation.
- [ ] F04: estimator validation for applicable simulation/calibration cases before general promotion.

## Completion evidence

- [ ] Focused red/green evidence and independent mathematical references for each feature.
- [ ] Fresh full native/core/public/portable-binding verification.
- [ ] Applicable four-backend verification and exact implementation-head CI checks.
- [ ] Python and Excel parity, generated-source integrity, and documentation integrity.
- [ ] Existing nine-target performance gate plus changed production-workload coverage.
- [ ] Current-state method docs, examples, and necessary changelog entries.
- [ ] Local read-first review with no unresolved correctness or compatibility findings.
- [ ] Requirement-by-requirement audit of the actual final state.

## Current evidence and next action

Isolated sources: `/tmp/dal-aad-baseline` and `/tmp/dal-aad-implementation`.
Evidence/build root: `/tmp/dal-aad-evidence`; dependencies are checked out at baseline gitlink SHAs.
Full native baseline/head Release builds use matching benchmark-enabled configuration.

At implementation commit `247c7aefab0f54af788b8a44f9dd71e606562c8a`, the fresh full native/core/public/
portable-Excel and non-slow example run passed 2,286 cases. The initial nine-target paired gate
found approximately 50% regressions in vector propagation and the small Jacobian harvest;
these failures are retained in `p0-paired-initial/` and are not waived.

The corrective kernel uses exact IEEE magnitude-bit classification (portable comparison fallback),
continuous vector arithmetic for finite derivatives, and an out-of-line non-finite path that skips
zero channels. Mode and common widths are selected once per sweep; other widths use the same
semantics through the dynamic loop. No per-node data members or approximate thresholds are added.

Nine focused native propagation tests pass, including new scalar/vector subnormal regressions,
signed-zero handling, alias accumulation, repeated sweeps, and specialized/fallback widths.
The subnormal cases fail against the original baseline. The corrected kernel's isolated diagnostic
pairing passed both rounds for tape and Jacobian targets; this is not the final nine-target verdict.
Raw evidence is in `width-specialized-paired-diagnostic/` (same CPU affinity for both sides).

The original CoDiPack CI failures exposed its explicit `sqrt(0)` zero-gradient convention rather
than a missing numeric-result check. Retain that boundary test for native, and validate all backends
with a smooth positive-domain payoff whose derivative overflows while its primal remains finite;
the new public test covers interpreted/compiled paths and subsequent recovery.

At `83399b5a31389563a26b7e69d82bfc2b5bd4b68e`, fresh full native and CoDiPack runs passed
2,290 and 2,238 cases respectively. All 46 exact-head CI checks subsequently passed.
Later increments still require their own exact-head CI audit.

The first P01 measurement increment adds explicit native tape snapshots for logical storage,
cursor storage including block padding, and retained block capacity. No recording/propagation
counters or node fields are added. The snapshot's full-block boundary regression first crashed,
then passed after bounding its scan by the known node count; ASan/UBSan passes all 11 focused
statistics/block-list tests. A growth/rewind test distinguishes occupied storage from capacity.

`tape_perf` now retains fresh handles after its Clear/Rewind timings, checks analytic values and
gradients, and covers passive constants and vector widths 1/4/10/16/64. Historical case names
and workloads remain. A vector fixture seeds multiple channels of one output; it does not claim
to exercise distinct portfolio outputs. `jacobian_perf` additionally calls the actual
`HarvestCurveJacobian` for 23-by-24 and 95-by-96 matrices and checks every entry analytically.
Diagnostic scans are enabled explicitly with `--diagnostics`; default timing runs omit them.

With the measurement increment, full native and CoDiPack runs pass 2,293 and 2,239 cases.
Both changed benchmark executables pass their result checks. Native statistics are not an RSS
budget, cumulative allocation counter, or automatically sampled high-water mark. MC phase
attribution, worker scaling, and the production multi-output cases remain P01 work.

The first full gate with P01 coverage (`e64a2690e7d4ee2ce6cc857aad5929325ea7a409`)
passed every existing case except the small dense reference Jacobian (+6.52%/+6.80%).
Raw evidence remains in `p0-paired-final/`. The same current core library with the original
benchmark fixture passed both rows, localizing the difference to the expanded fixture.
Separating the established timed fixture from command parsing/additional cases passed both
Jacobian rows (-4.83%/-4.41% dense and -4.30%/-4.49% prefix). Result checks, work, case names,
sample counts, and thresholds remain. Graph construction is separately factored, and production
prefix widths are selected before timing rather than copied in each measured harvest.
This also corrects Codacy's two new complexity findings without relaxing its limit.

The fresh nine-target gate at `13b0964b870af232ff4bed745835ac076249ded8` passes every
performance acceptance check. Raw evidence is in `p0-paired-verified/`, including immutable
source SHAs, compiler/cache/dependency metadata, CPU affinity, and binary SHA-256 digests.
Both small Jacobian rows improve in both rounds; tape costs remain within the unchanged policy.
The [performance report](../perf/aad-native-stage-a.md) preserves every case and the coverage
limits. C01-C05 are verified by their mathematical tests, the full core-repair CI audit, and
this performance gate; new increments still require their own current-head checks.

The [recording lifecycle contract](../specs/aad-recording-lifecycle.md) specifies D01 ownership,
states, checkpoint validation, and cleanup/recovery acceptance. The independent ownership
increment implements thread-affine `RecordingScope_`, scoped nesting rejection, explicit
close, noexcept fallback, cleanup-failure retention, and recovery before the next recording.
`TapeGuard_` delegates ownership to it; curve Jacobian, node-risk, and GSR/SLV Jacobian callers
close explicitly after passive extraction. Raw graph operations remain unchanged.

Nine native and eight CoDiPack ownership tests pass, including nesting during entry/exit
reset callbacks, exception recovery, foreign-thread rejection, idempotent closure, and
native vector-capacity retention. The nested-guard and entry-reset tests first failed.
ASan/UBSan passes all nine native cases. Fresh full native CTest passes 2,324 cases
(core/public/portable Excel, 33 regular examples, one slow example, and 21 benchmark smoke
tests); full CoDiPack CTest passes 2,247. Logs are retained under `recording-*-ctest.log`
and `recording-owner-sanitized.log`. Benchmark smoke tests are not paired performance proof.

All 46 exact-head CI checks passed at `aec6689ae6d029f9a7eb4f810e3e074fd4ed6030`,
before the ownership increment. Its fresh nine-target paired gate passes at
`080d16e0047798766116d1dba34b03ff8d860815`; supplemental end-to-end calibration also
passes all 25 common cases under the same sampling/confirmation criterion. The
[ownership acceptance report](../perf/aad-recording-ownership.md) records conditions,
case movements, raw evidence, and coverage limits. All 46 exact-head checks pass
at `bf89c6b66cf0dd2f0253543cf5fc3fcbe02af5d7` for this ownership increment.
D01 remains open: scoped recording states,
validated checkpoint handles, vector clearing, mode boundaries, and MC/LSM migration
are not implemented by this increment.

The next local increment implements scoped states, unique validated checkpoint tokens,
native scalar/vector clearing, mode-selection boundaries, reverse-failure recovery,
and ordinary/LSM worker-batch ownership. Twenty native and fifteen CoDiPack focused
recording cases pass. The new nested-MC-batch test first failed, then passed with
ownership before reset; its existing simulation error/recovery test still passes.
Fresh full native and CoDiPack CTest pass 2,336 and 2,255 cases respectively;
twenty native ownership/state cases also pass ASan/UBSan with leak detection.
Logs are `lifecycle-*-ctest.log` and `lifecycle-sanitized.log` under the evidence root.
Four-backend/exact-head CI, nine-target performance, and supplemental short/long
MC and LSM comparisons remain pending.
Do not reuse the ownership snapshot's acceptance as proof for these new changes.

Next: finish lifecycle verification and measure changed MC boundary costs.
Public numeric-result validation remains outside per-path loops. Do not mark Stage A
complete before its remaining requirements are verified.
