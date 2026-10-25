# DAL-398: Script contract, binding and calibration error API v1

Status: accepted upstream design for the DAL-396 implementation chain. DAL-398
and its P0/P1 implementation issues are complete. The P0 restrictions, proposed
signatures, source citations and implementation questions below describe the
design baseline named here; they are not the current capability inventory.
The [current script calibration guide](../../../docs/methodology/script_calibration_contract.md)
documents the implemented contract, later product families and language interfaces.
Retain this decision as a control for the remaining DAL-396 acceptance work.
Audience: core/compiler, curve/GSR, public C++, Python and Excel implementers, and reviewers.
Research input: DAL-396 latest attachment `01a15c20-3723-7ddd-8ef1-c0ac706cdc16`, §§3–4, 6, 9, 11.
Research baseline, actual checkout HEAD and fetched `origin/master` at design start:
`0f1feabee193d0976ab7a5336c9e9e29cb2aa28d` (2026-10-21).
This note resolves that draft's schema/ABI questions; its numerical acceptance rules remain applicable.

## 1. Ownership and three separate contracts

An instrument owns one immutable definition and one immutable, complete effective binding.
The definition owns script source and the header needed to interpret that source. Native objects
are one-time import inputs. Neither a native option nor a second quote script is retained as a
pricing authority. Fast descriptors, coefficient views, observation plans and compiled programs
are derived, disposable data. All executors consume the same effective parameter slots before
folding constants, replaying history or preparing the model.

| Layer | v1 names | Responsibility |
|---|---|---|
| Contract | `ScriptInstrumentDefinition_`, `CalibrationInstrument_` | Cashflows, dates, fixed contract parameters, currency and settlement |
| Observable | `Price`, `ParRate`, reserved `BasisSpread`/`FxOutright` | What model value is compared with a quote |
| Output representation | `Price`, `Scalar` | Discounted receiver PV versus a derived numeric value |
| Execution | `FastDeterministic`, `FastGaussian`, `GenericDeterministic`, `ScriptMC` | How that observable is evaluated |
| Derivative | `Analytic`, `AAD`, `FiniteDifference`, `None` | How its native-parameter partials are evaluated |

`Price` observable has `Price` representation even under a deterministic fast executor.
`ParRate`/`BasisSpread` have `Scalar` representation derived from a Price contract.
A pure Scalar program without shared PAYS cashflows fails `NoSharedPriceDefinition` at the new
instrument boundary. Existing bare Product calls retain their current behavior. v1 has no
QuoteOnly, volatility conversion, zero-PV-at-quote residual or calibrated quote-risk endpoint.

Core types belong under `dal-cpp/dal/script/` (contract/binding) and a core calibration protocol
header (quotes, observations, report/error); public factories belong in a new
`dal-public/src/calibrationinstrument.hpp`, with domain facades in the existing public component.
Core must not include public headers. Existing examples stay in place and keep their purpose;
any future facade example is a separate `dal-public/examples/script_calibration/` program.

## 2. Scalar types, units and validation vocabulary

Unless a table explicitly says optional, every field is required. A nullable field must be
serialized as null, rather than omitted. C++ fields have trailing `_`; wire fields below are
snake_case. Python exposes the wire spelling as read-only properties. Spec objects may be
assembled mutably, but factories copy, validate and freeze them; retained caller containers
cannot mutate handles. All numeric values are finite IEEE-754 binary64. Booleans are never
accepted as numeric input, including in Python/Excel. Counts/ordinals are nonnegative exact
integers, at most `2^31-1`. Dates are valid Gregorian `YYYY-MM-DD`.

IDs (instrument, quote, observation and measure) match `[A-Za-z0-9][A-Za-z0-9_.:/-]{0,127}`.
They compare byte-for-byte, case-sensitively; do not use `String_`'s case-insensitive comparator
for IDs. DSL identifiers follow the existing case-insensitive language; parameter keys and
receiver references are canonical uppercase ASCII identifiers, so `k` and `K` cannot coexist.
Enum tokens and table keys are case-sensitive. Text must be valid UTF-8 without NUL. Source
text is preserved exactly; no whitespace, newline or Unicode normalization changes its identity.

`CalibrationValueUnit_` has `kind_: CalibrationUnitKind_`, optional `currency_: Ccy_`, optional
`baseCurrency_: Ccy_`, optional `quoteCurrency_: Ccy_`. Only the combinations below are valid.
Wire/Excel use the canonical unit token; C++/Python parse it into this typed value.

| Token | Fields beyond kind | Meaning / allowed values |
|---|---|---|
| `Rate` | none | Decimal annual rate; negative allowed; 1 bp is 0.0001 |
| `BasisSpread` | none | Decimal additive spread in the declared leg convention; negative allowed; P0 unsupported |
| `Price:USD` | currency USD (substitute actual currency) | Currency amount for the bound contract, **not per-unit notional**; signed generally |
| `FxOutright:EUR/USD` | base EUR, quote USD, distinct | USD per EUR, strictly positive; P0 unsupported |
| `BondPrice` | none | Bond-price strike per unit face, dimensionless, strictly positive in P0 |
| `YearFraction` | none | Fixed accrual number, strictly positive in P0 |
| `Dimensionless` | none | Finite numeric constant, only if a future admitted template needs it |

`Price:USD` is also the unit for USD notional N. The compiler knows from its role that N is a
notional, not another payment. Multiplication by N occurs once in the source. There is no
silent percent/bp, per-100, currency or notional normalization. A price quote for N=1,000,000
must be an amount for that contract. A legacy unit-price importer explicitly multiplies both
target and scale by N and records that conversion; the market-quote factory does not.

`CalibrationQuoteConvention_` fields are `id_: CalibrationQuoteConventionId_`, `version_: int`.
v1 accepts `(DecimalRate,1)` with Rate, `(DecimalSpread,1)` with BasisSpread,
`(PresentValueAmount,1)` with Price, `(QuotePerBase,1)` with FxOutright. No other pair is valid.
Convention is quotation semantics, not a second day-count, payment schedule or strike.
When convention is omitted at the factory it is derived solely from unit by this table and
materialized; saved quotes always have it. BondPrice/YearFraction are parameter units, not
market-quote units. Negative option targets are rejected by observation/template admission,
not by a global prohibition on negative Price quotes.

## 3. Definition schema: exactly one owner per economic field

`ScriptInstrumentDefinitionSpec_` contains `source_: ScriptInstrumentSource_`,
`header_: ScriptInstrumentHeader_`, `parameters_: Vector_<ScriptParameterDeclaration_>`,
`measures_: Vector_<CalibrationMeasureSpec_>`. The immutable handle exposes const getters.

| Definition wire field / C++ type | Value and rule |
|---|---|
| `schema` / version tag | Exactly `dal.script-instrument-definition/1` |
| `language` / `String_` | Exactly `dal.script/1`; denotes existing DSL plus typed-slot preparation contract below |
| `semantics` / `String_` | Exactly `dal.calibration-contract/1`; economic interpretation, independent of compiler build |
| `source.rows` / `Vector_<ScriptSourceRow_>` | Ordered `{kind,key,text}` rows; kind `Definition` (key numeric label) or `Event` (key date). No parallel arrays of unequal lengths |
| `source.settings.default_index` / `String_` | Materialized `""` in P0; explicit DF FIX only |
| `source.settings.regression_features` / `Vector_<String_>` | Materialized empty in P0; EXERCISE unsupported |
| `header` / `ScriptInstrumentHeader_` | Required fields in next table; no extra economic sidecar |
| `parameters` / declaration vector | Ordered in source-definition order; every bindable numeric constant declared once |
| `measures` / measure vector | Ordered, nonempty, unique IDs; must include exactly one Price measure with ID `Price` |

