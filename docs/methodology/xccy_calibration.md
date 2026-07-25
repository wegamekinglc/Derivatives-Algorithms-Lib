# Cross-Currency Pricing and Calibration

DAL prices fixed-notional, resettable-notional, and mark-to-market (MTM)
cross-currency swaps, and calibrates their basis either after the two currency
markets are known or simultaneously with both currency curve blocks. This note
defines the current pricing, fixing, collateral, and calibration contracts.

## Market and FX-Forward Convention

For a currency pair `(domestic, foreign)`, FX spot $S$ is quoted in domestic
currency per unit of foreign currency. With domestic and foreign discount
factors $P_d(t,T)$ and $P_f(t,T)$ and a domestic-currency basis discount factor
$P_b(t,T)$, the FX forward is

$$
F(t,T)=S\frac{P_f(t,T)}{P_d(t,T)P_b(t,T)}.
$$

The current implementation supports domestic-currency collateral only. The
domestic and foreign discount curves used by an XCCY instrument come from the
collateral selectors on its two rate-index conventions. Projection-enabled
indices route to the matching tenor forward curves; non-projection indices use
their discount curves for forecasting.

## Cashflow and Notional Modes

Let $N_f$ be the constant foreign notional, $N_{d,0}$ the configured domestic
notional, and $X_i$ the observed or forward FX fixing for the reset effective
date of domestic coupon period $i$. DAL creates FX resets from the second
domestic period onward. The three `XccyNotionalMode_` values are:

- `FIXED`: $N_{d,i}=N_{d,0}$ for every domestic period.
- `RESETTABLE`: $N_{d,0}$ applies to the first period and
  $N_{d,i}=N_fX_i$ for $i\ge1$.
- `MARK_TO_MARKET`: notionals follow the same formula as `RESETTABLE`, and each
  reset effective date exchanges the domestic notional change
  $\Delta N_{d,i}=N_{d,i}-N_{d,i-1}$. Under DAL's domestic-leg PV sign
  convention, the adjustment has the same receive/pay orientation as the
  positive domestic final exchange and is $+\Delta N_{d,i}P_d(t,T_i)$.

Coupon PVs use the period notional, accrual fraction, projected or observed
rate, and the payment-date discount factor. Foreign coupons and notional
exchanges are converted to domestic PV with $S P_f(t,T)/P_b(t,T)$. Initial
notional exchanges are included only when the leg start date is on or after the
valuation date. Final exchanges are included when maturity is on or after the
valuation date.

A cashflow whose payment date is strictly before the valuation date is omitted.
A cashflow on the valuation date remains in the PV, regardless of the valuation
time. The same date rule applies to MTM notional adjustments: an adjustment is
omitted only when its effective date is strictly before the valuation date.

The model quote is the par spread. If the spread is on the foreign leg,

$$
s=\frac{\mathrm{PV}_d-\mathrm{PV}_f}{A_f};
$$

if it is on the domestic leg,

$$
s=\frac{\mathrm{PV}_f-\mathrm{PV}_d}{A_d}.
$$

Here $A_d$ and $A_f$ are the remaining positive spread annuities in domestic
PV units. Calibration rejects an instrument with no remaining annuity on its
quoted leg.

## Reset and Fixing Timing

`FxResetConvention_` defines a non-negative business-day fixing lag, fixing
holiday calendar, business-day convention, and fixing hour/minute. The reset
for domestic period $i\ge1$ is effective on that period's accrual start; its FX
fixing date is the lagged and adjusted effective date. `FixingIdentity_` gives
the index name and hour/minute for each domestic and foreign floating-rate
fixing. `FxIndexName(domestic, foreign)` gives the canonical FX fixing index
name `FX[foreign/domestic]`, matching the domestic-per-foreign market
convention.

At valuation time $v$:

- a fixing time before $v$ must exist in the supplied snapshot;
- a fixing exactly at $v$ uses the snapshot value when present, otherwise the
  active forward value; and
- a fixing after $v$ uses the active forward value.

