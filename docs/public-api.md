# DAL C++ Public API Guide

DAL exposes related workflows through C++, Python, and Excel. This guide
focuses on the C++ public facade and points to the dedicated [Python](python/README.md)
and [Excel](excel/README.md) chapters. It is not an exhaustive reference for
every core numerical type.

## API Layers

| Layer         | Intended use                                                                             | Compatibility contract                                                    |
|---------------|------------------------------------------------------------------------------------------|---------------------------------------------------------------------------|
| `DAL::cpp`    | Direct access to quantitative algorithms and core types                                  | Source-level core API; advanced consumers track core changes              |
| `DAL::public` | Construction, calibration, scripted valuation, random generation, and repository helpers | Convenience facade; exposes core types and does not promise ABI isolation |
| Python `dal`  | Python-friendly wrappers over the public facade                                          | Supported names are those exported by `_dal` and `dal/api.py`             |
| Excel XLL     | Worksheet functions and repository handles                                               | Supported worksheet names come from generated registrations               |

Installed C++ consumers should link imported targets instead of copying library
paths. See the [installation guide](installation.md#installed-cmake-packages).

## C++

### CMake consumption

```cmake
find_package(dal-cpp 1.0 CONFIG REQUIRED)
find_package(dal-public 1.0 CONFIG REQUIRED)

add_executable(my_pricer main.cpp)
dal_cpp_apply_msvc_runtime(my_pricer)
target_link_libraries(my_pricer PRIVATE DAL::public)
```

`DAL::public` links `DAL::cpp` transitively. Link `DAL::cpp` directly when using
only core algorithms. The core package exports
`DAL_CPP_MSVC_RUNTIME_LIBRARY`; `dal_cpp_apply_msvc_runtime` applies that
configuration-aware ABI choice to a consumer target under MSVC and is a no-op
on other toolchains.

### Public facade headers

| Header                                 | Main entry points                                                                                                     |
|----------------------------------------|-----------------------------------------------------------------------------------------------------------------------|
| `<dal-public/src/global.hpp>`          | `InitGlobalData`, `SetEvaluationDate`, `GetEvaluationDate`                                                            |
| `<dal-public/src/script.hpp>`          | `NewScriptProduct`, `DescribeScriptProduct`, `DebugScriptProduct`, `DebugScriptProductJson`, `DebugScriptProductTree` |
| `<dal-public/src/models.hpp>`          | `NewBSModelData`, `NewDupireModelData`                                                                                |
| `<dal-public/src/value.hpp>`           | `ValueByMonteCarlo`, `ExplainScriptValuation`, `ExplainScriptSimulation`                                              |
| `<dal-public/src/random.hpp>`          | Pseudo/Sobol constructors and uniform/normal matrix fills                                                             |
| `<dal-public/src/curveprotocol.hpp>`   | Day-basis, tenor, collateral, rate-leg/index, currency-pair, FX-reset, and fixing-snapshot builders                   |
| `<dal-public/src/curveinstrument.hpp>` | Deposit, FRA, future, swap, OIS, basis-swap, and fixed/resettable/MTM cross-currency-swap builders                    |
| `<dal-public/src/curvedata.hpp>`       | Piecewise-linear-forward, zero-rate, and curve-block builders                                                         |
| `<dal-public/src/curvespec.hpp>`       | `CurveCalibrationSpecBuilder_`, `CalibrateSingleCurve`, `CalibrateMultiCurveBundle`                                   |
| `<dal-public/src/xccycalibration.hpp>` | Staged and joint XCCY spec builders, calibration, and joint-result accessors                                          |
| `<dal-public/src/curvepricing.hpp>`    | Typed rate-cashflow planning, batch pricing, node sensitivity, and family registry                                    |
| `<dal-public/src/interp.hpp>`          | Linear one-dimensional interpolation builder                                                                          |
| `<dal-public/src/repository.hpp>`      | Repository find, erase, and size helpers for a configured host environment                                            |

The installed include path intentionally retains `dal-public/src/`. The facade
also uses core `Handle_`, `Date_`, curve, model, and diagnostics types directly.

### Sobol normal-draw policy

The public constructor is:

```cpp
Dal::NewSobolRSG(name, iPath, ndim = 1, precise = false, polish = false)
```

The two policy flags are independent and are forwarded unchanged. `polish`
controls whether the Acklam inverse-CDF result receives a Newton correction;
when polishing is enabled, `precise` selects the precise CDF instead of the fast
CDF for that correction. The default `false, false` path is Acklam-only, and the
precise-CDF correction requires `precise = true, polish = true`. Python
`dal.SobolRSG_New` and Excel `SOBOLRSG.NEW` use the same defaults and semantics.
See the [random methodology policy table](monte-carlo/sampling.md#normal-draw-inverse-cdf-modes)
for all four combinations.

### Scripted Monte Carlo

This minimal pattern is exercised by the public API tests:

```cpp
#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

Dal::InitGlobalData();
Dal::SetEvaluationDate(Dal::Date_(2022, 9, 25));

const Dal::Vector_<Dal::Cell_> dates = {
    Dal::Cell_("STRIKE"),
    Dal::Cell_(Dal::Date_(2023, 9, 25)),
};
const Dal::Vector_<Dal::String_> events = {
    Dal::String_("100.0"),
    Dal::String_("call pays MAX(spot() - STRIKE, 0.0)"),
};

const auto product = Dal::NewScriptProduct(Dal::String_("call"), dates, events);
const auto model = Dal::NewBSModelData(Dal::String_("bs"), 100.0, 0.2, 0.05, 0.02);
const auto result = Dal::ValueByMonteCarlo(product, model, 1 << 16);
```

`ValueByMonteCarlo` requires non-null product/model handles, a BS or Dupire
model, and a positive `int` path count. The original three-to-eight-argument
overload retains random-generator, Brownian-bridge, AAD, smoothing, and compiled
arguments and defaults. It forwards to the same preparation used by
`ValueByMonteCarlo(product, modelData, numPath, valuation, simulation)`.
The typed fourth argument is required; simulation settings are optional.
Write an explicit `ScriptValuationSettings_`, since a bare fourth-argument
`{}` is ambiguous. Results contain only `PV` and optional `d_` parameter risks.

`NewScriptProduct(name, dates, events, contract)` accepts
`ScriptProductSettings_`; the three-argument form uses an empty default index.
`contract.defaultIndex_` gives legacy `SPOT()` an identity. The example above
uses `SPOT()`, the retained zero-argument compatibility form for model spot at
the event date; unbound future-only scripts need no change. The named form for
new scripts is unquoted `FIX(EQ[AAPL])` or `FIX(EQ[AAPL], 2026-09-11)`.
Historical EQ/FX
requests resolve at exact midnight, and a missing required fixing is an error,
never a model or placeholder fallback. Model-sourced requests bind the model's
`spot` output to one ordinary EQ, taken from the script's own future FIX index
by name; no settings are involved. A product
default does not supply that model index or model market data. The
per-form, per-date rules are in the
[SPOT/FIX boundary](methodology/script_engine.md#spot-compatibility-and-the-fix-boundary).

`valuation.evaluationDate_` is optional: when supplied it causes zero global
evaluation-date reads/writes; otherwise the entry captures the global date
once. `valuation.todayFixingPolicy_` defaults to
`TodayFixingPolicy_::Value_::MODEL`; `REQUIREHISTORICAL` requires today's
history instead. Earlier fixings always require history, later fixings use the
model, and a fixing after its event date fails. A null `valuation.fixings_`
captures required global history for this call. A non-null snapshot, including
an empty one, is authoritative and never falls back to global history.

`MonteCarloSettings_` defaults to Sobol, no bridge/AAD, `smooth_=0.01`, and
`compiled_=std::nullopt` (false). Smoothing must be finite and strictly positive
in all modes. Tree/compiled selection preserves observation policies; AAD
rebuilds historical parameter state and uses fuzzy future comparisons. Full
field, validation, and numerical contracts are in
[public C++ settings](methodology/script_engine.md#public-c-settings).

Product construction copies the original table/settings without market access;
valuation copies settings and prepares afresh. Snapshot handles share immutable
data. Callers must prevent concurrent input mutation and fixing writes during
global capture, which is sequential rather than an atomic cross-series snapshot.
Native Value and Explain retain the valuation/mutation barrier, so public calls
serialize even with explicit dates. Global date setters wait for them; getters
remain available. The executable
[settings example](../dal-public/examples/script_settings.cpp) checks historical
compiled-AAD `PV=160` and `d_SCALE=80` with an explicit date/snapshot and zero rates.

`DescribeScriptProduct(product)` returns `dal.script-product/2`: original and
canonical identities, all dated syntax, default/explicit fixing dates, and
source locations, with zero history, model, or global-date access. It has no
valuation-dependent phase and does not establish pricing readiness.
`ExplainScriptValuation(product, modelData, valuation=ScriptValuationSettings_())`
returns `dal.script-valuation/1` from default price preparation. It can read
history and initialize a model, but generates no paths and submits no workers.
It reports actual requests/uses, historical values, the model index, event-to-sample
and numeraire mappings. Every Explain and Value prepares independently; use
the same explicit date/snapshot to compare the same market.
`ExplainScriptSimulation(product, modelData, numPath, valuation=ScriptValuationSettings_(),
simulation=MonteCarloSettings_())` returns `dal.script-simulation/1`. Unlike the
valuation Explain it explicitly runs a full Monte Carlo valuation with `numPath`
paths on exercise products, so its cost is path generation plus worker
parallelism plus the exercise regressions; products without `EXERCISE` skip the
run, since the LSMC driver is the only source of exercise statistics. It
supports the double tree-walk and compiled modes and rejects
`enable_aad` settings. Products without `EXERCISE` return an empty
`exercise_events` array; exercise products report per-exercise-event regression
degree, regressor index, in-the-money condition-true path count, coefficients,
degenerate flag with PascalCase reason, and exercise rate.

Product archives write v2 with optional `default_index` and retain the v1
reader. They preserve contract text/identity and exclude runtime market data.
There is no public v1 export or promise that old binaries can read v2; build
consumers and bindings against matching headers/libraries.

The legacy text and width-aware tree dumps remain available.
`DebugScriptProductJson` retains `dal.script-product/1` and its date-based
phases, but rejects FIX or any nonempty default with `DebugSchemaUnsupported`
and a Describe /2 hint. See the
[archive and diagnostic contracts](methodology/script_engine.md#product-archive-and-diagnostics)
and [legacy dumps](methodology/script_engine.md#product-debug-outputs).

### C++ curve calibration

The public zero-rate factory is:

```cpp
Dal::Handle_<Dal::DiscountCurve_> Dal::DiscountZeroRateNew(
    const Dal::String_& name,
    const Dal::String_& ccy,
    const Dal::Date_& anchorDate,
    const Dal::Vector_<Dal::Date_>& nodeDates,
    const Dal::Vector_<>& zeroRates,
    const Dal::DayBasis_& dayCount = Dal::DayBasis_("ACT_365F"),
    Dal::LogDfScheme_ scheme = Dal::LogDfScheme_::Value_::LOG_LINEAR,
    const Dal::Handle_<Dal::DiscountCurve_>& base = {});
```

`nodeDates` are strictly future dates and `zeroRates` are continuously compounded
decimal rates in matching order. The factory maps each node to
`logDF = -zeroRate * YearFrac(anchorDate,nodeDate)`, then applies the selected shared
log-DF interpolation and extrapolation scheme. The optional base is multiplied into the
curve, so the supplied rates describe a spread component. The result retains its
`DiscountZeroRate_` type and zero-rate bump coordinates when archived and restored.

The facade separates construction from solving:

1. Build conventions with `PeriodLength_New`, `DayBasis_New`,
   `RateLegConvention_New`, and `RateIndexConvention_New`.
2. Build quoted instruments with `DepositNew`, `FRANew`, `FutureNew`, `SwapNew`,
   `OISSwapNew`, or `BasisSwapNew`.
3. Fill `CurveCalibrationSpecBuilder_` and call `Build()`.
4. Call `CalibrateSingleCurve`, optionally selecting `CurveJacobianMode_`.
5. Read `CalibrationResult_::curve_` and its diagnostics.

For staged calibration, assemble `MultiCurveCalibrationSpec_` and call
`CalibrateMultiCurveBundle`. Cross-currency calibration has two paths:

- `CrossCurrencyCalibrationSpecBuilder_` / the one-argument public convenience
  facade `CalibrateXccyMarket(spec)` calibrates a basis curve over supplied
  domestic and foreign blocks.
- `JointXccyCalibrationSpecBuilder_` / `CalibrateJointXccyMarket` solves the
  domestic declarations, foreign declarations, and basis declaration together.

`CrossCurrencySwapConfigBuilder_` selects
`XccyNotionalMode_::Value_::FIXED`,
`XccyNotionalMode_::Value_::RESETTABLE`, or
`XccyNotionalMode_::Value_::MARK_TO_MARKET`, explicit domestic/foreign
`FixingIdentity_` values, and an `FxResetConvention_`.
`MarketFixingSnapshotNew` creates an immutable rate-and-FX observation set for
in-progress swaps.

The public XCCY header includes the core staged and joint result types. Staged
C++ callers can use either the backward-compatible
`CalibrateXccyMarket(spec)` entry point or
`CalibrateXccyMarket(spec, options)`. A default
`CrossCurrencyCalibrationOptions_` selects `ANALYTIC` and requests both the
forward Jacobian and effective inverse. Both matrices remain owned by
`CrossCurrencyCalibrationResult_::diagnostics_`;
`XccyResultDiagnostics`, `XccyResultJacobian`, and
`XccyResultEffJacobianInverse` are read-only facade accessors.

Joint results own their two matrices at the top level. The `JointXccyResult*`
facade helpers expose the three solved curve handles, FX forwards,
market/model/residual vectors, both the forward Jacobian and effective inverse,
and named parameter/residual ranges.

For staged XCCY, the forward/inverse shapes are
`nInstruments x nBasisParameters` and
`nBasisParameters x nInstruments`. For joint XCCY they are
`totalResiduals x totalParameters` and
`totalParameters x totalResiduals`. Staged rows follow instrument input order;
`instrumentNames_` contains row labels that may repeat. Staged columns follow
`parameterKnotDates_` in `spec.knotDates_` order, which is the
piecewise-constant basis curve's right-forward parameter order.

Staged diagnostics publish `jacobianScaling_ = "unscaled"`,
`effJacobianInverseScaling_ = "solver_scaled"`, and
`residualTolerance_ = spec.tolerance_`. The availability fields distinguish
`available`, `not_requested`, and `not_available_for_mode`; an unavailable
matrix is empty, so callers should inspect the availability field rather than
infer the reason from its numeric carrier. For the solver-scaled effective
inverse $E$, a raw decimal quote bump maps as
$\Delta x = E\,\Delta q/\mathrm{residualTolerance}$. See
[cross-currency pricing and calibration](ccy-curves/pricing-calibration.md) and
the [Jacobian methodology](yield-curves/jacobian-risk.md#staged-xccy-jacobian-layout).

Set `parameterization_ = CurveParameterization_::Value_::ZERO_RATE` to calibrate future
zero-rate nodes. `initialGuess_` and `initialGuessPerNode_` are decimal continuously
compounded rates for this representation. Single, staged, generic joint, and
joint XCCY calibration support ZERO_RATE.

### C++ rate cashflow pricing

Include `<dal-public/src/curvepricing.hpp>` for the typed pricing surface. It
exposes the core `RateTradeDefinition_`, family-specific terms,
`RatePricingMarket_`, pricing result, and node-sensitivity types together with:

```cpp
Dal::BuildRateCashflowPlan(trade, market.valuationTime_);
Dal::BuildRateCashflowPlan(trade, market);
Dal::PriceRateTrade(trade, market);
Dal::PriceRateTrades(trades, market);
Dal::RateTradeNodeSensitivities(trade, market, componentKey);
Dal::RateTradeNodeSensitivitiesBatch(trades, market, componentKeys);
Dal::AggregateRatePortfolioNodeRisk(trades, market, componentKeys);
Dal::RateNodeSensitivityAxisLabels(market, componentKey);
Dal::CurvePricingFamilyRegistry();
```

The supported family enum is closed to `DEPOSIT`, `FRA`, `FUTURE`, `OIS`,
`IRS`, `BASIS_SWAP`, and `XCCY`. Planning determines curve dependencies and
required historical rate/FX fixing keys before valuation. Batch pricing retains
a success/failure result per trade. Both plan overloads agree for the
single-currency families; the market-aware form additionally emits the XCCY
dependency keys of the curves the trade actually consumes (the
collateral/tenor-selected domestic and foreign discount and forecast curves
plus the basis curve), each addressed by pointer identity against
`RatePricingMarket_::curveComponents_`.

Native node AAD currently admits deposit, FRA, futures, OIS, IRS, basis swap,
and XCCY trades; for FRA, OIS, and IRS the requested component may be either
dependency (forecast or discount), for futures the forecast dependency, for
deposits the discount dependency, for basis swaps any of the three
dependencies (spread forecast, reference forecast, or discount), and for XCCY
any consumed curve registered under a component key. XCCY node risk ships
rate axes only — the FX spot is a constant, not an AAD input — so consumers
must not read it as complete XCCY risk. The first
failing gate
selects the reason in this order: family (`TRADE_FAMILY_NOT_AAD_ENABLED`),
requested dependency (`TRADE_DOES_NOT_DEPEND_ON_COMPONENT`), component
availability (`CURVE_COMPONENT_UNAVAILABLE`), curve representation
(`CURVE_REPRESENTATION_NOT_AAD_ENABLED`), passive trade validation
(`TRADE_VALIDATION_FAILED`), then AAD evaluation (`AAD_EVALUATION_FAILED`).
`TRADE_VALIDATION_FAILED` is the stable token for a supported trade that
fails passive pricing validation; field-level detail remains available through
`PriceRateTrade.error_`. For the single-currency families the availability and
representation gates walk every component the trade depends on, in dependency
order, and the first failing key decides the token — a passive (non-target)
dependency that is unavailable or not AAD-representable fails the cell even
when the addressed component itself is healthy.

Every node-sensitivity failure uses the canonical four-field result:
`eligible_ == false`, `pv_ == 0.0`, an empty `gradient_`, and a non-empty stable
`reason_`. Python projects the equivalent `eligible == False`, `pv == 0.0`,
`gradient == []`, and `reason` token. Consumers that apply a central-parameter
fallback must label it separately.

For XCCY the classification gate walks the consumed curves in a fixed order —
domestic discount, domestic forecast, foreign discount, foreign forecast, then the
basis curve — and the first curve in that order that cannot be classified decides
the token: `CURVE_REPRESENTATION_NOT_AAD_ENABLED` when it is the addressed
component, `AAD_EVALUATION_FAILED` for any other consumed curve (that token stays
a failure of the addressed representation). This walk runs before passive trade
validation, so a consumed non-target curve that cannot be classified reports
`AAD_EVALUATION_FAILED` even when passive pricing would also fail. When the XCCY
market itself cannot be resolved — no `xccyMarket_`, or a block the config cannot
route — no component key is addressable and the request reports the passive
pricing failure as `TRADE_VALIDATION_FAILED` instead of
`TRADE_DOES_NOT_DEPEND_ON_COMPONENT` (an expired trade prices to zero without
touching the XCCY market and keeps the dependency token).

`RateTradeNodeSensitivitiesBatch(trades, market, componentKeys)` applies one
shared component key list to every trade (Cartesian product), serially and in a
deterministic trade-major then key order; each (trade, component) entry carries
exactly the single-trade `RateTradeNodeSensitivityResult_` shape plus its
addressing fields, failures are isolated per entry and nothing is thrown.
Successful entries are numerically identical to the corresponding single-trade
calls. The batch hoists each trade's passive pricing (the
`TRADE_VALIDATION_FAILED` gate) to one passive PV per trade and each component's
classification and preparation to one preparation per curve, never per (trade,
component) pair.

`PreparedRateTrades_(trades)` owns an immutable portfolio snapshot and prepares
IRS/OIS/basis coupon geometry once. Its const `Price(market)` and
`NodeSensitivities(market, componentKeys)` methods preserve the ordinary batch
result shapes and failure rules. Every call evaluates current curves, valuation
time and fixing snapshots; trade changes require a new prepared object. Copies
share immutable geometry and support concurrent const calls. See
[prepared pricing](yield-curves/node-risk.md#repeated-pricing-with-prepared-trades)
for ownership, supported families and lifetime details.

`AggregateRatePortfolioNodeRisk(trades, market, componentKeys)` runs the same
sweep and returns the portfolio aggregate: one dense `Report_` per component over
its node axis (the parameter count and order take
`BuildCurveParameterLayout().parameterCount_` as the single source of truth; the
per-node header rows pair each parameter's date with its
`DescribeCurveFreeParameters` component; components with no eligible contribution
keep their dense zero tensor), PV totals grouped by each trade's actual PV
currency — the trade currency, or the domestic currency a XCCY swap's PV takes
from covered-interest parity; `RatePricingTradeResult_.currency_` is never the
grouping key — under the explicit `UnconvertedByActualPvCcy` policy (no FX
conversion; each trade's PV counts once, not once per component), and a parallel
meta table carrying one row per (trade, component) entry with its failure token,
actual PV currency, and PV. A component whose classification or preparation
fails carries no tensor; its failures live only in the meta table, and no padding
convention is introduced. `RateNodeSensitivityAxisLabels(market, componentKey)`
exposes the same `<date>:<component>` node labels standalone.

The component tensor sums gradients by component key and has no currency axis;
the currency grouping applies to PV totals and the parallel metadata. For node
gradients separated by PV currency, group eligible batch cells by
`(componentKey, actualPvCcy)` before summing. Duplicate requested keys retain
their metadata cells but contribute once per trade and component. See the
[node-risk methodology](yield-curves/node-risk.md).

The wiring is name-based: terms address curves through their `*ComponentKey_`
fields, those keys must resolve in `RatePricingMarket_::curveComponents_`, and
the requested `componentKey` selects which curve's parameters are registered as
AAD inputs. This deposit example is illustrative — it mirrors the fixtures in
`dal-cpp/tests/curve/test_ratecashflowpricing.cpp`:

```cpp
#include <dal-public/src/curvedata.hpp>
#include <dal-public/src/curvepricing.hpp>
#include <iostream>

namespace {
    Dal::RateIndexConvention_ QuarterlyIndex() {
        Dal::RateIndexConvention_ result;
        result.forecastTenor_ = Dal::PeriodLength_("3M");
        result.dayBasis_ = Dal::DayBasis_("ACT_365F");
        result.collateral_ = Dal::CollateralType_("OIS");
        return result;
    }
}

int main() {
    const Dal::Date_ today(2026, 1, 15), maturity(2027, 1, 15);

    // Curve components are registered by name in the market.
    Dal::RatePricingMarket_ market;
    market.valuationTime_ = Dal::DateTime_(today, 10, 30);
    market.resultCurrency_ = Dal::Ccy_("USD");
    market.curveComponents_["discount"] = Dal::DiscountPWCNew("flat", "USD", {maturity}, {0.04});
    market.curveComponents_["forecast"] = Dal::DiscountPWCNew("flat", "USD", {maturity}, {0.04});
    market.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());

    Dal::DepositTradeTerms_ terms;
    terms.notional_ = 100.0;
    terms.contractRate_ = 0.05;
    terms.lend_ = true;
    terms.index_ = QuarterlyIndex();
    terms.discountComponentKey_ = "discount";
    const Dal::RateTradeDefinition_ trade{"deposit-1", Dal::RateInstrumentType_("DEPOSIT"),
                                          today, today, maturity, Dal::Ccy_("USD"), terms};
    const Dal::Vector_<Dal::RateTradeDefinition_> trades{trade};
    const Dal::Vector_<Dal::String_> keys{"discount", "forecast"};

    // Single trade: a deposit only depends on the discount component, so a
    // "forecast" request returns TRADE_DOES_NOT_DEPEND_ON_COMPONENT.
    const auto single = Dal::RateTradeNodeSensitivities(trade, market, "discount");
    std::cout << "eligible=" << single.eligible_ << " pv=" << single.pv_
              << " |grad|=" << single.gradient_.size() << " reason='" << single.reason_ << "'\n";

    // Batch: shared key list, Cartesian product, trade-major then key order.
    const auto cells = Dal::RateTradeNodeSensitivitiesBatch(trades, market, keys);
    for (const auto& c : cells)
        std::cout << c.instrumentId_ << " x " << c.componentKey_
                  << " -> eligible=" << c.result_.eligible_
                  << " reason='" << c.result_.reason_ << "'\n";

    // Portfolio aggregation: dense tensor per component, PV by actual PV currency, meta table.
    const auto agg = Dal::AggregateRatePortfolioNodeRisk(trades, market, keys);
    std::cout << "policy=" << agg.policy_ << "\n";
    for (const auto& comp : agg.components_)
        std::cout << "component " << comp.componentKey_
                  << " nodes=" << comp.values_->Size("node") << "\n";
    for (const auto& [ccy, pv] : agg.pvByActualPvCcy_)
        std::cout << "PV[" << ccy << "] = " << pv << "\n";

    // Node labels, one per tensor row, "<date>:<component>".
    const auto labels = Dal::RateNodeSensitivityAxisLabels(market, "discount");
    return 0;
}
```

`RateTradeNodeSensitivityResult_` is `{ eligible_, pv_, gradient_, reason_ }`;
`RateTradeNodeSensitivityCell_` adds the `instrumentId_` / `componentKey_`
addressing fields; `RatePortfolioNodeRiskMetaEntry_` rows carry
`{ instrumentId_, componentKey_, eligible_, reason_, actualPvCcy_, pv_ }`.
The other families differ only in their terms struct — Python names the same
fields in snake_case:

| Family  | Terms type                                                        | Fields beyond notional and the index convention                                                                            | Addressable components                              |
|---------|-------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------|
| FRA     | `FraTradeTerms_`                                                  | `contractRate_`, `receiveFloating_`, `settleAtStart_`, `fixingIdentity_`                                                   | forecast or discount                                |
| Future  | `FutureTradeTerms_`                                               | `contractCount_`, `long_`, `referencePrice_`, `contractValuePerPricePoint_`, `convexityAdjustment_`, `fixingIdentity_`     | forecast                                            |
| OIS/IRS | `FixedFloatTradeTerms_` (via `OisTradeTerms_` / `IrsTradeTerms_`) | `contractRate_`, `payFixed_`, `fixedLeg_`, `floatLeg_`, `fixingIdentity_`                                                  | forecast or discount                                |
| Basis   | `BasisTradeTerms_`                                                | `contractSpread_`, `receiveReferencePaySpread_`, `spreadFixingIdentity_`, `referenceFixingIdentity_`, both leg conventions | spread forecast, reference forecast, or discount    |
| XCCY    | `XccyTradeTerms_`                                                 | `positionCount_`, `contractSpread_`, `spreadOnForeignLeg_`, `receiveNonSpreadPaySpread_`, `config_`                        | any consumed curve registered under a component key |

Fixing treatment is common to all families: future fixings project (nonzero
gradient), past fixings must be supplied in the snapshot (that period's gradient
is structurally zero), and a past-but-missing fixing fails passive validation —
`PriceRateTrades(...)[i].missingHistoricalFixings_` names the gap while the
sensitivity returns the `TRADE_VALIDATION_FAILED` token.

### C++ quote-space DV01

Quote-space aggregation is a two-step workflow. First freeze a calibration result,
its quote/parameter axes, its effective inverse, and the state of every bound market
component in a `RateQuoteRiskProvenance_`. Then aggregate a portfolio against one or
more provenances:

```cpp
const auto provenance = Dal::BuildSingleCurveQuoteRiskProvenance(
    spec, calibration, options, market,
    Dal::RateQuoteRiskProvenanceConfig_{
        "usd-ois", {{spec.curveName_, "discount"}}});
const auto quoteRisk = Dal::AggregateRatePortfolioQuoteRisk(
    trades, market, {provenance});
```

The supported provenance factories are
`BuildSingleCurveQuoteRiskProvenance`, `BuildJointXccyQuoteRiskProvenance`,
`BuildStagedXccyBasisQuoteRiskProvenance`, and
`BuildJointMultiCurveQuoteRiskProvenance`. They cover exact single-curve,
simultaneous domestic/foreign/basis XCCY, staged XCCY basis, and generic
same-currency joint calibration, respectively. The public
`CalibrateJointMultiCurveBundle(spec, options)` facade exposes the generic
joint result; its effective inverse request defaults to false. See the
[generic joint mapping contract](yield-curves/joint-quote-risk.md),
including the explicit solution-selection semantics for underdetermined systems.
Ordinary staged multi-curve chain rules do not have a provenance factory. Quote risk also
requires an available effective inverse; unavailable results retain a stable
reason such as `QUOTE_RISK_INVERSE_NOT_REQUESTED`,
`QUOTE_RISK_NOT_AVAILABLE_FOR_SOLVE_MODE`, or
`QUOTE_RISK_EFFECTIVE_INVERSE_UNAVAILABLE`.

`RateQuoteRiskAxis_` publishes named parameter/residual ranges and ordered
coordinates. Its scheme is `dal.quote-risk-axis/1+jcs+sha256`; the bound curve
state uses `dal.quote-risk-state/1+jcs+sha256` for the three v1 domains.
Generic joint provenance uses `/2+jcs+sha256` schemes; the v1 bytes and global
scheme accessors remain unchanged. Fingerprint values begin with
`sha256:`. Aggregation recomputes the component-state fingerprints and rejects a
stale provenance atomically instead of mixing states.

Each `RateQuoteRiskBucket_` is addressed by calibration ID, axis fingerprint,
residual block, quote ordinal/key/name, and actual PV currency.
`dPvDDecimalQuote_` has units of price per `+1.0` decimal quote move and
`dv01_ = dPvDDecimalQuote_ * 1e-4` is price per `+1 bp`. Portfolio PV and quote
risk are grouped under `UnconvertedByActualPvCcy`; DAL performs no FX
conversion. Eligible trades, structural zeros, failures, and provenance-state
failures remain explicit in the parallel metadata. Provenance construction is
the only calibration-time step: `AggregateRatePortfolioQuoteRisk` neither bumps
quotes nor recalibrates curves.

See the runnable [C++ quote-risk example](../dal-cpp/examples/quote_risk) and
the [Jacobian methodology](yield-curves/jacobian-risk.md#production-quote-space-dv01).

## Python

See the dedicated [Python interface chapter](python/README.md) for binding names,
valuation settings, curve calibration, and examples.

## Excel

See the dedicated [Excel interface chapter](excel/README.md) for worksheet
functions, handles, settings, and examples. The [FIX settings guide](excel/script-settings.md)
covers the exact input matrices and diagnostics.

## Error and State Conventions

- C++ failures use `Dal::Exception_` through `REQUIRE`/`THROW` paths.
- Python maps native exceptions to Python exceptions; invalid binding shapes and
  indices use `ValueError`/`IndexError` where appropriate.
- Excel returns worksheet error text annotated with the failing argument.
- Evaluation date and the legacy fixing store are process-wide state. Evaluation-date mutation
  is serialized with native valuation; evaluation-date reads use the store lock
  and remain available during valuation. Reset-aware XCCY APIs consume an
  immutable `MarketFixingSnapshot_`; an omitted snapshot is copied once from the
  global store before calibration starts. Callers must still externally
  serialize direct mutation of the legacy fixing store.
- AAD tapes are thread-local and must not be shared across recording frames.

For ownership details, see [architecture](architecture.md).
