# F01 market-request integration audit

Status: active implementation audit after common C++/Python acceptance and
Excel publication `a522b16bc0b6473dbe23ed45c7305faaeaa0f072`. This records
remaining work; it is not implementation acceptance or a replacement for F01.
The full [ledger](aad-implementation.md) controls scope, performance policy and
the user-authorized F01-first merge of PR #480. Later stages require new PRs.

## Source and findings

The original detailed plan section 4.1 requires deterministic-carry Dupire
spread risk followed by a common calibration boundary. Appendix G combines
D04/F01/F02/P02: its weighted outputs and blocked Jacobian belong to F02, but
market-coordinate planning, ownership and budget requirements still need
explicit integration. The accepted common boundary alone does not provide it.

| Requirement | Current source/evidence | Remaining gap |
|-------------|-------------------------|---------------|
| Frozen discrete Dupire and Hybrid chain | `dal/model/dupirerisk.hpp`, public `dupirerisk.hpp/.cpp`; accepted calibration and common-path oracles | Retain current method, fixed-input boundary and raw native layouts |
| Common curve adaptation | Public `calibrationrisk.hpp/.cpp`; retained native inverse and full captured content | None in native math; preserve independent domain/currency grouping |
| Language reachability | Accepted C++/Python; Excel common/capture published for own CI | Inspect exact Excel head, including XLL runtime |
| Model-coordinate scalar request | Core `RiskRequest_`, `PlanScalarRiskRequest`, public `riskvalue.cpp` | Inputs are model/constant IDs, not quote coordinates |
| Complete surface inputs | Public `ExtractDupireParameterAdjoints` verifies retained model/axis and refuses missing selected inputs | Plan required surface ordinals before history/compilation/workers instead of requiring callers to discover them |
| Source identity | Complete Dupire content and case-sensitive curve record; existing Hybrid component/offset checks | Reuse typed ownership and validate prospective model mapping before execution |
| Quote selection and report projection | Common result retains complete native contributions; scalar report factors apply to model/constant coordinates | Define quote-coordinate selection and apply report factors once without changing stored raw values |
| Result numeric budget | `RiskResultPayloadBytes(1, model_columns)` guards scalar values/Jacobian before preparation | Include newly retained quote contribution matrices; define exact scope and checked arithmetic |
| Direct contribution | Checked typed common/legacy direct seeds add once after calibration VJP | Define explicit request ownership/validation and how script-constant direct bindings participate; do not infer from matching labels |
| Compatibility | Old Value/PV/d-label projections remain; old curve DV01 grouping/scaling remain | Verify new opt-in planning cannot change these legacy entry points or default costs |

The existing `RiskValueTest.TestInvalidRequestsFailBeforeHistoryCompilationOrWorkers`
is the planning-failure analogue. `DupireScriptRisk` tests provide the actual
unsorted-component mapping, selected-input rejection, full retained model
verification and fixed-path node/quote oracles. Reuse their mathematical
protocols; copying wrapper output is not an independent derivative oracle.

## Next increment boundaries

1. Write and locally critique a focused public API/spec before behavior changes.
   Keep request planning above `dal-cpp`: core model/script requests must not
   depend on public common calibration types. Preserve old signatures and paths.
   Required source/configuration precedes optional valuation/simulation controls.
2. Plan one scalar payoff and one frozen calibration boundary. Expose an owning
   passive plan with canonical quote selection/order, required model inputs,
   direct-input requirements, method/units/fixed-input boundary, shape and exact
   numeric payload estimate. Do not retain active nodes, callbacks or worker state.
3. Reuse typed source axes. Preserve Dupire strike/maturity and surface spot/time
   layouts; preserve curve global ordinals and block coordinates. IDs must resolve
   within the owned source, not display labels or fingerprints alone. Reject
   unknown/repeated IDs and mismatched sources before recording or execution.
4. For Hybrid Dupire, resolve the named typed component and complete required
   surface inputs from actual model parameter order before running MC. Check
   surface values/grids and deterministic-carry assumptions explicitly. Recheck
   retained execution identity at extraction. Different owners with equal grids
   are not interchangeable by label or coordinate alone.
5. Define direct inputs explicitly. Typed precomputed direct seeds retain their
   accepted contract; an automatic script-constant binding, if included, must
   validate owner, ordinal, quote value and uniqueness before execution. Require
   fixed-surface partial derivatives, with no second normalization or double add.
   Do not advertise arbitrary market-dependent scripts as automatically supported.
6. Count all numeric value/Jacobian and quote-contribution payload retained by the
   new result, including full matrices retained underneath a selected projection.
   Share existing immutable source storage; specify which source/metadata and
   temporary worker/tape allocations are outside this numeric-payload budget.
   Preserve that honest distinction from RSS or total-request memory limits.
   Check extent multiplication/addition overflow before allocation/preparation.
7. Keep raw calibration/direct/total contributions and an explicit report-only
   projection. Default factor one and vol-point/DV01 factors must be testable.
   No getter performs history lookup, calibration, valuation or another reverse.
8. Expose the validated request/plan/result across C++, Python and Excel. Python
   keeps strict bool/enum/finite/shape checks, detached values and callback-free
   release. Excel keeps immutable handles, raw range guards, generated registration
   integrity and XLL-local runtime verification. Bindings must share native plans.
9. Preserve the scalar estimator, normalization, paths, reduction and exercise
   method. Empty quote selections must not silently switch an AAD/smoothed
   execution to a price-only estimator. Multi-output weights, blocked reverse,
   workspace reuse, implicit calibration and second-order behavior remain later.

## Executable acceptance to add

- Missing planner/API RED followed by canonical-plan tests for Dupire and all
  four captured curve providers, ordered/subset/empty selections, duplicate names,
  complete IDs, source mismatches and actual unsorted Hybrid component owners.
- Budget equal to exact retained numeric payload succeeds; one byte smaller
  rejects before fixing observers, compilation, task submission and calibration
  recording. Dimension/count overflow rejects without large allocations. Include
  selected outputs whose underlying retained full matrices are larger.
- Planned required gradients match the accepted manual full-input path exactly.
  Same paths, smoothing, compiled/interpreted modes and native mean normalization
  remain. Preserve independent flat/Merton full bump/recalibrate oracles and
  every previously declared step/tolerance; include signed/direct/zero cases.
- Snapshot/direct-binding mismatch, NaN/infinity, unknown input, archive and
  partial-operation failures preserve earlier results and allow a later valid
  request. Plans/results survive mutation/destruction of caller-owned inputs.
- Report factors change only reported copies. Old PV/d-label behavior and legacy
  quote/DV01/currency outputs remain exact. Ambiguous display projections reject
  rather than overwrite coordinates or silently net currencies.
- OFF/combined, sanitizer, installed C++, Python joint/standalone, Excel portable
  and real Windows runtime verification. Measure changed legacy entry points with
  frozen inputs and the unchanged two-round/ten-process/4% policy; report new
  request cost separately. Inspect each increment's exact publication-head CI.

## Delivery after this increment

Complete the above acceptance before closing F01. Then perform the requested
whole-PR problem review/fixes, resolve P01 production performance, update against
master and inspect final-head checks/reviews before merging PR #480 with the
match-head guard. Continue remaining stages in new PRs. No additional user
permission is required for this authorized sequencing.