Historical requests are deduplicated and include only observations that still
feed unsettled coupons, domestic notionals, or MTM reset dependencies. Missing
required observations fail with the index, timestamp, and pricing context.

## Immutable Market-Fixing Snapshot

`MarketFixingSnapshot_` is an immutable nested map
`index name -> DateTime_ -> value`. The same snapshot can contain rate and FX
observations and is retained by the calibration result. Index names must be
non-empty, timestamps valid, and observations positive and finite. The
positive-value requirement means that rate fixings from negative-rate regimes
(for example EURIBOR, ESTR, TIBOR, or SARON over 2014–2022) are not accepted.

Direct lookup wins. If that observation is absent and the name is canonical FX,
lookup returns the reciprocal of the reverse canonical observation. When both
directions exist at the same timestamp, the snapshot requires
`abs(direct * reverse - 1) <= 1e-10`.

Core and Python map inputs can contain at most one value for each `(index name,
timestamp)` pair. The Excel parallel-array adapter rejects duplicate pairs
explicitly. Supplying a snapshot makes it authoritative, including when it is
explicitly empty. When a snapshot is omitted, staged and joint XCCY calibration
first collect and deduplicate all required historical observations across all
instruments and copy them from the process-wide fixing store once. Later global
mutations cannot change the captured solve.

## Staged Basis Calibration

`CalibrateCrossCurrencyMarket` takes pre-calibrated domestic and foreign
`CurveBlock_` objects and solves only for the basis-curve parameters. For XCCY
instrument $j$ and basis parameters $b$,

$$
r_j(b)=s_j^{\text{model}}(b)-s_j^{\text{market}}.
$$

The basis curve is piecewise-constant forward in this path. Exact mode solves
the tolerance-scaled system and may retain the effective inverse Jacobian;
approximate mode minimizes the fit subject to smoothness regularization. The
result contains the basis-bearing `CrossCurrencyMarket_`, FX forwards at the
basis knots, fit diagnostics, and optional matrices.

For `nInstruments` quotes and `nBasisParameters` basis parameters, the staged
forward Jacobian has shape `nInstruments x nBasisParameters`; the effective
inverse has shape `nBasisParameters x nInstruments`.
Both matrices belong to `CrossCurrencyCalibrationDiagnostics_` on the staged
result. `instrumentNames_` labels forward-Jacobian rows and effective-inverse
columns in instrument input order. Names are labels only and may repeat; the
integer position is authoritative. `parameterKnotDates_` labels
forward-Jacobian columns and effective-inverse rows in `spec.knotDates_` order,
which is the piecewise-constant basis curve's right-forward parameter order.

## Joint Domestic, Foreign, and Basis Calibration

`CalibrateJointXccyMarket` solves one residual system over three ordered groups:

1. every domestic `JointCurveDeclaration_`, in declaration order;
2. every foreign declaration, in declaration order; and
3. the `XccyBasisCurveDeclaration_`.

Within each declaration, parameter columns follow its representation:

| Parameterization         | Columns per declaration                        |
|--------------------------|------------------------------------------------|
| `PIECEWISE_CONSTANT_FWD` | right-hand forwards in knot order              |
| `PIECEWISE_LINEAR_FWD`   | left/right forward pair at each knot           |
| `LOG_DISCOUNT`           | future-knot log discount factors               |
| `ZERO_RATE`              | future-knot continuously compounded zero rates |

Residual rows use the same group order: domestic instrument groups, foreign
instrument groups, then XCCY quotes. `parameterRanges_` and `residualRanges_`
name every contiguous slice and give its zero-based `offset_` and `size_`.
`jacobianAtSolution_` therefore has shape `total residuals x total parameters`.
The smoother is block diagonal by declaration; cross-block Jacobian entries
come from pricing and curve routing, not regularization.

