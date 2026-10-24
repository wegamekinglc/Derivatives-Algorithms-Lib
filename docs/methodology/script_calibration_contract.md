# Script calibration contracts and public interfaces

The core headers `dal/script/instrument.hpp` and `dal/calibration/protocol.hpp`
provide immutable contract data, typed parameter binding and ordered calibration
observations. `dal/script/calibrationpreparation.hpp` adds structural preparation
for Deposit, bounded deterministic curve families and Gaussian BondOption,
Caplet and Swaption contracts.
Existing Product, curve and GSR entry points keep their own behavior.
The separate core Deposit adapter in
`dal/curve/depositscript.hpp` supports one-node curve fitting with Brent.

A `ScriptInstrumentDefinition_` owns the exact source rows, fixed-date header,
numeric parameter declarations and measures. Source numeric definitions are the
only defaults. The header references N and PAY rather than duplicating notional
or cashflows. The initial schema uses a single currency, OIS collateral, AtEvent
settlement and explicit discount routing. N and K are bindable; Deposit ALPHA is
fixed. P0 parameter roles are Notional, Coupon, Strike and Accrual.
Caplet/Swaption add RateStrike for a finite signed K in decimal Rate units;
the original Strike role remains a strictly positive BondPrice.
Bounded curve coupons can use signed decimal BasisSpread with the existing
Coupon role and a matching ParAffineSpread measure; no basis-point conversion
is implicit. Deposit's fixed ALPHA and Rate unit rules remain unchanged.
Dates use the core Date_ range, 1970-01-01 through 2149-06-05; out-of-range
or invalid Gregorian dates fail explicitly.

`NewScriptCalibrationInstrument` copies and validates updates over all defaults.
`BindCalibrationInstrumentParameters` returns a new contract, starting from the
current complete binding; omitted values retain their current values. Clones
preserve identities and share only immutable definitions. Changes to source,
header, dates, declarations or measures require a new definition.

```cpp
const auto rate = Dal::CalibrationValueUnit_::Parse("Rate");
const auto rebound = Dal::BindCalibrationInstrumentParameters(
    instrument, {{{"K", 0.03, rate}}, {}});
// instrument keeps its original fixed coupon.
```

An instrument's `Product()` builds a fresh disposable script tree. It applies
typed effective constants after preprocessing numeric definitions and before AST
construction, constant folding or historical replay; it never edits event text.
The returned raw core tree is not an admitted calibration execution plan. Use
`PrepareCalibrationInstrument` for receiver, complete template, context and
execution-method admission.

## Structural preparation

Preparation reads the whole bound AST before constant folding or historical
replay. PAY must be the sole PAYS receiver. Its only ordinary assignment is
literal-zero initialization before any payment. A payment in either branch, or
in history, prevents subsequent receiver resets. Vector output, extra receivers,
ON settlement and unrecognized control flow fail explicitly. Native fast
matchers additionally reject fees or other unmatched cashflows. Explicit general
Price MC has its own bounded scalar admission described below. Payment
dates must match the admitted template, including the Caplet payment lag
described below. The bare Product's last-variable fallback remains available
through its existing entry.

The admitted Deposit has `-N` at S and `N*(1+K*ALPHA)` at T, with valuation at S.
The BondOption has one payment at E, `N*MAX(FIX(IR[CCY,DF,T])-K,0)` or its put
counterpart, with valuation < E < T. All dates are concrete, the currency matches
the header and context, and settlement is AtEvent under same-currency OIS
collateral. Product names and fingerprints never bypass these structural checks.

For non-XCCY preparation, `CalibrationPreparationContext_` requires an explicit
valuation date, fixing snapshot, currency, discount route and `GSRCurveData_` snapshot. Gaussian
preparation additionally requires an exact one- or multi-factor GSR model with
the same curve. This fixes ACT/365 model time and the native Gaussian numeraire
contract. The hard option requires zero smoothing. A different stochastic model
cannot receive the Gaussian descriptor by providing a matching name.

The returned immutable plan owns its instrument, cashflow metadata, disposable
descriptor, capability and compiler/context/plan identities. A contingent
cashflow has no numeric `amount_`; its metadata retains payment/fixing dates and
source coordinates. A fast descriptor carries the matched financial structure.
Deposit amounts come from evaluating the original PAYS expressions. The plan
retains no independently editable native product or mutable market object.

Source, header, effective bindings, actual curve/model data, exact fixing
timestamps and values, today policy, convention state and smoothing contribute
to identity. Sampling contributes when MC is selected. Call `RequireCompatible`
with the current instrument and context before reusing a prepared artifact;
changes require preparation again. No shared descriptor cache is maintained.
Rate-option, curve-family and general Price MC contexts additionally track the actual Libor day basis; this
extension preserves the original Deposit/BondOption identity components.

## Native Deposit import and candidate curves

`NewDepositScriptInstrument` imports a native `Deposit_` once. Import settings
require an instrument ID, currency and explicit discount route. Positive lender
notional defaults to one because the native Deposit has no notional or payment
direction. The exporter resolves the native adjusted start/end and day-count
context once, then writes N, K, fixed ALPHA and concrete principal/redemption
PAYS into the definition. K is the native MarketRate at import. Later quotes
are independent; an explicit K=q update creates a new binding.

The import requires trade=start=valuation and an adjusted start equal to that
date. Forecast must equal discount under OIS collateral. Projection curves,
spot starts, other collateral and reversed payment direction are outside this
template. Native date adjustment at maturity is supported; both executors use
the exported date and accrual. Native interfaces retain their broader existing
conventions.

`PreparedDepositCurve_` derives its descriptor and coefficient views through
the shared preparation API. It caches ACT/365F log-linear weights for a fixed,
anchored `GSRCurveData_` node layout covering maturity. Each `Evaluate` refreshes
discounts, Price(K), A/B, ParRate and analytic raw log-DF node derivatives;
the rate value reuses `Tape::ForwardRateFromDf` with the source accrual. No
candidate rebuilds a schedule or reads a native product. Candidate identity
includes the original plan and new curve content. Date/layout/currency changes,
projection rows, smoothing or other convention states require a new plan or
fail admission. Prepare this deterministic adapter without a model.

```cpp
const auto instrument = Dal::NewDepositScriptInstrument(
    nativeDeposit, {"deposit-1y", Dal::Ccy_("USD"), "USD.OIS", 100.0});
const Dal::PreparedDepositCurve_ plan(instrument, curveContext);
const auto quote = Dal::NewCalibrationMarketQuote(
    "deposit-rate", 0.035, Dal::CalibrationValueUnit_::Parse("Rate"));
const auto observation = Dal::NewCalibrationObservation(
    "deposit-fit", instrument, "ParRate", quote, 1e-10);
const auto fit = Dal::FitDepositCurveNode(
    plan, observation, curveContext.curve_, 1, -0.1, 0.05, 64);
```

The fit changes one non-anchor log-DF coordinate; other nodes stay fixed. It
uses a ParRate observation for exactly the prepared binding, and accepts only
`abs(rawResidual/errorScale)<1` inside the explicit bracket/evaluation budget.
The result retains the curve, raw/scaled residuals, native partials, evaluation
count and shared observation/quote axes. Unbracketed roots and exhausted budgets
throw owning Solve reports with the last validated curve. This is a narrow
scalar adapter, with no general basket, approximate objective or quote-risk API.

To reprice, construct a matching Gaussian model directly from `fit.curve_` and
pass the original instrument to `PrepareCalibrationMc`. The original coupon
usually gives nonzero PV at the fitted curve. Price and ParRate validation use
their own amount/rate scales. Same binding, currency, discount route, ACT/365F
time, log-linear interpolation and native Gaussian numeraire are required;
other curve parameterizations need an explicit snapshot Jacobian. Zero-g
agreement is only an additional deterministic check in this matched domain.

## Gaussian BondOption import and selected-g fitting

The core `dal/model/gsrbondoption.hpp` API imports a native `BondOption_` once
with an instrument ID, currency, discount route and explicit positive notional.
`NewBondOptionScriptInstrument` writes expiry, observed bond maturity, strike,
direction and N into the immutable source/header. Binding, persistence, fast
Price and MC/AAD use that contract; no native option is retained as another
pricing authority.

The admitted template has one hard Gaussian payment at expiry under the native
one-factor or multi-factor GSR numeraire, a matching curve snapshot and `AtEvent`
settlement. Extra fees, delayed payment, fixing-dependent strike, SLV and positive
smoothing fail admission. `PrepareCalibrationInstrument` derives the complete
descriptor. `PriceGaussianBondOption` validates plan compatibility and calls the
native Gaussian kernel with an ephemeral descriptor-derived option, applying N
once. Numerical kernel failures carry an owning Evaluate report, original cause
and instrument/measure IDs through `CalibrationError_`.

`CalibrateGaussianBondOptions` accepts native MultiFactor GSR, immutable Price
observations and selected g coordinates. It uses bounded Gauss-Newton finite
differences with the existing parameter scaling, bounds, prior and smoothing
meanings. Every row requires the fast template; fitting does not execute MC.

```cpp
const auto instrument = Dal::NewBondOptionScriptInstrument(
    nativeOption, {"bond-option", Dal::Ccy_("USD"), "USD.OIS", 100.0});
const auto quote = Dal::NewCalibrationMarketQuote(
    "option-price", marketPrice, Dal::CalibrationValueUnit_::Parse("Price:USD"));
const auto observation = Dal::NewCalibrationObservation(
    "option-fit", instrument, "Price", quote, 1e-6);
const auto result = Dal::CalibrateGaussianBondOptions(
    modelContext, {observation}, {{0, 0, 0.0, 0.1}});
```

