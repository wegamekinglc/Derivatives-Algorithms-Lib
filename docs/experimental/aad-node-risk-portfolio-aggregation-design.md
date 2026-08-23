# Full Product-Family AAD Node Risk and Portfolio Aggregation — Design

> Status: revised design, not yet implemented. Identifiers match the local tree.
> P0 contracts that the implementation review found still open are frozen below.
> Staged execution, tests, and acceptance criteria live in the companion
> [implementation plan](aad-node-risk-portfolio-aggregation-plan.md).

## Goals and Scope

- **Goal.** Widen the success domain of `RateTradeNodeSensitivities(trade, market, componentKey)`
  from Deposit to all seven rate families (Deposit, FRA, Future, OIS, IRS, Basis,
  XCCY); add batch node-sensitivities and portfolio-aggregation APIs; and carry the
  capability through the dal-public, Python, and Excel surfaces.
- **Widening the success domain is the feature itself.** Trades that today return
  `TRADE_FAMILY_NOT_AAD_ENABLED` because they are not Deposits become eligible. The
  token stays in the closed set — its meaning narrows to "the family does not have
  AAD enabled, or the terms do not match the family" — so it remains available for
  future families shipped without AAD.
- **Compatibility promises.** The single-trade API signature, the closed set of six
  failure tokens and their priority order, the canonical failure shape, and the
  passive batch `PriceRateTrades` behavior are all unchanged. Existing Deposit tests
  — including the case where mismatched terms return `TRADE_FAMILY_NOT_AAD_ENABLED` —
  stay green without touching their assertions. The family gate generalizes to a
  registry lookup of whether (family, terms) has AAD enabled, so the existing
  terms-mismatch cases continue to hit the family gate by construction.
- **Non-goals.** No new product families (cap/floor/swaption are P2); no vega or
  volatility axis; no parallel batch execution in P0 (serial, deterministic order —
  see risk 3); no AAD axis on the FX spot (the hook already exists:
  `XccyMarketView_::fxSpot_` is already `T_`, a later small step can activate it);
  no new JSON/serialization contracts (P1); the `Report_` storage format and the
  `RateInstrumentType_` closed set are untouched; no ABI-isolation layer rework.

## Frozen P0 contracts

These are closed for the first implementation series. Reopening any of them
requires a new design revision, not a silent choice inside a stage-5 issue.

1. **Non-target curves are truly passive `double` curves.** The active component
   is built as `AAD::Number_` and registered through `RegisterCurveParameters`.
   Every other curve the kernel reads is a `DiscountCurve_<double>` (or the
   existing mixed-base `<AAD::Number_, DiscountCurve_<double>>` handle when the
   active curve has a double base). Unregistered constant `AAD::Number_` curves
   are not an acceptable "passive" stand-in: they still record OIS daily
   compounding on the tape. Isolation tests must show the non-target gradient is
   identically zero and that tape size does not scale with passive-node count.
2. **Batch `componentKeys` is one shared list.**
   `RateTradeNodeSensitivitiesBatch(trades, market, componentKeys)` applies the
   same key list to every trade (Cartesian product, deterministic trade-major
   then key order). A trade that does not depend on a listed key returns the
   existing `TRADE_DOES_NOT_DEPEND_ON_COMPONENT` cell. Per-trade key lists are
   out of scope for P0.
3. **Aggregation is unconverted, grouped by actual PV currency.** The grouping
   key is each family's actual PV denomination: non-XCCY in the trade currency,
   XCCY in the domestic currency produced by covered-interest parity inside
   `PriceXccyContract`. `RatePricingTradeResult_.currency_` is
   `market.resultCurrency_` and must never be used as a grouping key. The
   aggregate result carries an explicit policy label `UnconvertedByActualPvCcy`.
   FX conversion is a later increment and needs an explicit FX-input contract.
