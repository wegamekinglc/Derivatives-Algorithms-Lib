# DAL Public API Guide

DAL exposes the same main workflows through C++, Python, and Excel. This guide
identifies the supported entry points and their ownership contracts; it is not an
exhaustive reference for every core numerical type.

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

| Header                                 | Main entry points                                                                                   |
|----------------------------------------|-----------------------------------------------------------------------------------------------------|
| `<dal-public/src/global.hpp>`          | `InitGlobalData`, `SetEvaluationDate`, `GetEvaluationDate`                                          |
| `<dal-public/src/script.hpp>`          | `NewScriptProduct`, `DebugScriptProduct`, `DebugScriptProductJson`, `DebugScriptProductTree`       |
| `<dal-public/src/models.hpp>`          | `NewBSModelData`, `NewDupireModelData`                                                              |
| `<dal-public/src/value.hpp>`           | `ValueByMonteCarlo`                                                                                 |
| `<dal-public/src/random.hpp>`          | Pseudo/Sobol constructors and uniform/normal matrix fills                                           |
| `<dal-public/src/curveprotocol.hpp>`   | Day-basis, tenor, collateral, rate-leg/index, currency-pair, FX-reset, and fixing-snapshot builders |
| `<dal-public/src/curveinstrument.hpp>` | Deposit, FRA, future, swap, OIS, basis-swap, and fixed/resettable/MTM cross-currency-swap builders  |
| `<dal-public/src/curvedata.hpp>`       | Piecewise-linear-forward, zero-rate, and curve-block builders                                       |
| `<dal-public/src/curvespec.hpp>`       | `CurveCalibrationSpecBuilder_`, `CalibrateSingleCurve`, `CalibrateMultiCurveBundle`                 |
| `<dal-public/src/xccycalibration.hpp>` | Staged and joint XCCY spec builders, calibration, and joint-result accessors                        |
| `<dal-public/src/curvepricing.hpp>`    | Typed rate-cashflow planning, batch pricing, node sensitivity, and family registry                  |
| `<dal-public/src/interp.hpp>`          | Linear one-dimensional interpolation builder                                                        |
| `<dal-public/src/repository.hpp>`      | Repository find, erase, and size helpers for a configured host environment                          |

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
See the [random methodology policy table](methodology/random.md#normal-draw-inverse-cdf-modes)
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

`ValueByMonteCarlo` requires `numPath > 0`. Its optional arguments select the
random generator, Brownian bridge, AAD risks, fuzzy smoothing, and compiled
script evaluator. The evaluation date is process-wide. Native Monte Carlo
valuation holds the valuation/mutation barrier for its full date-dependent
interval, so evaluation-date setters wait until valuation finishes while
getters remain available through the store lock. Monte Carlo valuations are
serialized within one process; callers that require independent concurrent
dates should use isolated processes.

Script products dump three ways: `DebugScriptProduct` returns the legacy
indented s-expression listing, `DebugScriptProductJson` a compact versioned
JSON AST (schema `dal.script-product/1`, past events included, variable and
constant tables resolved), and `DebugScriptProductTree(product, ascii, width)`
a width-aware Unicode tree with a pure-ASCII fallback. See the
[script engine methodology](methodology/script_engine.md#product-debug-outputs)
for the formats and the dump grammar.

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
[cross-currency pricing and calibration](methodology/xccy_calibration.md) and
the [Jacobian methodology](methodology/yield_curve_jacobian.md#staged-xccy-jacobian-layout).

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
and `BuildStagedXccyBasisQuoteRiskProvenance`. They cover exact single-curve,
simultaneous domestic/foreign/basis XCCY, and staged XCCY basis calibration,
respectively. Ordinary staged multi-curve chain rules and generic joint
multi-curve calibration do not have C++ provenance factories. Quote risk also
requires an available effective inverse; unavailable results retain a stable
reason such as `QUOTE_RISK_INVERSE_NOT_REQUESTED`,
`QUOTE_RISK_NOT_AVAILABLE_FOR_SOLVE_MODE`, or
`QUOTE_RISK_EFFECTIVE_INVERSE_UNAVAILABLE`.

`RateQuoteRiskAxis_` publishes named parameter/residual ranges and ordered
coordinates. Its scheme is `dal.quote-risk-axis/1+jcs+sha256`; the bound curve
state uses `dal.quote-risk-state/1+jcs+sha256`. Fingerprint values begin with
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

See the runnable [C++ quote-risk example](../dal-cpp/examples/quote_risk/) and
the [Jacobian methodology](methodology/yield_curve_jacobian.md#production-quote-space-dv01).

## Python

Import the installed package with:

```python
import dal
```

### Common workflows

| Workflow                | Python entry points                                                                                                                                                                            |
|-------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Dates/global state      | `Date_`, `Year`, `Month`, `Day`, `EvaluationDate_Set`, `EvaluationDate_Get`                                                                                                                    |
| Script products         | `Product_New`, `Product_Debug`, `Product_DebugJson`, `Product_DebugTree`                                                                                                                                                        |
| Models                  | `BSModelData_New`, `DupireModelData_New`                                                                                                                                                       |
| Valuation               | `MonteCarlo_Value`                                                                                                                                                                             |
| Random generation       | `PseudoRSG_New`, `SobolRSG_New`, `*_Get_Uniform`, `*_Get_Normal`                                                                                                                               |
| Calendar operations     | `Holidays_`, `Is_BizDay`, `NextBizDay`, `PrevBizDay`, `Adjust`                                                                                                                                 |
| Curves                  | `DiscountZeroRate_New`, convention/instrument builders, `CurveCalibrationSpecBuilder_`, `CalibrateSingleCurve`, `CalibrateMultiCurveBundle`, `CalibrateXccyMarket`, `CalibrateJointXccyMarket` |
| XCCY reset data         | `FixingIdentity_`, `FxResetConvention_`, `MarketFixingSnapshot_New`, `CrossCurrencySwapConfigBuilder_`, `XccyNotionalMode`                                                                     |
| Rate cashflow pricing   | `RateTradeDefinition_`, typed terms, `RatePricingMarket_`, `PriceRateTrades`, `RateTradeNodeSensitivities`, `RateTradeNodeSensitivitiesBatch`, `AggregateRatePortfolioNodeRisk`, quote-risk provenance builders, `AggregateRatePortfolioQuoteRisk`                               |
| Convenience calibration | `calibrate_curve` from `dal/api.py`                                                                                                                                                            |

The basic valuation shape is:

```python
import dal

dal.EvaluationDate_Set(dal.Date_(2022, 9, 25))
product = dal.Product_New(
    ["STRIKE", dal.Date_(2023, 9, 25)],
    ["100.0", "call pays MAX(spot() - STRIKE, 0.0)"],
)
model = dal.BSModelData_New(100.0, 0.2, 0.05, 0.02)
result = dal.MonteCarlo_Value(product, model, 2**16, enable_aad=True)
```

`MonteCarlo_Value` requires `num_path > 0`. The binding releases the Python GIL
around native valuation, and `EvaluationDate_Get` / `EvaluationDate_Set` release
it before waiting on native synchronization. A setter waits for an in-progress
valuation; a getter can read the stable current date while valuation runs.

For precise-CDF-polished Sobol normal draws, pass both flags explicitly:

```python
rsg = dal.SobolRSG_New(i_path=0, ndim=3, precise=True, polish=True)
normals = dal.SobolRSG_Get_Normal(rsg, 1024)
```

### Matrix and Dupire surface input

`DoubleMatrix_` supports all of the following:

```python
surface = dal.DoubleMatrix_(3, 2, 0.20)
surface[1, 0] = 0.21

surface = dal.DoubleMatrix_([
    [0.24, 0.23],
    [0.21, 0.20],
    [0.22, 0.21],
])
```

Rows must be rectangular numeric sequences. `DupireModelData_New` expects a
spots-by-times matrix, so its shape must be
`len(spots) × len(times)`.

### Python curve calibration

`dal.calibrate_curve(...)` covers the common single discount-curve path. Use
`CurveCalibrationSpecBuilder_` directly for projection-curve inputs, staged
multi-curve calibration, or lower-level solver settings. Python enum names are:

- `CurveParameterization`: `PIECEWISE_LINEAR_FWD`,
  `PIECEWISE_CONSTANT_FWD`, `ZERO_RATE`, `LOG_DISCOUNT`;
- `CurveSolveMode`: `EXACT`, `APPROXIMATE`;
- `CurveJacobianMode`: `ANALYTIC`, `BUMPED`; and
- `LogDfScheme`: `LOG_LINEAR`, `LOG_CUBIC_NATURAL`, `MIXED`.

`ZERO_RATE` is supported by `CalibrateSingleCurve` and `dal.calibrate_curve`. Supply only
strictly-future knots; the anchor is internal and contributes no solver or Jacobian
column. The scalar `initialGuess_` is a decimal continuously compounded zero rate.
`dal.calibrate_curve(..., base_curve=...)` treats the calibrated zero rates as spreads
over that base.

Direct construction uses:

```python
curve = dal.DiscountZeroRate_New(
    "usd_zero", "USD", today, node_dates, zero_rates,
    day_count=dal.DayBasis_("ACT_365F"),
    log_df_scheme=dal.LogDfScheme.LOG_LINEAR,
    base=None,
)
```

The returned `DiscountZeroRate_` exposes read-only `anchor_date`, `node_dates`,
`zero_rates`, `day_count`, and `log_df_scheme` properties.

Python staged XCCY exposes both `CalibrateXccyMarket(spec)` and
`CalibrateXccyMarket(spec, options)`. `CrossCurrencyCalibrationOptions_`
provides trailing-underscore and snake-case properties for the Jacobian mode
and the two independent compute flags; its defaults are `ANALYTIC`, `True`, and
`True`. The result keeps matrices under `result.diagnostics`, not at the result
top level. That diagnostics object exposes the forward `jacobian`, the
`eff_jacobian_inverse`, instrument-name and parameter-knot axes, residual
tolerance, scaling labels, and availability states, with matching
trailing-underscore aliases.

Python joint XCCY exposes the declarations, builder,
`JointXccyCalibrationOptions_`, calibration entry point, and result surface
with both trailing-underscore and snake-case aliases.
`JointXccyCalibrationResult_` provides `domestic_curve_block`,
`foreign_curve_block`, `basis_curve`, `fx_forward_curve`, `fixings`, group
diagnostics, `market_rates`, `model_rates`, `residuals`,
`jacobian_at_solution`, `eff_jacobian_inverse`, `parameter_ranges`, and
`residual_ranges`. `CalibrateJointXccyMarket(spec, options)` selects analytic or
bumped Jacobians and optional matrix construction. The effective inverse has
shape `totalParameters x totalResiduals`; applying it to a raw decimal quote
bump requires division by the spec's `tolerance_`, as described in the
[Jacobian methodology](methodology/yield_curve_jacobian.md#joint-xccy-jacobian-layout).

### Python rate cashflow pricing

Python exports the seven-family enum, all family-specific terms classes,
`RateTradeDefinition_`, `RatePricingMarket_`, `PriceRateTrades`,
`RateTradeNodeSensitivities`, `RateTradeNodeSensitivitiesBatch`, and
`AggregateRatePortfolioNodeRisk`. The pricing and sensitivity functions use
keyword-only arguments and release the GIL around native work.
`component_keys` must be a Python `list` — a tuple is rejected with `TypeError`
before any native work starts. The minimal single-trade call:

```python
r = dal.RateTradeNodeSensitivities(trade=trade, market=market, component_key="discount")
# r.eligible, r.pv, list(r.gradient), r.reason
```

Results are read-only projections of the C++ shapes. A complete runnable
deposit example covering the batch and aggregation calls is in the
[dal-python README](../dal-python/README.md#rate-cashflow-pricing-and-node-risk).

### Python quote-space DV01

Python exposes `RateQuoteRiskProvenanceConfig_`, all three supported provenance
builders, and `AggregateRatePortfolioQuoteRisk` as keyword-only calls. They
release the GIL around native construction or aggregation and return read-only
objects. The axis/state fingerprint schemes, stable availability reasons,
price-per-decimal and DV01 units, and `UnconvertedByActualPvCcy` policy are
identical to C++.

The runnable [single-curve quote-risk example](../dal-python/examples/009.quote_risk.py)
prints both fingerprints, the policy, and every bucket. The
[joint XCCY example](../dal-python/examples/007.xccy_joint_calibration.py) also
constructs joint provenance. Staged multi-curve chain rules and generic joint
multi-curve provenance remain outside the supported Python surface.

`Storable_` exposes read-only `name` and `type` properties, and the native
`YieldCurve_` / `CurveBlock_` / `Bag_` hierarchy is bound for archive
compatibility with the standalone web application. `_StorableToJson`,
`_StorableFromJson`, `_BagNew`, and `_BagContents` are private integration
helpers rather than supported general serialization functions.

See [dal-python/README.md](../dal-python/README.md) for package-focused examples.

## Excel

The Windows XLL exposes worksheet functions and stores constructed objects in an
Excel-side repository. Constructors return handles; pass those handles into later
functions rather than attempting to unpack native objects in cells.

### Script valuation

```text
=PRODUCT.NEW("call", dates, events)
=BSMODELDATA.NEW("bs", 100, 0.20, 0.05, 0.02)
=MONTECARLO.VALUE(product_handle, model_handle, 65536, "sobol", FALSE, TRUE, 0.01)
```

For a local-volatility model, use
`DUPIREMODELDATA.NEW(name, spot, rate, repo, spots, times, vols)`. The volatility
range must be a rectangular spots-by-times matrix. `MONTECARLO.VALUE` requires a
strictly positive path count.

`SOBOLRSG.NEW(name, i_path, n_dim, precise, polish)` uses the same independent
normal-draw flags as C++ and Python. Pass `TRUE, TRUE` for the precise-CDF Newton
correction; leaving both optional flags `FALSE` selects the Acklam-only default.

### Curve workflows

Primary worksheet families are:

| Purpose         | Worksheet functions                                                                                                                                                        |
|-----------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Conventions     | `PERIODLENGTH.NEW`, `DAYBASIS.NEW`, `RATELEGCONVENTION.NEW`, `RATEINDEXCONVENTION.NEW`, `COLLATERALTYPE.*`                                                                 |
| XCCY reset data | `XCCYRESETCONVENTION.NEW`, `MARKETFIXINGSNAPSHOT.NEW`                                                                                                                      |
| Instruments     | `DEPOSIT.NEW`, `FRA.NEW`, `FUTURE.NEW`, `SWAP.NEW`, `OISSWAP.NEW`, `BASISSWAP.NEW`, `CROSSCURRENCYSWAP.NEW`, `CROSSCURRENCYSWAPCONFIG.NEW`, `CROSSCURRENCYSWAP.CONFIG.NEW` |
| Direct curves   | `DISCOUNTPWLF.NEW`, `DISCOUNTZERORATE.NEW`, `CURVEBLOCK.NEW.SIMPLE`                                                                                                        |
| Calibration     | `CALIBRATE.SINGLECURVE`, `CALIBRATE.XCCYMARKET`, `CALIBRATE.JOINTXCCY`                                                                                                     |
| Results         | `CALIBRATIONRESULT.GET`, `CALIBRATIONRESULT.GET.CURVE`, `XCCYCALIBRATIONRESULT.*`, `JOINTXCCYCALIBRATIONRESULT.GET*`                                                       |
| Rate risk       | `RATETRADEHEADER.NEW`, `RATEFIXINGIDENTITY.NEW`, `RATEDEPOSITTRADE.NEW`, `RATEFRATRADE.NEW`, `RATEFUTURETRADE.NEW`, `RATEFIXEDFLOATTRADE.NEW`, `RATEBASISTRADE.NEW`, `RATEXCCYTRADE.NEW`, `RATEPRICINGMARKET.NEW`, `RATETRADENODESENSITIVITIESBATCH.SPILL`, `RATEPORTFOLIONODERISK.SPILL` |
| Repository      | `REPOSITORY.FIND`, `REPOSITORY.ERASE`, `REPOSITORY.SIZE`                                                                                                                   |

`DISCOUNTZERORATE.NEW` takes name, currency, anchor, future dates, and continuously
compounded decimal zero rates, with optional day count, log-DF scheme, and base handle.
`CALIBRATE.SINGLECURVE` accepts a two-column optional settings range. Supported keys
include curve name, target, solve mode, parameterization (`ZERO_RATE` included), log-DF
scheme, smoothing/tolerances, scalar initial guess, and evaluation budgets. Its optional
`baseCurve` input is the curve multiplied under the calibrated curve; it is distinct from
the `discountCurve` used to price a forward-curve calibration.

`CALIBRATE.XCCYMARKET` accepts `jacobianMode`,
`computeForwardJacobian`, and `computeEffJacobianInverse` in its optional
two-column settings range. Omitting them preserves the `ANALYTIC`, `TRUE`,
`TRUE` defaults. `XCCYCALIBRATIONRESULT.GET` exposes `instrumentNames`,
`parameterKnotDates`, `jacobian`, `effJacobianInverse`,
`residualTolerance`, both scaling labels, and both availability states in
addition to the fit vectors and scalars. The staged matrix axes and scaling
contract match C++ and Python.

`CALIBRATE.JOINTXCCY` accepts one domestic discount-instrument/knot group, one
foreign discount-instrument/knot group, configured XCCY instruments, basis
knots, an optional immutable snapshot handle, and two-column settings. Dedicated
result functions return the domestic block, foreign block, and basis curve
handles.
`JOINTXCCYCALIBRATIONRESULT.GET` returns `fxForwards`, `marketRates`,
`modelRates`, `residuals`, `jacobian`, `effJacobianInverse`,
`parameterRanges`, or `residualRanges`. Joint settings can request both matrix
computations independently.

### Rate risk

The rate-risk family is handle-based: build the index convention, trade header,
and trade with their constructors, assemble the market, then spill the results.
A minimal deposit sequence:

```text
C1: =RATEINDEXCONVENTION.NEW("3M", "ACT_365F", "OIS")
D1: =RATETRADEHEADER.NEW("deposit-1", DATE(2026,1,15), DATE(2026,1,15), DATE(2027,1,15), "USD")
E1: =RATEDEPOSITTRADE.NEW(D1, 100, 0.05, TRUE, C1, "discount")
F1: =DISCOUNTPWLF.NEW("flat-discount", "USD", DATE(2027,1,15), 0.04)
F2: =DISCOUNTPWLF.NEW("flat-forecast", "USD", DATE(2027,1,15), 0.04)

' the index convention's first three arguments are plain strings, not handles;
' the component-key array and the curve-handle range must be equal-length
' parallel arrays; the six trailing market arguments (fixings, domestic block,
' foreign block, fxSpot, collateral currency, basis curve) stay empty for a
' single-currency market, while an XCCY market requires both blocks, a positive
' fxSpot, and a collateral currency
G1: =RATEPRICINGMARKET.NEW(NOW(), "USD", {"discount","forecast"}, F1:F2, , , , , , )

H1: =RATETRADENODESENSITIVITIESBATCH.SPILL(E1, {"discount","forecast"}, G1)
I1: =RATEPORTFOLIONODERISK.SPILL(E1, {"discount"}, G1)
```

Both spill functions take `(trades, componentKeys, market)` and return long-form
spills rather than node-gridded columns, since components can carry different
node counts. `RATETRADENODESENSITIVITIESBATCH.SPILL` emits the six columns
`trade, component, reason, pv, node, value` — one row per node of each eligible
(trade, component) entry plus a reason row per failed entry.
`RATEPORTFOLIONODERISK.SPILL` emits the same columns plus a trailing
`currency`, with one aggregate row per actual PV currency. Only trades with
past fixing dates need a fixing-snapshot handle, and XCCY additionally needs
the domestic/foreign blocks and the basis curve.

Quote-space DV01 uses provenance handles and a separate fixed-width spill:

```text
J1: =SINGLECURVEQUOTERISKPROVENANCE.NEW(calibrationResult, "usd-ois", parameterBlockKeys, componentKeys, G1)
K1: =RATEPORTFOLIOQUOTERISK.SPILL(E1, G1, J1)
```

`JOINTXCCYQUOTERISKPROVENANCE.NEW` and
`STAGEDXCCYBASISQUOTERISKPROVENANCE.NEW` cover the other supported calibration
domains. `RATEQUOTERISKPROVENANCE.NEW(result, calibrationId,
parameterBlockKeys, componentKeys, market)` dispatches from a result handle;
ordinary staged chains and generic joint calibration produce explicit rows with
`QUOTE_RISK_NOT_AVAILABLE_FOR_STAGED_CHAIN_RULE` and
`QUOTE_RISK_EFFECTIVE_INVERSE_UNAVAILABLE` rather than an invented transform.

The quote-risk spill columns are `calibration`, `axis_fingerprint`,
`quote_key`, `quote_name`, `block`, `currency`, `quote_sensitivity`, `dv01`,
`availability`, and `reason`. Quote sensitivity is price per decimal quote;
DV01 is price per `+1 bp`. Rows remain separated by actual PV currency under
`UnconvertedByActualPvCcy`, with no FX conversion. A paste-ready worksheet
recipe is in [dal-excel/examples/008.quote_risk.md](../dal-excel/examples/008.quote_risk.md).

Generated function help under `dal-excel/auto/*.htm` is the argument-level
catalog used by Excel registration.

See [dal-excel/README.md](../dal-excel/README.md) for build and add-in guidance.

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