The result owns the fitted model, raw/scaled data residuals, observation/quote
axes, parameter labels, final plan identities and actual model/preparation/Price
call counts. Its common report carries axes/errors; numeric metrics remain on
the domain result. `quoteJacobian_` is finite raw dPrice/dg at fixed bindings,
excludes penalty rows and is not calibrated quote risk. Unit restoration avoids
an overflowing scale ratio; unrepresentable partials raise an owning numerical
failure. `oneSidedColumns_` identifies bound-truncated FD stencils. A selected
zero g or degenerate payoff sets `nativeJacobianAvailable_=false` and leaves
the raw Jacobian empty while double Price can remain available.

After `StepStalled`, the adapter rechecks the original projected-gradient
criterion at the final accepted point, including penalties. Convergence,
per-row fit tolerance and the fast backend's numerical indicator are distinct
decisions; this adapter preserves legacy solver behavior.

For independent repricing, copy the resolved context, set its model to
`Handle_<ModelData_>(result.model_)`, and call `PrepareCalibrationMc` on the
original instrument with explicit sampling/AAD settings. `ValidateCalibrationMc`
compares independent validation values and native gradients with a fast
reference, including FD refinement indicators. This statistical validation is
separate from fitting and is not a strict probability or floating-point bound.
The adapter fits BondOption only. Caplet/Swaption Price support is described
below; general-script fitting, SLV and public/Python/Excel importers remain
outside these core APIs.

## Gaussian Caplet and Swaption Price

The core `dal/model/gsrrateoption.hpp` APIs `NewCapletScriptInstrument` and
`NewSwaptionScriptInstrument` import native options once. Import settings require
an instrument ID, currency, explicit discount route and positive notional.
`PriceGaussianRateOption` prices the prepared Price plan using the native double
Black/quadrature kernels. These APIs add no script Caplet/Swaption calibrator.

Both templates declare exactly bindable N and K: N is positive in the contract's
Price unit, and K has role RateStrike, type Real and unit Rate. Zero and negative
finite strikes are valid declarations and rebindings, subject to declared bounds
and the evaluator's numerical domain. Coupon accruals are positive finite source
literals, rather than an extra ALPHA parameter. The complete bound AST is the
authority for every derived descriptor; no imported native option is retained.
Extra arithmetic, parameters, cashflows or control flow fail full matching.

The importers encode Caplet coupon accruals and Swaption fixed/floating coupon
accruals as single plain-decimal event literals. Scientific notation from numeric
formatting is expanded without discarding significant digits (for example,
`1e-7` is written as `0.0000001`); normal positive doubles retain their exact
value when parsed. Already plain-decimal accrual text is preserved, along with
source rows, identities and portable JSON for existing successful imports at
unchanged inputs, bindings and context. N/K definition formatting, general
`NumberText`/canonical JSON and the event DSL keep their existing contracts.

Subnormal accruals follow the existing decimal parser's underflow behavior and
may be rejected. Import and constant fidelity at normal extremes do not imply
successful native preparation or pricing.

The Caplet pays `N*(couponAlpha*MAX(L(E;S,T)-K,0))` at P; reversing the difference
gives a floor. Its source explicitly fixes the Libor observation at E with a
concrete start S. Admission requires valuation < E <= S < T <= P and a canonical
`LIBOR_3M_LCH` or `LIBOR_6M_LCH` index. T is the native nominal 3M/6M maturity,
without business-day adjustment, and index accrual uses the currency's actual
Libor day basis. Import rejects mismatched native end dates or index accruals.
The coupon accrual may differ from index accrual. Payment lag P > T is supported
by the native payment-measure kernel; it is not replaced by expiry discounting.

The European physical Swaption pays `N*MAX(floatingPV-K*annuity,0)` at exercise
E, reversed for receiver. Its floating PV is the sum of literal coupon accrual
times exercise-date Libor times DF(index end); its annuity is the sum of literal
fixed accrual times DF(fixed payment). Both schedules must be nonempty, with
positive finite accruals. Native floating fixings must lie between E and index
start, use the same nominal 3M/6M conventions, and pay at index end. Payment=end
makes native future-fixing convexity zero, so the exercise-date conditional PV
mapping is exact. The exported source retains this mapping, rather than unused
original future-fixing dates. Floating payment lag and cash settlement are
outside the admitted template.

Preparation requires a matching frozen single discount/forecast curve, exact
one- or multi-factor GSR, same-currency OIS/AtEvent header, zero smoothing and
curve coverage of all payments. Projection curves, SLV, conditional/early
exercise and unsupported source shapes fail admission. Swaption quadrature
supports at most three effective Gaussian directions; this is an effective-rank
limit, so a rank-one four-factor model can be admitted. Preparation reuses the
native domain/rank check. Model, convention, source or binding changes require
a new plan, including when MC is selected.

FastGaussian supports native finite differences or no derivatives; it does not
provide fast AAD. Explicitly opted-in MC evaluates the original bound Price
program with the existing tree/compiled engines. Hard-payoff MC/AAD admission
additionally requires finite positive rate exposure and positive native option
expectation, along with the shared selected-g restrictions. Zero volatility,
zero exposure and identically zero payoffs can retain double pricing while
failing AAD admission with DerivativeUnavailable. Independent validation still
has to satisfy the frozen value/gradient and precision budgets; pricing support
does not establish MC calibration convergence or sufficient sampling precision.
Numerical failures at the fast Price boundary carry CalibrationNumericalFailure,
the original cause, instrument/measure IDs and an owning Evaluate report.

## Affine observables and methods

For Deposit, preparation proves K is structurally affine and extracts per-payment
A/B coefficients from the same AST. Nonlinear dependence and K-dependent branch
conditions are rejected. Price remains the discounted original cashflows.
`Coefficients(discounts, discountDerivatives)` accepts one discount per cashflow
and an optional cashflow-by-native-parameter matrix, returning separate A, B,
dA and dB. Contract parameters such as N and ALPHA are fixed in these native
partials. The caller supplies discounts and partials from the compatible context.

```cpp
const auto plan = Dal::PrepareCalibrationInstrument(instrument, "ParRate", context);
const auto ab = plan.Coefficients({1.0, maturityDiscount}, discountPartials);
const auto par = plan.AffineMeasure(ab);
const double price = plan.Price({1.0, maturityDiscount});
```

The Scalar observable is $m=-B/A$, with independent coefficient partials
$dm=-dB/A+(B/A)(dA/A)$. Every evaluation checks the declared annuity sign and
minimum absolute magnitude, dimensions and finite outputs. ParRate and Price
have different units and residuals. The bounded curve-family path also supports
BasisSpread/ParAffineSpread; scalar-only programs have no shared Price capability.

`AutoFast` and `RequireFast` select an admitted deterministic curve or Gaussian
option descriptor. `GenericDeterministic` admits Deposit and bounded curve-family cashflows; it cannot
replace a stochastic option expectation with a forward payoff. Deposit exposes
analytic coefficient partials. Gaussian descriptor admission supports native
finite differences or no derivatives; requesting AAD fails rather than changing
the derivative method.

`ScriptMC` admission requires explicit calibration opt-in, a matching Gaussian
context and explicit sampling. Existing `irn` Price admission retains derivative
None. Executable `mrg32` admission also supports Deposit ParRate and native AAD.
General scalar Price admission requires executable `mrg32` and an explicit
`ScriptMC` request. Unknown scripts never switch silently to MC.

## Native curve instrument calibration

`dal/curve/curveinstrumentcalibration.hpp` adds `CalibrateCurveInstruments` and
`PreparedCurveInstruments_`. The spec separates `geometry_`, explicit `market_`
routes, frozen `valuation_`, ordered `observations_` and `solver_`. An explicit
valuation date and fixing snapshot are required. Preparation admits the shared
Deposit or bounded curve-family cashflows once; candidates refresh native curves and numeric measures
without parsing source or importing native products again. Gaussian options
continue through their separate model adapter.

The provider uses native discount/projection curves, collateral and tenor keys,
and an optional passive base. Route IDs are case-sensitive. Missing routes,
currency/collateral conflicts and unsupported requests fail without a global
market lookup. Ordered multi-curve stages inherit prior curves by key; explicit
stage bindings win; all stages share currency and valuation. Projection stages
inherit their discount base when one is not supplied. Deposit cashflows alone do not constrain projection
parameters; the bounded family adapters below supply explicit live projection dependencies.

LOG_DISCOUNT requires an explicit first knot at today and excludes its fixed
zero log-DF anchor from the free vector. ZERO_RATE takes future input knots
and excludes its implicit anchor. PWC columns are right forwards; PWLF columns
are left/right forwards interleaved at each knot. INPUT preserves the submitted
layout; INSTRUMENTS/AUGMENTED resolve sorted unique pillars while keeping
observation and distinct-quote axes in their original first-use order. Pillars
and native nodes must cover each admitted cashflow dependency.

The initial native vector theta0 stays fixed. Optional positive parameter scales
define `theta=theta0+S*z`; default S is identity. `nativeWeights_` stores the
existing smoothing metric W_theta, and `weights_` stores `S' W_theta S`.
EXACT accepts finite outputs with scaled infinity norm <=1. APPROXIMATE requires
explicit finite positive `fitWeight_` (lambda) and `fitNormTolerance_` (eta).
Each local problem uses the prior displacement and lambda-weighted squared
scaled residuals; acceptance is scaled L2<=eta. It may accept the initial point
and does not certify an objective minimum. Scales never determine lambda.

The new `Underdetermined::Fit` regularized local solve uses dense Cholesky and
requests a fresh Jacobian each iteration. It does not reuse the legacy CG
absolute residual floor or Broyden/secant refresh schedule. These numerical
choices do not promise the legacy floating-point trajectory; the
[underdetermined search note](underdetermined_search.md) describes the original
`Find`/`Approximate` paths, which retain their existing algorithms.

FD is the default; explicitly requested AAD uses the same cashflows and native
curve representation, including all log-DF schemes. Unsupported derivative or
MC requests fail. Solve FD bumps z; independent `Evaluate(theta)` FD bumps native
theta, using `differenceStep_` in the respective coordinates. `quoteJacobian_`
is the raw model-measure derivative with respect to native theta, not quote risk.
`computeQuoteJacobian_` optionally requests it after a successful fit. Solver
evaluation/gradient counters include attempted FD work. A final value refresh
and optional Jacobian diagnostics have separate counters and do not consume
the fit budget. Initial-fit results report derivative None unless diagnostics run.

