# DAL AAD implementation ledger

Status: active implementation under the native-only AAD scope.
No stage is complete until its correctness, compatibility,
performance, and applicable CI evidence has been inspected.

Scope amendment (2026-10-04): the user requires removing XAD, CoDiPack and Adept
support and keeping only DAL's built-in native AAD. This replaces the earlier
four-backend compatibility goal. D00 is now a Stage A requirement; D03 is limited
to useful native operations and capability contracts. See the controlling
[native-only specification](../specs/aad-native-only.md).
Removal is implemented locally; fresh native functionality and paired performance pass.
All 28 exact `b42f9eb` CI checks and the D00/D01/D02/D03 audit pass.
Earlier external-backend results below remain
historical evidence, not continuing support obligations or removal acceptance.

Controlling design: [detailed AAD plan](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/de5dd8b20089223e6938dfd8100d463aa6d4a169/.codex/artifacts/plans/aad-improvement-plan.md).
Its appendix expansions preserve the original scope and add worked examples,
task cards, resource models, complete request execution, operator pullbacks,
cache/failure boundaries and explicit no-regression acceptance.
The further method expansion specifies solver precision, nonsmooth estimators,
sparse reconstruction, segmented state seeds, backend effects and result failures;
The native-only amendment adds external support removal and supersedes prior
external-backend obligations while preserving the remaining feature scope.
Initial implementation baseline: `b5e3caca85bb1a5b1aa3acf7898b387832d443c8`.

The user authorized full implementation on 2026-10-04 and requires existing functionality
to retain performance and CI compatibility. This ledger preserves the full scope across
incremental implementation turns and PRs; a green first stage does not complete the goal.

## Delivery rules

- Work in isolated writable sources; preserve the original workspace and unrelated changes.
- Establish a failing independent test before each behavioral change.
- Remove external AAD implementation, dependency, export, example and CI paths;
  validate native diagnostic OFF/ON and their actual operation contracts.
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
- [x] D00: remove XAD, CoDiPack and Adept code, gitlinks, configuration, exports,
  examples, scripts and CI; verify fresh native-only builds and migration errors.
- [ ] P01: correct production-oriented tape/Jacobian/MC benchmarks and explanatory resource metrics.
- [x] D01: explicit recording lifecycle, checkpoints, nested-use rejection, and exception recovery.
- [x] D02: optional owner/slot-lifetime diagnostics without release per-node overhead.
- [x] D03: thin native operation/capability contracts; remove unpublished external
  adapters and selection metadata rather than introducing a pluggable framework.

## Stage B: market and portfolio risk

- [ ] D04: structured requests/results with stable axes, methods, units, provenance, and budgets.
- [ ] D04: compatibility projections for existing `PV` and `d_...` outputs.
- [ ] F01: deterministic-rate Dupire spread-quote pullback connected to Hybrid valuation gradients.
- [ ] F01: direct quote dependencies and snapshot/axis mismatch validation.
- [ ] F01: calibration-only and full bump/recalibrate/common-path oracles.
- [ ] F01: common calibration pullback integration with existing curve quote-risk semantics.
- [ ] F02: fixed-weight VJP for multiple prepared-script outputs, including aliases/constants.
- [ ] F02: budgeted native blocked Jacobian with explicit rerecording behavior.
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
- [ ] F04: native mixed-mode prototype on smooth kernels and actual capability validation.
- [ ] F04: estimator validation for applicable simulation/calibration cases before general promotion.

## Completion evidence

- [ ] Focused red/green evidence and independent mathematical references for each feature.
- [ ] Fresh full native/core/public/portable-binding verification.
- [ ] Fresh native-only OFF/ON, no external dependency/export/selection paths,
  explicit old-config migration errors and exact implementation-head CI checks.
- [ ] Python and Excel parity, generated-source integrity, and documentation integrity.
- [ ] Existing nine-target performance gate plus changed production-workload coverage.
- [ ] Current-state method docs, examples, and necessary changelog entries.
- [ ] Local read-first review with no unresolved correctness or compatibility findings.
- [ ] Requirement-by-requirement audit of the actual final state.

