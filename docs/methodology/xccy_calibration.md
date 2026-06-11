# Cross-Currency Calibration Methods

Documentation of the cross-currency basis calibration framework in `dal-cpp/dal/curve/xccymarket.hpp` and `dal-cpp/dal/curve/xccymarket.cpp`.

## Purpose

Cross-currency (XCCY) basis calibration fits a basis discount curve that captures the spread between two currencies' funding costs. Given:

- Pre-calibrated domestic and foreign curve blocks
- FX spot rate
- Cross-currency swap market quotes

The solver finds the piecewise-constant basis curve that reprices the input swaps while preferring smooth solutions via regularization.

## File Map

| File                                                          | Purpose                                                                        |
|---------------------------------------------------------------|--------------------------------------------------------------------------------|
| `dal-cpp/dal/curve/xccymarket.hpp`                            | `CrossCurrencyMarket_`, `CrossCurrencySwap_`, calibration spec/result types     |
| `dal-cpp/dal/curve/xccymarket.cpp`                            | Calibration implementation using `Underdetermined::Find/Approximate`           |
| `dal-cpp/tests/curve/test_xccymarket.cpp`                     | Unit tests for market, swap pricing, and calibration                           |

## Cross-Currency Market

### CrossCurrencyMarket_

Immutable container holding exactly one domestic/foreign currency pair:

```cpp
class CrossCurrencyMarket_ {
    CrossCurrencyMarket_(const Handle_<CurveBlock_>& domesticBlock,
                         const Handle_<CurveBlock_>& foreignBlock,
                         double fxSpot);
    // ...
};
```

**Key methods:**

| Method                                    | Returns                                                                 |
|-------------------------------------------|-------------------------------------------------------------------------|
| `DomesticDiscountCurve(collateral)`       | Discount curve for domestic currency                                    |
| `ForeignDiscountCurve(collateral)`        | Discount curve for foreign currency                                     |
| `DomesticForwardCurve(tenor, collateral)` | Forward curve for domestic LIBOR                                        |
| `ForeignForwardCurve(tenor, collateral)`  | Forward curve for foreign LIBOR                                         |
| `FxSpot()`                                | FX spot rate (domestic per foreign)                                     |
| `BasisDiscountFactor(from, to)`           | Basis adjustment factor (1.0 if no basis curve set)                     |
| `FxForward(from, to, collateral)`         | FX forward via covered interest parity                                  |

**FX forward parity:**

```
F(domestic/foreign) = S × DF_foreign(from, to) / (DF_domestic(from, to) × DF_basis(from, to))
```

The basis curve captures deviations from pure interest rate parity.

### Basis Curve Semantics

The basis curve is a `DiscountPWC_` (piecewise-constant forward rates) representing the cross-currency basis spread. It is:

- **Set internally** by calibration (private `SetBasisCurve` method)
- **Applied multiplicatively** to the domestic discount factor in FX forward calculations
- **Used in swap pricing** to convert foreign-currency PVs to domestic currency

## Cross-Currency Swap

### CrossCurrencySwap_

A two-leg interest rate swap exchanging floating payments in different currencies:

```cpp
CrossCurrencySwap_(tradeDate, start, maturity, marketRate, pair,
                   domesticNotional, foreignNotional,
                   domesticIndexConvention, domesticLegConvention,
                   foreignIndexConvention, foreignLegConvention,
                   convention);
```

**Leg structure:**

| Component           | Domestic Leg                    | Foreign Leg                           |
|---------------------|---------------------------------|---------------------------------------|
| Notional            | `domesticNotional`              | `foreignNotional`                     |
| Floating index      | Domestic LIBOR (e.g., SOFR)     | Foreign LIBOR (e.g., ESTR)            |
| Discount curve      | Domestic OIS                    | Foreign OIS                           |
| FX conversion       | None                            | Multiply by `FxSpot / BasisDF`        |
| Spread              | Usually 0                       | Usually the calibrated spread         |