4. **`Report_` is the numeric tensor, not the whole result.** Successful node
   values live in a `Report_` whose axes come from
   `DescribeCurveFreeParameters` / `BuildCurveParameterLayout`. Failures,
   currencies, and the aggregation policy live in a parallel meta table.
5. **Excel emits a long-form spill.** Columns are
   `trade, component, reason, pv, node, value` (plus currency on aggregate
   rows). Mixed components have different node counts and dates, so a single
   "columns = nodes" grid is not a P0 contract. The surface is
   `dal-excel/src/__curvepricing.cpp` plus Machinist-generated stubs.
6. **P0 is serial on one tape.** `RateTradeNodeSensitivitiesBatch` must not
   dispatch onto the existing thread pool. The tape is `thread_local`.
7. **XCCY "depends on" means curves actually consumed.** A market-aware
   `BuildRateCashflowPlan` overload (existing `(trade, valuationTime)` signature
   unchanged) emits keys for the collateral/tenor-selected curves, not for every
   in-block member. Pointer identity is valid only when the XCCY block and
   `market.curveComponents_` share the same `Handle_`. An unused in-block member
   is a negative case (must not be eligible-with-all-zeros). An in-block curve
   that cannot be classified is not `CURVE_REPRESENTATION_NOT_AAD_ENABLED`; that
   token remains a representation failure.

## Current-Code Facts

These facts, verified against `origin/master@ec89eb72`, determine the shape of the
design:

- The single-trade entry `RateTradeNodeSensitivities(trade, market, componentKey)`
  in `dal-cpp/dal/curve/ratecashflowpricing.cpp` hardcodes Deposit. The six-gate
  pipeline, the closed set of six failure tokens, and their priority order are
  fixed in `dal-cpp/dal/curve/ratecashflowpricing_internal.hpp` and documented in
  `docs/public-api.md`. The failure shape is `eligible_=false / pv_=0 / empty
  gradient_ / non-empty reason_`.
- The Deposit AAD pricing formula is **hand-copied** inside the stage lambda and
  exists in parallel with the passive `Price` Deposit branch. This is the direct
  motivation for "kernel templatization first": without removing the fork, every new
  family would hand-copy its formula again and maintain numerical consistency
  independently.
- The curve layer is already fully templated:
  `BuildCurveParameterLayout` / `DescribeCurveFreeParameters` (a date + component
  descriptor per parameter) / `RegisterCurveParameters` /
  `BuildDiscountCurveUnique<T_,B_>`, including an existing mixed-base
  instantiation `<AAD::Number_, DiscountCurve_<double>>` in
  `dal-cpp/dal/curve/curveparameterization.cpp`. The XCCY kernel
  (`PriceXccyContract<T_>`, `XccyMarketView_<T_>`) is already instantiated for
  both double and AAD. The ready-made template for assembling multi-curve active
  blocks is `BuildTypedCurveBlock<T_>` (with base layering,
  `dal-cpp/dal/curve/jointcalibration_internal.hpp`), and the complete stage
  pattern — `Residuals<T_>` + `TapeGuard_` + `RegisterCurveParameters` +
  `NewRecording` + `HarvestCurveJacobian` — is in
  `dal-cpp/dal/curve/xccyjointcalibration.cpp`.
- Not yet templated: the linear-family pricing kernels themselves —
  `PriceFixedFloat`, `PriceBasis`, the Deposit/FRA/Future branches of `Price`, and
  the `Discount`/`ForwardRate`/`ResolveRate` helpers (all double-only).
- The AAD tape is `thread_local` on all four backends
  (`dal-cpp/dal/math/aad/expr.hpp`); `TapeGuard_` rewinds on scope exit (block
  reuse rather than reallocation); per-backend adjoint-cleanup differences already
  have the `PrepareAdjoints`/`ClearIndependentAdjoints` paradigm in
  `dal-cpp/dal/curve/aadjacobian.cpp`.