## Current evidence and next action

Current resumed state: exact `8886c083` passes all 35 CI checks, including the
four MSVC native/profiling/lifetime combinations and sanitizer/TSan jobs.
P01's 24-profile resource/scaling/diagnostic-cost sweep completes 2,496 processes;
all prices and risks satisfy rel/abs 1e-10, and every LSM sample is bitwise equal.
The [complete production report](../performance/aad-production-profiling.md)
records all 65 formal, 44 MC, 25 curve and 44 matched-prefix cases, plus all
scaling/resource/overhead rows. The nine-target gate passes unchanged. Curve
confirmation passes 25/25, but full MC confirmation is 43/44 and matched-prefix
confirmation is 42/44 with different failing cases. Same-binary control passes
44/44; CPU-4 European isolation passes 7/7. Overall production performance remains
inconclusive on the shared WSL2 host, so P01 and Stage A stay open.

Evidence now persists under the home cache; exact source/baseline checkouts and
the `/tmp` alias were restored after cleanup. All 1,681 frozen source/helper/binary
hashes match. Measured executables were retained unchanged.

Independent Stage B work proceeds under the [D04 specification](../specs/aad-risk-results.md),
[API boundary](../api-notes/aad-risk-results.md) and [critique](../critiques/aad-risk-results.md).
The scalar converter has RED missing-interface evidence followed by nine focused
GREEN tests. Its first standard OFF build passes all 2,341 non-benchmark CTest
cases. The public planner and actual execution/product/model/history provenance
are implemented, with seven focused C++ tests, sixteen Python cases, three
portable Excel cases and the independent date-capture consumer passing.
Callback-mutation and malformed Excel factor tests first fail, then pass after
fixing the actual snapshot/error-context defects. Native empty-column requests
retain smoothing; LSM/RQMC results remain bitwise equal to legacy results and
retrained policy risks carry a mixed-method label. Twelve Excel function stubs
are regenerated with their markup.

The complete OFF build passes 2,351 non-benchmark CTest cases (809 Python tests
and all 34 examples); combined profiling/lifetime passes 2,364 cases. Final
review fixes add snapshot geometry/history validation, correct generated optional
boolean defaults and share Excel path-error context. Their actual failing tests
and compile probe are retained; final OFF focused checks pass 25 cases, combined
checks pass 89, and legacy Excel contracts pass 28 per configuration. The final
fully instrumented ASan/UBSan shared libraries pass 77 core and 10 public cases.
Both installed consumers and the independent date-capture consumer pass after
the fixes. Production complexity passes the unchanged CCN-eight threshold;
regeneration has no drift and changes only two optional-boolean inc/HTML pairs.

The [new-entry cost report](../performance/aad-risk-entry-cost.md) retains 640
alternating process samples over 16 cases, all with bitwise single-worker parity
and unchanged source/helper/binary hashes. Structured metadata adds about four
microseconds to short requests; longer native cases differ by -0.27% to +1.70%.
These are additional-capability costs, not an old-entry performance verdict.
All nine final OFF gate executables remain identical to the retained measured
binaries. The simulation/LSM/tape hot loops are unchanged by D04.

Exact new-publication-head CI remains pending. D04 acceptance remains open until
that result is reconciled; P01 stays inconclusive and the complete Stage B/C/D
scope remains required. F01 now has its [frozen-calibration specification](../specs/aad-dupire-pullback.md),
[API decision](../api-notes/aad-dupire-pullback.md) and [critique](../critiques/aad-dupire-pullback.md)
ready for core RED/GREEN implementation, followed by Hybrid and binding oracles.

### Earlier snapshots retained for acceptance context

