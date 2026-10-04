# DAL AAD implementation ledger

Status: active implementation. No stage is complete until its correctness, compatibility,
performance, and applicable CI evidence has been inspected.

Controlling design: [detailed AAD plan](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3c2d0bdbdf6532ae9edae507d073f765e7e31f8a/.codex/artifacts/plans/aad-improvement-plan.md).
Its appendix expansions preserve the original scope and add worked examples,
task cards, resource models, complete request execution, operator pullbacks,
cache/failure boundaries and explicit no-regression acceptance.
The further method expansion specifies solver precision, nonsmooth estimators,
sparse reconstruction, segmented state seeds, backend effects and result failures;
it preserves the existing full implementation scope.
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

The locally committed lifecycle increment `8f4f09c` passes 2,336 native and 2,255
CoDiPack CTest cases and all 20 native scoped-recording cases under ASan/UBSan.
Its fresh nine-target pairing passes. Supplemental ordinary MC detects genuine
short-path regressions: vanilla AAD tree +15.00%/+10.74%, compiled +11.25%/+15.08%.
Long weekly AAD and BS/local-vol LSM profiles pass; all supplemental LSM PV/risk
entries agree with baseline at relative/absolute tolerance 1e-10. Raw evidence is
retained in `lifecycle-paired/` and `lifecycle-mc-paired/`.

Corrected C++ head `71f41a81e4d939b192a986199104019bbbe9230c` inlines existing
checked operations and builds errors out of line. It passes 2,315 native functional/
example cases, all 21 quiet serial benchmark smoke checks, 2,255 CoDiPack cases,
and twenty native scope cases under ASan/UBSan. Its fresh nine-target gate, all
35 ordinary MC cases, three LSM profiles, and 25 calibration cases pass. A second
complete MC/LSM pairing confirms the short vanilla results; all four rounds remain.
Matching four-thread RSS observations show no material increase. The
[lifecycle acceptance report](../perf/aad-recording-lifecycle.md) contains full
tables, conditions, initial failures, and remaining coverage limits. Publication-
head four-backend/Windows/binding CI remains required; the prior ownership head's
green CI does not validate this increment.

At first publication head `ca977e2`, Codacy reports complexity 10 (limit 8) in
the combined scalar/vector full-block test. Split the two cases with shared
graph setup and preserve every assertion. All 21 native/fifteen CoDiPack cases
and all 21 native ASan/UBSan cases pass locally after this test-only correction;
production source and all measured nine-target binaries remain unchanged.
Corrected publication head `0ee84e1d06aed29e93c21639953dd8ff65314828` now passes
all 46 exact-head CI checks, including static analysis, sanitizers, the four-
backend compiler matrix and Windows/Python/Excel jobs. The audit is retained in
`recording-lifecycle-0ee84e1-ci.jsonl`. This validates the published D01 increment,
not subsequent local changes; the final D01 requirement audit still precedes its
ledger check-off.

Current D02 work adds the default-OFF native lifetime option, PUBLIC/exported
ABI definition and incompatible-backend rejection. An ignored-option oracle
first fails; the native export and all three external-backend rejection checks
now pass. Three native stale-handle/assignment regressions first fail; all 23
initial diagnostic cases pass ASan/UBSan, including full reset, suffix/generation ABA,
foreign/exited threads, saved expressions, scalar/vector modes, count/epoch/
identity exhaustion and recovery after partial allocation. The added model-copy
case also passes: 24 diagnostic cases now pass ASan/UBSan, and 45 lifetime/
scoped cases pass the full ON library. Native ON core/public/portable Excel CTest
passes 2,307 cases, followed by the additional model-copy check; Python ON passes
792 tests with one skip. ON and OFF installed consumers verify ABI propagation,
analytic gradients and supported lifetime behavior. Default native CTest passes
2,318 functional/example cases; CoDiPack passes 2,257. OFF number/node/tape size
and alignment match the original baseline (16/40/368 bytes on this host), and
diagnostic functions are absent from its tape object. All 21 quiet serial OFF
benchmark smoke checks pass. Final include-order rebuild passes 71 ON and 47 OFF
focused cases. At immutable implementation head `c83bcc9`, the fresh nine-target
gate passes all 65 comparable cases; 35 ordinary MC cases, three LSM profiles,
and 25 calibration cases also pass. Every LSM paired PV/risk agrees at relative/
absolute tolerance 1e-10. Three four-thread RSS pairs have overlapping ranges
and no major faults. The [D02 acceptance report](../perf/aad-native-lifetime-diagnostics.md)
records all rows and limits. Current publication-head CI and the final requirement
audit remain open; smoke timing is not paired acceptance.
Post-pairing review reproduces two independent-rebinding failures and one
expression-assignment failure under counter exhaustion. Diagnostic assignment
now commits primal/handle only after successful allocation/materialization;
default OFF assignment bodies are unchanged. All 28 diagnostic cases now pass
ASan/UBSan, including actual scoped-registration failure/recovery. Corrected
complete ON/OFF builds now pass: ON CTest has 2,313 cases including Python,
and OFF has 2,318 functional/example cases. Evidence is retained in
`lifetime-assignment-{on,off}-{build,ctest}.log`. Fresh default OFF pairing and
current publication CI remain required before accepting the corrected increment;
keep the earlier `c83bcc9` data.
The independent full-final-block `BlockList::Size` repair has its
bounded timeout RED and eleven-case ASan/UBSan GREEN evidence; it also awaits
final paired/CI acceptance; its full native/CoDiPack functional checks pass.

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
This ownership increment covers ownership only; scoped phases, tokens, clearing,
mode boundaries, and MC/LSM migration belong to the following lifecycle increment.

The next local increment implements scoped states, unique validated checkpoint tokens,
native scalar/vector clearing, mode-selection boundaries, reverse-failure recovery,
and ordinary/LSM worker-batch ownership. Twenty native and fifteen CoDiPack focused
recording cases pass. The new nested-MC-batch test first failed, then passed with
ownership before reset; its existing simulation error/recovery test still passes.
Fresh full native and CoDiPack CTest pass 2,336 and 2,255 cases respectively;
twenty native ownership/state cases also pass ASan/UBSan with leak detection.
Logs are `lifecycle-*-ctest.log` and `lifecycle-sanitized.log` under the evidence root.
The first paired run then reveals the short-path failures recorded above. The
corrected head passes fresh correctness and all changed-workload comparisons;
all 46 exact publication-head CI checks subsequently pass at `0ee84e1`.
Do not reuse the ownership snapshot's acceptance as proof for these new changes.

Next: freeze the D02 increment, finish default OFF paired acceptance and inspect
its exact-head CI. Finish the D01 requirement audit and continue P01/D03 under
the full controlling scope.
Public numeric-result validation remains outside per-path loops. Do not mark Stage A
complete before its remaining requirements are verified.