P0 source accepts concrete Event dates and finite numeric Definition rows only. It rejects
schedule text, textual macros, vectors, loops, SPOT/default-index indirection, EXERCISE,
extra receivers, delayed PAYS and any structure outside the two templates in §10.
This narrow admission does not restrict the existing bare DSL. An importer expands and
adjusts dates/accrual once, then writes concrete rows, rather than asking each backend to
rebuild a schedule. Non-numeric header/declaration values are never inserted into script text.

| Header field / type | Required P0 value or interpretation |
|---|---|
| `pay_currency` / `Ccy_` | Source PAYS amounts are in this currency |
| `result_currency` / `Ccy_` | Equals pay currency; no implicit conversion |
| `collateral_currency` / `Ccy_` | Equals pay currency |
| `collateral_type` / `CollateralType_` | `OIS` in P0 |
| `receiver` / `String_` | Canonical `PAY`, the single prepared payoff receiver |
| `notional_parameter` / `String_` | `N`; references declaration, no duplicate notional amount |
| `settlement` / enum | `AtEvent`: PAYS on event date, no lag/cash-settlement reinterpretation |
| `valuation_cashflows` / enum | `IncludeUnsettledAtValuation`: event date equals valuation date is live; earlier PAYS excluded |
| `discount_route` / `String_` | Required symbolic route key; context binds actual curve/model; no global fallback |
| `convention_set` / `String_` | `dal.fixed-dates/1`; import convention details have already become concrete source dates/accrual |

There is no header strike, coupon, maturity, payment schedule, accrual value or notional value.
The source owns those. DF observation currency/maturity comes from its canonical FIX syntax;
it must agree with header/context. N's sign does not encode direction in P0: N>0, direction is
the signs of cashflows or CALL/PUT AST shape. Event/payment and observation dates remain distinct.
An importer may retain an optional origin label and native type name as *provenance outside*
the definition; no executor may read these to recover terms or decide fast eligibility.

| Parameter declaration field / type | Rule |
|---|---|
| `key` / `String_` | Unique canonical source numeric-definition label; schema cannot invent an absent slot |
| `type` / enum | `Real` only in v1 |
| `unit` / `CalibrationValueUnit_` | Exact unit token; role/template checks dimensional use |
| `role` / enum | `Notional`, `Coupon`, `Strike`, `Accrual`; exactly N/Notional and one K/Coupon or K/Strike in P0 |
| `bindable` / `bool` | Only N and K true in P0; ALPHA/Accrual false |
| `lower`, `upper` / `optional<double>` | Null means unbounded |
| `lower_inclusive`, `upper_inclusive` / `bool` | Both required, even for null bounds; must be true when corresponding bound null |

There is **no declaration default_value**. The numeric source definition is the sole default.
The constructor resolves that row once, validates it against type/unit/bounds, and records its
source location. Duplicate source keys, case collisions, duplicate declarations, role conflicts,
non-numeric defaults or undeclared numeric constants are errors. P0 domains:
N `(0,+inf)` Price in pay currency; Deposit K `(-inf,+inf)` Rate; BondOption K `(0,+inf)`
BondPrice; ALPHA `(0,+inf)` YearFraction fixed. Future templates may narrow these domains in
a new definition. No default nonnegative-rate or positive-maturity-PV-at-every-K restriction.

`CalibrationMeasureSpec_` fields:

| Field | Rule |
|---|---|
| `id: String_` | Stable, unique; P0 IDs `Price` and optionally `ParRate` |
| `kind: CalibrationMeasureKind_` | `Price`, `ParAffineRate`, reserved `ParAffineSpread` / `FxOutright` |
| `receiver: String_` | Must equal header receiver |
| `parameter: optional<String_>` | null for Price, `K` for affine measures |
| `unit: CalibrationValueUnit_` | Price:pay_currency or Rate respectively |
| `annuity_sign: optional<int>` | null for Price, +1 for Deposit; future spread templates declare +1 or -1 |
| `min_abs_annuity: optional<double>` | null for Price; required finite >0 for affine, in receiver amount per parameter unit |

For affine measures the compiler proves `V(theta,K)=B(theta)+K*A(theta)` from the complete AST,
with K absent from dates, fixing identities and branch conditions. It evaluates `m=-B/A`,
checks the declared sign and `abs(A)>=min_abs_annuity` at every candidate, and differentiates
the full quotient: `dm=-dB/A+B*dA/A^2`. This is never a separately authored rate script.
The annuity threshold is an explicit amount-per-rate constraint; changing N may fail it and
requires a new prepared plan. It is not errorScale or a solver weight. P0 Price receivers may
have a literal-zero initialization before any PAYS; other assignments/overwrites, including
historical or branch-dependent resets after payments, fail `InvalidCalibrationOutputBinding`.

## 4. Complete immutable bindings and identity

`CalibrationParameterValue_` is `{key_: String_, value_: double, unit_: CalibrationValueUnit_}`.
`CalibrationParameterBindings_` is `{values_: Vector_<CalibrationParameterValue_>,
quoteCopies_: Vector_<CalibrationQuoteCopy_> = {}}`; it is an **update request**, never the
stored effective binding. Each quote-copy annotation contains `parameterKey_`, `quoteId_`,
`quoteFingerprint_`; it references an already copied numeric value, with no live quote handle.
Its fingerprint is SHA-256 of the canonical quote's four fields (§5), represented exactly as
`{id,value,unit,convention:{id,version}}` with unit serialized as its token.

Creation starts from every source default, overlays the request, validates exact units and
domains, then freezes a complete ordered value vector (including fixed ALPHA). Rebinding
starts from the instrument's current effective values; omitted keys retain their current
values, never revert to defaults. Supplying fixed ALPHA is an error even with its current
value. Empty updates return an equivalent immutable instance. Duplicate/unknown keys and
QuoteCopy annotations without one matching update fail. A QuoteCopy is provenance only;
ParRate observations never trigger binding changes. Caller must explicitly copy q into K.

Each effective row exposes provenance `SourceDefault`, `Literal` or `QuoteCopy`; the latter
also exposes the quote ID/fingerprint. Rebinding a row without QuoteCopy replaces its origin
with Literal. The complete wire row in §7 stores value, unit, origin and nullable quote fields.
Default rows must match source defaults; fixed rows must always match source defaults.

| Identity | Exact content / scope |
|---|---|
| `sourceIdentity` | `sha256(JCS(source))`; includes source defaults/settings and exact source text |
| `definitionIdentity` | `sha256(JCS(full definition))`; includes schema/language/semantics/header/declarations/measures |
| `bindingIdentity` | `sha256(JCS({definition_identity,values}))`; values are ordered `{key,value,unit}` for all declarations, excluding provenance |
| `instrumentIdentity` | `sha256(JCS({instrument_id,definition_identity,binding_identity,pillar}))`; pillar null when absent |
| `compilerIdentity` | `sha256(JCS({build,parser,lowering,matcher,constant_binding,receiver_guard}))`; each entry is a nonempty version/build token |
| `contextIdentity` | Content fingerprint of resolved valuation/settlement policy, fixing requests and values, routes and curve/model identities/parameters, frozen convention state and model time/numeraire contract |
| `planIdentity` | Fingerprint of definition/binding/compiler/context identities, measure spec, admitted rule ID, actual backend/derivative and effective numerical/sampling settings |