The result retains both solved currency blocks, the basis curve, FX forwards,
the fixing snapshot, group and joint diagnostics, market/model/residual vectors,
ranges, and optional forward/effective-inverse Jacobian matrices.
`effJacobianInverse_` has shape `totalParameters x totalResiduals`. Both
matrices live on the top-level joint result rather than inside the domestic,
foreign, or XCCY group diagnostics.

## Analytic and Bumped Jacobians

Both staged and joint calibration accept `ANALYTIC` and `BUMPED`. A
default-constructed staged `CrossCurrencyCalibrationOptions_` selects
`ANALYTIC` and requests both matrices. The one-argument staged overload is
equivalent to passing those defaults. At the accepted exact solution, the
exposed forward matrix is the unscaled analytic residual Jacobian. Staged
diagnostics record this as `jacobianScaling_ = "unscaled"`.

The effective inverse is the weighted pseudoinverse formed from the solver's
tolerance-scaled Jacobian at that same solved state; it is not the literal
inverse of the exposed forward matrix. Staged diagnostics record
`effJacobianInverseScaling_ = "solver_scaled"` and
`residualTolerance_ = spec.tolerance_`. For a raw decimal quote perturbation
$\Delta q$, write $E$ for the effective inverse. The parameter move is

$$
\Delta x = E\,\Delta q / \mathrm{residualTolerance}.
$$

Exact analytic calibration may produce both matrices. Exact bumped calibration
produces only the effective inverse. Approximate calibration produces neither,
and the forward/inverse options can suppress their computations independently.
The staged availability fields describe that outcome without requiring callers
to infer intent from an empty matrix:

| Solve mode    | Jacobian mode | Requested forward Jacobian | Requested effective inverse |
|---------------|---------------|----------------------------|-----------------------------|
| `EXACT`       | `ANALYTIC`    | `available`                | `available`                 |
| `EXACT`       | `BUMPED`      | `not_available_for_mode`   | `available`                 |
| `APPROXIMATE` | `ANALYTIC`    | `not_available_for_mode`   | `not_available_for_mode`    |
| `APPROXIMATE` | `BUMPED`      | `not_available_for_mode`   | `not_available_for_mode`    |

For either matrix, a false compute flag takes precedence and reports
`not_requested`. Unavailable matrices use an empty numeric carrier.

Joint XCCY analytic calibration is fail-fast. Every domestic and foreign
declaration must satisfy the joint curve AAD gates, including `ACT_365F`, a
supported curve representation and templated instrument route, and consistent
discount/forward slot usage. Every XCCY plan mode (`FIXED`, `RESETTABLE`, and
`MARK_TO_MARKET`) is supported when its plan and fixing identities are valid.
If any analytic gate fails, the exception identifies the ineligible currency,
declaration, instrument, or reset. Select `BUMPED` explicitly to run the same
valid residual system without the analytic eligibility requirement.

## Surface Availability

- **Core/public C++:** staged XCCY has full options and both matrices on
  `CrossCurrencyCalibrationDiagnostics_`; joint XCCY has full options and both
  top-level matrices. The public facade has read-only helpers for each.
- **Python:** staged XCCY has both overloads, options, matrices, axes, scaling,
  and availability metadata under `result.diagnostics`, with
  trailing-underscore and snake-case aliases. Joint XCCY has options, named
  ranges, a forward Jacobian, and an effective inverse at the result top level.
- **Excel:** staged settings select the Jacobian mode and independent compute
  flags; `XCCYCALIBRATIONRESULT.GET` exposes both matrices, both axes,
  tolerance, scaling, and availability. Joint settings expose both matrices
  through `JOINTXCCYCALIBRATIONRESULT.GET`.

## Examples

The three standalone programs under `dal-cpp/examples/` exercise the staged,
joint, and pricing surfaces declared in `dal-cpp/dal/curve/xccycalibration.hpp`
and `dal-cpp/dal/curve/xccyjointcalibration.hpp`. The snippets below condense
their real call sequences; class and enum names, factory functions, and
include paths match the current source. See the citations in each example.

### Staged basis-only calibration

