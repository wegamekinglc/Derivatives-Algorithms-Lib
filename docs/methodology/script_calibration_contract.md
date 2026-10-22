# Core script calibration contracts

The core headers `dal/script/instrument.hpp` and `dal/calibration/protocol.hpp`
provide immutable contract data, typed parameter binding and ordered calibration
observations. `dal/script/calibrationpreparation.hpp` adds structural preparation
for Deposit and Gaussian BondOption contracts. Existing Product, curve and GSR
entry points keep their own behavior. The separate core Deposit adapter in
`dal/curve/depositscript.hpp` supports one-node curve fitting with Brent.

A `ScriptInstrumentDefinition_` owns the exact source rows, fixed-date header,
numeric parameter declarations and measures. Source numeric definitions are the
only defaults. The header references N and PAY rather than duplicating notional
or cashflows. The initial schema uses a single currency, OIS collateral, AtEvent
settlement and explicit discount routing. N and K are bindable; Deposit ALPHA is
fixed. P0 parameter roles are Notional, Coupon, Strike and Accrual.
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
fees, delayed payments and unrecognized control flow fail explicitly. The bare
Product's last-variable fallback remains available through its existing entry.

The admitted Deposit has `-N` at S and `N*(1+K*ALPHA)` at T, with valuation at S.
The option has one payment at E, `N*MAX(FIX(IR[CCY,DF,T])-K,0)` or its put
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
cashflow has no numeric `amount_`; it records its DF observation and fixing date.
Deposit amounts come from evaluating the original PAYS expressions. The plan
retains no independently editable native product or mutable market object.

Source, header, effective bindings, actual curve/model data, exact fixing
timestamps and values, today policy, convention state and smoothing contribute
to identity. Sampling contributes when MC is selected. Call `RequireCompatible`
with the current instrument and context before reusing a prepared artifact;
changes require preparation again. No shared descriptor cache is maintained.

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
The adapter is core-only; general-script fitting, caplet/swaption, SLV and
public/Python/Excel importers remain outside its scope.

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
have different units and residuals. BasisSpread remains reserved in the v1
definition schema; scalar-only programs have no shared Price capability.

`AutoFast` and `RequireFast` select the admitted Deposit deterministic or Gaussian
option descriptor. `GenericDeterministic` admits Deposit cashflows; it cannot
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
Deposit cashflows once; candidates refresh native curves and numeric measures
without parsing source or importing native products again. Gaussian options
continue through their separate model adapter.

The provider uses native discount/projection curves, collateral and tenor keys,
and an optional passive base. Route IDs are case-sensitive. Missing routes,
currency/collateral conflicts and unsupported requests fail without a global
market lookup. Ordered multi-curve stages inherit prior curves by key; explicit
stage bindings win; all stages share currency and valuation. Projection stages
inherit their discount base when one is not supplied. Deposit cashflows alone do not constrain projection
parameters; projection-product lowering belongs to the separate family adapters.

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
