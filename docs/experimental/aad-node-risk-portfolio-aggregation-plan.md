# Full Product-Family AAD Node Risk and Portfolio Aggregation — Implementation Plan

> Status: revised implementation plan, not yet started. P0 contracts from the
> implementation review are frozen below and in the companion
> [design](aad-node-risk-portfolio-aggregation-design.md).

## Frozen P0 follow-ups

These are no longer open questions. They match the Frozen P0 contracts in the
design; implementation issues must not reopen them.

1. **XCCY dependency keys (stage 4).** Add a market-aware
   `BuildRateCashflowPlan` overload (existing `(trade, valuationTime)` signature
   unchanged). "Depends on" means curves the config actually consumes (collateral
   / tenor), not block membership. Unused in-block members are a negative case.
   Unclassifiable in-block curves are not `CURVE_REPRESENTATION_NOT_AAD_ENABLED`.
2. **Aggregation grouping (stage 5).** Group by each family's actual PV currency
   (XCCY domestic via CIP). Never group on `RatePricingTradeResult_.currency_`.
   Label the policy `UnconvertedByActualPvCcy`. Do not convert FX in P0.
3. **Batch `componentKeys` (stage 5).** One shared key list applied to every
   trade (Cartesian product, trade-major then key order). Missing dependency
   returns `TRADE_DOES_NOT_DEPEND_ON_COMPONENT`.

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
  variables via `RegisterCurveParameters`. Every other curve the kernel reads is
  a `DiscountCurve_<double>` (or the existing mixed-base
  `<AAD::Number_, DiscountCurve_<double>>` handle when the active curve has a
  double base). Unregistered constant `AAD::Number_` curves are not an acceptable
  passive stand-in. Isolation tests must show non-target gradients identically
  zero and tape size independent of passive-node count. Keep the current stage
  skeleton: `TapeGuard_` → register parameters →
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
  gradients are strictly zero (passive curves are `DiscountCurve_<double>`, not
  unregistered `AAD::Number_`), and cover both legs having different fixing
  identities.
- **Gate.** The Basis test family is green.
- **Rollback boundary.** Registry-only closure of Basis, as in stage 2.

### Stage 4 — XCCY

- **Scope.** Follow the frozen componentKey contract (design, decision point 1):
  locate in-block slots by pointer identity, and walk classification +
  preparation for each of the domestic/foreign blocks and the basis curve;
  assemble `XccyMarketView_<AAD::Number_>` on the `Residuals<T_>` pattern
  (non-target blocks stay `double` / mixed-base; fxSpot remains a constant
  `double` in P0); give `PriceXccy` the `result` accounting parameter so it
  matches the other families (a missing fixing goes through
  `TRADE_VALIDATION_FAILED` — no new token). `BuildRateCashflowPlan` emits the
  XCCY dependency keys per frozen P0 follow-up 1 (market-aware overload;
  "depends on" = actually consumed curves). Test coverage: multi-currency pairs,
  collateral currencies, wrong/unregistered keys, in-block unclassifiable
  representations, before/after maturity, missing fixings in both currencies,
  and the unused-block-member negative case.
- **Gate.** The XCCY test family is green, including all negative cases; the
  planner emits non-empty dependency keys for XCCY.
- **Rollback boundary.** Reverting closes XCCY in the registry and removes its
  planner key emission; stages 0–3 families are unaffected.

### Stage 5 — batch/aggregation + bindings + performance

- **Scope.** `RateTradeNodeSensitivitiesBatch` and aggregation as a `Report_`
  numeric tensor plus a parallel meta table (failures, currencies,
  `UnconvertedByActualPvCcy`). `componentKeys` is one shared list (frozen P0
  contract 2). P0 is serial: do not dispatch onto the thread pool. dal-public
  passthrough; Python batch binding (keyword-only, read-only, one GIL release
  for the whole batch, mirroring `_CurveCalibrationGilBarrier_EnableForTesting`);
  Excel long-form spill + Machinist regeneration; a new `rate_risk_perf`
  benchmark wired into the regression-gate script subset; docs/CHANGELOG
  close-out.
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
  negative case from frozen P0 follow-up 1.
- **Base curve.** For each family that prices against a layered curve, the
  active overlay yields a nonzero gradient while the double base stays
  identically zero (reuse the mixed-base
  `<AAD::Number_, DiscountCurve_<double>>` instantiation).
- **Expiry.** Every family, not only XCCY, has a before/after-maturity case:
  expired cashflows contribute zero PV and a structurally zero gradient; live
  cashflows remain comparable to central differences.
- **Batch partial failure.** Mixed lists of eligible and per-token-failing
  trades; per-entry isolation with successful entries numerically identical to
  single-trade calls; empty lists; duplicate keys.
- **Token priority matrix extension.** Each newly opened family adds a priority
  case such as "wrong component key precedes missing component"; the existing
  Deposit priority cases are kept verbatim as compatibility guards.
- **Surface contracts.** dal-public passthrough is a compile-time contract;
  Python keyword-only + read-only + signature snapshots (reusing the
  `__doc__` first-line assertions) + GIL-release proof; the Excel long-form
  spill contract (`trade, component, reason, pv, node, value`, plus currency
  on aggregate rows). Node order within a component still follows
  `BuildCurveParameterLayout`.
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
- XCCY: in-block/basis addressing, wrong keys, multi-currency, unused in-block
  members, and the three fixing states pass; `BuildRateCashflowPlan` emits
  non-empty dependency keys for XCCY.
- Every family has a before/after-maturity case and, where a layered curve
  applies, a base-curve isolation case.
- Batch: partial-failure isolation + numerical identity with single-trade calls
  + deterministic serial order; aggregation groups by actual PV currency under
  `UnconvertedByActualPvCcy`; axis labels correspond one-to-one with
  `DescribeCurveFreeParameters`, and the parameter count matches
  `BuildCurveParameterLayout`. Failures and currency/policy live in the
  parallel meta table, not inside `Report_`.
- The Python/Excel surface contract tests pass; `rate_risk_perf` is wired into
  the gate and within its baseline.
- Before and after stage 0 merges, `dal_cpp_tests`/`dal_public_tests`/pytest are
  all green with no assertion modifications.