`xccy_curve_calibration` performs staged basis-only calibration against
supplied domestic and foreign curve blocks and prints fit diagnostics,
including the FX spot and maximum residual, plus elapsed time. See
[`dal-cpp/examples/xccy_curve_calibration/`](../../dal-cpp/examples/xccy_curve_calibration/)
for a runnable version.

```cpp
// from dal-cpp/examples/xccy_curve_calibration/xccy_curve_calibration.cpp
#include <dal/platform/platform.hpp>
#include <dal/platform/initall.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/timer.hpp>

using namespace Dal;

RegisterAll_::Init();
const Date_ today(2024, 1, 15);
XGLOBAL::SetEvaluationDate(today);

// Pre-calibrated domestic and foreign CurveBlock_ objects are inputs; only the
// basis-curve parameters are solved. The example builds them with the file-local
// MakeXccyBlock helper on top of NewDiscountPWLF.
CrossCurrencyCalibrationSpec_ spec;
spec.today_              = today;
spec.basisPair_          = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
spec.domesticCurveBlock_ = MakeXccyBlock("usd_ois", "USD", today, 0.02);
spec.foreignCurveBlock_  = MakeXccyBlock("eur_ois", "EUR", today, 0.01);
spec.fxSpot_             = 1.10;
spec.knotDates_          = {Date::AddMonths(today, 6), Date::AddMonths(today, 12), Date::AddMonths(today, 24),
                            Date::AddMonths(today, 60), Date::AddMonths(today, 120)};

// Each CrossCurrencySwap_ carries its market-implied par spread on the foreign
// leg; the example prototypes every swap against a quote market to derive them
// before calibration. maturities and marketSpreads are parallel vectors.
for (int i = 0; i < static_cast<int>(maturities.size()); ++i) {
    spec.instruments_.push_back(
        Handle_<CrossCurrencySwap_>(new CrossCurrencySwap_(MakeXccySwap(today, marketSpreads[i], maturities[i]))));
}

Timer_ timer;
timer.Reset();
const auto result = CalibrateCrossCurrencyMarket(spec);
const auto elapsedMs = timer.Elapsed<milliseconds>();
// result.diagnostics_ exposes residuals_, marketRates_, modelRates_, and the
// optional effJacobianInverse_ and jacobian_ matrices described above.
```

### Joint domestic, foreign, and basis calibration

`xccy_mtm_calibration` builds known domestic, foreign, and basis curves,
derives self-consistent quotes, supplies an immutable fixing snapshot for an
already-started MTM swap, and recovers five declaration blocks across the
domestic, foreign, and basis groups in one joint calibration. It prints the
convergence residual, Jacobian shape, block ranges, and parameter-recovery
errors. See
[`dal-cpp/examples/xccy_mtm_calibration/`](../../dal-cpp/examples/xccy_mtm_calibration/)
for a runnable version.

```cpp
// from dal-cpp/examples/xccy_mtm_calibration/xccy_mtm_calibration.cpp
#include <dal/platform/platform.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/xccyjointcalibration.hpp>
#include <dal/indice/fixingsnapshot.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/datetime.hpp>

using namespace Dal;

const Date_ today(2025, 1, 16);
const DateTime_ valuationTime(today, 9, 0);
const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));

// Already-started MTM swap: one historical fixing each for the domestic rate,
// foreign rate, and FX index. MarketFixingSnapshot_ is the immutable map the
// joint solver retains on the result.
MarketFixingSnapshot_::values_t observations;
observations["USD-JOINT-3M"][historicalFixing] = 0.040;
observations["EUR-JOINT-3M"][historicalFixing] = 0.030;
observations[FxIndexName(pair)][historicalFixing] = 1.20;
const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_(observations));

// domestic_, foreign_, and basis_ each carry declarations and instrument
// groups; the example builds them with five knots per curve and quotes the
// basis swaps off a known truth market.
JointXccyCalibrationSpec_ spec;
spec.valuationTime_      = valuationTime;
spec.pair_               = pair;
spec.collateralCurrency_ = pair.domestic_;
spec.fxSpot_             = 1.10;
spec.domestic_           = domestic.declaration_;
spec.foreign_            = foreign.declaration_;
spec.basis_              = basis;
spec.fixings_            = fixings;
spec.tolerance_          = 1.0e-10;
spec.initialGuess_       = 0.005;

const JointXccyCalibrationResult_ result = CalibrateJointXccyMarket(spec);
// result.converged_, jointMaxAbsResidual_, jacobianAtSolution_,
// parameterRanges_, and residualRanges_ cover every declaration block; the
// example cross-checks recovered parameters against the known truth.
```

