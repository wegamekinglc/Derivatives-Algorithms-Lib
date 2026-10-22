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
ON settlement and unrecognized control flow fail explicitly. Payment
dates must match the admitted template, including the Caplet payment lag
described below. The bare Product's last-variable fallback remains available
through its existing entry.

The admitted Deposit has `-N` at S and `N*(1+K*ALPHA)` at T, with valuation at S.
The BondOption has one payment at E, `N*MAX(FIX(IR[CCY,DF,T])-K,0)` or its put
counterpart, with valuation < E < T. All dates are concrete, the currency matches
the header and context, and settlement is AtEvent under same-currency OIS
collateral. Product names and fingerprints never bypass these structural checks.

`CalibrationPreparationContext_` requires an explicit valuation date, fixing
snapshot, currency, discount route and `GSRCurveData_` snapshot. Gaussian
preparation additionally requires an exact one- or multi-factor GSR model with
the same curve. This fixes ACT/365 model time and the native Gaussian numeraire
contract. The hard option requires zero smoothing. A different stochastic model
cannot receive the Gaussian descriptor by providing a matching name.

The returned immutable plan owns its instrument, cashflow metadata, disposable
descriptor, capability and compiler/context/plan identities. A contingent
cashflow has no numeric `amount_`; its metadata retains payment/fixing dates and
source coordinates. The descriptor carries the matched financial structure.
Deposit amounts come from evaluating the original PAYS expressions. The plan
retains no independently editable native product or mutable market object.

Source, header, effective bindings, actual curve/model data, exact fixing
timestamps and values, today policy, convention state and smoothing contribute
to identity. Sampling contributes when MC is selected. Call `RequireCompatible`
with the current instrument and context before reusing a prepared artifact;
changes require preparation again. No shared descriptor cache is maintained.
The rate-option and curve-family contexts additionally track the actual Libor day basis; this
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
Unknown scripts never switch silently to MC.

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

Python and Excel markup use the same factory/action names:
`ScriptInstrumentDefinition_New`, `ScriptCalibrationInstrument_New`,
`CalibrationInstrument_BindParameters`, `DepositScriptInstrument_New`,
`EuropeanRateOptionScriptInstrument_New`, `CalibrationMarketQuote_New`,
`CalibrationObservation_New`, `CalibrationPlan_New`,
`Calibrate_CurveInstruments` and
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
These interfaces do not add a general GSR MC calibration driver, overnight
compounding, futures convexity, XCCY MC or calibrated quote risk.

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
A curve failure returns Failed with its owned diagnostics and, when
available, its last validated curve. `Get_Curve` and `Get_Mean` reject
absent or wrong-kind candidates. Factory and type-conversion errors remain
worksheet errors. No getter uses a thread-local last-error value.

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