P01's controlling [production measurement contract](../specs/aad-production-measurement.md)
now specifies actual phase/resource windows, default instrumentation exclusion,
fixed-path thread scaling, long-path/surface/real-output coverage and complete
numeric/no-regression acceptance. In local unpublished work, profiling package
configuration first fails, then all three OFF/ON/combined settings pass. Native
scoped timing, explicit tape high-water samples, actual successful block-allocation
events and task-owned collectors are implemented. Focused tests first expose absent
interfaces and an incorrectly complete unexecuted task, then pass after correction.
Ordinary passive/AAD and LSM training/regression/replay call sites are instrumented;
the earlier 15 focused tests pass, including passive/failure handling and strict
LSM price/every-risk bit comparisons. Fresh full OFF/ON builds pass 2,332/2,346
CTest cases, each with 793 passing Python tests, and both installed consumers pass
in both configurations. Regeneration has zero drift after staging the three new
generated outputs with their markup; the initial untracked-output rejection is
retained. Default Release has no allocation/tape/span/clock/collector hook symbols
or references. Number/node/tape/recording sizes remain 16/40/368/72 bytes; the
task-group size remains 48 bytes OFF and is 56 bytes ON. This supports layout and
instrumentation exclusion, not a throughput claim. The combined lifetime/profiling
full build passes 2,375 CTest cases, including 793 Python tests, and both installed
consumers pass. The 1,179 compiled/test input hashes still match the final build
manifest for that earlier snapshot. The 28 published-head checks at `6161e5a` all pass and do not cover
these unpublished changes. P01 remains open until workload/resource/scaling,
diagnostic-cost, default-OFF performance, complete feature and new CI acceptance.

The latest unpublished increment implements setup-prefix samples, selected
array payload/capacity, same-request self thread CPU, and lazy memory callbacks
that skip scans outside an explicit measurement scope. Missing interfaces and
prefix samples first fail; the fully rebuilt ON library passes all 20 focused
tests. The production CLI supports five scenarios, cold/warm/phases, expanded
surfaces, and 1/4/16/64 distinct outputs with explicit scalar-output execution.
Independent short-BS analytic price/four-risk checks preserve the 64-output
kink failure evidence rather than weakening tolerance. The permanent CLI
contract and 38 representative functional cases pass. Current ON passes all
2,351 non-benchmark tests, including 793 Python tests; the command also ran
21 benchmarks under simultaneous builds and returned failure for the existing
rate-risk overhead timing assertion. Keep that failure and perform quiet serial
revalidation followed by the formal paired gate. Latest OFF/combined rebuilds,
consumers and erasure proofs have since passed: OFF 2,332 complete functionality
checks, two installed consumers in each of all three configurations, refreshed
Release symbols/layouts, and an O0 Debug call-erasure probe. Combined passes
20 focused cases and all 2,380 full-suite projects, including 793 Python tests
and the slow European MC example. Quiet serial ON benchmark smoke passes all
21 targets, including the initial parallel timing failure; both logs remain.
Sixteen focused profiler ASan/UBSan cases pass;
the supporting Release archive is not fully instrumented, so full sanitizer
coverage remains the new CI's responsibility. Resource/scaling/overhead,
formal default-OFF performance and new-head CI are pending. Full details and
raw log paths are in the production contract.

Published profiling source `5a6382b` passes the frozen original-baseline formal
nine-target gate: all 65 comparable cases pass the unchanged two-by-ten/4%
policy, ten head-only cases remain informational, and Sobol precise/fast is
9.38x below 10x. Raw results are `production-profiling-paired-01/` under the
evidence root. This does not complete affected production, scaling, resources
or diagnostic-cost acceptance. Its first Codacy check reports 26 new checker/
consumer issues. A local test-tool refactor keeps every numeric case/tolerance,
uses always-on unittest checks and validated shell-free build-tree execution,
passes ON/OFF-optimized contracts and all installed consumers, and measures
maximum complexity 8/4. No library/benchmark source changes in this corrective
increment. Merge b37b49f integrates master's 3fe44ecd documentation and resolves
the Copilot conflict while retaining source/docs-only classification and the
native-only matrix. Production/build/benchmark inputs are unchanged by that
documentation merge. Its Codacy check succeeds and Actions actually run; two
MSVC profiling-ON legs fail because windows.h's VOID macro replaces the generated
StackInfoType_ enum value. Clear the macro locally after the private Windows
include. Preserve both failing job logs; exact corrected-head MSVC compilation,
runtime and all applicable CI checks remain required.