Failures throw `CalibrationError_` with owned axes and a
`CurveInstrumentCalibrationCandidate_` whose `Result()` retains the last valid
candidate, coordinates, metric, norms, settings and termination reason. Plan,
candidate and objective identities distinguish frozen geometry, bindings,
history, routes and numeric state. These are session objects; quote-risk and
calibration-session persistence remain unavailable.

`ImportLegacyCurveObjective` records the homogeneous mapping
`lambda=M*(tau/f)^2`, `eta=f`, with S=identity. An unrepresentable lambda is
reported as an empty optional. `CalibrateLegacyCurve` still invokes the original
native spec/options and preserves its defaults, strict EXACT boundary and
APPROXIMATE last-point behavior. Algebraic mapping does not promise the new
driver's floating-point trajectory. This entry adds no FX/XCCY instrument
admission, generic FIX lowering, box bounds, joint solve or public bindings.

## Deterministic FRA, swap, basis and custom cashflows

The core `dal/curve/curvescript.hpp` provides one-time
`NewFraScriptInstrument`, `NewSwapScriptInstrument`,
`NewPeriodForwardOisScriptInstrument` and `NewBasisSwapScriptInstrument` import.
Settings reuse the instrument ID, currency, explicit discount route, positive
notional and annuity floor from Deposit import. The resolved native schedule
becomes concrete source dates, accruals and payments. No native product remains
as a second pricing authority; future candidate values come from the bound source.

- FRA requires an explicit payment date. Arrears payment can be at or after the
  accrual end. Advance payment must be at the start and retains the native
  `alpha*(K-L)/(1+alpha*L)` settlement amount. Intermediate settlement and
  Future/STIR convexity or daily settlement are unsupported.
- IRS imports resolved fixed and floating periods, including supported stubs,
  fixing lags and actual delayed payments. Each coupon discounts at its own
  payment event, with its source accrual and observation dates.
- Period-forward OIS is explicitly single-curve: each period uses its DF ratio
  and resolved accrual. This API does not represent overnight compounding,
  averaging, lookback, lockout or observation-shift conventions.
- Basis imports spread-leg PV plus K annuity minus reference-leg PV. K/Coupon
  and the BasisSpread measure use signed decimal BasisSpread units; the
  annuity sign and positive minimum magnitude are explicit. This is a
  same-currency contract under OIS collateral.

The complete payment AST must have `PAY PAYS N*(coupon expression)`, with N
appearing once as the outer notional. Literals, K and admitted FIX leaves can
use scalar arithmetic and unary signs. K-dependent products and divisors are
rejected even when algebraic cancellation would hide their dependence. MAX,
EXP, control flow, arbitrary callbacks, unknown indices and ON settlement do not
acquire this deterministic capability. Known Gaussian templates retain their
own admission rules.

Weights, accruals and fractional notionals can be source literals. For example,
this event on 2027-10-25 uses a .8 notional fraction, independent .7/.47 floating
weight/accrual, .5 fixed accrual, actual payment lag and explicit observation:

```text
PAY PAYS N*(0.8*(0.7*0.47*((1/FIX(IR[USD,DF,2027-10-21,2027-04-21],2027-04-19)-1)/(183/360))-0.5*K))
```

Declare N/Notional in Price:USD, K/Coupon in Rate and ParRate/ParAffineRate with
annuity sign -1 and a positive floor for this direction. Add further concrete
payment rows to express amortization without another native instrument type or
new parameter keys. The same source-derived coefficient program supplies Price
and `-B/A`; each candidate checks the actual annuity sign/floor. Observed coupons
must use `PreparedCurveInstruments_`, rather than treating their amounts as
fixed inputs to the discount-only `Price`/`Coefficients` convenience methods.

Single-curve forwards use explicit `DF(start,end)` observations. Separate
projection uses canonical 3M/6M LCH Libor with concrete starts. Native maturity
and index accrual must match nominal maturity and the actual currency Libor day
basis; mismatched projection stubs/conventions are rejected. Register the exact,
case-sensitive route returned by `CurveScriptProjectionRoute`:

```cpp
spec.market_.routes_.push_back({
    Dal::CurveScriptProjectionRoute(Dal::Ccy_("USD"), Dal::PeriodLength_("3M")),
    true, Dal::CollateralType_("OIS"), Dal::PeriodLength_("3M")});
spec.market_.forwardCurves_[Dal::PeriodLength_("3M")] = backgroundThreeMonthCurve;
```

The background entry is required when that live tenor is not the calibrated
projection target. Preparation validates actual binding, role, tenor, currency,
collateral and initial forward execution before publishing a capability or
entering Solve. A target projection uses the candidate curve; it needs an
explicit compatible discount background instead of a redundant target entry.
No global market fallback is used.

Historical fixings and RequireHistorical today use the exact frozen snapshot
and need no model projection route; their curve partials are zero. Model today
uses the live route, and a future snapshot entry never substitutes for a live
observation. Missing fixings and invalid live bindings fail during Prepare.
Each row's owning report retains its instrument/observation/quote/measure IDs,
original field and cause, one-based source row, date and zero-based statement,
including after the input spec leaves scope. Failed admission consumes no Solve
evaluations and publishes no failed capability.

AutoFast/RequireFast and GenericDeterministic consume the same complete lowered
cashflows. The curve adapter supplies raw native FD/AAD Price and par partials;
these are not calibrated quote risk. New curve families reject ScriptMC,
including explicit opt-in. Their deterministic admission does not imply a
matching stochastic model, XCCY support or a general script optimizer.

## Deterministic XCCY script cashflows

The core `dal/curve/xccyscript.hpp` provides `NewXccyScriptInstrument` for
one-time import of a native `CrossCurrencySwap_`. Resolved coupons, signed
spread, both currencies, principal exchanges and supported reset/MTM notional
changes become ordinary source. The immutable definition and binding are the
pricing authority. Preparation checks the complete bound AST and every payment
event date against the derived cashflow plan; a template name cannot admit
changed or moved payments. No native product remains beside the source.

`PrepareCalibrationInstrument` takes the dedicated `XccyScriptContext_` through
`context.xccy_`, with its own valuation date and fixing history. Leave the generic
`model_`, `curve_`, `valuation_.evaluationDate_` and `valuation_.fixings_` empty.
This produces the bounded `dal.xccy-cashflows/1` FastDeterministic capability for AutoFast or
RequireFast. `EvaluateXccyScript<double/AAD::Number_>` consumes an explicit
`XccyMarketView_`. Price is `B+K*A`; signed decimal BasisSpread is `-B/A`, using
the declared annuity sign and positive floor at each evaluation. Both spread
legs and receive/pay orientations come from the source. Native raw AAD and
caller finite differences are supported, alongside None; they are not
calibrated quote risk. Nonfinite A/B fail `CalibrationNumericalFailure` before
forming either measure, with an owning Evaluate report, instrument/measure IDs,
the coefficient field `/affine/a` or `/affine/b` and the nonfinite value.

Supported contracts use domestic-currency collateral, explicit OIS discount
routes, matching live projection tenor/currency and domestic-currency basis.
Canonical 3M/6M LCH Libor identities, fixing-to-start/end geometry and actual
currency accrual conventions must match exactly. Unequal leg payment dates
and the existing fixed, FX-resettable and MTM domestic-notional mappings are
supported. This does not introduce arbitrary amortization, stubs or collateral
conventions; unmatched geometry and anonymous/custom fixing identities fail.
Historical-only coupons need no unused live projection route or forecast fallback.

`XccyFxForward` keeps EUR/USD as USD per EUR:
`F=spot*foreignDF/(domesticDF*basisDF)`. The reverse quote uses the reciprocal;
the existing basis sign is unchanged. Payment `SPOT()` is the active market
forward conversion at its payment event, including valuation-date payments.
Reset `FIX(FX[EUR/USD],date)` is a distinct observation: a supplied same-time
reset fixing does not replace payment conversion.

The dedicated **NativeXccyTimestamp** policy uses exact identity and timestamp:

- `t<V`: require the exact historical rate/FX fixing in the frozen snapshot.
- `t=V`: use a supplied snapshot value first, otherwise the active forward.
- `t>V`: use the active forward, even when a snapshot contains a future entry.

An explicitly supplied snapshot, including an empty one, is authoritative and
never supplemented from globals. An omitted snapshot captures the strict-past,
unsettled dependency closure once at Prepare. That owned snapshot stays frozen
through evaluation and bumps; changing global history cannot alter the plan.
Valuation and supported native rate/reset observations must be at exact
midnight. Intraday values are never copied to midnight; intraday valuation and
generic today-policy overrides are rejected. Canonical currency conventions,
routes and captured snapshot contents remain part of plan compatibility.

The exporter spells embedded scientific constants as equivalent plain decimal
DSL literals, retaining their effective double ratio and significant digits.
It does not change the bare parser or existing archives. Native notionals are
represented by N and the floating-point foreign/domestic ratio; ill-conditioned
normalization does not guarantee uniform absolute precision at arbitrary scales.
Legacy native entry points keep their original behavior.

GenericDeterministic is not an XCCY executor. Generic ScriptMC and the generic
MC entry point retain `MCCrossCurrencyUnsupported`. The dedicated stochastic
XCCY interface below supplies its own model, observations and phase mapping.
Deterministic PV/par and raw derivative evidence does not establish stochastic
numerical acceptance or calibrated quote risk.

### Staged XCCY instrument calibration

