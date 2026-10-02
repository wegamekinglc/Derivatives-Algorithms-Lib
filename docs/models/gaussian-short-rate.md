# Gaussian Short Rate (GSR) model

`GSRModelData_` supplies a one-factor Gaussian short-rate model;
`MultiFactorGSRModelData_` supplies named factors in one currency. Both use the
time-dependent `g(t)` and `H(t)` parameterization from Thomas Hyer's
*Derivatives Algorithms*, Volume 1, sections 13.1–13.2. Functions are piecewise
constant on dated knots; a knot starts its new segment.
The model fits a supplied initial OIS discount curve exactly at its nodes;
between nodes it interpolates log discount factors linearly on an ACT/365 time
axis. The first curve node and both first volatility knots must equal the
valuation date.

## State, bonds, and discounting

The vector state follows `dX(t) = diag(g(t)) dW(t)`, with `X(0) = 0` and
Brownian correlation matrix `R`. Define

`C(t) = diag(g(t)) R diag(g(t))`, `B(t,T) = integral(t,T) H(u) du`,
`V(t) = integral(0,t) C(u) du`, and `m(t) = -integral(0,t) C(u) B(u,t) du`.

For the supplied discount curve `P0`, the conditional zero-coupon price is

`P(t,T) = P0(T)/P0(t) * exp(-B' (X-m) - B' V B/2)`.

For one factor these are scalar quantities and `R = 1`. Singular correlations
and zero-volatility factors are supported without adding a volatility floor.

This construction reprices `P0` and supports zero volatility. The model
generates one Gaussian increment per factor and positive event interval, in
time-major order and then input factor order. Each interval's
discount factor is the exact conditional expectation of the continuous-time
discount factor given its two endpoint states. Consequently the script engine
can discount payments and exercise values on its event grid with a stochastic
numeraire. LSM training and validation use the discount ratio of each path;
the pricing pass discounts at that same path's event numeraire. No extra
Gaussian bridge dimension is needed for payoffs determined by event states.
This discount rule applies to the standalone rate model. A coupled asset model
needs discounting conditioned on its joint states.

The model reports AAD sensitivities to every non-anchor OIS log-discount node,
every non-anchor projection log-discount node, and every `g` and `H` segment.
The labels are `logdf:OIS:YYYY-MM-DD`, `logdf:<tenor>:YYYY-MM-DD`,
`g:YYYY-MM-DD`, and `H:YYYY-MM-DD` for the one-factor factory. Multi-factor
labels are `g:<factor>:YYYY-MM-DD` and `H:<factor>:YYYY-MM-DD`; correlations
are passive inputs. These are **model input** derivatives;
they are not calibrated quote DV01s. Curve snapshots detach the inputs from
the source curve's calibration graph.
At the degenerate `g = 0` boundary, AAD uses a zero subgradient for the
Gaussian step size, so a one-sided volatility sensitivity of a kinked payoff
should be evaluated with a positive bump instead.

## Curves and observations

Create `GSRCurveData_` from dated OIS log discount factors and, optionally,
one row of projection log discount factors per tenor. The C++ helper
`NewGSRCurveDataFromYieldCurve` snapshots a `YieldCurve_` at requested dates;
Python exposes `GSRCurveDataFromYieldCurve_New` and Excel exposes
`GSRCurveDataFromCurveBlock_New`. The snapshot's date range must cover all
exercise, fixing, accrual, and payment dates; intermediate dates are
interpolated. Include dates as nodes when their exact source-curve discount
factors must be preserved. The model rejects observations beyond the last
curve node rather than extrapolating them.

Pair the curve with `GSRVolData_` in `GSRModelData_`. The C++ public factories
are `NewGSRCurveData`, `NewGSRVolData`, and `NewGSRModelData`. Python and Excel
expose `GSRCurveData_New`, `GSRVolData_New`, and `GSRModelData_New`.

For multiple factors, use `NewMultiFactorGSRVolData(name, settings)` and
`NewMultiFactorGSRModelData(name, curve, vol)` in dal-public. The settings contain
ordered factor names, separate g/H knot dates, matrices with factor rows and
date columns, and a symmetric, unit-diagonal PSD correlation matrix. Every g
value must be nonnegative; multi-factor H values may be signed. Factor names
must be nonempty and unique ignoring case. The one-factor factory retains its
positive-H requirement and existing archive format.

Python and Excel expose `MultiFactorGSRVolData_New` and
`MultiFactorGSRModelData_New`. For example, with an existing curve snapshot:

```python
vol = dal.MultiFactorGSRVolData_New(
    "vol", ["level", "slope"], [today], dal.DoubleMatrix_([[0.02], [0.01]]),
    [today], dal.DoubleMatrix_([[1.0], [-0.4]]),
    dal.DoubleMatrix_([[1.0, 0.3], [0.3, 1.0]]),
)
model = dal.MultiFactorGSRModelData_New("rates", curve, vol)
```

Both factories share the [rate kernel](../../dal-cpp/dal/model/gsr.hpp).
Initialization prepares transition and observation coefficients; path generation
does not rebuild schedules or integrate dated parameters. Brownian bridge
sampling applies independently to each factor before the covariance transform.

