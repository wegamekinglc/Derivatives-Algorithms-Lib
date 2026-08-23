# Full Product-Family AAD Node Risk and Portfolio Aggregation — Design

> Status: reviewed design, not yet implemented. The plan passed an L-level review
> (AAD/numerics and public-contract focus) with no blockers against baseline
> `origin/master@ec89eb72`. The staged execution steps, test strategy, and
> acceptance criteria live in the companion
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
  XCCY, and `RatePricingMarket_::resultCurrency_` is passed through without
  conversion (each family's PV is denominated in the trade's own currency).
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
- **Multi-currency treatment.** Aggregation groups by trade currency; the
  pass-through-no-conversion status of `resultCurrency_` is preserved, and
  cross-currency totals are explicitly labeled "unconverted, grouped by currency".
  Optional conversion at the valuation time is a later increment (it depends on an
  FX-input contract, which this feature deliberately does not introduce
  implicitly).
- **Python.** The new batch/aggregation functions stay keyword-only with
  read-only results and release the GIL for the whole native execution (one
  release per batch, marshalling afterwards); existing bindings are untouched.
- **Excel.** A new `dal-excel/src/__curvepricing.cpp` surface plus the Machinist
  `public` marker (regenerate the stubs and commit them), results emitted as a
  spill table (rows = trade/component, columns = nodes), naming following the
  existing `public`-function conventions; no internal object structure leaks.
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
   cannot be classified inside a block falls through to
   `CURVE_REPRESENTATION_NOT_AAD_ENABLED`.
2. **Batch result shape: reuse the single-trade structure.** Consistent with the
   precedent of `PriceRateTrades` reusing `RatePricingTradeResult_`; the new
   function is a purely incremental surface with no compatibility risk, and per
   (trade, component) entries make failure isolation align naturally with
   single-trade semantics.
3. **Aggregation currency treatment: group without converting.** The same
   componentKey within a single market always points to the same curve object
   (map semantics), the parameter axis is aligned through
   `BuildCurveParameterLayout`, and gradient additivity holds within a currency
   group. One caveat from the review — the grouping key must derive from each
   family's *actual* PV pricing currency — is pinned in the implementation plan
   (review follow-up 2).

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
