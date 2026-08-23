# Full Product-Family AAD Node Risk and Portfolio Aggregation — Implementation Plan

> Status: reviewed implementation plan, not yet started. The underlying design
> passed an L-level review with no blockers against baseline
> `origin/master@ec89eb72`; the review's three non-blocking follow-ups are
> incorporated below. Design rationale, current-code facts, the incremental API,
> and the frozen decision points live in the companion
> [design](aad-node-risk-portfolio-aggregation-design.md).

## Review follow-ups (non-blocking, pinned here)

The L-level review raised three suggestions that must be resolved when the
corresponding implementation issue is created. They narrow or clarify the plan
below without reopening the design:

1. **XCCY dependency-key derivation must be pinned (stage 4).** The current
   signature `BuildRateCashflowPlan(trade, valuationTime)` cannot see the market,
   and `XccyTradeTerms_` carries no component keys — the "XCCY emits non-empty
   dependency keys" requirement can only be met by a market-aware overload
   (incremental; the existing public signature is unchanged) or an equivalent
   mechanism. The review also pins the meaning of "depends on": the curves the
   trade's config **actually consumes** (selected by collateral/tenor), not block
   membership — otherwise an in-block but unused curve would yield the ambiguous
   eligible-with-all-zero-gradient result. A negative test case covers exactly
   that unused-member shape.
2. **Aggregation grouping key must derive from each family's actual PV pricing
   currency (stage 5).** XCCY PV is denominated in the domestic currency via
   covered interest parity inside the kernel (`PriceXccyContract` returns the
   combined domestic−foreign value); non-XCCY families are denominated in the
   trade's own currency. `RatePricingTradeResult_.currency_` is only a
   `market.resultCurrency_` passthrough (constant within a batch) and
   **must never** be used as the grouping key —
   grouping on it would silently sum across currencies. The implementation issue
   fixes the XCCY treatment explicitly: either the pair key forms its own group,
   or it is explicitly folded into domestic with an annotation.
3. **The `componentKeys` parameter semantics of the batch API need one pinned
   sentence (stage 5).** Either one key list applied to every trade (a cross
   product of unified keys × trades) or per-trade key lists. The new surface
   carries no compatibility risk either way, but the aggregation axis definition
   depends on the choice.

## Stages 0–5

Each stage is one independently mergeable, independently revertable PR. A stage's
rollback boundary is what reverting that PR restores; later stages never assume
unmerged earlier state.

| Stage | Scope                            | Gate                             | Rollback                       |
|-------|----------------------------------|----------------------------------|--------------------------------|
| 0     | Kernel templatization            | Existing tests green, unmodified | Passive pricing unchanged      |
| 1     | Multi-component AAD + FRA/Future | FRA/Future tests green           | Success domain back to Deposit |
| 2     | OIS/IRS                          | OIS/IRS tests green              | Success domain back to stage 1 |
| 3     | Basis                            | Basis tests green                | Success domain back to stage 2 |
| 4     | XCCY                             | XCCY tests green                 | Success domain back to stage 3 |
| 5     | Batch + bindings + perf          | Full acceptance criteria         | New APIs/surfaces removed      |

### Stage 0 — kernel templatization (pure refactor, no behavior change)

- **Scope.** Templatize `Discount`/`ForwardRate`/`ResolveRate`/`PriceFixedFloat`/
  `PriceBasis` and the Deposit/FRA/Future branches of `Price` to `T_`;
  `Price(trade, market, result)` collapses into a thin wrapper over
  `Price<double>`; add explicit double/`AAD::Number_` instantiations. Fixing reads
  and exception paths (the THROW inside `ResolveRate` plus the
  `missingHistoricalFixings_` accounting) keep identical logic inside the template.
  Switch the Deposit AAD stage to the templated kernel and delete the hand-written
  formula in the lambda — from then on "active PV == passive PV" is guaranteed by
  the same code, not maintained by comparison.
- **Gate.** All existing tests (core/public/Python) stay green without assertion
  changes; no public semantic change to any header or symbol.
- **Rollback boundary.** Reverting restores the pre-templatization kernels;
  passive pricing is bit-identical before and after, so the revert is invisible
  to every consumer.