Script `FIX` supports these single-currency observations:

| Script name             | Value at the event date                                   |
|-------------------------|-----------------------------------------------------------|
| `IR[USD,DF,2028-09-28]` | OIS bond price to the dated maturity                      |
| `IR[USD,LIBOR_3M_LCH]`  | Forward Libor using the 3M projection row, if supplied    |
| `IR[USD,SWAP,5Y]`       | Par swap rate using currency fixed and floating schedules |

Libor and swap forwards fall back to the OIS curve when their projection tenor
is absent. Projection curves enter through the deterministic initial forward
spread; the stochastic factors and OIS bond ratios drive future changes.
The parser also accepts the canonical names `IR[DF]:USD,2028-09-28`,
`IR:USD,LIBOR_3M_LCH`, and `IR:USD,5Y`.

## Bermudan products

Use the script's existing `EXERCISE` statement on each permitted date and set
`regressionFeatures_` to a rate observation. For a bond call, an exercise
event can be:

```text
EXERCISE MAX(FIX(IR[USD,DF,2028-09-28]) - 0.94, 0)
```

For a **cash-settled** payer swaption, the event payoff is
`MAX((FIX(IR[USD,SWAP,5Y]) - K) * A(t), 0)`, where `A(t)` is the sum of fixed
accrual fractions times `FIX(IR[USD,DF,<payment date>])`. Build its fixed
schedule with the currency's swap convention, and use
`IR[USD,SWAP,5Y]` as the regression feature. This representation pays the
annuity value at exercise. It does not create a delivered swap or future
floating and fixed cashflows after exercise.

GSR accepts a valuation date matching the curve's evaluation date. Historical
Libor fixings can be supplied through the existing fixing snapshot path;
future observations come from simulated model paths. For multiple factors,
choose regression observations that capture the exercise decision; a single
swap rate need not capture every relevant factor. Cross-currency dynamics are
separate features. The existing
[equity-rate Hybrid](hybrid-model.md) accepts the one-factor GSR component.

## European option pricing

`PriceGSREuropeanOption` in [dal-public](../../dal-public/src/gsr.hpp) prices
unit-notional bond calls/puts, caplets/floorlets and physically settled European
swaptions. Python exposes `GSR_EuropeanOptionPrice`; Excel exposes
`GSR.EUROPEANOPTIONPRICE`. `CALL` means bond call, caplet or payer swaption;
`PUT` means bond put, floorlet or receiver swaption.

Dates and accrual fractions are explicit. A floating coupon specifies fixing,
accrual start/end, payment, index accrual, coupon accrual and projection tenor.
Require `expiry <= fixing <= start < end <= payment`; a caplet's expiry is its
fixing date. Fixed coupons specify payment and accrual. Curves must cover every
date. Negative rates and strikes, zero volatility and singular correlations are
supported. Already fixed coupons and cash-settlement conventions are excluded.

Bond options and caplets use analytic Black expectations of positive bond ratios.
Payment lag changes the forward under the payment measure; the pricer includes
this adjustment, including future fixing dates in a swaption's floating leg.
A swaption is the positive part of floating PV minus strike times fixed annuity,
with the sign reversed for receivers. Under the expiry-forward measure this is
a sum of exponentials of normal variables. The pricer finds all exercise intervals
and integrates one normal direction analytically. Gaussian quadrature handles the
remaining directions, supporting at most three effective directions for swaptions.
Analytic bond options and caplets have no factor-count restriction.

`GSRPricingSettings_` selects the quadrature order (2–64, default 16) and refinement.
With refinement enabled, the result uses twice the requested outer order and
reports the price difference as `numericalError_`. This is an estimate, not a
guaranteed error bound; check stability with a larger order when accuracy matters.
The one-direction integration also reports its negligible Gaussian-tail bound.

## Gaussian volatility calibration

`CalibrateGSRVolatility` fits selected entries of a `MultiFactorGSRModelData_`
g matrix. Curves, H, correlations and other g entries remain fixed. Each quote
contains an option, nonnegative price, positive price error scale and unique name.
Each selected parameter contains zero-based factor/knot indices and finite bounds
`0 <= lower < upper`; the initial value must lie inside them.

The bounded damped Gauss–Newton solver minimizes

`sum((modelPrice - quotePrice)/priceScale)^2 + priorWeight * sum(dx^2)`

plus `smoothingWeight * sum((dx[j] - dx[j-1])^2 / knotGapYears)` for adjacent
g buckets touched by calibration. Here `dx` is the change from the initial g,
divided by `parameterScale` (default 0.01). Unselected neighbors have zero change.
The prior and smoothing weights default to zero. These explicit penalties follow
Hyer's section 7.4 approach of choosing among underdetermined solutions through
a metric on parameter changes.

Calibration uses a fixed quadrature order (default 16, allowed 2–32) and checks the
fitted model with finer integration. Results own a new model and report three
separate checks: optimizer convergence, prices within quote scales, and numerical
errors within `numericalErrorFraction * priceScale` (default fraction 0.25).
Failure returns diagnostics and a fitted candidate; it does not imply a successful
calibration. Inspect the termination reason, bounds, residuals and all three checks.