`dal/curve/xccybasiscalibration.hpp` provides `XccyInstrumentCalibrationSpec_`,
`PreparedXccyInstruments_` and `CalibrateXccyInstruments`. A spec owns ordered
shared observations, the dedicated XCCY context, explicit domestic and foreign
curve blocks, positive FX spot and future, strictly increasing basis knot dates.
Preparation clones the background curves and freezes each instrument's fixing
snapshot; every candidate rebuilds only the domestic-currency piecewise-constant
basis-forward curve. The native parameter axis follows those knots. Each source
is imported once; the adapter retains no parallel native product.

Observations may mix Price in its declared currency and signed decimal
BasisSpread. Their error scales normalize residuals without changing market
quote fingerprints. Observation rows, deduplicated quote columns and native
parameter columns remain separate. The published `quoteJacobian_` is raw
`d(model measure)/d(native basis parameter)`, before error or parameter scaling;
it is available on request and does not supply calibrated quote risk.

The existing `CurveInstrumentSolver_` controls this new adapter. Its initial
basis guess defaults to zero. `initialGuessPerNode_` fixes theta0 when supplied;
positive `parameterScales_` define theta=theta0+S*z. The native smoothing metric
is transformed as Wz=S'Wtheta*S without inserting knot-gap weights. EXACT
requires finite scaled infinity norm at most one. APPROXIMATE requires explicit
positive `fitWeight_` (lambda) and `fitNormTolerance_` (eta), and accepts scaled
L2 norm at most eta. Its reported objective is
`0.5*(theta-theta0)'*Wtheta*(theta-theta0) + 0.5*lambda*||scaled residual||^2`.
This acceptance condition does not certify objective optimality. Unavailable or
nonrepresentable objective values are nullable; finite zero remains zero.

The adapter uses the existing bounded `Underdetermined::Fit`. Finite-difference
solve perturbations consume its evaluation budget in z coordinates; an AAD solve
transforms raw native columns by S. A standalone Evaluate computes requested raw
finite differences in native coordinates, using actual representable spacing.
Fit counters exclude the final report refresh; C++ results separately expose
`diagnosticEvaluations_` and `diagnosticGradients_` for that refresh. Failures
carry an owning report and, when validated, the last candidate basis curve with
its residuals and axes. The existing native staged/joint APIs retain their
original raw gates, defaults and exception behavior.

The public C++ facade exposes `NewXccyScriptInstrument`,
`CalibrateXccyInstrumentMarket`, basis-curve/report getters and
`XccyInstrumentCapabilities`. Python provides keyword-only copied settings and
specs through `XccyScriptContext_`, `XccyScriptImportSettings_` and
`XccyInstrumentCalibrationSpec_`, plus `Calibrate_XccyInstruments`,
`Evaluate_XccyInstruments` and `CalibrationInstrument_Get_XccyCapabilities`.
The three settings types also have `_New` aliases. Conversion and ownership
complete before the wrapper releases the GIL for core work.

Excel exposes corresponding typed session factories and
`Calibrate.XccyInstruments`, returning the shared status/report/nullable-curve
outcome. Field/value settings reject unknown or duplicate fields, wrong typed
handles and booleans used as numbers. Instruments use the shared strict JSON
archive; contexts, specs and failure candidates are session objects. Capability
reports for the deterministic context include the explicit generic
`MCCrossCurrencyUnsupported` rejection. The
[public XCCY example](../../dal-public/examples/xccy_script_calibration/xccy_script_calibration.cpp)
imports once, fits BasisSpread and reprices Price from the same bound instrument.
All native geometry, collateral, notional and NativeXccyTimestamp limits above
continue to apply.

### Stochastic XCCY model and phased MC

`dal-public/src/xccymontecarlo.hpp` exposes `NewXccyGaussianModel(name, spec)`
and `ValueXccyInstrumentByMonteCarlo(instrument, model, request)`. The original
immutable source, complete bound AST and payment dates remain authoritative;
the model owns snapshots of its input curves. There is no second native trade.
`ValueXccyInstrumentByGaussian` evaluates A, B and Price or eligible par from
Gaussian moments over that same source. Both routes support FIXED, RESETTABLE
and MARK_TO_MARKET notionals within the canonical dual-leg import domain above.

The required `XccyGaussianMarketSpec_` fields are exact midnight valuation V,
pair, domestic collateral, positive spot, domestic and foreign curve blocks,
explicit domestic-currency basis and discount route. EUR/USD denotes USD per
EUR; effective foreign discounting is Pf/B. Curves must have a lossless
log-linear log-discount representation with explicit coverage through every
required date. Projection routes are distinct from OIS; unused historical-only
routes need not exist. No extrapolation or curve resampling is introduced.

`XccyGaussianDynamicsSpec_` requires domestic/foreign mean reversions, dated
bucket ends, matching domestic-rate/foreign-rate/FX volatility vectors and
named three-factor correlation. Mean reversions are nonnegative; sigmas are
nonnegative native annual short-rate/log-FX volatilities. Correlation must be
positive definite with minimum eigenvalue above 1e-10. The domestic collateral
measure includes the foreign rate's negative quanto drift. Basis and tenor
spreads remain deterministic; dynamics are supplied and frozen, rather than
estimated from the basis quotes. Exact rate endpoints, integrals and FX use
five reserved innovations per advancing interval for three economic factors.

Set `XccyInstrumentCalibrationSpec_.gaussianDynamics_` explicitly to select
stochastic Gaussian staged residuals. The default remains deterministic.
The explicit Gaussian path additionally supports `basisParameterization_ =
LOG_DISCOUNT`; the existing piecewise-constant-forward default remains valid.
The existing solver, observation scales, EXACT/APPROXIMATE criteria and raw
native axes apply. `XccyInstrumentResultGaussianModel` returns the validated
candidate containing the fitted basis. Synthetic quotes generated by the fast
route are useful demonstrations, and do not constitute independent acceptance.

`XccyMonteCarloRequest_` requires `measureId_`, `phase_` and the existing typed
`sampling_`. Phase is exactly Fit, Validation or HeldOut, with no valid default.
Use PseudoRandom, `dal.mrg32-normal-paths/1`, at least two batches of two paths,
nonnegative int32 seed, Precise normals, no bridge and zero smoothing. The XCCY
request defaults smoothing to zero. It has no separate valuation/today policy;
V and market identity come from the model. Fixing omission captures strict-past
history once; explicit empty snapshots remain authoritative. At-V supplied FIX
wins, otherwise it is active; future FIX is realized on its path, and payment
SPOT stays active at the actual payment date. All supported times are midnight.