### Stage 1 — generalized multi-component AAD stage + FRA/Future

- **Scope.** Generalize `PrepareNodeSensitivityCurve` into "prepare
  definition + passive parameters separately for **every** component the trade
  depends on": the target component's parameters are registered as independent
  variables via `RegisterCurveParameters`, and the remaining dependent curves are
  built as constant (unregistered) `AAD::Number_` of type `<AAD::Number_>` and fed
  to the templated kernel together — gradients are naturally nonzero only for the
  target component. Where base layering is needed, reuse the existing
  `<AAD::Number_, DiscountCurve_<double>>` mixed instantiation (base stays
  passive). Keep the current stage skeleton: `TapeGuard_` → register parameters →
  `NewRecording` (order as today) → price → seed → `PropagateToStart` → the
  `FinalizeNodeSensitivityCandidate` consistency check. Onboard FRA (forecast +
  discount, including the settleAtStart division path) and Future (forecast
  only); open those two families in the family-eligibility registry.
- **Gate.** The FRA/Future tests of the test strategy below are green; OIS/IRS/
  Basis/XCCY still return `TRADE_FAMILY_NOT_AAD_ENABLED` (registry not yet open).
- **Rollback boundary.** Reverting closes FRA/Future in the registry and removes
  the generalized stage; Deposit behavior (already shipped since stage 0 only
  changed its internals) is untouched.

### Stage 2 — OIS/IRS

- **Scope.** `PriceFixedFloat` was already templated in stage 0; this stage opens
  the registry and focuses the tests: the tape path of the OIS daily-compounding
  loop, fixed/float both legs, and the forecast≠discount two-component setting
  (a gradient per component, compared against central differences).
- **Gate.** The OIS/IRS test families are green.
- **Rollback boundary.** Reverting closes OIS/IRS in the registry only; no
  shared code is removed, so FRA/Future remain eligible.

### Stage 3 — Basis

- **Scope.** Three-component dependency (spread forecast, reference forecast,
  discount); verify that when any one forecast is active the other curves'
  gradients are strictly zero (the structural zeros of constant `Number_`), and
  cover both legs having different fixing identities.
- **Gate.** The Basis test family is green.
- **Rollback boundary.** Registry-only closure of Basis, as in stage 2.

### Stage 4 — XCCY

- **Scope.** Follow the frozen componentKey contract (design, decision point 1):
  locate in-block slots by pointer identity, and walk classification +
  preparation for each of the domestic/foreign blocks and the basis curve;
  assemble `XccyMarketView_<AAD::Number_>` on the `Residuals<T_>` pattern
  (non-target blocks built from constant `Number_`, fxSpot constant); give
  `PriceXccy` the `result` accounting parameter so it matches the other families
  (a missing fixing goes through `TRADE_VALIDATION_FAILED` — no new token).
  `BuildRateCashflowPlan` emits the XCCY dependency keys per review follow-up 1
  (market-aware overload or equivalent; "depends on" = actually consumed curves).
  Test coverage: multi-currency pairs, collateral currencies, wrong/unregistered
  keys, in-block unclassifiable representations, before/after maturity, missing
  fixings in both currencies, and the unused-block-member negative case.
- **Gate.** The XCCY test family is green, including all negative cases; the
  planner emits non-empty dependency keys for XCCY.
- **Rollback boundary.** Reverting closes XCCY in the registry and removes its
  planner key emission; stages 0–3 families are unaffected.

### Stage 5 — batch/aggregation + bindings + performance

- **Scope.** `RateTradeNodeSensitivitiesBatch` and the aggregation (a `Report_`
  export helper reusing the existing storable containers — no new storage
  format); the `componentKeys` semantics and the currency grouping rule are
  pinned per review follow-ups 3 and 2 before the issue is created. dal-public
  passthrough; Python batch binding (keyword-only, read-only, one GIL release
  for the whole batch, mirroring `_CurveCalibrationGilBarrier` for the
  release-proof test); the Excel spill surface + Machinist regeneration; a new
  `rate_risk_perf` benchmark (naming follows the existing `*_perf` convention)
  wired into the regression-gate script subset; docs/CHANGELOG close-out.