**Par spread calculation:**

The par spread equates the PV of both legs. With `spreadOnForeignLeg = true`:

```
spread = (PV_domestic - PV_foreign_base) / Annuity_foreign_converted
```

Where `Annuity_foreign_converted` is the foreign leg annuity converted to domestic currency.

### Convention Options

`CrossCurrencyConvention_` controls swap mechanics:

| Field                    | Default | Meaning                                                    |
|--------------------------|---------|------------------------------------------------------------|
| `initialNotionalExchange_` | `true`  | Exchange notionals at trade start                          |
| `finalNotionalExchange_`   | `true`  | Re-exchange notionals at maturity                          |
| `spreadOnForeignLeg_`      | `true`  | Spread applied to foreign leg (vs. domestic)               |
| `resettableNotional_`      | `false` | Notional resets with FX (not implemented)                  |
| `markToMarketNotional_`    | `false` | MTM notional exchanges (not implemented)                   |

## Calibration

### CrossCurrencyCalibrationSpec_

Input specification for calibration:

```cpp
struct CrossCurrencyCalibrationSpec_ {
    CurrencyPair_ basisPair_;                    // e.g., USD/EUR
    Handle_<CurveBlock_> domesticCurveBlock_;    // Pre-calibrated USD curves
    Handle_<CurveBlock_> foreignCurveBlock_;     // Pre-calibrated EUR curves
    double fxSpot_;                              // e.g., 1.10 USD per EUR
    CollateralType_ fxForwardCollateral_;        // Usually OIS
    Vector_<Handle_<CrossCurrencySwap_>> instruments_;  // XCS quotes
    Vector_<Date_> knotDates_;                   // Basis curve knot dates
    double smoothingWeight_ = 1.0;               // Regularization strength
    double tolerance_ = 1.0e-10;                 // Per-instrument tolerance
    double fitTolerance_ = 1.0e-6;               // For approximate mode
    double initialGuess_ = 0.0;                  // Starting basis rate
    int maxEvaluations_ = 200;
    int maxRestarts_ = 20;
    CurveSolveMode_ solveMode_ = EXACT;          // or APPROXIMATE
};
```

### CrossCurrencyCalibrationResult_

Output from calibration:

```cpp
struct CrossCurrencyCalibrationResult_ {
    CrossCurrencyMarket_ market_;                // Market with calibrated basis
    std::map<CurrencyPair_, Handle_<DiscountCurve_>> basisCurves_;
    CrossCurrencyFxForwardCurve_ fxForwardCurve_;  // Calibrated FX forwards
    CrossCurrencyCalibrationDiagnostics_ diagnostics_;
};
```

**Diagnostics:**

| Field                  | Meaning                                                          |
|------------------------|------------------------------------------------------------------|
| `instrumentNames_`     | Names of input instruments                                       |
| `marketRates_`         | Observed market par spreads                                      |
| `modelRates_`          | Model-implied par spreads                                        |
| `residuals_`           | `modelRates - marketRates`                                       |
| `effJacobianInverse_`  | Effective Jacobian inverse (for risk/sensitivity)                |
| `maxAbsResidual_`      | Maximum absolute residual                                        |
| `rmsResidual_`         | Root mean square of residuals                                    |
| `usedApproximateFit_`  | True if approximate solver was used                              |

### Calibration Pipeline

