# Core script calibration contracts

The core headers `dal/script/instrument.hpp` and `dal/calibration/protocol.hpp`
provide immutable contract data, typed parameter binding and ordered calibration
observations. `dal/script/calibrationpreparation.hpp` adds structural preparation
for Deposit and Gaussian BondOption contracts. Existing Product, curve and GSR
entry points keep their own behavior. These core interfaces do not implement a
calibration solver.

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

Tree and compiled execution use the same hard payoff, including native AAD.
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