- Python bindings are already keyword-only with GIL release
  (`dal-python/src/bindings/curve.cpp`); the calibration side has the
  `_CurveCalibrationGilBarrier_EnableForTesting` artificial-barrier test paradigm
  that can be mirrored. Excel currently has **no** rate-pricing surface at
  all (`dal-excel/src/` has no corresponding file), so it is a net-new binding.
- `BuildRateCashflowPlan` currently leaves `dependencyComponentKeys_` empty for
  XCCY, and `RatePricingTradeResult_.currency_` copies
  `RatePricingMarket_::resultCurrency_` without conversion. Non-XCCY PV is
  denominated in the trade currency; XCCY PV is already in the domestic
  currency via covered-interest parity inside `PriceXccyContract`. Aggregation
  must use that actual PV currency, not the passthrough field.
- Existing test assets generalize directly: `AssertRawNodeGradientMatchesCentralBumps`
  in `dal-cpp/tests/curve/test_ratecashflowpricing.cpp` (central-difference
  comparison across the four curve representations, borrow/lend sign antisymmetry,
  structurally zero columns), the token-priority matrix, the finalizer/tape-rewind
  unit tests, and the Python contract tests locking keyword-only and read-only
  results.

## Incremental Public API Design

- **Single trade (unchanged, success domain only widened).**
  `RateTradeNodeSensitivities(trade, market, componentKey)`; each call produces a
  gradient for one component, and multiple components are handled by repeated calls
  or composed through the batch entry point.
- **Batch (new, C++ core layer).**
  `RateTradeNodeSensitivitiesBatch(trades, market, componentKeys)` returns, per
  (trade, component), exactly the same `RateTradeNodeSensitivityResult_` shape as
  the single-trade call; failures are isolated per entry (a failed entry carries
  its token, nothing is thrown, other entries are unaffected), aligning with the
  per-trade isolation semantics of `PriceRateTrades`. P0 executes serially with a
  deterministic result order.
- **Portfolio aggregation (new).** Sum the successful entries along the
  (component, currency, node/parameter) axes and return the aggregate plus axis
  labels. The axis labels (parameter dates, component types) **must** be derived
  from `DescribeCurveFreeParameters`, and the parameter count and order **must**
  take `BuildCurveParameterLayout().parameterCount_` as the single source of
  truth — no hand-written parameter ordering or counting is acceptable (the
  existing `FinalizeNodeSensitivityCandidate` already consistency-checks against
  that count; keep doing so). The gradient keeps its meaning of "native curve
  parameter perturbation" (the same convention as the existing Deposit path and
  the central-difference tests); quote-space conversion is deliberately not done —
  that is the job of the calibration Jacobian, an existing capability.
- **Multi-currency treatment.** Frozen P0 contract 3: group without converting,
  using each family's actual PV currency and the `UnconvertedByActualPvCcy`
  label. Do not group on `RatePricingTradeResult_.currency_`. Optional
  conversion at the valuation time is a later increment (it depends on an
  FX-input contract, which this feature deliberately does not introduce
  implicitly).
- **Python.** The new batch/aggregation functions stay keyword-only with
  read-only results and release the GIL for the whole native execution (one
  release per batch, marshalling afterwards); existing bindings are untouched.
- **Excel.** Frozen P0 contract 5: `dal-excel/src/__curvepricing.cpp` plus
  Machinist-generated stubs; long-form spill
  (`trade, component, reason, pv, node, value`). No internal object structure
  leaks.
- **Documentation.** The sentence "Native node AAD currently admits deposit trades
  only" in `docs/public-api.md` is updated in the same PR as each family lands (the
  docs must never disagree with the success domain), and the CHANGELOG records the
  behavior change.

### Frozen decision points

The L-level review verified all three decision points against the code; none
carries production-level risk:

1. **XCCY addressing by pointer identity.** The curves an XCCY trade depends on
   live in the domestic/foreign `CurveBlock_` of `CrossCurrencyMarket_` (keyed by
   `CollateralType_`/`PeriodLength_`) and in the basis curve — a different
   namespace from the `String_` keys of `curveComponents_`. The contract: require
   the addressed curve object to be *also* registered under a stable key in
   `market.curveComponents_`, and locate its slot in the XCCY market block by
   **pointer identity**. The review confirmed this is well-defined and safe: the
   `CurveBlock_` discount/forward maps and `market.curveComponents_` both hold
   `Handle_<DiscountCurve_>` (shared ownership, in
   `dal-cpp/dal/curve/curveblock.hpp` and
   `dal-cpp/dal/curve/ratecashflowpricing.hpp`), and `BasisCurve()` also takes
   the raw pointer from a Handle, so cross-namespace pointer comparison is
   well-defined with no false positives — strictly better than matching by Name.
   In the same step, the XCCY branch of `BuildRateCashflowPlan` is extended to
   emit these dependency keys (currently empty), which makes the
   `TRADE_DOES_NOT_DEPEND_ON_COMPONENT` gate effective for XCCY. A curve that
   cannot be classified inside a block is not
   `CURVE_REPRESENTATION_NOT_AAD_ENABLED`; that token remains a representation
   failure (frozen P0 contract 7).
2. **Batch result shape: reuse the single-trade structure**, with one shared
   `componentKeys` list (frozen P0 contract 2).
3. **Aggregation currency treatment: group without converting**, using actual PV
   currency and the `UnconvertedByActualPvCcy` label (frozen P0 contract 3).
   `RatePricingTradeResult_.currency_` must never be the grouping key.

## Key Risks and Countermeasures

1. **Tape/adjoint lifetime differences across the four backends** (native vs
   xad/adept/codipack differ in registration order and adjoint-cleanup semantics):
   reuse the existing `RegisterCurveParameters` → `NewRecording` order and the
   per-backend cleanup paradigm inside `HarvestCurveJacobian`; invent nothing new.
   Between sweeps, depend on `TapeGuard_` rewinds rather than manual resets
   (the rewind pattern is documented in `dal-cpp/dal/curve/tapeguard.hpp`).
   Verification belongs to the CI full compiler × AAD-backend matrix (full matrix
   on PRs).
2. **Tape memory on long batches.** Every (trade, component) sweep rewinds on
   scope exit (block reuse; the `tapeguard.hpp` comment documents the pattern);
   the `rate_risk_perf` benchmark joins the regression-script subset to guard
   against memory and latency drift.
3. **GIL/thread boundary.** One `gil_scoped_release` for the whole native batch,
   with no Python callbacks inside; results are marshalled under the GIL; the
   calibration-side barrier-test paradigm proves the release actually happens.
   The tape is `thread_local`, so any future parallelism designed as "one task
   owns a complete sweep" is naturally safe. P0 stays serial; parallelism is an
   explicit later increment gated by TSan (`DAL_ENABLE_SANITIZERS=thread`) and the
   `dal-cpp/tests/math/aad/test_concurrent_aad.cpp` pattern.
4. **XCCY addressing and the empty-dependency-key status quo.** The most
   error-prone link (the prior weekly report flagged it too). The countermeasure
   is the frozen pointer-identity contract (decision point 1) plus planner key
   emission plus negative cases for wrong keys, unregistered keys, and
   unclassifiable in-block curves — all landing on existing tokens.
5. **Tape size of OIS daily compounding.** Long-period OIS daily observation
   counts amplify the number of tape nodes; the benchmark covers that shape, and
   if it exceeds thresholds, interval collapsing is evaluated later (only under a
   numerical-equivalence proof) — not pre-built in P0.
6. **Passive-path regressions from templatization.** Stage 0 is a pure refactor
   and the "active == passive PV" invariant runs through all later tests, so any
   templatization mistake surfaces on that invariant.
7. **Downstream visibility of the widened success domain.** External consumers
   that use `TRADE_FAMILY_NOT_AAD_ENABLED` to detect non-Deposit trades will see
   the behavior flip; each stage calls it out in the CHANGELOG, and
   `docs/public-api.md` is updated in the same PR.
