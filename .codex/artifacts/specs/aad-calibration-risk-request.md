# F01 calibration-coordinate request contract

Status: active; the common passive request is locally implemented, with
publication acceptance pending. The contract was specified before implementation. The
[market-request audit](../plans/aad-market-request-integration.md) and
[implementation ledger](../plans/aad-implementation.md) retain full F01 scope
and the current-PR merge boundary. This first increment implements the common
passive request/plan/result; automatic Hybrid script planning and bindings
remain separate required increments of F01.

## Problem and source

`dal-public/src/calibrationrisk.hpp` provides an owning, typed calibration
boundary and three canonical raw contribution matrices. It has no quote
selection, report projection or numeric-result budget. Core
`dal-cpp/dal/script/riskresults.hpp` plans model/constant coordinates; quote
ownership must remain above that dependency boundary. Native Dupire inputs
retain decimal-vol spread values and strike/maturity grids. Native curve
provenance retains block/global quote ordinals and units, but exposes no typed
raw quote-value array. Do not manufacture a curve quote value or parse its
captured record to recover one.

## Goals and scope

Provide one inspectable passive plan for one frozen calibration source, with
canonical quote axes, ordered selection, detached report projections and an
exact numeric-result payload estimate. Reuse the native common VJP and source
identity checks. Keep old entry points outside the new planner.

This increment does not value products, infer direct script dependencies, net
currencies, alter inverse/normalization methods, impose tape/RSS limits or
implement F02/F03. These are limits of the increment, not removal of the
remaining F01 requirements.

## Requirements

R1. `CalibrationRiskRequest_` owns optional input IDs, optional report factors
and an optional numeric-payload byte budget. Omitted inputs mean all quotes in
canonical order. An explicit empty selection remains empty. A supplied order
is preserved exactly. Unknown, repeated and embedded-NUL IDs fail before VJP
recording. Canonical IDs are `quote:<globalOrdinal>`, scoped to the owned source;
display labels and fingerprint equality alone are never ownership evidence.

R2. `CalibrationRiskPlan_` privately owns its common boundary and complete and
selected passive quote axes. It exposes the selected ordinals and payload
estimate. No setter, active number, worker, callback or calibration recording
is retained. Copying a plan shares the immutable native source and copies its
passive request metadata. Mutation/destruction of original request/source
handles cannot change a published plan.

R3. `CalibrationQuoteCoordinate_` exposes ID, global ordinal, native row/column,
display label, native unit and report scale. Dupire additionally exposes actual
strike, maturity and decimal-vol spread value; row is strike, column is maturity.
Curves expose the original case-sensitive block key and block ordinal and keep
strike, maturity and quote value absent. Curve native row is global ordinal and
column zero. Curve coordinates must agree with contiguous native ordinals and
the common boundary dimensions. Full typed source identity stays inspectable
through the plan, including grids, state and captured calibration content.

R4. Omitted report factors become one. Supplied factors match the selected
count and are finite and strictly positive. Factors apply once to detached
projections; raw matrices and stored raw derivatives never change. Validate
reported calibration/direct/total values before publishing a result, including
finite-overflow failures. Empty selections still execute the native VJP and
preserve its method/source contract.

R5. The common request result retains exactly the native three full matrices
(calibration, direct, total) and no cached projected matrix. The numeric-result
payload is `3 * quoteRows * quoteCols * sizeof(double)`, independently of the
selected count. Extent multiplication and byte arithmetic reject overflow before
allocation. The collapsed quote count must fit the matrix column limit before
constructing axes. Budget equality succeeds; one byte less fails during
planning. Default budget is absent/unlimited, matching scalar D04.

R6. This budget counts retained numerical result arrays. It excludes source
calibration storage, metadata/axis numeric fields, request inputs, getter return
copies, temporary VJP matrices, tape and worker allocations. It is neither an
RSS nor a total-request memory guarantee. The estimate and documentation must
state this scope consistently. A later automatic script result adds the retained
scalar valuation value/Jacobian to this estimate using checked addition; selected
quote counts never hide the three full native matrices.

R7. `PullbackCalibrationWithRisk` accepts a sealed plan, typed owning parameter
adjoints and optional typed owning direct quote adjoints. Reuse existing
parameter-source matching and finite/shape validation before recording. Curves
require full direct-source identity. Dupire direct seeds require equal quote
strikes, maturities and spread values; their fixed base IVS may differ, preserving
the accepted direct partial-derivative contract. Add the direct
term exactly once after calibration pullback. Publish an immutable result only
after all mapping and report validation succeeds. Failed operations leave prior
plans/results usable and do not poison a subsequent native recording.

R8. A result exposes its plan, native common quote risk and four detached scalar
row projections: raw total, raw calibration, raw direct and reported total.
Each projection follows the selected axis order, including 1-by-0 for empty.
Getters perform only copies/projection; no history, valuation, calibration,
recording, JSON parsing or reverse propagation. Native raw matrices preserve
their original Dupire/curve shapes and units. No PV/currency is inferred from
an external parameter gradient.

R9. The future automatic Dupire plan is a distinct valuation plan above this
common plan: it owns deep passive product/model snapshots, reports every required
surface/model/constant input, and checks shape/source/deterministic carry before
history or workers. Quote selection does not select model risk inputs; the
returned valuation contains the explicitly exposed required inputs. Optional
direct bindings use exact script constant ordinals and quote IDs, verify equal
native values and uniqueness, and are mutually exclusive with external direct
seeds. The retained model result remains `calibration=fixed`; the combined result
labels the subsequent mapping. These choices resolve the audit decisions but
require their own concrete execution tests before implementation acceptance.

R10. Legacy common/native/valuation APIs remain behaviorally and numerically
unchanged. Changed old entry points must meet the original two-round,
ten-process, 4% policy; new request cost is reported separately. Exact native
oracles, methods, finite-difference steps/tolerances and currency groups remain
unchanged. No fallback to label dictionaries or weaker calibration semantics.

## Acceptance

- Confirm missing public planner/API fails first. Then test Dupire canonical
  row/column/value/strike/maturity axes; all four captured curve kinds use the
  actual native block/global ordinals, units and absent quote values.
- Test omitted, reordered, subset and explicit-empty selections, independent
  sources with matching display labels, repeated/unknown/NUL IDs, wrong factors,
  zero/negative/NaN/infinite factors and immutable copied request/source data.
- Test exact/full-payload budget and one-byte-short subset/empty budgets;
  multiplication overflow rejects without large allocation. Planning/errors
  within an unrelated live recording preserve its node count and derivative.
  Execution with an empty selection must still hit the native independent-scope
  guard, proving that the VJP was not replaced by a no-op.
- Compare planned raw three-contribution outputs with accepted native VJP using
  nonzero signed seeds and direct terms. Preserve independent coupled/calibrated
  curve and Dupire oracles; wrapper parity supplements rather than replaces them.
- Verify factor-one, vol-point and DV01 projections, finite scaling overflow,
  detached getter mutations, source mismatch, passive getters within unrelated
  recordings and failed-operation recovery. Empty selection retains full native
  results and method rather than suppressing VJP work.
- OFF/combined tests, leak-enabled ASan/UBSan, public installed consumer,
  native default-entry compatibility and changed-input measurement inventory.
  Python/Excel exposure and exact publication-head CI remain required before
  full F01 closure.

## Open work

Implement/review the common request first, then the sealed automatic Hybrid plan
and execution, explicit constant direct bindings and Python/Excel parity. Complete
whole-PR fixes, P01 production acceptance, master reconciliation and final-head
checks/reviews before the authorized merge of PR #480. Later phases use new PRs.