### Pricing fixed, resettable, and MTM swaps

`xccy_reset_pricing` prices future fixed, resettable, and MTM swaps against
piecewise-constant discount, projection, and basis curves, validates their
reset and notional behavior, and prices an already-started MTM swap from an
immutable snapshot of historical domestic-rate, foreign-rate, and FX fixings.
See [`dal-cpp/examples/xccy_reset_pricing/`](../../dal-cpp/examples/xccy_reset_pricing/)
for a runnable version.

```cpp
// from dal-cpp/examples/xccy_reset_pricing/xccy_reset_pricing.cpp
#include <dal/platform/platform.hpp>
#include <dal/platform/initall.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/indice/fixingsnapshot.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/time/datetime.hpp>

using namespace Dal;

RegisterAll_::Init();
const Date_ today(2025, 1, 16);
const DateTime_ valuationTime(today, 9, 0);

// Config selects one of FIXED, RESETTABLE, or MARK_TO_MARKET notional handling;
// BuildXccyCashflowPlan returns the period and reset schedule the pricer walks.
const CrossCurrencySwapConfig_ config = Config(XccyNotionalMode_::Value_::MARK_TO_MARKET);
const XccyCashflowPlan_ plan = BuildXccyCashflowPlan(start, maturity, config);

// XccyMarketView_ bundles discount, forward, and basis curve slots plus FX
// spot. The example fills view.domestic_ and view.foreign_ from
// Tape::JointCurveBlock_<double> records built out of a CrossCurrencyMarket_.
XccyMarketView_<double> view;
view.valuationTime_      = market.ValuationTime();
view.pair_               = CurrencyPair_(market.DomesticCcy(), market.ForeignCcy());
view.collateralCurrency_ = market.CollateralCurrency();
view.fxSpot_             = market.FxSpot();
view.domestic_           = &domestic;
view.foreign_            = &foreign;
view.basis_              = market.BasisCurve();

// ResolveXccyNotionals fills the per-period domestic notionals and the
// mtmDeltas_ vector that only MARK_TO_MARKET produces; PriceXccyParSpread
// returns the par basis spread on the configured quoted leg. The example
// asserts the direct pricer agrees with CrossCurrencySwap_::Precompute.
const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_());
const XccyResolvedNotionals_<double> notionals = ResolveXccyNotionals<double>(plan, view, *fixings);
const double parSpread = PriceXccyParSpread<double>(plan, view, *fixings);
```

### Python

`dal-python/examples/007.xccy_joint_calibration.py` runs joint calibration
through the installed Python surface and prints convergence, matrix
dimensions, named ranges, and FX forwards.

## Performance Smoke Surface

`xccy_perf` emits 24 unique timing rows. They cover four pricing cases (future
fixed, resettable, and MTM plus started MTM), staged calibration including the
reset-aware analytic case, and joint calibration. Linux and Windows CI execute
the benchmark to completion, but it is not in the paired base/head regression
allowlist; this target is execution-smoke and reporting coverage rather than a
regression threshold gate.

## See Also

- [Yield-curve construction](yield_curve.md) — curve declarations and single or
  multi-curve calibration.
- [Yield-curve Jacobian](yield_curve_jacobian.md) — Jacobian layouts and
  inverse-Jacobian risk.
- [Underdetermined search](underdetermined_search.md) — exact and approximate
  solver behavior.