`sha256` yields lowercase 64 hex characters with prefix `sha256:`. Canonical JSON uses
[RFC 8785 JCS](https://www.rfc-editor.org/rfc/rfc8785); source strings are not normalized.
Arrays retain specified order. -0 is normalized to +0 at validation. No NaN/Inf.
IDs/digests are values, not pointer addresses. Equal effective values give the same binding
identity even if provenance differs; an actual K/N change gives a different identity.
Compiler upgrades without semantic changes keep definition/binding identities but rebuild plans.
Economic-rule changes require a new `semantics` version and therefore a new definition identity.

| Change | Required action |
|---|---|
| K/N update | New instrument; invalidate coefficient/fast/historical/prepared/compiled/value/derivative artifacts for old binding; original instrument still usable |
| Source/default/domain/header/measure/date change | New definition and instrument; no in-place editing |
| Compiler/matcher/guard version change | Reparse/re-lower/re-admit; no old descriptor reuse |
| Valuation/fixings/route/model/market/conventions/smoothing change | New resolved context and plan; no global reads after freezing |
| Candidate solver theta changes | New evaluation-context identity; only theta-independent structural analysis can be reused |
| Only target or observation scale changes | Rebuild residual/objective/report identity; contract and eligible structure may be reused |

A hash hit does not prove financial equivalence: the full AST/domain admission remains mandatory.
P0 may conservatively rebuild everything on each bind. Worker constant-array patching after
history replay or constant folding is not a conforming implementation.

## 5. Quote, observation and stable axes

| Immutable object | Exact fields |
|---|---|
| `CalibrationMarketQuote_` | `id_: String_`, `value_: double`, `unit_: CalibrationValueUnit_`, `convention_: CalibrationQuoteConvention_` **only** |
| `CalibrationInstrument_` | `id_: String_`, `definition_: Handle_<ScriptInstrumentDefinition_>`, complete `binding_`, `pillar_: optional<Date_>`; derived identities/getters, optional non-authoritative origin |
| `CalibrationObservation_` | `id_: String_`, `instrument_: Handle_<CalibrationInstrument_>`, `measureId_: String_`, `quote_: Handle_<CalibrationMarketQuote_>`, `errorScale_: double` |

No scale/weight/product/executor resides in a market quote. errorScale is strictly positive,
finite, in the row's quote/measure unit. Unit and convention must match the selected measure
exactly; v1 has no implicit observation conversion. A pillar is curve-layout metadata, not a
second maturity; it must be compatible with cashflow dependencies at curve admission.

The spec's observation vector defines row ordinal `i` (zero-based), never insertion order of
a map. Observation IDs are unique. Within a run, the same instrument ID must identify the
same definition/binding/pillar, and the same quote ID must have identical value/unit/convention;
conflicting duplicates fail before solving. Identical quote IDs collapse to one quote axis.
Distinct quote columns are ordered by first appearance in observation order.

For M observations, Q distinct quotes and N native parameters, publish `quoteIndex[i]` and
`B_map[i,j]=1 if quoteIndex[i]==j else 0` (M×Q). v1 target is exactly q, so
`rawResidual[i]=modelMeasure[i]-q[quoteIndex[i]]`, `scaledResidual[i]=rawResidual[i]/errorScale[i]`,
`F_q=-B_map`, and scaled `F_q=-diag(1/errorScale)*B_map` for fixed bindings.
`quoteJacobian_` retains the research/legacy name but means M×N **raw model-measure partials**
with respect to native parameters; it does not mean N×Q calibrated quote risk.

Example: observations `[oA(I1,ParRate,Q1,1e-8), oB(I2,ParRate,Q1,2e-8),
oC(I3,Price,Q2,1e-4)]` give quote axis `[Q1,Q2]`, indices `[0,0,1]`,
`B_map=[[1,0],[1,0],[0,1]]`. Shared quotes may have different observation scales. Reordering
rows changes row/first-use axes explicitly, never the observation IDs. Copying Q1 into K is
a separate frozen contract change; it does not introduce an implicit F_q chain into this residual.

For par `r=m-q` and an explicitly rebound K=q, zero-PV residual would be `F=-A*r`,
`F_theta=-A*dm-r*dA`, `F_q=A*B_map`. Those derivatives and amount scales differ off root.
v1 does not expose PVAtQuote or claim its regularized/underdetermined solution equals par fitting.

## 6. Execution and domain spec boundaries

`CalibrationEvaluationOptions_`: `evaluation_=AutoFast`, `derivativeMethod_=Auto`,
`allowMonteCarloCalibration_=false`, optional `sampling_` (null by default).
The request values are `AutoFast`, `RequireFast`, `GenericDeterministic`, `ScriptMC`; report
backend values are the four actual executors in §1. Auto derivative chooses only a supported
method and records the choice. A caller explicitly requiring AAD cannot silently receive FD.
P0 Deposit fast supports analytic/templated AAD/FD as implemented and verified; Gaussian fast
is double with native-g FD, not analytic AAD. MC AAD is a separate plan. A plan comparing
the Gaussian hard-payoff price must preserve that payoff for value and derivative. If a
backend's fuzzy/smoothed target differs, reject equivalence or report a separate smoothing
bias and use a matching-target fast formula; never label the hard and soft objectives identical.

AutoFast prefers the complete admitted deterministic/analytic template, then an admitted
generic deterministic evaluation, and fails if neither exists. It only considers MC when
`allowMonteCarloCalibration=true` **and** explicit valid sampling is present. ScriptMC as a
calibration request also requires this opt-in; a standalone MC reprice is already explicit.
RequireFast failures carry all rejected-rule reasons and never fall back. Price value support
and derivative support are separate admission checks. GenericDeterministic is not permission
to replace a stochastic expectation with forward substitution; BondOption needs Gaussian fast
or explicitly supported MC. P0 does not promise arbitrary-script deterministic evaluation.

`CurveInstrumentCalibrationSpec_` owns `geometry_`, `market_`, `valuation_`, `observations_`,
`solver_`. Geometry contains today/currency/name, knot dates, parameterization, knot policy,
log-DF scheme, base curve and target component/tenor/collateral. It is validated directly,
never by building an empty legacy instruments spec. Market holds explicit discount/projection
route bindings. Valuation requires an explicit evaluation date equal to geometry today and
an explicit snapshot (an empty snapshot is allowed); P0 timestamps are midnight. Freeze
mutable global conventions/history once at ingress or reject the request.

`CurveInstrumentCalibrationOptions_` owns `evaluation_`, `jacobianMode_` and diagnostics options.
Solver keeps the existing single-curve initial guess 0.05, smoothingWeight 1, budgets 200/20
and EXACT as new-constructor starting defaults; per-row errorScale replaces a global fit
unit. Native layout and initial guesses retain their existing meaning. EXACT accepts finite
outputs with scaled infinity norm <=1. APPROXIMATE requires explicit finite positive
`fitWeight=lambda` and `fitNormTolerance=eta`, with fixed theta0, S and transformed W;
its fit test is scaled L2<=eta, not RMS or each-row tolerance. Report all of these separately.
The precise regularized local equations and homogeneous legacy import mapping are DAL-396 §4.3;
no default inferred from a mixed-unit sum of scales is allowed. These driver changes are
downstream implementation, not changes to the legacy solver's existing controls.

`GSRInstrumentCalibrationSettings_` contains `solver_: GSRCalibrationSettings_` (unchanged
native g scale/bounds/prior/smoothing interpretation), `valuation_`, `evaluation_`, and an
optional independent `validationSampling_`. Initial model is a separate required argument;
valuation date/currency/curve routes must match it. Parameters are selected existing
`GSRCalibrationParameter_` coordinates; report stable `g:<factorName>:<date>` labels, bounds,
scales and order. Invalid/unsupported inputs and computation failures throw; optimizer
MaxIterations/NoDescent/StepStalled returns a candidate with separate acceptance flags.

`XccyInstrumentCalibrationSpec_` and facade names are reserved for the later domain task;
P0 fails `MCCrossCurrencyUnsupported` for MC and `UnsupportedCalibrationProduct` for XCCY
construction/calibration. No placeholder successful XCCY result or invented random model.

`CalibrationMonteCarloRequest_`: required `measureId_`, `valuation_`, `sampling_`;
optional `simulation_: CalibrationSimulationSettings_ = {}`, `derivativeMethod_=None`.
This new limited simulation value has `useBb_=false`, `compiled_=nullopt`,
`smooth_=Script::DEFAULT_SMOOTH`, `normalPrecision_="Precise"` only. The adapter constructs
existing core MonteCarloSettings_ internally, derives enableAad from derivativeMethod, and
derives the RNG from sampling mode. Thus no second caller-controlled derivative/RNG flag
competes with the request. It does not change any legacy settings type or default.
Sampling fields are `mode_` (`PseudoRandom` in P0), `pathsPerBatch_>1`, `batches_>=2`,
`seed_` (nonnegative exact 32-bit integer), `streamVersion_` (required nonempty implementation
version). Normal precision belongs only to simulation and accepts the existing exact tokens
`Default`, `Fast`, `Precise`; no competing copy belongs to sampling.
Batch ID and seed must map to disjoint reproducible streams; freeze the mapping for CRN FD.
P0 maps PseudoRandom to `rsg_="mrg32"` only and records the effective stream mapping; the existing
generic simulation driver has a fixed seed 1024, so repeatedly invoking it is not an independent
batch implementation. The new adapter must explicitly supply/seek validated batch streams.
For Price, SE is sampleSD(batch means)/sqrt(batches), not path dispersion or duplicate-run SE.
Deterministic Sobol has no IID standard error; RQMC replicate protocol is a later extension.
This struct needs no arbitrary adequate default path count. Budget/error acceptance belongs
to the explicit validation request. For a par measure MC returns `-mean(B)/mean(A)`, never
`mean(-B/A)`; estimate uncertainty and derivatives with paired coefficient covariance.

## 7. Persistence and version evolution

The new portable format is UTF-8 JSON `dal.calibration-instrument/1`; a saved definition alone
uses `dal.script-instrument-definition/1`. It carries the full definition, complete effective
binding and pillar, no native object/market/model/compiled cache. The root fields are exactly
`schema`, `instrument_id`, `definition`, `binding`, `pillar`. Binding fields are exactly
`schema=dal.calibration-parameter-binding/1` and `values`. Each value row has the six fields in
the example below. Optional import origin is session/report provenance, not part of this wire.
Identities are recomputed, not trusted from optional supplied hash fields (which v1 rejects).

The following is a complete valid Deposit specimen, not a Product archive. Property order
is for readability; dump emits JCS. Every null/bool/number is typed, not a text escape convention.

```json
{
  "schema": "dal.calibration-instrument/1",
  "instrument_id": "usd-deposit-1y",
  "definition": {
    "schema": "dal.script-instrument-definition/1",
    "language": "dal.script/1",
    "semantics": "dal.calibration-contract/1",
    "source": {
      "rows": [
        {"kind": "Definition", "key": "N", "text": "1"},
        {"kind": "Definition", "key": "K", "text": "0.025"},
        {"kind": "Definition", "key": "ALPHA", "text": "1.0138888888888888"},
        {"kind": "Event", "key": "2026-10-21", "text": "pay PAYS -N"},
        {"kind": "Event", "key": "2027-10-21", "text": "pay PAYS N*(1+K*ALPHA)"}
      ],
      "settings": {"default_index": "", "regression_features": []}
    },
    "header": {
      "pay_currency": "USD", "result_currency": "USD", "collateral_currency": "USD",
      "collateral_type": "OIS", "receiver": "PAY", "notional_parameter": "N",
      "settlement": "AtEvent", "valuation_cashflows": "IncludeUnsettledAtValuation",
      "discount_route": "USD.OIS", "convention_set": "dal.fixed-dates/1"
    },
    "parameters": [
      {"key": "N", "type": "Real", "unit": "Price:USD", "role": "Notional", "bindable": true,
       "lower": 0, "upper": null, "lower_inclusive": false, "upper_inclusive": true},
      {"key": "K", "type": "Real", "unit": "Rate", "role": "Coupon", "bindable": true,
       "lower": null, "upper": null, "lower_inclusive": true, "upper_inclusive": true},
      {"key": "ALPHA", "type": "Real", "unit": "YearFraction", "role": "Accrual", "bindable": false,
       "lower": 0, "upper": null, "lower_inclusive": false, "upper_inclusive": true}
    ],
    "measures": [
      {"id": "Price", "kind": "Price", "receiver": "PAY", "parameter": null,
       "unit": "Price:USD", "annuity_sign": null, "min_abs_annuity": null},
      {"id": "ParRate", "kind": "ParAffineRate", "receiver": "PAY", "parameter": "K",
       "unit": "Rate", "annuity_sign": 1, "min_abs_annuity": 1e-12}
    ]
  },
  "binding": {
    "schema": "dal.calibration-parameter-binding/1",
    "values": [
      {"key": "N", "value": 1, "unit": "Price:USD", "origin": "SourceDefault", "quote_id": null, "quote_fingerprint": null},
      {"key": "K", "value": 0.03, "unit": "Rate", "origin": "Literal", "quote_id": null, "quote_fingerprint": null},
      {"key": "ALPHA", "value": 1.0138888888888888, "unit": "YearFraction", "origin": "SourceDefault", "quote_id": null, "quote_fingerprint": null}
    ]
  },
  "pillar": "2027-10-21"
}
```

Load verifies exact fields/types, unique JSON keys, version compatibility, complete binding
order/units/domains and source consistency. No missing binding rows are filled from defaults
on load. `SourceDefault` requires null quote fields and matching default; `Literal` requires
null quote fields; `QuoteCopy` requires a valid quote ID and fingerprint. It cannot establish
current market freshness. Fixed declarations must be SourceDefault. Recompile/admit on first
use under current compiler/context; never deserialize a supposedly trusted fast certificate.

Round trip (proposed C++, not runnable at baseline):

```cpp
auto restored = LoadCalibrationInstrumentJson(DumpCalibrationInstrumentJson(instrument));
// Same definition/binding/instrument identities and full effective values/provenance.
// K remains 0.03 despite source default 0.025; no model/quote/context is resurrected.
auto rebound = BindCalibrationInstrumentParameters(restored, {{{"K", 0.04, rateUnit}}, {}});
// Same definition, different binding; restored still has K=0.03.
```

All six version axes are independent: definition wire, binding wire, instrument envelope,
language, contract semantics and compiler implementation. v1 rejects unknown fields/enum values/versions
with field path and supported set. Future additive wire fields require a new wire version with
an explicit reader migration, never silent ignore. Meaning changes require new semantics;
migration returns a new definition and discloses changed identities. Readers continue to
accept supported old versions, writers emit their explicit current version. Unsupported older
versions fail rather than executing a guessed interpretation. Report schema evolves separately.

For DAL archive integration, register new object types
`ScriptInstrumentDefinition_v1` / `CalibrationInstrument_v1`; Write stores `json` containing
the canonical respective portable document and Read delegates to the same validator. Do not
claim persistence via an empty Write. No new definition/binding fields are added to existing
`ScriptProductData_v1/v2/v3` readers or the current v3 writer. A raw Product archive remains a
raw Product and does not become a calibration contract without an explicit header/schema import.
Prepared plans, results and reports are owning session objects; their Write throws
`CalibrationPersistenceUnsupported`. Portable full calibration replay/quote-risk records remain
future work; saving an instrument alone does not save a calibration or its market snapshot.

## 8. C++, Python and Excel signatures

These are **new** entry points. Required arguments come first, all optional defaults last.
No positional factory exceeds five economic arguments; compound settings use typed structs.

```cpp
Handle_<ScriptInstrumentDefinition_> NewScriptInstrumentDefinition(
    const ScriptInstrumentDefinitionSpec_& spec);
Handle_<CalibrationInstrument_> NewScriptCalibrationInstrument(
    const ScriptCalibrationInstrumentSpec_& spec);
// spec = {id_, definition_, bindings_={}, pillar_=nullopt}
Handle_<CalibrationInstrument_> BindCalibrationInstrumentParameters(
    const Handle_<CalibrationInstrument_>& instrument, const CalibrationParameterBindings_& updates);
Handle_<CalibrationInstrument_> NewDepositScriptInstrument(
    const Handle_<YCInstrument_>& deposit, const ScriptInstrumentImportSettings_& settings);
Handle_<CalibrationInstrument_> NewEuropeanRateOptionScriptInstrument(
    const EuropeanRateOption_& option, const ScriptInstrumentImportSettings_& settings);
Handle_<CalibrationMarketQuote_> NewCalibrationMarketQuote(
    const String_& id, double value, const CalibrationValueUnit_& unit,
    const std::optional<CalibrationQuoteConvention_>& convention = std::nullopt);
Handle_<CalibrationObservation_> NewCalibrationObservation(
    const String_& id, const Handle_<CalibrationInstrument_>& instrument, const String_& measureId,
    const Handle_<CalibrationMarketQuote_>& quote, double errorScale);
CurveInstrumentCalibrationResult_ CalibrateCurveInstruments(
    const CurveInstrumentCalibrationSpec_& spec, const CurveInstrumentCalibrationOptions_& options = {});
GSRInstrumentCalibrationResult_ CalibrateGSRInstruments(
    const Handle_<ModelData_>& initial, const Vector_<Handle_<CalibrationObservation_>>& observations,
    const Vector_<GSRCalibrationParameter_>& parameters, const GSRInstrumentCalibrationSettings_& settings);
CalibrationMonteCarloResult_ ValueCalibrationInstrumentByMonteCarlo(
    const Handle_<CalibrationInstrument_>& instrument, const Handle_<ModelData_>& model,
    const CalibrationMonteCarloRequest_& request);
String_ DumpScriptInstrumentDefinitionJson(const Handle_<ScriptInstrumentDefinition_>& definition);
Handle_<ScriptInstrumentDefinition_> LoadScriptInstrumentDefinitionJson(const String_& json);
String_ DumpCalibrationInstrumentJson(const Handle_<CalibrationInstrument_>& instrument);
Handle_<CalibrationInstrument_> LoadCalibrationInstrumentJson(const String_& json);
```

GSR settings is required because frozen valuation is required; this deliberately replaces the
research sketch's `settings={}` *for this new function only*. Legacy GSR defaults are unchanged.
`ScriptInstrumentImportSettings_` requires `instrumentId_`, `currency_`, `notional_`,
`discountRoute_`, `minAbsAnnuity_` for Deposit (null for option); optional `pillar_=nullopt`.
P0 infers only the fixed AtEvent/same-currency OIS header values stated in §3 and materializes
them; incompatible native collateral is rejected. Deposit must dynamic-cast to Deposit_,
uses TradeDate/TimeSpan/FloatConvention/MarketRate once, validates its expanded cashflows,
copies MarketRate into source K, and records its origin. Option must hold BondOption_, not
Caplet_/Swaption_; it writes strike/type/dates into source once. Import settings never override
those native dates/strike. N is explicit since native rate instruments have no notional.

Curve/GSR results retain domain `curve_` / `model_` and add required
`report_: Handle_<CalibrationReport_>`. MC result has `mean_`, optional `standardError_`,
`report_`; derivatives/axes are on report. No untyped JSON is accepted instead of construction
specs; explicit LoadJson is the sole portable-format ingress.

| C++ | Python / Excel markup | Excel display name |
|---|---|---|
| NewScriptInstrumentDefinition | ScriptInstrumentDefinition_New | SCRIPTINSTRUMENTDEFINITION.NEW |
| NewScriptCalibrationInstrument | ScriptCalibrationInstrument_New | SCRIPTCALIBRATIONINSTRUMENT.NEW |
| BindCalibrationInstrumentParameters | CalibrationInstrument_BindParameters | CALIBRATIONINSTRUMENT.BINDPARAMETERS |
| NewDepositScriptInstrument | DepositScriptInstrument_New | DEPOSITSCRIPTINSTRUMENT.NEW |
| NewEuropeanRateOptionScriptInstrument | EuropeanRateOptionScriptInstrument_New | EUROPEANRATEOPTIONSCRIPTINSTRUMENT.NEW |
| NewCalibrationMarketQuote | CalibrationMarketQuote_New | CALIBRATIONMARKETQUOTE.NEW |
| NewCalibrationObservation | CalibrationObservation_New | CALIBRATIONOBSERVATION.NEW |
| CalibrateCurveInstruments | Calibrate_CurveInstruments | CALIBRATE.CURVEINSTRUMENTS |
| CalibrateGSRInstruments | Calibrate_GSRInstruments | CALIBRATE.GSRINSTRUMENTS |
| ValueCalibrationInstrumentByMonteCarlo | CalibrationInstrument_ValueByMonteCarlo | CALIBRATIONINSTRUMENT.VALUEBYMONTECARLO |
| Dump/LoadScriptInstrumentDefinitionJson | ScriptInstrumentDefinition_Get_Json / ScriptInstrumentDefinition_LoadJson | SCRIPTINSTRUMENTDEFINITION.GET.JSON / SCRIPTINSTRUMENTDEFINITION.LOADJSON |
| Dump/LoadCalibrationInstrumentJson | CalibrationInstrument_Get_Json / CalibrationInstrument_LoadJson | CALIBRATIONINSTRUMENT.GET.JSON / CALIBRATIONINSTRUMENT.LOADJSON |

Python signatures follow C++ argument order, with `*, convention=None`, `*, options=None`,
`*, settings` on the three respective calls; the MC request and import settings are required
typed objects. Every spec/settings constructor uses keyword-only fields, copies nested values,
and rejects unknown keywords/types. Convert and own all Python inputs before releasing GIL;
no worker callbacks into Python or borrowing spec/tape storage. Read-only handle properties
return immutable objects or independent copies of vectors/tables.

Excel signatures (typed handle names below are new Storable wrappers, not generic strings):

```text
SCRIPTINSTRUMENTDEFINITION.NEW(source, header, parameters, measures, [product_settings]) -> Definition
SCRIPTCALIBRATIONINSTRUMENT.NEW(id, definition, [bindings], [pillar]) -> Instrument
CALIBRATIONINSTRUMENT.BINDPARAMETERS(instrument, bindings) -> Instrument
DEPOSITSCRIPTINSTRUMENT.NEW(deposit, import_settings) -> Instrument
EUROPEANRATEOPTIONSCRIPTINSTRUMENT.NEW(option, import_settings) -> Instrument
CALIBRATIONMARKETQUOTE.NEW(id, value, unit, [convention]) -> MarketQuote
CALIBRATIONOBSERVATION.NEW(id, instrument, measure_id, quote, error_scale) -> Observation
CALIBRATE.CURVEINSTRUMENTS(spec, [options]) -> CalibrationOutcome
CALIBRATE.GSRINSTRUMENTS(initial, observations, parameters, settings) -> CalibrationOutcome
CALIBRATIONINSTRUMENT.VALUEBYMONTECARLO(instrument, model, request) -> CalibrationOutcome
CALIBRATIONOUTCOME.GET.REPORT(outcome) -> CalibrationReport
CALIBRATIONOUTCOME.GET.STATUS(outcome) -> string
CALIBRATIONOUTCOME.GET.CURVE(outcome) / GET.MODEL(outcome) / GET.MEAN(outcome) -> domain value
CALIBRATIONREPORT.GET.SUMMARY(report) / GET.OBSERVATIONS(report) / GET.PARAMETERS(report) -> spill
CALIBRATIONREPORT.GET.QUOTES(report) / GET.PLANS(report) / GET.ERRORS(report) -> spill
CALIBRATIONREPORT.GET.QUOTEJACOBIAN(report) / GET.QUOTEMAP(report) -> numeric spill
CALIBRATIONREPORT.GET.JSON(report) -> text chunks
```

Source is a three-column table `kind,key,text`; Event keys may be Excel dates converted once
to Date_, but numeric serials in Definition keys fail. Header/settings/import options are
two-column exact `field,value` tables (keys from the corresponding struct's wire spelling).
Parameters/measures/binding rows have named header rows and precisely their schema columns;
binding input columns are `key,value,unit,quote_id,quote_fingerprint` (last two blank unless
QuoteCopy). Unknown/duplicate/missing columns and duplicate row keys fail. Blank optional
tables mean omitted; a partially filled row fails. Omitted product_settings materializes empty
default_index/regression_features. observations are an ordered one-column vector of typed
Observation handles; GSR parameters are strict `factor,knot,lower,upper` rows. Compound curve
spec, evaluation settings and MC request use typed handles built from fields in §6, not JSON
or a mutable global registry of tables. Their factory names are
`CurveInstrumentCalibrationSpec_New`, `CalibrationEvaluationOptions_New`,
`GSRInstrumentCalibrationSettings_New`, `CalibrationMonteCarloRequest_New` (each takes one
strict field/value table; nested geometry/market/valuation/sampling are typed handles).
Metadata/source/identity getters follow `<Type>_Get_<Field>` and cannot edit descriptors.

## 9. Diagnostics, errors and stable report tables

Add a **new**, independently exported `CalibrationError_ : public Exception_` with an owning
private `std::shared_ptr<const CalibrationFailure_>` payload. It does not modify Exception_,
ScriptError_, Underdetermined::ConvergenceError_ or the CalibrationConvergenceError_ alias.
New ABI is rebuilt with consumers; no claim of binary compatibility for a previously nonexistent
class. Payload types are core-owned, copy-safe, have no borrowed worker/tape references.

```cpp
class BASE_EXPORT CalibrationError_ : public Exception_ {
public:
    CalibrationError_(const std::string& file, long line, const std::string& function,
                      std::shared_ptr<const CalibrationFailure_> failure);
    const CalibrationFailure_& Failure() const noexcept;
private:
    std::shared_ptr<const CalibrationFailure_> failure_;
};
```

`CalibrationFailure_` fields: `diagnostic_: CalibrationDiagnostic_`,
`report_: Handle_<CalibrationReport_>` (always nonnull, possibly partial),
`candidate_: Handle_<Storable_>` (nullable domain curve/model; typed getters check kind).
`CalibrationDiagnostic_` fields: `code_`, `message_`, `stage_` (`Input`, `Prepare`, `Evaluate`,
`Solve`, `Validate`, `Load`), nullable `field_`, `observationId_`, `instrumentId_`, `quoteId_`,
`measureId_`, `parameterKey_`, `sourceRow_`, `eventDate_`, `statement_`, `actual_`, `expected_`.
Source row is one-based; statement is zero-based; field paths use wire JSON-pointer syntax.
actual/expected are safe UTF-8 text, never raw handles or whole market dumps. what() begins
with code and concise offending field/constraint; callers inspect fields rather than parse it.

On a new curve fit/budget/numerical failure, throw this error with last *fully validated*
candidate if available; partial or nonfinite candidate states are not exposed as a priceable
curve. New GSR optimizer termination returns candidate and flags, while invalid/admission/
evaluation errors throw. Catch internal ScriptError_/ConvergenceError_ at the new boundary,
preserve code/source/cause in diagnostics, and cancel/drain workers before throwing. Never
swallow an evaluator failure as an optimizer retry. Old entry points retain old exception behavior.

Python exports `CalibrationError(RuntimeError)` with read-only `code`, `diagnostic`, `report`,
`candidate` (None if absent), preserving ownership after C++ unwind. Add a specific translator
that recognizes this type ahead of generic lenient-runtime translation, preserving UTF-8
error data. Python argument *type* errors still raise TypeError before entry; typed but invalid
values raise CalibrationError with a partial Input report. No change to legacy
`_CalibrationConvergenceError` or RuntimeError mappings.

Excel new execution facades catch CalibrationError and return `StorableCalibrationOutcome_`
with status `Succeeded`, `Candidate` (GSR optimizer not converged), or `Failed`; it owns the
report and nullable domain candidate. Status Succeeded for calibration requires all applicable
acceptance flags; converged GSR with failed fit/numerics is Candidate. Outcome getters never
read thread-local last-error state. Domain getters may return a fully validated Candidate;
they fail with `CalibrationCandidateUnavailable` for absent/wrong-kind values. Users must
check status/flags; a candidate is not an accepted calibration. Factory/type-conversion failures
remain normal worksheet errors with offending cell/field; there is no outcome yet. Unexpected
non-calibration failures propagate normal worksheet errors. Do not change old worksheet APIs.

`CalibrationReport_` has `schema_=dal.calibration-report/1` and the following exact groups.
All inapplicable/unavailable values are optional/null; missing matrices are null, never zero
matrices presented as evidence. Invalid-input reports may have empty axes. JSON includes all
groups, even empty, and uses null for unavailable metrics rather than NaN/Inf.

| Group | Fields and order (also stable Excel column/key order) |
|---|---|
| `summary` (ordered key/value rows) | `schema,domain,status,stage,mode,converged,fit_within_tolerance,numerical_validation_passed,termination_reason,raw_l2,scaled_l2,scaled_infinity,fit_weight,fit_norm_tolerance,objective,data_objective,penalty_objective,iterations,evaluations,jacobian_evaluations,jacobian_rank,jacobian_condition_estimate,context_identity,compiler_identity,quote_risk_available` |
| `observations` (M rows) | `ordinal,observation_id,instrument_id,definition_identity,binding_identity,measure_id,representation,quote_id,quote_index,unit,target,error_scale,model_value,raw_residual,scaled_residual,fit_row_passed,validation_value,validation_error,standard_error,backend,derivative_method,plan_identity` |
| `quotes` (Q rows) | `quote_index,quote_id,value,unit,convention_id,convention_version` |
| `parameters` (N rows) | `ordinal,id,unit,initial,value,scale,lower,upper,active_bound` |
| `plans` (ordered per observation/attempt) | `observation_id,measure_id,rule_id,admitted,reason_code,reason,backend,derivative_method,source_identity,definition_identity,binding_identity,compiler_identity,context_identity,plan_identity` |
| `errors` (ordered diagnostics) | `code,message,stage,field,observation_id,instrument_id,quote_id,measure_id,parameter_key,source_row,event_date,statement,actual,expected` |
| `bindings` (first-use instrument, declaration order) | `instrument_id,instrument_identity,parameter_key,value,unit,origin,quote_id,quote_fingerprint` |
| `objective_settings` | `theta0[N],parameter_scales[N],native_layout[N],weight_matrix[N,N] or null,weight_provenance,lambda or null,eta or null,legacy_import or null` |
| `execution` | `valuation_date,today_fixing_policy,fixing_snapshot_identity,route_identities,model_identity,model_time_basis,numeraire_convention,convention_state_identity,simulation,sampling,validation_sampling` (fully materialized settings or null) |
| `validation` | ordered records `{observation_id,edge,quantity,value,difference,standard_error,absolute_tolerance,sigma_multiplier,accepted,reason}`; edges OracleFast/FastMC/OracleMC, quantity Price/NativeDerivative/ParRate |
| `quote_jacobian` | M×N raw model partials (C++ quoteJacobian_); rows/columns exactly observation/parameter axes; null if unavailable |
| `quote_map` | M×Q B_map from §5; target derivation only, not quote risk |

quote_risk_available is false throughout P0. raw_l2 is descriptive only and is null for mixed-unit rows (no dimensionally meaningless
aggregate). Scaled norms are dimensionless. fit_row_passed is abs(scaled residual)<=1 even
in APPROXIMATE; overall fit flag uses the mode's criterion. `objective` and its data/penalty
parts use the actual domain objective and native/normalized coordinates in objective_settings;
EXACT may leave scalar objective null. GSR prior/smoothing have their own settings and do not
masquerade as curve W/lambda. QuoteJacobian row unit is observation unit and column unit is
parameter unit, so entry unit is row/column. Output backend/derivative labels reflect actual
execution, including validation, not the requested enum.

Spills include one header row, preserve the listed column order, use blank cells for null,
numeric cells for finite numbers, booleans for flags and ISO text for dates. Zero-row tables
spill just headers. Numeric matrices spill numeric data without headers; their axes come from
the corresponding row tables. An unavailable numeric matrix returns a clear worksheet error,
not a 1×1 zero. JSON getter reuses 30,000-character ASCII-escaped chunks; concatenate without
separators. Additive report columns require report schema /2; no silent reordering. Nested
objective/execution/validation/bindings remain accessible through Get_Json in v1; additional
dedicated getters may be added without changing existing spills.

| Failure / stable code | Required offending context |
|---|---|
| `InvalidCalibrationQuote` | quote ID, value/unit/convention field, finite/compatible constraint |
| `InvalidCalibrationObservation` | observation ID, quote/instrument/measure IDs, scale or conflicting ID path |
| `InvalidCalibrationParameterBinding` | key, actual value/unit, declared domain/unit; unknown/duplicate/fixed slot |
| `InvalidScriptInstrumentDefinition` | header/source/declaration path and conflicting value/source row |
| `NoSharedPriceDefinition` / `InvalidCalibrationOutputBinding` | receiver/source/assignment or missing PAYS |
| `UnsupportedCalibrationMeasure` / `InvalidCalibrationAnnuity` | measure/K dependency, A/sign/minimum and candidate |
| `UnsupportedCalibrationProduct` / `UnsupportedFastEvaluator` | unsupported template or every failed rule/model constraint |
| `DerivativeUnavailable` | requested derivative, measure, backend and parameter coordinate |
| `InvalidCalibrationPlan` / `CalibrationContextMismatch` | expected/actual definition/binding/context/compiler identities |
| `UnsupportedCalibrationFixingTime` / `UnsupportedHistoricalIndex` | exact request date/time/index and valuation/today policy |
| `MCCrossCurrencyUnsupported` | currency/collateral/model incompatibility, no matching stochastic model |
| `InvalidCalibrationObjective` | missing/inconsistent scale/lambda/eta/layout/weight field |
| `CalibrationFitFailed` / `CalibrationBudgetExceeded` / `CalibrationNumericalFailure` | mode, last validated norms/candidate, budgets, parameter/observation IDs |
| `UnsupportedCalibrationSchema` / `InvalidCalibrationArchive` | JSON-pointer, actual version/type/field, supported constraint |

Normal Python usage (proposed; t0/end/native/model are caller's existing typed inputs):

```python
instrument = dal.DepositScriptInstrument_New(native, dal.ScriptInstrumentImportSettings_(
    instrument_id="usd-deposit-1y", currency="USD", notional=1.0,
    discount_route="USD.OIS", min_abs_annuity=1e-12))
quote = dal.CalibrationMarketQuote_New("usd-1y", 0.025, dal.CalibrationValueUnit_(kind="Rate"))
obs = dal.CalibrationObservation_New("fit-1y", instrument, "ParRate", quote, 1e-8)
spec = dal.CurveInstrumentCalibrationSpec_(geometry=geometry, market=market,
    valuation=valuation, observations=[obs], solver=solver)
fit = dal.Calibrate_CurveInstruments(spec)
pv = dal.CalibrationInstrument_ValueByMonteCarlo(instrument, matched_model, mc_price_request)
# Same bound contract in both calls; editing quote.value is not supported.
```

The caller supplies geometry/market/solver and an explicit frozen valuation;
mc_price_request selects Price and explicit sampling/valuation. matched_model must reflect
the calibrated curve, with matching currency/date/routing; its name is not an equivalence proof.

Error examples (proposed):

```text
InvalidCalibrationParameterBinding: instrument=usd-deposit-1y; parameter=N;
field=/bindings/values/0/value; actual=0; expected=finite Price:USD in (0,+inf)
InvalidCalibrationObservation: observation=fit-1y; field=/error_scale;
actual=0; expected=finite Rate > 0
UnsupportedFastEvaluator: instrument=bond-call; measure=Price;
actual=MultiFactorSLV; expected=MultiFactorGaussian for dal.gaussian-bond-option/1
InvalidCalibrationArchive: field=/binding/values/1/unit;
actual=BondPrice; expected=Rate declared for K
```

## 10. P0 admission: Deposit and Gaussian BondOption only

| Rule | Contract and model constraints | Observable / execution |
|---|---|---|
| `dal.deposit-origination/1` | V=trade=start, midnight, S<T, concrete ACT/360 or explicitly imported positive alpha; source N/K/ALPHA plus `pay PAYS -N` at S and `pay PAYS N*(1+K*ALPHA)` at T; USD shown but any validated single matching currency; OIS collateral and same forecast/discount curve, no fee/lag/FX | Price plus ParRate. `B=N*(P_T-P_S)`, `A=N*ALPHA*P_T`. Fast uses explicit source alpha via ForwardRateFromDf. Same cashflows under matched Gaussian model for Price MC; no custom Libor convention needed |
| `dal.gaussian-bond-option/1` | V<E<T at midnight; N>0, K>0; CALL `pay PAYS N*MAX(FIX(IR[USD,DF,T])-K,0)` at E, or PUT with subtraction reversed; currency/header/model match; single currency OIS, full AST check, no fees/lag/extra branch | Price only. Fast descriptor reconstructed from source/effective slots; Gaussian Black kernel under MultiFactorGaussian, MC on same Gaussian curve/g/H/correlation/time-axis/numeraire; multiply N once |

Bond T is observed maturity; payment is E. Source parameters for option are only N and K;
dates/type are structural and require a new definition. Deposit accrual is fixed source ALPHA;
native exporter must reject conventions whose adjusted start differs from V in this first rule.
No restriction is imposed on a pre-existing legacy native Deposit call by this new admission.
Price MC may remain available on a rebound contract after valuation if model/history admission
passes, but P0 ParRate calibration admission is origination-only; an ex-cash deposit must not
reuse the origination par formula. Re-prepare and reject inappropriate measure admission.

Verification gate for each admitted case is independent oracle ↔ script fast ↔ **same binding**
script MC, for prices and applicable native-parameter derivatives. Retain K/N update propagation,
old-binding immutability, stale-cache rejection, unit/scale and persistence cases. The research
§8.3 statistical/step-size/error-budget rules govern these future checks; this document does
not assert that they have been executed or that a selected path budget is sufficient.

Deferred/rejected in P0: FRA/futures convexity/day-settlement, IRS/OIS/Basis/MTM reset importers,
custom schedules/indices, intraday timestamps, XCCY collateral/basis/FX stochastic consistency,
GSR caplet payment-measure and swaption-direction matcher, SLV, path dependence, early exercise,
arbitrary source optimization, scalar-only QuoteOnly, vol reference conversion, PVAtQuote,
calibrated quote risk and portable calibration replay. Existing capabilities in legacy APIs do
not imply these have been admitted to the shared contract. P1/P2/P3 extend one family/rule at a time.

## 11. Static compatibility evidence and downstream questions

The following declarations/behavior were inspected at the exact baseline above. They are
evidence of current behavior, not declarations of newly implemented support.

| Source | Static consequence |
|---|---|
| `dal-cpp/dal/script/event.hpp:268`, `event.cpp:377` | Existing Product stores dates/events/settings; current writer is ScriptProductData_v3, readers v1/v2/v3 remain. No definition/header/binding saved there |
| `dal-cpp/dal/script/preprocessor.cpp:145`, `event.hpp:164` | Numeric definitions become constant slots; new typed bind must intervene before optimization/history; labels alone provide no unit/identity guarantees |
| `dal-cpp/dal/string/strings.hpp:32` | String_ comparisons are case-insensitive; new stable IDs/JCS property ordering need byte-sensitive storage/comparison |
| `dal-cpp/dal/curve/ycinstrument.hpp:55`, `ycinstrument.cpp:69`, `jointrate.hpp:22` | Native Deposit has market rate/convention, no notional; fast precompute can reconstruct schedule, so new descriptor must use resolved source alpha rather than rebuild it |
| `dal-cpp/dal/protocol/rateoption.hpp:22`, `dal-cpp/dal/model/gsreuropean.cpp:299` | BondOption has expiry/maturity/strike/type; Gaussian double pricing kernel is reusable, not a shared-script matcher or fast AAD API |
| `dal-public/src/script.hpp:18`, `curvespec.hpp:65`, `gsr.hpp:23` | Keep NewScriptProduct overloads and CalibrateSingleCurve/CalibrateGSRVolatility signatures/defaults; new APIs are separate |
| `dal-public/src/curvespec.hpp:18`, `dal-cpp/dal/math/optimization/underdetermined.hpp:49`, `dal-cpp/dal/utilities/exceptions.hpp:75` | Existing convergence alias/exception layout remains; diagnostics attach only through new CalibrationError_ |
| `dal-cpp/dal/model/gsrcalibration.hpp:31` | Existing quoteJacobian_ is raw dPrice/dg; three flags separate optimizer/fit/numerics; preserve old result ABI and names |
| `dal-python/src/bindings/script.cpp:52`, `curve.cpp:1551`, `curve.cpp:1574`, `gsr.cpp:60`, `gsr.cpp:107`, `module.cpp:40` | Preserve Product_New keyword-only settings, CalibrateSingleCurve, _CalibrationConvergenceError, CalibrationQuote_ and Calibrate_GSRVolatility; new exception translator must preserve owned payload |
| `dal-excel/src/__script.cpp:85`, `__curvespec.cpp`, `__gsr.cpp`, `__script_storable.hpp:14`, `__scriptdiagnostics.cpp:50` | Preserve worksheet markup/old CalibrationQuote meaning; existing empty-Write settings wrapper is not a new persistence implementation; diagnostic chunking can be reused |
| `dal-cpp/dal/script/settings.cpp:80`, `simulation.cpp:18`, `dal-cpp/dal/math/random/pseudorandom.cpp:42` | Normal precision is Default/Fast/Precise, generic RNG construction uses seed 1024, and normal-path seeking differs from uniform antithetic seeking; adapter streams need separate validation |
| `docs/methodology/script_engine.md`, `docs/methodology/monte-carlo/simulation.md:34`, `docs/methodology/underdetermined_search.md` | Preserve FIX/historical/numeraire and native solver vocabulary; new strict receiver guard/admission and decoupled approximate driver still need implementation |
| `dal-cpp/examples/products/script/script.cpp`, `dal-cpp/examples/models/gsr_swap_swaption/gsr_swap_swaption.cpp`, `dal-public/examples/aad_public_risk/aad_public_risk.cpp` | Core preparation examples use core; facade owning-result examples live in public; do not move core examples to introduce facade calls |

No old constructors, default settings, aliases, instrument-vector types, archive type tags or
failure/return semantics change as part of this design. LegacyNative continues until a family
passes its own compatibility/equivalence gate; the shared API must not silently claim it.
Documentation-only scope means no product builds/tests or performance benchmarks: runtime,
ABI, numerical and performance acceptance remain downstream work, with their own evidence.

Decisions resolved here: header ownership, K/N domains/default authority, complete binding and
identity, portable save/load, quote/observation split and repeated-quote mapping, C++ failure
payload, Python exception, Excel outcome/spill behavior, new required valuation settings and
P0 product limits. Rejected alternatives are live native/script pairs, text substitution,
quote-attached scale, per-unit Price defaults, last-error globals, automatic MC fallback and
silent extension of legacy archives.

Actual remaining questions are implementation/validation gates, not unspecified economic terms:
which exact compiler build/pass tokens ship, which reproducible pseudorandom stream mapping
the adapter can support, and which explicit budgets satisfy the research statistical criteria.
If no suitable core stream/batch implementation exists, downstream work must add/verify it or
report unsupported; it cannot invent IID errors for Sobol. New family conventions and the XCCY
stochastic model remain later scoped decisions. Windows workbook execution is separate from
portable Excel contract verification. None is claimed solved by this static decision note.
