# F01 automatic Dupire script request

Status: active C++ increment, locally implemented and under verification;
own publication CI, performance and binding acceptance remain open. This extends
the [common quote request](aad-calibration-risk-request.md) and preserves the
[accepted Hybrid extraction](aad-dupire-pullback.md). The
[integration audit](../plans/aad-market-request-integration.md) and
[implementation ledger](../plans/aad-implementation.md) retain full F01 and merge scope.

## Problem and scope

Callers currently obtain scalar model/constant risk, manually select every
surface input, then call `ExtractDupireParameterAdjoints` and a calibration VJP.
A missing required input fails only during extraction. Quote selection/report
budgets are now available over the common boundary, but do not plan valuation.
The automatic path must identify complete required surface/direct inputs and
the final numeric payload before history lookup, compilation, calibration
recording or worker submission.

First automatic execution supports one scalar script payoff, one frozen Dupire
calibration and one named local-vol component in a single-currency flat-rate
Hybrid. Other known BS/local-vol equity components remain fixed calibration
inputs and retain their pricing dependence. No new estimator, inverse, solver,
joint stochastic-rate calibration, multi-output root or portfolio netting is added.
Existing conditional extraction and legacy pricing APIs keep their broader scope.

## Requirements

R1. `DupireScriptRiskRequest_` is an owning passive configuration: required
positive `numPaths_`, common quote request, explicit direct constant bindings,
optional common direct adjoints, valuation settings and native AAD simulation
settings. Defaults match scalar D04, with AAD enabled. Required product/model/
calibration/component/request precede execution; no long positional settings list.

R2. `PlanDupireScriptRisk` returns an immutable owning passive plan. Expose its
component, common quote plan, complete model/constant axis, required input axis,
direct bindings, path count, captured settings and exact numeric-result payload.
Do not expose private sealed product/model handles, active nodes, AST evaluators,
callbacks or workers. Copy plans through shared const data. Execution can outlive
or run after mutation/destruction of original caller data.

R3. Before any model serialization, reject unsupported custom dynamic types.
The first sealed path accepts exact `HybridModelData_`, exact
`HybridConstantCorrelationData_`, exact nested `LocalVolSurfaceData_` and exact native BS/local-vol equity and flat
deterministic-rate component types. Validate through passive `CreateModel<double>`
and deep-copy known model data through its native archive round trip. Copy
product date/event/settings data into a new exact native `ScriptProductData_`.
Do not borrow a caller's mutable public component vectors, surface matrices,
correlations or model settings. Reject subclasses whose virtual archive behavior
could introduce callbacks or change the promised model graph.

R4. Locate the target component by typed identity/name and the actual native
sorted component order. Reuse accepted surface grid/value and parameter-layout
checks. Target spot/dividend and the unique flat deterministic domestic rate
must equal the frozen calibration's spot/dividend/rate. The existing Hybrid
factory validates unique numeraire provider and common currency. Unknown target,
wrong component type, changed surface/carry or stochastic/nonflat rate reject
during planning. These are restrictions of the new opt-in automatic path only.

R5. Build the complete scalar model/constant axis using the same native helper
as D04. Expose every required local-vol node in native spot-row/time-column order,
mapped to actual `model:<ordinal>` IDs. Append the explicitly bound script
constant IDs in binding order. Quote selection does not select these valuation
inputs; their complete required axis is inspectable before execution and is the
returned scalar valuation's selected input axis. There is no hidden expansion
of a caller-selected model axis because this API takes only quote selections.

R6. `DupireQuoteBinding_` owns a script `constantOrdinal_` and a source-scoped
`quoteId_`. Verify the constant exists, the quote ID belongs to the owned source,
their finite native values agree exactly, and both constant and quote IDs are
unique. Bindings are structural caller declarations; matching labels/values
never infer a dependency. For each binding, extract the scalar constant partial
with the calibrated surface held fixed, assign it to that quote cell and add
it once after calibration VJP. Preserve native mean normalization.

R7. Automatic bindings and externally computed direct quote seeds are mutually
exclusive to prevent double addition. External seeds retain the accepted common
Dupire quote-only identity (strikes/maturities/spread values, fixed IVS may differ),
finite and shape checks before execution. Own the checked external input in the
plan; do not retain it as a separate numeric array in the published result.
The passive native `MatchesQuotes` helper shares the accepted `SameQuotes`
comparison; complete parameter matching and legacy direct validation keep their
existing bodies and domains.