- **Gate.** The full acceptance criteria below.
- **Rollback boundary.** Reverting removes the new APIs and surfaces; the
  single-trade success domain from stages 0–4 is unaffected.

## Per-Family Testing Strategy

Core tests carry the load; public/Python/Excel contract tests close it out.

- **AAD vs central differences.** Generalize
  `AssertRawNodeGradientMatchesCentralBumps` from Deposit to per-family trade
  builders; for each family × each of the four available curve representations
  (PWC/PWLF/LogDF/ZeroRate), compare every native parameter column (tolerances
  keep the existing mixed absolute + relative form).
- **Active == passive PV.** For each family and component, assert `aad.pv_`
  equals `PriceRateTrade().pv_` (the existing Deposit case uses 1e-12); when the
  borrow/pay direction flips, PV and gradient flip sign together (reusing the
  borrow-antisymmetry case pattern).
- **Three fixing states.** Future fixing (projection path, nonzero gradient);
  past and supplied (that period's gradient is structurally zero, asserted
  explicitly); past and missing (passive pricing fails →
  `TRADE_VALIDATION_FAILED`, with the `missingHistoricalFixings_` accounting
  inspectable).
- **Multi-component.** Call separately for each dependent component; non-target
  component columns are all zero; forecast≠discount, the Basis three-component
  case, and XCCY dual-block + basis each covered.
- **XCCY specifics.** Multi-currency pairs, collateral currencies, wrong keys,
  unregistered keys, in-block unclassifiable representations, before/after
  maturity, missing fixings in both currencies, and the unused-block-member
  negative case from review follow-up 1.
- **Batch partial failure.** Mixed lists of eligible and per-token-failing
  trades; per-entry isolation with successful entries numerically identical to
  single-trade calls; empty lists; duplicate keys.
- **Token priority matrix extension.** Each newly opened family adds a priority
  case such as "wrong component key precedes missing component"; the existing
  Deposit priority cases are kept verbatim as compatibility guards.
- **Surface contracts.** dal-public passthrough is a compile-time contract;
  Python keyword-only + read-only + signature snapshots (reusing the
  `__doc__` first-line assertions) + GIL-release proof; the Excel spill column
  contract (columns = nodes, order = layout order).
- **Registry.** The consistency assertion between the family-enabled set and
  `RateInstrumentTypeListAll()` is updated as stages land.

## CI Verification Points

Gates belong to GitHub CI; this section only marks where they attach.

- The PR-tier full compiler × AAD-backend matrix must include all new tests
  (backend differences are the biggest numerical risk of this feature — risk 1 in
  the design); the push-tier reduced matrix stays as is.
- Coverage of the new code paths joins the centralized coverage job.
- `rate_risk_perf` joins the `.github/scripts/check_benchmark_regressions.py`
  gate subset (long-batch tape memory/latency and the OIS daily-compounding
  shape included).
- `docs/public-api.md` changes pass the `check_docs.py` lint.
- If parallel batching lands as a later increment: the TSan matrix entry.

## Acceptance Criteria

- All seven families × (each applicable curve representation) have passing
  AAD-vs-central-difference cases; each family has a passing active == passive
  case.
- Gradients for any non-target dependent component are strictly zero;
  terms-mismatch still returns `TRADE_FAMILY_NOT_AAD_ENABLED`; the six-token
  closed set and its priority are unchanged, and the existing Deposit assertions
  are unmodified.
- XCCY: in-block/basis addressing, wrong keys, multi-currency, and the three
  fixing states pass; `BuildRateCashflowPlan` emits non-empty dependency keys
  for XCCY.
- Batch: partial-failure isolation + numerical identity with single-trade calls
  + deterministic order; aggregation axis labels correspond one-to-one with
  `DescribeCurveFreeParameters`, and the parameter count matches
  `BuildCurveParameterLayout`.
- The Python/Excel surface contract tests pass; `rate_risk_perf` is wired into
  the gate and within its baseline.
- Before and after stage 0 merges, `dal_cpp_tests`/`dal_public_tests`/pytest are
  all green with no assertion modifications.