The same head's default Windows configuration passes 2,311 functional tests,
then fails the CLI step's second CMake configure: vendored pybind11 caches FOUND
without reloading its commands. An independent installed-Python consumer
reproduces first-configure success and second-configure failure. Change the
three discovery guards to actual command availability; two corrected configure
passes succeed without changing lookup order or skipping the existing Windows
CLI reconfiguration. Python 3.12 vendored standalone binding configures twice,
builds and passes 792 tests with one skip for its absent monorepo-only native
test module. Refreshed default/profiling/combined core builds pass; profiling/
combined each pass 20 focused cases and default/profiling six CLI contracts each.
Exact corrected-head Windows CI remains open. Both CI and local RED logs are
retained.

The corrected Windows lifetime leg loads pybind11 successfully, then exposes a
second CLI compile defect: MSVC tries to copy a noncopyable ScriptProductData_
through the conditional expression. Use direct prvalue returns in a local
lambda; keep product ownership and compiler flags unchanged. Preserve that RED
job log and require fresh MSVC compilation.
The corrected CLI rebuilds in OFF/ON, passes six contracts in each and all 38
representative diagnostics again. Formatting and docs checks pass. No core
algorithm changes require repeating otherwise-unaffected library tests locally;
the new exact-head Windows compile/run coverage remains mandatory.

The frozen 7ead7ecd default-OFF nine gated binaries, configuration rows and gate
script match the passing 5a6382b inputs exactly. Its 44-case supplemental MC
pairing passes all cases at two-by-ten; all paired LSM prices/risks agree. The
GSR 1F bond's +5.5307%/+3.9994% movement remains visible. The curve supplement
passes 24/25, with a 24-node PWL DF query failure at +6.58%/+4.29%. Preserve this
initial failure. Before further sampling, freeze a complete 25-case prior-native
8816951 control at two-by-ten and complete original-baseline confirmation at
two-by-thirty, retaining the same 4%/min/interleaving rule. Persistent movement
requires diagnosis; this increment is not accepted from a partial performance
pass or an older green CI snapshot.

The native-only development source is `/tmp/dal-aad-backend-adapter`. External
implementations, three gitlinks, build/export paths and CI jobs are removed;
native operation services use `native.hpp` without adapter inheritance or selection.
Fresh OFF/ON full builds succeed with external dependency directories absent.
Final full CTest passes 2,330 OFF and 2,359 ON cases, including 793 Python tests
each and all 34 examples. Both final prefixes pass the two installed consumers.
Configuration/header migration checks, generation/drift and 60 focused ASan/UBSan
tests pass. The AAD example matches an independent price/five-partial oracle;
the corrected vanilla example matches price and six partials. Preserve its
initial diagnostic failure and include-order build failure logs.

Frozen native-only head `8816951662fc6788f3e9876c45ac72f7e48c65f7` passes
the unchanged nine-target gate (65 comparable cases; ten new informational rows),
25 curve cases and all 44 production cases in the predefined two-by-thirty
confirmation. The initial two-by-ten production run fails GSR 1F swaption
(+5.75%/+6.84%) and four-node AAD market fit (+4.46%/+5.13%); retain both.
The published-head native control passes all 44 cases. Original-baseline fixed
confirmation gives -1.30%/-3.63% and +3.52%/+5.24%, respectively. The latter
remains a borderline positive cost; passing the two-round rule is not a claim
of zero runtime overhead. Three resource pairs have overlapping RSS ranges,
zero major faults and equal daily-LSM results. Source/configuration/five pins,
all 22 binaries and helpers are unchanged through measurement.
The [native-only report](../perf/aad-native-only.md) records every case, initial
failures, control provenance/annotation correction, fixed confirmation and limits.
At publication `b42f9eb9c12a5987e8a81a869f59b38d8096a1de`, all 28 exact-head
checks pass, including complete diagnostic sanitizers, native compilers, both
stable gates, MSVC/consumers and all wheel platforms. The
[native contract audit](../reviews/aad-native-contract-audit.md) verifies D00,
D01, D02 and D03 requirements. The library/build/benchmark diff from measured
`8816951` is empty and all 22 binary digests still match.
D01's initial numeric-coverage gap is filled by the
[production comparison](../perf/aad-recording-numeric-validation.md): all ten
ordinary MC and 25 curve cases, 34,028 numeric cells, pass rel/abs 1e-10, including
complete risk vectors, residuals and Jacobian/inverse matrices. The existing
three LSM profiles also agree in every paired sample. Remaining P01
phase/resource/scaling measurement and all later
stages remain required.