`quoteJacobian_` has quote rows and selected-parameter columns, with derivatives
of the fixed-order calibration price with respect to g. Rank and condition diagnostics use price-scaled
residuals and normalized parameters; the condition estimate is a pivoted QR
diagonal ratio, with infinity for deficient rank. It is an identifiability
diagnostic, not a calibrated quote sensitivity. These pricing/calibration APIs
use doubles; the script Monte Carlo API still supplies model-input AAD.

For example, with an existing multi-factor model:

```python
option = dal.GSRBondOption_(expiry, maturity, 0.97, "CALL")
quote = dal.GSRCalibrationQuote_("1Y bond", option, market_price, 1e-6)
parameter = dal.GSRCalibrationParameter_(0, 0, 0.0, 0.10)
settings = dal.GSRCalibrationSettings_()
settings.prior_weight = 0.01
result = dal.Calibrate_GSRVolatility(model, [quote], [parameter], settings)
assert result.converged and result.fit_within_tolerance
assert result.numerical_validation_passed
price = dal.GSR_EuropeanOptionPrice(result.model, option).price
```

Excel builds coupon/option handles with `GSRFIXEDCOUPON.NEW`,
`GSRFLOATINGCOUPON.NEW`, `GSRBONDOPTION.NEW`, `GSRCAPLET.NEW` and `GSRSWAPTION.NEW`.
Use `GSRCALIBRATIONQUOTE.NEW`, `CALIBRATE.GSRVOLATILITY`, and
`GSRCALIBRATIONRESULT.GET` to fit and inspect results; parameter tables have
columns factor, knot, lower, upper. `GSRCALIBRATIONRESULT.GET.MODEL` returns a
model usable for both option pricing and Monte Carlo. Worksheet value/result
handles are process-local; fitted model data retains archive support.

## C++ and Python examples

The C++ example in [`dal-cpp/examples/gsr_swap_swaption/`](../../dal-cpp/examples/gsr_swap_swaption/)
and [Python example](../../dal-python/examples/014.gsr_swap_swaption.py) build
an input USD OIS `YieldCurve_`, snapshot it into `GSRCurveData_`, and supply
piecewise constant `g = 0.02` and `H = 1.0`. Their curve tables compare input
discount factors with GSR bond prices at every non-anchor node. This is an
exact initial-curve fit; the example does not calibrate the volatility inputs.
The C++ example constructs core data types and calls `Script::MCSimulation`
directly; it builds with `DAL_BUILD_PUBLIC=OFF`.

Both examples price a standard one-year forward payer swap starting on
2027-09-28, with unit notional and a 3% fixed rate. Its floating leg fixes two
business days before each quarter starts and pays ACT/360 Libor coupons on
2027-12-28, 2028-03-28, 2028-06-28, and 2028-09-28. Its fixed leg pays
semiannual 30/360 coupons on 2028-03-28 and 2028-09-28. The swap script has
four payment events; it does not settle the entire swap at its start.

The examples also price a cash-settled European payer swaption expiring on
2027-09-28. Its exercise payoff uses the matching one-year swap rate and fixed
annuity. The examples print each script with `DebugScriptProductTree` in C++ or
`Product_DebugTree` in Python. Their tables show the input and GSR discount
factors, volatility, and product PVs. For the standard swap, they compare GSR
Monte Carlo PV with `PriceRateTrade`/`PriceRateTrades` using the original
`YieldCurve_` discount curve for both discounting and 3M forwarding. The
static price is also checked against
`P(0,start) - P(0,maturity) - K * sum(fixed DCF * P(0,payment))`.
The GSR and static swap prices agree within a Monte Carlo tolerance of
`2e-4` per unit notional.

The [third-party comparison benchmark](../../dal-python/benchmarks/README.md#gsr-swap-and-swaption)
reuses this curve and standard swap to compare static IRS pricing with
QuantLib and rateslib, both with reused cashflows and fresh cashflows. It also
compares the GSR European swaption with QuantLib's GSR Sobol Monte Carlo and
Gaussian1d numerical integration. Rateslib has no
corresponding GSR short-rate swaption engine, so that benchmark reports it as
unsupported.

After building the Release-linux workspace, run:

```bash
build/Release-linux/dal-cpp/examples/gsr_swap_swaption/gsr_swap_swaption
DAL_PY_SITE=$(dal-python/.venv/bin/python -c 'import site; print(site.getsitepackages()[0])')
PYTHONPATH="build/Release-linux/dal-python:$DAL_PY_SITE" dal-python/.venv/bin/python -S dal-python/examples/014.gsr_swap_swaption.py
```

To measure the path kernels, enable benchmarks and build the target:

```bash
cmake --preset=Release-linux -DDAL_CPP_BUILD_BENCHMARKS=ON
cmake --build build/Release-linux --target script_mc_perf
build/Release-linux/dal-cpp/benchmarks/script_mc_perf/script_mc_perf --gsr
build/Release-linux/dal-cpp/benchmarks/script_mc_perf/script_mc_perf --gsr-european
```
