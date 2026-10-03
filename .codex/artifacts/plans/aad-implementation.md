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

- [ ] C01: propagate every nonzero scalar adjoint; exact zero is the only default skip.
- [ ] C02: vector channels have independent propagation semantics and match scalar requests.
- [ ] C03: non-finite derivatives/seeds remain observable; public results diagnose invalid risk.
- [ ] C04: preserve consumed intermediate clearing, leaf accumulation, repeated sweeps, and checkpoints.
- [ ] C05: no implicit approximate gradient truncation.
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

Next: fresh full build/tests, applicable backend verification, final nine-target paired gate,
and exact corrected-head CI. Public validation remains outside per-path loops. Do not mark Stage A
complete before these checks and the remaining measurement/lifecycle work are verified.