R8. Let S be the complete required surface-node count, B the direct constant
binding count and Q the full native quote count. The result retains one scalar
value, a 1-by-(S+B) raw valuation Jacobian and the common three full quote
contribution matrices. Its exact numeric-result payload is
`RiskResultPayloadBytes(1, S+B) + CalibrationRiskPayloadBytes(rows, cols)`.
Check extent sums and byte addition overflow before allocation/history/workers.
The common request's numeric budget applies to this combined total, not just
the quote part. Full/subset/empty quote selections have the same payload for
fixed bindings. Equality succeeds; one byte less rejects during planning.

R9. This retains D04's numeric-result budget scope: source/archive/metadata,
plan/input seed storage, getter copies and temporary VJP/tape/worker allocations
are excluded. It is not RSS or a request-wide workspace cap. A result stores
valuation plus common quote result, component/method and payload count; it does
not keep the execution plan's extra input matrices or sealed live model/product.

R10. Copy simulation/valuation settings during planning and validate simulation
choices before work. Capture an omitted evaluation date during planning using
the existing date-capture helper, without reading fixings. Explicit fixing
snapshots already own immutable maps and are shared. Global fixing data is
resolved and frozen at execution through the existing scalar valuation path,
after the successful budget preflight and before workers. State this timing;
do not claim standalone planning freezes future global history.

R11. `ValueByMonteCarloWithDupireRisk(plan)` runs the accepted scalar native AAD
valuation with exactly the plan's required inputs, settings and paths. Then use
the accepted retained model/complete-axis/surface verification to extract native
surface seeds and apply the common quote plan. Model risk provenance remains
`calibration=fixed`; combined method is valuation method followed by the common
calibration method. Preserve expired and mixed retrained-policy method labels.
No price-only execution is accepted and empty quote selection retains the same
smoothed AAD estimator and full required valuation risk.

R12. Verify prepared/executed required IDs, values and extents against the sealed
plan before publishing. Getters return owned/detached passive values only, with
no fixing lookup, JSON parsing, another reverse or recalibration. A failure leaves
earlier plans/results valid and permits a later valid execution. New opt-in
restriction checks must not modify existing extraction behavior.

## Implementation sharing and acceptance

Share the native D04 axis builder and accepted Dupire surface-layout checks.
Use private public-layer helpers; do not send public calibration types into
core script planning. If sharing changes existing translation units, freeze
their accepted objects/installed consumers first and measure every affected
legacy entry under the unchanged paired 2-round/10-process/4% policy. New costs
remain separate. Do not duplicate the ordinal-layout algorithm in the new planner.

- Missing API RED, then valid flat/Merton plans with actual unsorted components.
  Required native ordinals and selected quote ordering are inspectable and correct.
- Unknown/wrong/mismatched surface or carry, unsupported custom/stochastic types,
  invalid paths/settings, invalid/duplicate direct bindings, external direct
  source mismatch and one-byte-short budgets reject while fixing/compilation/
  submission observers remain at zero. Passive planning preserves an unrelated
  native recording's node count and independent derivative.
- Equal budget succeeds for full/subset/empty selections. Pure extent arithmetic
  tests reject overflow without huge allocation. A subset budget cannot hide
  full native quote arrays or required valuation nodes.
- Plan execution after original model components/surface/correlations/product/
  request/settings mutations matches an independent detached pre-mutation reference.
  Also mutate caller data inside a fixing observer to prove workers use sealed data.
- Same paths, compiled/interpreted settings, smoothing and normalization match
  the accepted manual full-input valuation/pullback exactly. Preserve actual
  direct fixed-surface partials, signed/zero cases and flat/Merton full
  bump/recalibration oracles at the existing three steps/tolerances, with adjacent
  step agreement. Parity alone does not replace those mathematical oracles.
- Empty quote selection keeps the AAD finite-sample price and full required
  model/constant result, including smoothing-sensitive payoffs. Expired results
  retain correct zero risks and method; mixed policy methods remain explicit.
- OFF/combined, fully instrumented leak-enabled ASan/UBSan, installed C++, actual
  Windows runtime/own-head CI and strict Python/Excel request/plan/result parity.
  Legacy behavior/performance gates and all original failed evidence remain.

## Open work

The passive planner and scalar execution/direct extraction are implemented;
17 new request cases and two automatic oracle cases pass locally with the 50
accepted related cases. The new flat/Merton quote oracle
adds 84 original-protocol rows with unchanged steps, tolerances and adjacent-step
rules. Fresh OFF/combined CTest passes 2,457/2,471; leak-enabled fully instrumented
ASan/UBSan passes 69; MSVC syntax passes 12 units; installed consumers pass 128
processes. All 64 changed old-entry rows pass the unchanged paired performance
policy. Own publication CI and request bindings still require acceptance. Full F01
closes only after all these increments pass. Whole-PR fixes/P01/master/final
review/check gates precede the authorized merge of PR #480; later phases use new PRs.