```
┌─────────────────────────────────────────────────────────────────────────┐
│ CrossCurrencyCalibrationSpec_                                           │
│   ├── domesticCurveBlock_  ─────┐                                       │
│   ├── foreignCurveBlock_   ─────┤                                       │
│   ├── fxSpot_              ─────┤                                       │
│   ├── instruments_         ─────┤──► XccyCalibrationFunc_::F(x)         │
│   └── knotDates_           ─────┘         │                             │
│                                           ▼                             │
│                              Build basis curve from x                   │
│                              Price all XCS instruments                  │
│                              Return residuals                           │
└─────────────────────────────────────────────────────────────────────────┘
                                           │
                                           ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ Underdetermined::Find() / Approximate()                                 │
│   ├── Smoothness weights from WeightsPWC(knotDates)                     │
│   ├── Quasi-Newton iterations with backtracking                         │
│   └── Convergence when |residual| < tolerance                           │
└─────────────────────────────────────────────────────────────────────────┘
                                           │
                                           ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ CrossCurrencyCalibrationResult_                                         │
│   ├── market_ with calibrated basisCurve_                               │
│   ├── basisCurves_[pair] = calibrated DiscountPWC_                      │
│   ├── fxForwardCurve_ = FX forwards at knot dates                       │
│   └── diagnostics_ with repricing errors and Jacobian                   │
└─────────────────────────────────────────────────────────────────────────┘
```

### XccyCalibrationFunc_

The residual function passed to the underdetermined solver:

```cpp
class XccyCalibrationFunc_ : public Underdetermined::Function_ {
    Vector_<> F(const Vector_<>& x) const override {
        // x = basis rates at each knot date
        CrossCurrencyMarket_ market(domesticBlock_, foreignBlock_, fxSpot_);
        market.SetBasisCurve(BuildBasisCurve(domesticCcy_, knotDates_, x));

        Vector_<> result(instruments_.size());
        for (int i = 0; i < instruments_.size(); ++i)
            result[i] = (*rates_[i])(market) - marketRates_[i];
        return result;
    }
};
```

The solver adjusts the basis rates `x` to minimize residuals while maintaining smoothness.

### Solve Modes

**EXACT** (`Underdetermined::Find`):
- Finds basis curve where all scaled residuals are within `[-tolerance, +tolerance]`
- Uses smoothness weights to prefer smooth solutions among all exact fits
- Returns effective Jacobian inverse for risk calculations

**APPROXIMATE** (`Underdetermined::Approximate`):
- Finds basis curve minimizing `||residuals||²` subject to regularization
- Useful when exact fit is impossible (e.g., inconsistent quotes)
- Controlled by `fitTolerance_` parameter

## Example Usage

```cpp
const Date_ today(2024, 1, 15);
const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));

// Pre-calibrated curve blocks (from separate calibrations)
Handle_<CurveBlock_> usdBlock = CalibrateUSDCurves(today);
Handle_<CurveBlock_> eurBlock = CalibrateEURCurves(today);

// XCS market quotes
Vector_<Handle_<CrossCurrencySwap_>> instruments = {
    MakeXCS(today, Date::AddYears(today, 1), 0.0010),  // 1Y: 10bp
    MakeXCS(today, Date::AddYears(today, 2), 0.0015),  // 2Y: 15bp
    MakeXCS(today, Date::AddYears(today, 3), 0.0020),  // 3Y: 20bp
};

CrossCurrencyCalibrationSpec_ spec;
spec.basisPair_ = pair;
spec.domesticCurveBlock_ = usdBlock;
spec.foreignCurveBlock_ = eurBlock;
spec.fxSpot_ = 1.10;
spec.instruments_ = instruments;
spec.knotDates_ = {
    Date::AddYears(today, 1),
    Date::AddYears(today, 2),
    Date::AddYears(today, 3)
};
spec.smoothingWeight_ = 1.0;

const auto result = CalibrateCrossCurrencyMarket(spec);

// Use calibrated market for pricing
double fwdRate = result.market_.FxForward(Date::AddYears(today, 2));
double basisDf = result.market_.BasisDiscountFactor(today, Date::AddYears(today, 2));

// Check calibration quality
ASSERT_LT(result.diagnostics_.maxAbsResidual_, 1e-8);
```

## See Also

- `dal-cpp/dal/curve/yield_curve.md` — Single-currency yield curve framework
- `dal-cpp/dal/math/optimization/underdetermined.md` — Underdetermined solver details
- `dal-cpp/tests/curve/test_xccymarket.cpp` — Test coverage examples