Fit reserves paths `[0,262144)`, Validation `[262144,1310720)` and HeldOut
`[1310720,2359296)`. Counts select each phase's contiguous prefix. Capacity and
checked path/coordinate arithmetic fail before draws; there is no offset
override, clamping, hidden seed change or automatic budget extension. D=0
consumes zero scalar coordinates while retaining its requested phase. The
single-instrument independence checker rejects mixed mappings. The
[core prepared basket](#prepared-core-xccy-mc-baskets) provides a separate
scalar allocation for members with different dated mappings.
Only Validation estimates are eligible for `ValidateXccyMonteCarlo`; Fit and
HeldOut retain their own flags and cannot be relabeled by a binding.

The result retains mean, independent-batch standard error and an owning report.
`XccyMonteCarloDetails` and `DumpXccyMonteCarloResultJson` expose the separate
`dal.xccy-gaussian-public-result/1` extension: model/mapping identities, transition
and range-layout versions, phase, exact path/scalar-coordinate ranges, A/B and
their paired batch covariance, ordered native partials and gradient standard errors.
The covariance order is B,A,dB[0],dA[0],... in the reported parameter order.
Integer ordinals are decimal strings in this JSON to preserve exactness.
Existing `dal.calibration-report/1` groups and spill columns do not change.
Default derivative method is None. AAD requires explicit ordered native labels
for spot, supported curve/basis coordinates or interior sigmas. Fixed a/R,
zero-vol/rank boundaries and unsupported methods fail `DerivativeUnavailable`;
missing derivatives are unavailable. Raw partials hold fitted inputs fixed and
are not calibrated quote risk.

Python supplies keyword-only `XccyGaussianMarketSpec_`,
`XccyGaussianDynamicsSpec_`, `XccyGaussianModelSpec_` and
`XccyMonteCarloRequest_`, plus `XccyGaussianModel_New`,
`XccyGaussianDynamics_New`, `XccyInstrument_ValueByGaussian` and
`XccyInstrument_ValueByMonteCarlo`. Nested inputs are typed, copied and checked
before releasing the GIL. Omitted or None fixings mean omission; a typed empty
snapshot means explicit empty. Result getters copy containers. `CalibrationError`
owns reports/candidates; stochastic staged failures retain their curve in
`candidate` and model in `gaussian_model`. Capability queries use
`CalibrationInstrument_Get_XccyGaussianCapabilities` with the explicit model.

Excel uses typed session model-spec/dynamics/request/outcome handles. The market
factory takes required domestic/foreign/basis handles separately from its
scalar field/value settings. Dynamics buckets have named columns
`end_date,sigma_domestic,sigma_foreign,sigma_fx`; the 3x3 correlation table has
`factor` at the corner and matching named row/column headers. The request factory
takes required `measureId,phase,sampling`, followed by optional field/value
settings `fixings,simulation,derivative_method,parameter_labels`. Blank or omitted
fixings mean omission; an explicit empty snapshot handle remains distinct.
Unknown/normalized duplicate fields, wrong handles, booleans used as numbers,
nonfinite values and nonintegral/out-of-int32 counts are rejected. Diagnostics
retain function, argument, field and available cell coordinates.

`XccyMonteCarloOutcome_Get_Status/Get_Report/Get_Mean/Get_StandardError/Get_Json`
read the owning outcome. A completed estimate has status Estimate, not a
numerical acceptance claim. Get_Partials spills
`parameter_label,native_partial,standard_error`; unavailable partials raise an
error. Get_Model returns an available validated model. Typed reference/precision
factories and `XccyMonteCarloOutcome_Validate` expose the same core validator;
the separate flags getter uses `dal.xccy-gaussian-validation-flags/1` and blank
gradient acceptance fields when gradients were not requested. Plans, requests
and outcomes remain session objects. Portable tests do not establish native
Windows XLL or workbook execution.

Value parity, precision, combined error, gradient acceptance, fit and held-out
behavior are separate. A failed budget preserves estimates and reports; no
sampling increase or tolerance relaxation follows automatically. The frozen
N=110 MTM validation fixture has not passed precision/combined acceptance:
at its declared x4 budget, Price 3SE is about 0.059175 USD versus 0.0025 USD,
and par 3SE about 0.00107236 versus 0.000025. Independent V1 and large-sample
gradient acceptance are not established by this interface. The model does not
add joint calibration, smile/SLV, stochastic basis, early exercise, PVAtQuote,
RQMC or calibrated quote risk.

The [public stochastic example](../../dal-public/examples/xccy_script_monte_carlo/xccy_script_monte_carlo.cpp)
imports once, fits a synthetic Gaussian par observation, retrieves the candidate
model and reprices the same source through GaussianFast and explicit HeldOut MC.
It prints native-unit prices, batch errors and a raw spot partial.

### Prepared core XCCY MC baskets

`PreparedXccyMcBasket_` in `dal-cpp/dal/script/xccymcbasket.hpp` evaluates
ordered `XccyMcBasketMember_` entries with different dated mappings under one
immutable Gaussian model. Each member supplies a byte-exact unique ID, an
instrument and its Price or eligible BasisSpread measure. Supply the XCCY
preparation context, model and explicit `XccyMcSettings_`, including phase.
The context requires an explicit nonnull fixing snapshot; an empty snapshot
is valid subject to required history. Preparation owns the inputs and derives
every member's grid and allocation before drawing. `Evaluate(candidate)` accepts
a numeric model refresh only after all members pass the geometry, mapping and
derivative checks.

For member dimension `D_i`, block starts satisfy `O_0=0` and
`O_(i+1)=O_i+2359296*D_i`. Member-local path p uses scalar coordinate
`O_i+p*D_i+5*j+s` for zero-based interval j and innovation s=0,...,4.
The private MRG32 Precise adapter seeks this absolute scalar position,
including starts not divisible by D_i. Member order defines the allocation;
each member retains the phase reservations above. D=0 consumes no random
coordinates. Capacity checks cover full reservations and storage before execution.

The owning `XccyMcBasketResult_` exposes `Snapshot()` and `Report()`.
Its complete coefficient rows concatenate members in input order, with columns
`B,A,dB[0],dA[0],...` for the requested native labels. With b batches, the full
sample covariance `C_batch` retains measured cross-member blocks and
`C_mean=C_batch/b`. Price/par and native-gradient uncertainty use the full
corresponding Jacobian. Members are marginal estimates on disjoint scalar
blocks; equal batch/path ordinals do not represent a common economic scenario
or portfolio payoff. Member count adds no sampling replicates, and a singular
sample covariance does not require inversion.

`RequireXccyMcBasketIndependent` checks nonoverlapping stochastic executions
of the same allocation. Overlapping prefixes and repeated evaluations fail
that check; pass `commonRandomNumbers=true` explicitly for matching replay
comparisons. Changing seed, member order or allocation does not certify
independence. Existing single-instrument checks and `/1` streams retain their
own mapping requirements.

Catch `XccyMcBasketError_` before `CalibrationError_` to retain `Partial()`.
The immutable `XccyMcBasketPartial_` preserves committed complete batch rows,
completed member summaries, planned ranges and actual consumption after the
plan and error are destroyed. Generated scalars, completed paths and committed
batches are distinct counts. Early preparation errors have no partial; rejected
evaluation candidates have a zero-draw partial and no candidate. Later failures
can retain the actual model only after every member passed preflight. Diagnostics
preserve the cause's source row or its absence, event date and statement.
A failed basket has no complete result, joint covariance, basket SE or acceptance
flags; unfinished rows are not padded with zeros.

The public C++, Python and Excel interfaces expose this prepared basket and
owning partials through the [calling recipe below](#calling-prepared-xccy-mc-baskets).
There is no product basket validator; the single-instrument validator does not
accept basket results. The ordinary report remains `dal.calibration-report/1`.
A complete result has status Estimate. It does not establish frozen numerical
acceptance or calibrated quote risk. Already observed HeldOut data remain
observed; the new allocation does not create an untouched holdout.

### Calling prepared XCCY MC baskets

Use `dal-public/src/xccymontecarlobasket.hpp` with an explicit immutable
`XccyGaussianModelData_` model and the original imported script instruments.
The request requires phase, sampling and a nonnull fixing snapshot. An explicit
empty snapshot is valid when no history is required; omission/null is rejected.
Valuation time, pair, collateral and discount route come from the model.
The same PseudoRandom/Precise, no-bridge, zero-smoothing and phase-capacity
rules apply. Members retain input order and byte-exact IDs (`Short` and `short`
are distinct); each specifies its own Price or eligible BasisSpread measure.

```cpp
XccyMonteCarloBasketRequest_ request;
request.phase_ = XccyMonteCarloPhase_::Value_::Validation;
request.sampling_ = {"PseudoRandom", 64, 4, 1024, "dal.mrg32-normal-paths/1"};
request.fixings_.reset(new MarketFixingSnapshot_({}));
request.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
request.parameterLabels_ = {"fxspot:EUR/USD"};
const auto prepared = PrepareXccyBasketMonteCarlo(
    {{"Short", shortInstrument, "Price"},
     {"short", longInstrument, "BasisSpread"}}, model, request);
const auto result = prepared.Evaluate(); // optional same-geometry candidate model
const auto& snapshot = result.Snapshot();
const auto json = DumpXccyMonteCarloBasketResultJson(result);
```

`prepared.Allocation()` is a const view whose lifetime depends on `prepared`.
The complete result owns its snapshot and report independently of plan and
caller inputs. `snapshot.members_` contains finalized member estimates/SEs;
`coefficientAxes_` identifies every row/column of `coefficientCovariance_`
(C_batch) and `meanCovariance_` (C_mean). `outputAxes_`, `outputJacobian_` and
`outputCovariance_` identify the full measure/native-gradient projection.
Raw native partials hold model curves fixed; they are not calibrated quote risk.

Catch the derived error first and retain an owner before leaving the catch:

```cpp
Handle_<XccyMcBasketPartial_> partial;
try {
    const auto result = PrepareXccyBasketMonteCarlo(members, model, request).Evaluate();
} catch (const XccyMcBasketError_& error) {
    partial = error.Partial(); // may be null for early preparation failure
    const auto report = error.Failure().report_;
    const auto candidate = error.Failure().candidate_; // actual validated model, or null
} catch (const CalibrationError_& error) {
    const auto report = error.Failure().report_; // early facade/input failure; no partial
}
if (partial) {
    const auto json = DumpXccyMonteCarloBasketPartialJson(partial);
}
```

The partial and its report remain readable after the error and plan are
destroyed. Evaluation preflight rejection has zero draws, a partial and null
candidate; the rejected model never becomes a fallback candidate. A later
failure may retain the actual model after every member passed preflight.
Completed members remain member-scoped; unfinished estimates are unavailable.
A failure has no complete basket result, joint covariance/SE or acceptance.

Python uses the same required inputs; assuming `short_instrument`,
`long_instrument` and `model` have already been constructed:

```python
import dal

sampling = dal.CalibrationMonteCarloSampling_(
    paths_per_batch=64, batches=4, seed=1024,
    stream_version="dal.mrg32-normal-paths/1")
fixings = dal.MarketFixingSnapshot_New({})
request = dal.XccyMonteCarloBasketRequest_(
    "Validation", sampling, fixings,
    derivative_method="AAD", parameter_labels=["fxspot:EUR/USD"])
members = [dal.XccyMcBasketMember_("Short", short_instrument, "Price"),
           dal.XccyMcBasketMember_("short", long_instrument, "BasisSpread")]
try:
    prepared = dal.XccyBasket_PrepareMonteCarlo(members, model, request)
    result = prepared.evaluate()  # optional candidate=model with matching geometry
    details = result.details_json
except dal.XccyBasketCalibrationError as error:
    partial, report, candidate = error.partial, error.report, error.candidate
    details = partial.details_json if partial is not None else None
```

Omitted required arguments raise TypeError; invalid typed inputs retain owning
diagnostics. The error subclasses `CalibrationError`; `partial` and `candidate`
can be None. Allocation, nested result records and partials have independent
ownership in Python. Inspect `result.coefficient_axes`, `coefficient_covariance`,
`mean_covariance`, `output_axes`, `output_jacobian` and `output_covariance`
together. Native work releases the GIL after copying inputs.

In Excel, pass the entire three-column range, including these exact headers,
to `XCCYMONTECARLOBASKETMEMBERS.NEW`:

| member_id | instrument                | measure_id  |
|-----------|---------------------------|-------------|
| Short     | typed short script handle | Price       |
| short     | typed long script handle  | BasisSpread |

Create a sampling handle with `CALIBRATIONMONTECARLOSAMPLING.NEW`, using
field/value rows `mode=PseudoRandom`, `paths_per_batch=64`, `batches=4`,
`seed=1024` and `stream_version=dal.mrg32-normal-paths/1`. Create the explicit
snapshot with `MARKETFIXINGSNAPSHOT.NEW` (all three arrays blank means empty).
Call
`XCCYMONTECARLOBASKETREQUEST.NEW("Validation", sampling, fixings)`; optional
field/value settings are `simulation`, `derivative_method` and `parameter_labels`.
Then use `XCCYBASKET.PREPAREMONTECARLO(members, model, request)`, check
`XCCYMONTECARLOBASKETOUTCOME.GET.STATUS`, extract
`XCCYMONTECARLOBASKETOUTCOME.GET.PREPARED` and call
`XCCYBASKET.EVALUATEMONTECARLO(prepared[, candidate])`.

The `XCCYMONTECARLOBASKETOUTCOME` getters have these state-specific rules:

| Status   | GET.PREPARED | GET.RESULT  | GET.PARTIAL      | GET.MODEL                           |
|----------|--------------|-------------|------------------|-------------------------------------|
| Prepared | available    | unavailable | unavailable      | prepared model                      |
| Estimate | unavailable  | available   | unavailable      | actual evaluated model              |
| Failed   | unavailable  | unavailable | only if retained | only after full candidate preflight |

`GET.REPORT` and `GET.JSON` apply to every outcome. Getters for absent objects
raise `CalibrationCandidateUnavailable`; absence is never a zero result.
`GET.PARTIAL` returns an independent session owner readable through
`XCCYMONTECARLOBASKETPARTIAL.GET.JSON` after the outcome is released. Early
input failures may have no partial; malformed factory input can fail before an
outcome exists. These typed objects are transient session handles, not archives.
Concatenate JSON chunks without separators.

The JSON export families are:

| Export                         | Schema                                  |
|--------------------------------|-----------------------------------------|
| Complete C++/Python result      | dal.xccy-gaussian-public-basket-result/1  |
| C++/Python/Excel failed partial | dal.xccy-gaussian-mc-basket-partial/1     |
| Excel outcome envelope         | dal.xccy-gaussian-public-basket-outcome/1 |

The complete export contains every coefficient row and full covariance/axis
matrix; the outcome envelope nests that same result or the failed partial.
New basket ordinals/counts are exact decimal strings, including optional
failure locations when present. Unavailable fields are null (None in Python),
including gradients for derivative method None and failure joint statistics.
Nested ordinary report `/1` retains its existing format. Exporting a partial
does not certify an estimate or resume the failed run. Estimator covariance
does not describe joint economic scenarios or nonlinear portfolio payoffs.

The [public C++ example](../../dal-public/examples/xccy_script_monte_carlo_basket/xccy_script_monte_carlo_basket.cpp)
constructs two different dated mappings, prints exact member IDs, units,
plain/AAD means, SEs, complete covariance tables and JSON. Its small sample
illustrates the calls only. Build and run from the repository root:

```bash
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_PUBLIC_BUILD_EXAMPLES=ON
cmake --build build/Release-linux --target xccy_script_monte_carlo_basket --parallel
ctest --test-dir build/Release-linux -R '^xccy_script_monte_carlo_basket$' --output-on-failure --no-tests=error
build/Release-linux/dal-public/examples/xccy_script_monte_carlo_basket/xccy_script_monte_carlo_basket
```

## Same-instrument Monte Carlo

`dal/script/calibrationmc.hpp` provides `PrepareCalibrationMc` and its immutable
plan's `Evaluate(instrument, context, validation)` method. Callers pass the
original instrument and context; the wrapper derives its own disposable bound
script. It repeats identity checks before execution. K/N rebinding requires a
new plan and leaves previous plans unchanged. This core bridge adds no solver
or public/Python/Excel entry point.

Settings require explicit opt-in, fit/validation batch counts, paths per batch
and a maximum path budget per coefficient view. Both groups need at least two
batches of at least two paths; validation uses more paths than fit. Counts are
bounded by the protocol's exact integer range. The wrapper reserves one MRG32
stream with a nonnegative seed: fit starts at normal-path zero, validation
starts after the entire fit range, and batches occupy disjoint intervals.
`Mapping()` records stream version, seed, ACT/365 timeline, sample/factor normal
coordinates, native Gaussian numeraire, Precise normals and no Brownian bridge.
CRN reuse requires this mapping; a matching seed across products proves no
shared economic Brownian path. Sobol/RQMC error estimates are unsupported.

Compiled execution is the default; set `compiled_ = false` for tree execution.
Both use the same hard payoff, including native AAD.
Nonzero context smoothing is rejected. Option AAD requires a nondegenerate
Gaussian distribution; selected zero-g boundaries require a separate one-sided
method and return `DerivativeUnavailable`. Requested native labels match bytes
exactly and must occur once. `CalibrationMcGLabels` maps existing factor/knot
coordinates using the model's original factor names and knot dates. Missing,
duplicate or incorrectly cased labels fail with owning diagnostics.

Price results expose batch means, gradient batch means, their separate standard
errors and raw `quoteJacobian_(0,j)` in the requested native coordinates. Risks
are already path-normalized. Optional retained path values/partials support
diagnostics. Each evaluation creates its own model, evaluator and recording
scope; active values are never cached across calls or shared across threads.

Deposit affine views lower the proven payment RHSs to A and B and execute both
on the same normal paths. The estimate is `-mean(B)/mean(A)` and the derivative
uses the full quotient rule on pooled coefficient partials. The result exposes
paired sample covariance in the order B, A, dB0, dA0, dB1, dA1, etc. Value and
gradient uncertainties use the delta method with covariance of means equal to
sample covariance divided by batch count. The annuity's direction and lower
confidence magnitude must exceed the declared minimum. Affine batches retain
coefficients rather than per-batch ratios; retained path quote ratios are not
produced. `engineRuns_` counts logical batch/view executions (twice the batch
count for affine), with model initialization shared across batches. Tape node
and retained-capacity high water marks are reported separately.

`ValidateCalibrationMc` compares independent fit/validation results, or a
caller-supplied independent reference with numerical/FD indicators. The paired
overload checks instrument, measure, context, exact parameter axes and disjoint
stream ranges, and rechecks both affine denominators at the requested validation
sigma. Freeze value scale, sigma, numerical-error fraction, parameter
scales and numerical floors before sampling. Value parity, precision and total
error indicators must all pass; every requested gradient must satisfy its own
parity and precision tests. Floors must lie below their budgets. Failure returns
`numericalValidationPassed_=false` and an owning Validate report; invalid
requests or execution failures throw `CalibrationError_`. Exceeding the declared
path budget fails explicitly. No adaptive scale enlargement or retry is applied.
SEs, FD refinement and affine ratio errors are estimates, not strict bounds.

## Gaussian script Price MC calibration

`dal/model/gsrscriptcalibration.hpp` provides the separate core
`CalibrateGSRScriptPrices` driver for native-unit Price observations and selected
MultiFactor Gaussian g coordinates. It requires explicit MC opt-in; existing
native fast adapters and vanilla volatility conversion keep their behavior.
General Price targets may be signed. Recognized BondOption/Caplet/Swaption
contracts retain their original classification, domain checks and nonnegative
Price-target preflight, including held-out observations.
Two-payment Deposit shapes beginning with `PAYS -N` retain the native payoff
checks even with unrelated scalar assignments. Full native AST admission is
still required; a malformed maturity payment cannot acquire the general Price rule.

General admission reads the complete bound scalar AST and validates the actual
Gaussian model, fixing snapshot and same-currency OIS/AtEvent context. It permits
ordinary scalar state assignments and arithmetic with supported DF/Libor FIX,
including a running arithmetic average. PAY remains the sole receiver, with
no reset after historical or live payments. EXERCISE, vectors, SPOT and IF/
comparison controls are rejected. Context smoothing must be zero. The derived
MC descriptor contains no economic data; execution uses the original bound
program. This path grants no fast, deterministic or affine capability.

For example, N/Notional and K/RateStrike declarations with N=100 and K=.04 can
drive these event rows:

```text
2027-10-21: avg = .5*FIX(IR[USD,LIBOR_6M_LCH,2027-10-21])
2028-04-21: avg = avg+.5*FIX(IR[USD,LIBOR_6M_LCH,2028-04-21])
2028-10-21: pay PAYS N*MAX(avg-K,0)
```

Concrete dates, index accrual, payment date and numeraire remain part of the
contract. A single-fixing degeneration matches a native caplet only with the
same coupon accrual and payment semantics; the arithmetic-average amount has
coupon accrual one. Its multi-fixing payoff has no native European fast kernel.

Freeze the Price scale, parameter scale, bounds, seed, batches, maximum paths,
sigma and value/gradient precision settings before fitting. An explicit budget
configuration is:

```cpp
Dal::GSRScriptCalibrationSettings_ settings;
settings.sampling_.allowMonteCarloCalibration_ = true;
settings.sampling_.seed_ = 1024;
settings.sampling_.fitBatches_ = 8;
settings.sampling_.fitPathsPerBatch_ = 8192;
settings.sampling_.validationBatches_ = settings.sampling_.heldOutBatches_ = 16;
settings.sampling_.validationPathsPerBatch_ = settings.sampling_.heldOutPathsPerBatch_ = 16384;
settings.sampling_.maxPathsPerView_ = 589824;
const auto result = Dal::CalibrateGSRScriptPrices(
    context, {observation}, {{0, 0, .001, .06}}, settings, {heldOutObservation});
```

The reservation is per view, not the total optimizer work. Fit uses fixed CRN
paths. Validation follows the entire fit reservation, and optional held-out
occupies a third disjoint range. A shared seed alone does not establish CRN
across different timelines or normal-coordinate mappings. Held-out compares its
own mean and SE with its supplied target; it neither updates fit values nor
becomes ordinary validation. No held-out input leaves requested=false and
passed=false, representing NotRequested.
Held-out counts enter execution identity only when that stream is enabled;
disabled held-out preserves the existing native-template plan identity.

This driver requires every fit value/AAD and held-out plan to have the same
actual normal-draw dimension, with complete normal coordinates. It checks the
prepared mappings before optimizer work and rejects mixed dimensions with an
owning Prepare error. A path seeks `path*dimension` raw draws: distinct path
ranges at different dimensions can overlap. Common draw stride makes all fit,
validation and held-out draw ranges disjoint across the admitted basket while
preserving existing seeds, ranges, budgets and same-dimensional identities.
This restriction does not allocate new global RNG substreams or establish
identical coordinate meaning across different timelines.

Solver coordinates are normalized g values, while `quoteJacobian_` remains raw
dPrice/dNativeG with fixed bindings. Data residuals use observation error scales;
prior and knot-gap smoothing rows have separate axes and objective subtotals.
Prior/smoothing penalize movement from the immutable initial g, including
selected-to-unselected neighbors. Rank diagnostics exclude penalty rows.
AAD evaluates M observation rows per Jacobian; the FD baseline makes 2N complete
basket bumps. Native bounds and actual representable bump distances determine
the stencil, with one-sided columns reported explicitly. The result reports
the actual method, row/basket/batch/path counts and tape high-water usage.

Stored compiled/tree structures can be shared across g-only candidates.
Admission and context checks are repeated; general admission includes a fresh
uncompiled bound-script preflight. Each execution uses a fresh model, evaluator,
historical state and recording scope. Changed binding, curve, fixings, H,
correlation, geometry, convention or smoothing requires a new plan.

Read `converged_`, `fitWithinTolerance_`, `numericalValidationPassed_` and the
held-out flags separately. StepStalled/NoDescent/MaxIterations may return a valid
candidate without declaring convergence. A good fit does not override a failed
value or gradient error budget; increasing notional with a fixed absolute Price
scale can exhaust that budget. No automatic scale enlargement or resampling is
performed. Budget or execution exceptions own the available observation axes,
diagnostics and last completely evaluated candidate; no incomplete candidate
is represented as a successful evaluation. Row preparation, refresh and execution
errors bind the actual instrument/observation/quote/measure while preserving
the original field, source and cause. A held-out failure owns the held-out axis
group and identifies that scope in its message; it retains the last complete
fit candidate rather than publishing a partially executed basket.
`quoteRiskAvailable_` remains false.
This core driver adds no public/Python/Excel bindings, XCCY MC, early exercise,
SLV, joint calibration, RQMC or calibrated quote-risk propagation.

## Quotes, observations and reports

Quote units are exact: Rate and BasisSpread are decimal quantities, Price:USD is
an amount for the bound contract, and FxOutright:EUR/USD is USD per EUR. Currency
conversion, basis-point scaling and per-unit-notional conversion are explicit
caller operations. BondPrice and YearFraction are parameter units. Quote
conventions are materialized from units or validated when supplied.

A quote owns only its ID, value, unit and convention. An observation owns its ID,
instrument, measure ID, quote and positive finite error scale. Quote-copy
annotations on a parameter record an explicit numeric copy plus a quote
fingerprint; they never retain a live quote. Creating another quote or observation
cannot change a contract.

`BuildCalibrationObservationAxes` retains observation order as row ordinal.
Distinct quote columns follow first appearance, with an explicit `quoteIndex_`
and one-hot `quoteMap_`. IDs compare case-sensitively. Repeated quote IDs must
have identical data; repeated instrument IDs must identify the same contract.
Duplicate observation IDs fail.

Identity uses RFC 8785 canonical JSON and SHA-256. Source identity preserves
source text exactly. Definition identity includes the full schema and header;
binding identity includes every effective value and its unit, excluding
provenance; instrument identity additionally includes instrument ID and pillar.
Negative numeric zero is normalized. These identities identify data, not proven
financial equivalence.

Portable JSON save/load preserves the complete definition and effective binding,
including parameter origins and quote-copy annotations:

```cpp
auto saved = Dal::DumpCalibrationInstrumentJson(rebound);
auto restored = Dal::LoadCalibrationInstrumentJson(saved);
```

The wire schemas are `dal.script-instrument-definition/1`,
`dal.calibration-parameter-binding/1` and `dal.calibration-instrument/1`;
language and financial semantics have separate version tags. Readers reject
unknown/missing fields, duplicate properties, mismatched types, unsupported
versions and incomplete/inconsistent bindings. DAL archives store the same
portable JSON under `ScriptInstrumentDefinition_v1` and
`CalibrationInstrument_v1`. Legacy ScriptProductData v1/v2/v3 archives retain
their existing readers and v3 writer.
RateStrike uses the exact portable token `RateStrike` (Java `RATESTRIKE`) as an
additive role; existing role ordinals and the `/1` and `_v1` versions are unchanged.

Portable JSON loaders accept at most 50 MiB and 128 nested objects/arrays. The
nesting budget is checked without recursion before building or converting the DOM;
exceeding it returns InvalidCalibrationArchive with a Load report.

`CalibrationError_` owns structured diagnostics and a nonnull partial
`CalibrationReport_`, including field paths and available object/source
context. Input and Load stages are distinguished. Parser failures retain available
source row, event date and zero-based statement ordinal; the legacy Product
exception types are preserved. Reports currently provide
the owned axes and errors needed by these data boundaries; evaluator/solver
metrics are not populated as fabricated results. Their archive Write fails
with CalibrationPersistenceUnsupported. Saving an instrument does not save
a market snapshot, prepared plan, calibration session or quote-risk record.

## Public C++, Python and Excel interfaces

Include `dal-public/src/calibrationinstrument.hpp` for the shared definition,
binding, import, observation, preparation, curve-fit and same-instrument MC
interfaces. C++ retains CamelCase names such as `NewDepositScriptInstrument`,
`CalibrateCurveInstruments` and `ValueCalibrationInstrumentByMonteCarlo`.
The standalone `dal-public/examples/script_calibration/` program demonstrates
a curve fit, repricing the original fixed coupon, and explicit rebinding.
Include `dal-public/src/gsr.hpp` for `CalibrateGSRInstruments`. The separate
[`gsr_script_calibration` example](../../dal-public/examples/gsr_script_calibration/)
fits native g, then prices the same binding with plain MC and AAD and queries
an explicitly rebound strike. Existing core GSR examples retain their targets
and purpose.

Python and Excel markup use the same factory/action names:
`ScriptInstrumentDefinition_New`, `ScriptCalibrationInstrument_New`,
`CalibrationInstrument_BindParameters`, `DepositScriptInstrument_New`,
`EuropeanRateOptionScriptInstrument_New`, `CalibrationMarketQuote_New`,
`CalibrationObservation_New`, `CalibrationPlan_New`,
`Calibrate_CurveInstruments`, `Calibrate_GSRInstruments` and
`CalibrationInstrument_ValueByMonteCarlo`.
The bounded curve importers are `FraScriptInstrument_New`,
`SwapScriptInstrument_New`, `PeriodForwardOisScriptInstrument_New` and
`BasisSwapScriptInstrument_New`. FRA requires an explicit payment date.
Excel uppercases these names and replaces underscores with dots; for example,
`Calibrate_CurveInstruments` appears as `CALIBRATE.CURVEINSTRUMENTS`.

The options, constructors and reports expose the merged product boundaries
described above. FRA, IRS, period-forward OIS, Basis and custom curve
cashflows remain deterministic. Importing an option does not certify a
model or derivative method; preparation still checks the full bound source.
The GSR facade selects the existing bounded Gaussian drivers described below.
These interfaces do not add overnight compounding, futures convexity, XCCY MC
or calibrated quote risk. Scalar measures remain outside GSR Price fitting.

### GSR instrument calibration settings and results

C++ requires all four arguments:
`CalibrateGSRInstruments(initial, observations, parameters, settings)`.
Python uses `Calibrate_GSRInstruments(initial, observations, parameters, *, settings)`.
The initial model is native `MultiFactorGSRModelData_`; parameters select existing
factor/knot g coordinates with finite nonnegative bounds containing the initial g.
Parameter order determines the stable `g:<factorName>:<date>` column order.
The old `Calibrate_GSRVolatility`, its positional parameters, defaults and aliases
retain their existing behavior.

Construct Python coordinates with the strict keyword-only
`GSRCalibrationParameter_New(factor=0, knot=0, lower=0.0, upper=0.1)`.
`GSRInstrumentCalibrationSettings_` (also `_New`) requires `valuation` with an
explicit evaluation date and fixing snapshot, including an explicit empty
snapshot when appropriate. The domain validator checks the same semantics in
C++, Python and Excel; model, curve, date, currency, routes and full bound source
remain subject to core admission.

Settings fields are `solver`, `valuation`, `evaluation`, `fit_sampling`,
`validation_sampling`, `held_out_sampling`, `held_out_observations`,
`max_paths_per_view`, `max_basket_evaluations`, `validation_sigma`,
`epsilon_value`, `epsilon_gradient` and `compiled`. Solver is a copied native
`GSRCalibrationSettings_`; evaluation is `CalibrationEvaluationOptions_`.
Basket limit defaults to 1000, sigma to 3, value/gradient floors to 1e-10/1e-8,
and compiled to the existing script default. There is no default MC path budget
or batching protocol. These settings are session values without archive support.

AutoFast first selects the existing BondOption fast calibration driver, using
native FiniteDifference for derivative Auto. RequireFast never falls back.
Existing Caplet/Swaption fast *pricing* does not imply a fast basket calibration
driver. Sampling fields do not force AutoFast to MC. An unavailable fast driver
can fall back only with explicit MC opt-in and a complete MC configuration;
ScriptMC requests that route explicitly. Unsupported methods fail with
DerivativeUnavailable; they are never silently downgraded.

Fast fitting requires empty sampling fields, empty held-out observations and
zero path budget. Nondefault MC-only controls are rejected. There is no hybrid
fast fit with MC validation. Explicit MC requires
`evaluation.allow_monte_carlo_calibration=True`, fit and validation sampling,
and a positive int32 `max_paths_per_view`. Each sampling is an existing
`CalibrationMonteCarloSampling_` with at least two batches, at least two paths
per batch, a nonnegative int32 seed and exact stream version
`dal.mrg32-normal-paths/1`; all supplied streams share mode PseudoRandom, seed
and version. Hard payoff, MRG32, Precise normals and no bridge remain fixed.
Native quadrature settings must retain their defaults on MC.

For fit/validation/held-out totals F, V and H, checked arithmetic requires
V > F and F+V+H <= max_paths_per_view <= INT_MAX. Reserved path ranges are
[0,F), [F,F+V) and [F+V,F+V+H), with the core's common draw-dimension guard.
There is no requirement H > F. Nonempty held-out observations require their
sampling; orphan held-out sampling is invalid. Held-out order, quote axes and
acceptance remain independent of the fit objective and Jacobian. If
`evaluation.sampling` is supplied, its paths/seed/generator must exactly match
the fit total/seed/`mrg32`; it cannot supply or override batch counts.
MC derivative Auto selects AAD; explicit FiniteDifference keeps core CRN FD.

For example, with a native model, bound Price observation and explicit valuation:

```python
settings = dal.GSRInstrumentCalibrationSettings_(
    valuation=valuation,
    evaluation=dal.CalibrationEvaluationOptions_(
        evaluation="ScriptMC", allow_monte_carlo_calibration=True),
    fit_sampling=dal.CalibrationMonteCarloSampling_(
        paths_per_batch=8192, batches=8, seed=1024,
        stream_version="dal.mrg32-normal-paths/1"),
    validation_sampling=dal.CalibrationMonteCarloSampling_(
        paths_per_batch=16384, batches=16, seed=1024,
        stream_version="dal.mrg32-normal-paths/1"),
    max_paths_per_view=327680)
fit = dal.Calibrate_GSRInstruments(model, [observation],
    [dal.GSRCalibrationParameter_New(factor=0, knot=0, lower=0.001, upper=0.1)],
    settings=settings)
```

This shows the explicit configuration shape, not an automatic numerical
acceptance guarantee. Three-view sampling with another 16×16384 held-out paths
requires the corresponding held-out observations and total budget 589824.
The frozen statistical limits and independent acceptance flags still apply.

Results own the model, report, parameter labels, actual backend/derivative,
fit/validation values and SE, and independent held-out values and flags.
Report schema remains `dal.calibration-report/1`. Raw Jacobians are M×N
dPrice/dNativeG; the quote map is separate. Unavailable matrices raise
DerivativeUnavailable, and unavailable metadata stays null. Fast quadrature
errors are numerical errors, never statistical SE. Validation records compare
against fit values and use the frozen numerical budget; held-out differences
compare the market target and use its error scale.
Report publication copies source/definition/binding identities and final driver
plan identities without new preparation, pricing or simulation. MC context
identities come from the final fit results. Rule/compiler identities and fast
context identities are unavailable and remain null because the drivers do not
return them. Driver counters retain their original scope; the initial fast-route
admission is separate and can execute native rate-option domain checks.
Convergence, fit tolerance, numerical validation and requested held-out acceptance
are independent: a nonthrowing rejection is Candidate, while invalid inputs or
execution failures throw owning CalibrationError. Last complete models survive
failure and destruction of inputs. Calibrated quote risk remains unavailable.

### Python construction and ownership

All new spec/settings constructors take keyword-only fields, reject unknown
keywords and copy their nested values. Numbers exclude booleans; units and
enum tokens are exact. Handles and their properties are read-only, and
container getters return independent copies. Conversion finishes before
calibration, preparation or MC releases the GIL. No Python callback or
borrowed Python spec is retained by a worker.

For example, using an existing native Deposit and an explicit fixing snapshot:

```python
settings = dal.ScriptInstrumentImportSettings_(
    instrument_id="usd-deposit", currency="USD", notional=100.0,
    discount_route="USD.OIS", min_abs_annuity=1e-12)
instrument = dal.DepositScriptInstrument_New(native_deposit, settings)
quote = dal.CalibrationMarketQuote_New(
    "usd-1y", 0.035, dal.CalibrationValueUnit_(kind="Rate"))
observation = dal.CalibrationObservation_New(
    "fit-1y", instrument, "ParRate", quote, 1e-10)
geometry = dal.CurveInstrumentGeometry_(
    today=today, currency="USD", knot_dates=[today, maturity],
    parameterization="LOG_DISCOUNT")
market = dal.CurveInstrumentMarket_(
    routes=[dal.CurveScriptRoute_(id="USD.OIS")])
valuation = dal.ScriptValuationSettings_(
    evaluation_date=today, fixings=explicit_snapshot)
spec = dal.CurveInstrumentCalibrationSpec_(
    geometry=geometry, market=market, valuation=valuation,
    observations=[observation])
fit = dal.Calibrate_CurveInstruments(
    spec, options=dal.CurveInstrumentCalibrationOptions_(
        jacobian_mode="AAD", compute_quote_jacobian=True))
```

The imported coupon remains fixed. A ParRate observation does not copy its
quote into K. To change the contract, call
`CalibrationInstrument_BindParameters` with a
`CalibrationParameterBindings_` containing typed
`CalibrationParameterValue_(key="K", value=..., unit=...)` rows.
Omitted keys retain their current effective values. The old instrument and
its report stay usable. A QuoteCopy annotation records a copied value's
origin; it creates no live link to a quote.

`CalibrationError` derives from RuntimeError and has read-only `code`,
`diagnostic`, `report` and nullable `candidate` properties. These own their
data after C++ unwinds and after input objects are destroyed. Argument type
errors remain TypeError. Existing convergence exceptions, old defaults,
aliases, Product archives and `CalibrationQuote_New` retain their meanings.

### Explicit same-instrument MC requests

`CalibrationMonteCarloRequest_` requires `measure_id`, `valuation` and
`sampling=CalibrationMonteCarloSampling_(...)`. Supply
`paths_per_batch>=2`, `batches>=2`, a nonnegative seed and
`stream_version="dal.mrg32-normal-paths/1"`. Mode is `PseudoRandom`.
The merged bridge supports no Brownian bridge and Precise normal precision.
Use `CalibrationSimulationSettings_(smooth=0.0)` for these hard-payoff
contracts; the settings type retains the established smoothing default,
which the bounded bridge rejects when it is nonzero.

Derivative method is `None` or `AAD`. AAD uses the Gaussian runtime's
ordered native labels and retains its boundary/degeneracy refusals. The
matched model must contain the calibrated curve and compatible currency,
valuation and numeraire data; sharing a model name proves no equivalence.

Only the requested repricing batches execute. The adapter also reserves
the core bridge's unused, larger validation range: B batches of P+1 paths,
after the B requested batches of P paths. The combined reservation
B*(2P+1) must fit the core's signed 32-bit path bound. No validation run or
numerical acceptance flag is inferred from that reservation.
The report records actual executed ranges, mapping identity, methods and
engine-run count. Standard error comes from independent batch means.
No Sobol IID error or automatic MC calibration fallback is introduced.

### Excel tables and outcomes

New settings factories end in `_New` and take exact two-column field/value
tables. Nested geometry, market, valuation, solver, observations and sampling
are typed handles. `CalibrationDates_New` and `CalibrationNumbers_New`
construct typed vectors; `CurveScriptRoutes_New` takes named columns
`id,projection,collateral,tenor`. `CalibrationObservations_New` takes an
ordered observation-handle vector. Discount/forward binding factories take
`key,curve` tables. Wrong handle types, unknown or duplicate keys, nonfinite
numbers, numeric booleans and malformed tables fail at their input field.

Definition creation takes a `kind,key,text` source table, field/value
header, and named parameter/measure tables. Parameter columns are
`key,type,unit,role,bindable,lower,upper,lower_inclusive,upper_inclusive`;
measure columns are
`id,kind,receiver,parameter,unit,annuity_sign,min_abs_annuity`.
Binding columns are `key,value,unit,quote_id,quote_fingerprint`.
All named columns are required, in any order, with no extras or duplicates.
Optional cells are blank. Event keys accept ISO dates or integral Excel
date serials converted once; Definition keys and source text require text.
Flags require boolean cells.

New execution functions return a typed outcome. Inspect
`CalibrationOutcome_Get_Status` and `CalibrationOutcome_Get_Report`.
A curve or GSR failure returns Failed with its owned diagnostics and, when
available, its last validated curve or complete model. `Get_Curve`, `Get_Model` and `Get_Mean` reject
absent or wrong-kind candidates. Factory and type-conversion errors remain
worksheet errors. No getter uses a thread-local last-error value.

`CALIBRATE.GSRINSTRUMENTS(initial, observations, parameters, settings)` has four
required inputs. Observations are a vector of typed Observation handles;
parameters have exactly the named columns `factor,knot,lower,upper`, in any
order. Indices require nonnegative integral numeric cells, bounds finite numeric
cells, and neither accepts booleans or numeric text. Domain settings are built
by `GSRInstrumentCalibrationSettings_New` from exact field/value rows;
`GSRCalibrationSolver_New` wraps native solver values and
`GSRPricingSettings_New` wraps quadrature settings without changing the old
worksheet parser. Nested settings use typed handles; held-out observations use
`CalibrationObservations_New`. Malformed factory inputs remain worksheet errors
with function/field/row/column context. A valid typed execution returns an owning
Succeeded, Candidate or Failed outcome; `CalibrationOutcome_Get_Model` returns
an available model candidate.

`CalibrationReport_Get_Summary`, `Get_Observations`, `Get_Quotes`,
`Get_Parameters`, `Get_Plans` and `Get_Errors` have fixed header order.
An empty table contains its header row. Unavailable metrics are blank;
unavailable Jacobians raise an error. `Get_QuoteJacobian` is raw
dMeasure/dNativeParameter with the report's observation and parameter axes.
`Get_QuoteMap` is the distinct-quote mapping, not quote risk.
Curve summary evaluation/Jacobian counters describe the solver's existing
counters, including attempted FD work; direct Evaluate does not run that solver.
Initial-fit results retain derivative None unless diagnostics actually run.

The `Get_Json` getters for definitions, instruments and reports return
30,000-character ASCII-escaped chunks in Excel; concatenate them without a
separator. Python returns a single JSON string. Definition and instrument
LoadJson functions validate the portable schema and rebuild identities.
Plans, settings, outcomes and reports remain session objects and reject
archives. Portable Excel tests and generated XLL registration checks do not
establish native Windows or workbook execution.
