# Python Interface

The `dal` package wraps the C++ public facade with Python objects and
keyword-only settings. See [installation](../installation.md#python-bindings)
for supported interpreters and wheel platforms; the examples below are Python
specific. The numerical methods themselves are documented with C++ examples
in [yield curves](../yield-curves/README.md), [CCY curves](../ccy-curves/README.md),
[Monte Carlo](../monte-carlo/README.md), and [PDE](../pde/README.md).

Import the installed package with:

```python
import dal
```

## Common workflows

| Workflow                | Python entry points                                                                                                                                                                                                                                |
|-------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Dates/global state      | `Date_`, `Year`, `Month`, `Day`, `EvaluationDate_Set`, `EvaluationDate_Get`                                                                                                                                                                        |
| Script products         | `Product_New`, `Product_Describe`, `Product_Debug`, `Product_DebugJson`, `Product_DebugTree`                                                                                                                                                       |
| Models                  | `BSModelData_New`, `DupireModelData_New`                                                                                                                                                                                                           |
| Valuation               | `MonteCarlo_Value`, `MonteCarlo_ValueWithSettings`, `ScriptValuation_Explain`, `ScriptSimulation_Explain`                                                                                                                                          |
| Script settings         | `ScriptProductSettings_`, `ScriptValuationSettings_`, `MonteCarloSettings_`, `TodayFixingPolicy_`                                                                                                                                                  |
| Random generation       | `PseudoRSG_New`, `SobolRSG_New`, `*_Get_Uniform`, `*_Get_Normal`                                                                                                                                                                                   |
| Calendar operations     | `Holidays_`, `Is_BizDay`, `NextBizDay`, `PrevBizDay`, `Adjust`                                                                                                                                                                                     |
| Curves                  | `DiscountZeroRate_New`, convention/instrument builders, `CurveCalibrationSpecBuilder_`, `CalibrateSingleCurve`, `CalibrateMultiCurveBundle`, `CalibrateXccyMarket`, `CalibrateJointXccyMarket`                                                     |
| XCCY reset data         | `FixingIdentity_`, `FxResetConvention_`, `MarketFixingSnapshot_New`, `CrossCurrencySwapConfigBuilder_`, `XccyNotionalMode`                                                                                                                         |
| Rate cashflow pricing   | `RateTradeDefinition_`, typed terms, `RatePricingMarket_`, `PriceRateTrades`, `RateTradeNodeSensitivities`, `RateTradeNodeSensitivitiesBatch`, `AggregateRatePortfolioNodeRisk`, quote-risk provenance builders, `AggregateRatePortfolioQuoteRisk` |
| Convenience calibration | `calibrate_curve` from `dal/api.py`                                                                                                                                                                                                                |

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

Both Python Value entries require an integer or valid `__index__` path count
in `1..2147483647`, excluding bool and enums; floats such as `1.0` are rejected.
The bindings copy settings and native handles before releasing the GIL for
valuation. `EvaluationDate_Get` / `EvaluationDate_Set` also release it before
native synchronization. A setter waits for an in-progress valuation; a getter
can read the stable current date while valuation runs.

For precise-CDF-polished Sobol normal draws, pass both flags explicitly:

```python
rsg = dal.SobolRSG_New(i_path=0, ndim=3, precise=True, polish=True)
normals = dal.SobolRSG_Get_Normal(rsg, 1024)
```

## Python FIX settings and diagnostics

The settings entry points are:

```text
Product_New(events_dates, events, *, settings=None)
MonteCarlo_ValueWithSettings(product, modelData, num_path, *, valuation=None, simulation=None)
Product_Describe(product)
ScriptValuation_Explain(product, modelData, *, valuation=None)
ScriptSimulation_Explain(product, modelData, num_path, *, valuation=None, simulation=None)
```

`settings`, `valuation`, and `simulation` take `ScriptProductSettings_`,
`ScriptValuationSettings_`, and `MonteCarloSettings_` respectively, or `None`
for fresh defaults. Their constructors use keyword-only fields. Product settings
provide `default_index`; valuation settings provide `evaluation_date`,
`today_fixing` and `fixings`; simulation settings provide
`method`, `use_bb`, `enable_aad`, `smooth`, `compiled`, `lsmc_basis_degree`,
`lsmc_training_paths`, `lsmc_validation_paths`, `lsmc_rqmc_replicates`,
`lsmc_training_seed`, `lsmc_pricing_seed`, `lsmc_policy_risk_mode`, and
`lsmc_policy_bump_relative`. `Frozen` is the default AAD sensitivity mode;
`RetrainedBump` adds a common-path policy-retraining secant to model-parameter
and script-constant risks. RQMC pricing uses one fitted
policy and reports conditional replicate-mean uncertainty through
`ScriptSimulation_Explain`.

`today_fixing` accepts `TodayFixingPolicy_.MODEL` / `.REQUIREHISTORICAL` or exact,
case-sensitive `Model` / `RequireHistorical` strings. The three settings fields `default_index`,
`method`, and `today_fixing` reject foreign enums, including string-derived enum
members, with `TypeError` on construction or assignment. Ordinary string
subclasses, DAL `String_`, and the native today-policy members remain supported.
Event text accepts string-derived enum members. Dates require a valid DAL
`Date_`; snapshot keys require `DateTime_(date, 0)` for exact midnight.
`fixings=None` captures current global history; an explicit empty snapshot
never falls back to it. Global capture is sequential, not atomic across
sequences, and requires callers to exclude concurrent fixing writes.

The [FIX source rules](../methodology/script_engine.md#dates-and-structural-validation)
and [script model index](../methodology/script_engine.md#the-script-model-index-and-legacy-spot)
apply unchanged: past history, today's selected policy, future model, and no
fixing after its event. The complete
[Python example](../../dal-python/examples/012.fix_settings.py) supplies a legal
BS model and checks `PV=260` / `d_SCALE=80` for historical SCALE state plus a
retained future fixing. See the
[Python settings reference](../../dal-python/README.md#script-settings-and-copies)
for defaults, strict field types, setters, detached property copies and
copy/deepcopy semantics. Snapshot handles share immutable data; workers use
native copies without Python callbacks or mutable dictionaries.

High-level Describe returns a `dal.script-product/2` dictionary without market
I/O, global-date access or valuation phase. High-level Explain returns a
`dal.script-valuation/1` dictionary from one independent default exact/tree
price preparation. It may read history and initialize a model, but generates
no paths or workers and accepts no simulation settings. It neither describes
a preceding compiled/AAD call nor caches the next Value. High-level
`ScriptSimulation_Explain` runs the full double valuation with the given
`num_path` on exercise products (plain products skip the run, since their
exercise events array is empty either way) and returns the
`dal.script-simulation/1` exercise diagnostics dictionary. Low-level `dal._dal`
and `dal.dal` diagnostics return raw JSON strings with the same schemas.
Diagnostics are not loadable archives; Python has no public script-product
serializer. Value results contain only already-normalized `PV` and optional
`d_` parameter risks, with no fixing-risk or diagnostic keys.

Existing three-to-eight-argument `MonteCarlo_Value` retains its original
keywords/defaults and valid flag/float conversions. It cannot take the new
settings, and the settings entry cannot take flat simulation options. The
high-level product date keyword is `events_dates`; low-level `Product_New`
uses `dates` and requires `Cell_` elements. High-level conversion leaves existing
cells intact. Unknown keywords, wrong types or extra positional settings raise
`TypeError`; unknown attributes raise `AttributeError`; invalid values and native
failures raise `RuntimeError` with field/constraint and source context.
`Product_DebugJson` remains a JSON string with schema /1 and rejects FIX or
nonempty defaults with `DebugSchemaUnsupported`.

## Matrix and Dupire surface input

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

## Python curve calibration

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
[Jacobian methodology](../yield-curves/jacobian-risk.md#joint-xccy-jacobian-layout).

## Python rate cashflow pricing

Python exports the seven-family enum, all family-specific terms classes,
`RateTradeDefinition_`, `RatePricingMarket_`, `PriceRateTrades`,
`RateTradeNodeSensitivities`, `RateTradeNodeSensitivitiesBatch`, and
`AggregateRatePortfolioNodeRisk`. The pricing and sensitivity functions use
keyword-only arguments and release the GIL around native work.
Repeated pricing also exposes `PreparedRateTrades_New`,
`PreparedRateTrades_Get_Prices` and `PreparedRateTrades_Get_NodeSensitivities`
with keyword-only inputs and a read-only prepared `size` property.
`component_keys` must be a Python `list` — a tuple is rejected with `TypeError`
before any native work starts. The minimal single-trade call:

```python
r = dal.RateTradeNodeSensitivities(trade=trade, market=market, component_key="discount")
# r.eligible, r.pv, list(r.gradient), r.reason
```

Results are read-only projections of the C++ shapes. A complete runnable
deposit example covering the batch and aggregation calls is in the
[dal-python README](../../dal-python/README.md#rate-cashflow-pricing-and-node-risk).

## Python quote-space DV01

Python exposes `RateQuoteRiskProvenanceConfig_`, all four supported provenance
builders, and `AggregateRatePortfolioQuoteRisk` as keyword-only calls. They
release the GIL around native construction or aggregation and return read-only
objects. The axis/state fingerprint schemes, stable availability reasons,
price-per-decimal and DV01 units, and `UnconvertedByActualPvCcy` policy are
identical to C++.

The runnable [single-curve quote-risk example](../../dal-python/examples/009.quote_risk.py)
prints both fingerprints, the policy, and every bucket. The
[joint XCCY example](../../dal-python/examples/007.xccy_joint_calibration.py) also
constructs joint provenance. The [generic joint example](../../dal-python/examples/010.generic_joint_quote_risk.py)
uses constructible spec/options, `CalibrateJointMultiCurveBundle`, owning result
curves, and `BuildJointMultiCurveQuoteRiskProvenance`. Ordinary staged
multi-curve chain rules remain outside the supported Python surface.

`Storable_` exposes read-only `name` and `type` properties, and the native
`YieldCurve_` / `CurveBlock_` / `Bag_` hierarchy is bound for archive
compatibility with the standalone web application. `_StorableToJson`,
`_StorableFromJson`, `_BagNew`, and `_BagContents` are private integration
helpers rather than supported general serialization functions.

See [dal-python/README.md](../../dal-python/README.md) for package-focused examples.