Protected guidance sync item: `CLAUDE.md:35` still lists legacy AAD selection
options. AGENTS.md forbids editing Claude originals without explicit user
authorization. Editable current-state docs and Codex contracts are updated;
this protected line remains an identified follow-up.

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
`lifetime-assignment-{on,off}-{build,ctest}.log`. Fresh default OFF pairing is
recorded below; current publication CI remains required before accepting the
corrected increment. Keep the earlier `c83bcc9` data.

At frozen corrected head `8e1ef0941a949059858aa78cab46af4cb50b41e6`, fresh
nine-target pairing passes all 65 comparable cases; ten new rows remain
informational. All 35 ordinary MC cases and three LSM profiles pass, and all
LSM PV/risk entries match. Initial supplemental calibration fails its 8-node
query (+8.73%/+5.61%); retain the failure. With unchanged sources and binaries,
independent two-by-ten confirmation and fixed two-by-thirty stability checks
pass all 25 cases. The query becomes -0.75%/-2.39% and +1.54%/-1.32%,
respectively. A same-binary control is retained. This supports measurement
variation rather than a sustained regression, without establishing a specific
hardware cause. RSS ranges overlap and major faults are zero. Every measured
source/configuration/dependency/helper/binary identity is unchanged afterward.
The [corrected D02 report](../perf/aad-native-lifetime-diagnostics-corrected.md)
records every row, initial failure, confirmation and limitation. Exact-head CI,
the D02 requirement audit and D03 capability reporting remain required.

Direct D01 result-extraction failure coverage now completes a scoped reverse,
reads its analytic price/gradient, throws during extraction, preserves that
exception and proves the next independent scoped graph gives its analytic
result. Native ON/OFF focused runs pass 76/48 cases. The new test and include
ordering change no production or benchmark source. The complete CoDiPack
rebuild/CTest passes 2,258 cases, including this recovery test.
Publication head `9b5febcca79643e817999e76e8f5dda44922d618` now passes all
49 exact-head CI checks, including the new complete native diagnostics ON/OFF,
diagnostic sanitizer and Windows/consumer legs. Checked PR head and every
paginated check are retained in `lifetime-9b5febc-{pr-state,ci}` evidence.
The final D01/D02 requirement audit remains open.
The independent full-final-block `BlockList::Size` repair has its
bounded timeout RED and eleven-case ASan/UBSan GREEN evidence; it also awaits
final paired/CI acceptance; its full native/CoDiPack functional checks pass.

D03's pre-removal work is in `/tmp/dal-aad-backend-adapter`, based on the
unchanged successful publication head, with local commits `2b4bf89` and `5689ff9`.
Full native OFF/ON CTest passes 2,328/2,324 cases; XAD, CoDiPack and Adept each
pass 2,264. All five installed consumers pass; diagnostic ON passes 60 focused
ASan/UBSan cases with leak detection, and native OFF passes 21 serial benchmark
smoke cases. These are terminal local results, not publication or paired
performance acceptance. The compile/UBSan RED failures remain recorded.
The amended [native operation contract](../specs/aad-backend-adapter.md) preserves
useful seed/channel/lifecycle behavior while removing external adapters under
D00. This pre-removal evidence did not establish native-only acceptance; the
fresh removal pairing is now recorded above and publication CI remains open.

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

Next: finish D04 validation, complexity refactoring, new-entry cost evidence and
draft publication. Then connect F01 calibrated quote pullbacks and F02 output
seeds under their controlling designs. Reconcile P01's unresolved performance with controlled-environment evidence;
retain every failure and do not replace the threshold or sample until a pass.
Stage A and the full Stage B/C/D goal remain incomplete.
