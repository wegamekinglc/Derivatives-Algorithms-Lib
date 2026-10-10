# Python Interface

The `dal` package wraps the C++ public facade with Python objects and
keyword-only settings. See [installation](../installation.md#python-bindings)
for supported interpreters and wheel platforms; the examples below are Python
specific. The numerical methods themselves are documented with C++ examples
in [yield curves](../yield-curves/README.md), [CCY curves](../ccy-curves/README.md),
[Monte Carlo](../methodology/monte-carlo/README.md), and [PDE](../methodology/pde/README.md).

Import the installed package with:

```python
import dal
```

## Common workflows

| Workflow                | Python entry points                                                                                                                                                                                                                                |
|-------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Dates/global state      | `Date_`, `Year`, `Month`, `Day`, `EvaluationDate_Set`, `EvaluationDate_Get`                                                                                                                                                                        |
| Script products         | `Product_New`, `Product_Describe`, `Product_Debug`, `Product_DebugJson`, `Product_DebugTree`                                                                                                                                                       |
| Models                  | `BSModelData_New`, `LocalVolSurfaceData_New`, `BSLocalVolModelData_New`, `HybridModelData_New`                                                                                                                                                     |
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
provide `default_index` and `regression_features`; valuation settings provide `evaluation_date`,
`today_fixing` and `fixings`; simulation settings provide
`method`, `use_bb`, `enable_aad`, `smooth`, `compiled`, `lsmc_basis_degree`,
`lsmc_training_paths`, `lsmc_validation_paths`, `lsmc_rqmc_replicates`,
`lsmc_training_seed`, `lsmc_pricing_seed`, `lsmc_policy_risk_mode`, and
`lsmc_policy_bump_relative`, and `normal_precision`. The latter accepts exact
`Default`, `Fast`, or `Precise` strings. `Default` uses fast conversion for all
three generators; use `MonteCarloSettings_(method="mrg32", normal_precision="Precise")`
to use CDF-polished MRG32 normals. See [normal precision](../methodology/monte-carlo/sampling.md#monte-carlo-normal-precision)
for the numerical tradeoff. `Frozen` is the default AAD sensitivity mode;
`RetrainedBump` adds a common-path policy-retraining secant to model-parameter
and script-constant risks. RQMC pricing uses one fitted
policy and reports conditional replicate-mean uncertainty through
`ScriptSimulation_Explain`.

For multi-asset `EXERCISE`, pass a list of at most three state names, such as
`ScriptProductSettings_(regression_features=["EQ[A]", "VAR[runningAverage]"])`.
`EQ[...]` samples a model equity at each exercise date; `VAR[...]` reads a
scalar script variable at that event. The list is stored with the product and
is available in `Product_Describe`. With no list, `default_index` remains the
single regression state for a multi-asset exercise product.
Model-supported IR discount-factor, Libor, and swap names can also be selected.
Rate-only GSR exercise requires an explicit list, such as
`ScriptProductSettings_(regression_features=["IR[USD,SWAP,5Y]"])`; see
[GSR Bermudan products](../models/gaussian-short-rate.md#bermudan-products).

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

## Structured script risk

`MonteCarlo_ValueWithRisk(product, modelData, num_path, *, request=None,
valuation=None, simulation=None)` returns a read-only `RiskResult_`.
The omitted simulation enables native AAD; an explicit
`MonteCarloSettings_(enable_aad=False)` requests price only.

```python
request = dal.RiskRequest_(
    inputs=["constant:0", "model:1"], report_factors=[0.5, 0.01],
)
risk = dal.MonteCarlo_ValueWithRisk(product, model, 2**16, request=request)
raw = risk.jacobian.to_rows()        # [[strike derivative, volatility derivative]]
reported = risk.reported_jacobian.to_rows()
method = risk.provenance.method
```

Request fields are keyword-only `inputs`, `outputs`, `report_factors` and
`numeric_payload_budget_bytes`. IDs/factors accept lists or tuples, with `None`
meaning omission. Empty native inputs preserve the `(1, 0)` matrix and smoothed
price; price-only cannot select nonempty inputs. Budget is a nonnegative integer
excluding bool. It covers returned numeric values/Jacobian, excluding metadata,
worker/tape storage and getter copies.

`output_ids`, `values`, `input_axis`, `complete_input_axis`, `jacobian`,
`reported_jacobian`, `provenance` and `legacy_values` expose detached data.
Changing a returned matrix or copied simulation settings cannot change the result.
Coordinate IDs identify model/script ordinals within the retained snapshot;
physical units can be unknown. The raw legacy view rejects display collisions.
The [AAD methodology](../methodology/aad.md#structured-scalar-risk-results)
describes the retained product/model/history settings and mixed LSM policy risk.

## Weighted script risk

`Product_Get_RiskOutputs(product)` returns detached read-only scalar coordinates
with `id`, `label` and `slot`. The payoff receiver has ID `payoff`; other scalar
slots use `output:<ordinal>`. Labels are display text. Vector storage is excluded.

```python
request = dal.WeightedRiskRequest_(
    outputs=["output:0", "payoff"], weights=[2.0, -0.5],
    inputs=["model:0", "model:1"], report_factors=[1.0, 0.01],
)
risk = dal.MonteCarlo_ValueWithWeightedRisk(
    product, model, 2**16, request=request,
)
objective = risk.weighted_value
components = risk.component_means
gradient = risk.jacobian.to_rows()  # one row in requested input order
```

`WeightedRiskRequest_` fields are keyword-only: `inputs`, `outputs`, `weights`,
`report_factors` and `numeric_payload_budget_bytes`. `None` selects the default
payoff, unit weights and native input set. ID/numeric sequences accept lists or
tuples; numeric entries require int/float and exclude bool, enum and text.
Native preflight rejects nonfinite/mismatched weights, empty/repeated/unknown
outputs, unsupported exercise and fully expired products before history/workers.
Signed/zero weights are valid; every selected component must remain finite.

The valuation has the scalar entry's keyword-only `request`, `valuation` and
`simulation` settings and defaults to native AAD. Explicit price-only execution
uses `MonteCarloSettings_(enable_aad=False)`. Empty native inputs preserve AAD
smoothing and a `(1, 0)` matrix. Requests/settings are copied before the GIL is
released for native work.

`weighted_value`, `component_means`, `weights`, `output_axis`, `jacobian`,
`reported_jacobian`, `input_axis`, `complete_input_axis` and `provenance` are
read-only owning result properties. Container/matrix getters return independent
copies, and copy/deepcopy retain the passive result after caller objects die.
The numeric budget covers `8 * (1 + inputs + 2 * outputs)` bytes: objective,
raw gradient, component means and weights. Metadata, worker/tape storage,
source snapshots and detached getter copies are excluded. See the
[weighted AAD methodology](../methodology/aad.md#weighted-script-risk-results).

## Budgeted script Jacobians

`MonteCarlo_ValueWithJacobianRisk` returns owning output means and a complete
selected-output by selected-input matrix. Choices after product, model and
path count are keyword-only:

```python
request = dal.JacobianRiskRequest_(
    outputs=["output:0", "payoff"], inputs=["model:0", "model:1"],
    max_block_width=2, scratch_capacity_budget_bytes=8 * 1024 * 1024,
)
risk = dal.MonteCarlo_ValueWithJacobianRisk(
    product, model, 1024, request=request, valuation=valuation,
)
means = risk.values
raw = risk.jacobian.to_numpy()                 # shape (2, 2)
reported = risk.reported_jacobian.to_numpy()
work = risk.execution.executed_paths
```

Request fields are read-only: `inputs`, `outputs`, `report_factors`,
`max_block_width`, `numeric_payload_budget_bytes`,
`recording_capacity_budget_bytes` and `scratch_capacity_budget_bytes`.
Width defaults to one and must fit the native channel limit. Budgets accept
nonnegative `size_t` integers or `None`; booleans, enums, fractions and strings
are rejected. ID/factor containers accept lists or tuples. Omitted output
selection means payoff; an empty output list is invalid.

Result properties are `values`, `jacobian`, `reported_jacobian`, selected and
complete `output_axis`/`input_axis`, `provenance` and `execution`. Getters return
detached containers/matrices. Single rows remain two-dimensional and empty
inputs retain `(m, 0)`. Execution properties are `actual_widths`,
`replay_attempts`, `executed_paths`, `peak_recording_bytes` and
`peak_scratch_bytes`. Native work releases the GIL after copying typed request
and settings inputs. Each native output block replays all requested paths;
explicit passive mode performs one replay with no risk columns. The
[AAD methodology](../methodology/aad.md#budgeted-script-jacobians) defines
capacity scopes, exclusions and failure behavior.

## Compatible script portfolios

`ScriptPortfolio_New(trade_ids, products, modelData)` seals equal-length nonempty
lists or tuples. Repeating the same original model object establishes one owner;
distinct model objects remain separate even if their values agree. Matching
original sampling contracts determine which trades share scenarios. Trade IDs
must be unique, and all product/model elements must be typed handles.

```python
import dal

date = dal.Date_(2027, 1, 1)
trade_a = dal.Product_New(["X", date], ["5", "pay PAYS 2 * SPOT() + X"])
trade_b = dal.Product_New(["X", date], ["7", "pay PAYS 3 * SPOT() + X"])
model = dal.BSModelData_New(1, 0, 0, 0)
portfolio = dal.ScriptPortfolio_New(["A", "B"], [trade_a, trade_b], [model, model])
valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 1, 1))
inputs = ["model:0:parameter:0", "trade:0:constant:0", "trade:1:constant:0"]
weighted = dal.PortfolioMonteCarlo_ValueWithWeightedRisk(
    portfolio, 17, valuation=valuation,
    request=dal.PortfolioWeightedRiskRequest_(inputs=inputs, weights=[2, -1]),
)
assert weighted.component_means == [7, 10]
assert weighted.weighted_value == 4
assert weighted.jacobian.to_rows() == [[1, 2, -1]]
rows = dal.PortfolioMonteCarlo_ValueWithJacobianRisk(
    portfolio, 17, valuation=valuation,
    request=dal.PortfolioJacobianRiskRequest_(
        inputs=inputs, outputs=["trade:1:payoff", "trade:0:payoff"], max_block_width=2,
    ),
)
assert rows.values == [10, 7]
assert rows.jacobian.to_rows() == [[3, 0, 1], [2, 1, 0]]
assert rows.execution.groups[0].generated_scenarios == 17
```

Both value functions take `portfolio` and `num_path` first; `request`, `valuation`
and `simulation` are keyword-only. Their default simulation enables native AAD.
An explicit `MonteCarloSettings_(enable_aad=False)` retains sharp price-only rows
and zero risk columns; recording limits have no effect in that mode. Native
explicit empty `inputs=[]` retains native fuzzy prices and zero-column shapes.
Omitted inputs select all native columns; omitted outputs select each trade's
payoff in original order. Global IDs and reporting factors follow the
[C++ portfolio rules](../methodology/aad.md#sealed-script-portfolio-coordinates).

The immutable request types share `inputs`, `outputs`, `report_factors` and the
three optional byte budgets: `numeric_payload_budget_bytes`,
`recording_capacity_budget_bytes`, `scratch_capacity_budget_bytes`. Weighted
requests add `weights`; Jacobian requests add positive bounded `max_block_width`,
default one. Numeric payloads are respectively `8*(1+n+2*m)` and `8*m*(1+n)`;
runtime capacities include all admitted workers and native replacement headroom.
Capacity narrowing preserves requested maximum and estimator.

Owning results expose detached raw/reported matrices, selected/complete axes,
trade provenance and original group contracts. `provenance.trade_ids`,
`provenance.model_owners` and `provenance.trades` retain the sealed source mapping.
`execution.groups` reports actual widths, replay attempts and
scenario/evaluator/reversal counts; `execution` also reports actual peak
recording/scratch bytes. Sampling definitions, settings, lists and matrices are
copied by getters. Results survive destruction of portfolios or source handles;
getters perform no valuation or history reads. Path counts exclude bool, enums,
fractions and values beyond `INT_MAX`. Malformed requests fail with field and
trade/group/coordinate context, leaving prior completed results intact.

## Dupire quote risk

`DupireCalibration_New(base, inputs, *, name="")` creates a frozen calibration
from an existing BS model, `MertonIVS_`, or a Python subclass of `IVS_`.
Configuration is keyword-only; quote rows are strikes and columns are maturities.

```python
inputs = dal.DupireRiskInputs_(
    quote_strikes=[75.0, 105.0, 135.0], quote_maturities=[0.4, 1.2],
    quote_spreads=dal.DoubleMatrix_(3, 2),
    inclusion_spots=[60.0, 100.0, 140.0], max_spot_spacing=10.0,
    inclusion_times=[0.5, 1.0], max_time_spacing=0.5,
)
base = dal.BSModelData_New(100.0, 0.2, 0.05, 0.02)
calibration = dal.DupireCalibration_New(base, inputs, name="local_vol")
seeds = dal.DupireParameterAdjoints_(
    calibration, dal.DoubleMatrix_(len(calibration.spots), len(calibration.times), 1.0),
)
quotes = dal.DupireQuoteRisk_New(calibration, seeds)
raw = quotes.total_adjoints.to_rows()
```

This example differentiates the sum of calibrated surface nodes. For trade risk,
create `DupireModelData_New(calibration, index, currency, factor, *, name="",
max_step=1/12)` or use `calibration.surface` in a Hybrid local-vol component, then obtain a
`MonteCarlo_ValueWithRisk` result with every surface input selected. Then call
`DupireScriptQuoteRisk_New(valuation, calibration, component, *, direct=None)`.
Its `valuation`, `quote_risk`, `component` and `method` retain the source and
quote results. `DupireParameterAdjoints_FromRisk` extracts the surface seed
separately for compatible portfolio accumulation.
The convenience model copies the surface, preserves frozen spot/rate/dividend,
and names its local-vol component `equity` and deterministic-rate component `rate`.

`DupireDirectQuoteAdjoints_(calibration, matrix)` supplies an optional raw PV
quote contribution. Results separate `calibration_adjoints`, `direct_adjoints`
and `total_adjoints`; their `unit` is `decimal-vol`. Reporting factors and path
averaging are not applied again. Snapshot getters expose copied `inputs`,
`spots`, `times`, `vols` and a detached `surface`, plus `spot`, `rate`,
`dividend_yield`, `algorithm` and full-content `matches(other)`.
Numeric properties and copy/deepcopy cannot change retained calibration data.

Custom IVS subclasses call `super().__init__(spot=..., rate=...,
dividend_yield=...)` and implement `implied_vol(strike, maturity)` returning a
finite numeric volatility. `MertonIVS_` takes keyword-only `spot`, `vol`,
`intensity`, `average_jump`, `jump_std` and retains zero carry. Sampling holds
the GIL; the snapshot retains no Python callback, and native pullbacks release
the GIL. Later changes or destruction of the IVS do not affect prior results.
Numeric configuration excludes bool and enums; invalid domains, missing surface
columns and incompatible identities fail explicitly. See the
[discrete derivative and estimator boundaries](../methodology/aad.md#discrete-dupire-calibration-pullback).

## Common calibration quote requests

`CalibrationRiskPlan_New(calibration, *, request=None)` plans an immutable
quote selection over an owning `CalibrationPullback_`. It accepts frozen Dupire
or any supported curve provenance captured with `retain_calibration_record=True`.
`CalibrationRiskResult_New(plan, parameter_adjoints, *, direct=None)` performs
the native pullback using the existing typed parameter/direct seeds.

```python
boundary = dal.CalibrationPullback_New(calibration)
parameters = dal.CalibrationParameterAdjoints_New(
    boundary, dal.DoubleMatrix_(boundary.parameter_rows, boundary.parameter_cols, 1.0),
)
request = dal.CalibrationRiskRequest_(
    inputs=["quote:3", "quote:0"], report_factors=[0.01, 0.01],
    numeric_payload_budget_bytes=144,
)
plan = dal.CalibrationRiskPlan_New(boundary, request=request)
result = dal.CalibrationRiskResult_New(plan, parameters)
raw = result.jacobian.to_rows()
per_vol_point = result.reported_jacobian.to_rows()
```

Request fields are keyword-only and read-only. Lists/tuples are copied; `inputs=None`
selects every source quote, while `inputs=[]` retains an empty `(1, 0)` projection
and still performs the native pullback. Unknown/repeated IDs and nonpositive or
nonfinite factors fail during planning. IDs identify source ordinals; labels
cannot establish source compatibility. Factors apply once to reported copies:
use `0.01` for a decimal-vol point or `1e-4` for a decimal-rate basis point.

The plan exposes `calibration`, `complete_input_axis`, `input_axis`,
`selected_ordinals` and `numeric_payload_bytes`. Coordinates retain native
row/column, units and quote metadata: curve value/strike/maturity are `None`,
and Dupire block fields are `None`. The budget counts all three complete native
quote matrices even for subset/empty selections. It excludes source/metadata,
input seeds, getter copies and temporary recording storage.

Results expose `plan`, `quote_risk`, `jacobian`, `calibration_jacobian`,
`direct_jacobian` and `reported_jacobian`. All matrices, lists and nested values
are detached copies. The common result retains native method/unit/boundary
metadata without deriving a generic PV or currency from external seeds.
Constructors exclude bool/enums and implicit dictionary/container coercions.
Plans/results support copy/deepcopy; native planning and pullback release the
GIL after owning all Python inputs. See the
[native request contract](../yield-curves/jacobian-risk.md#c-quote-coordinate-requests)
for retained payload and source-identity rules.

## Automatic Dupire script risk requests

`DupireScriptRiskPlan_New(product, modelData, calibration, component, request)`
plans the complete surface inputs needed by the native Dupire pullback over a
flat-rate Hybrid. `DupireScriptRiskResult_New(plan)` runs the sealed valuation
and quote pullback. No manual node-risk extraction is needed.

```python
request = dal.DupireScriptRiskRequest_(
    num_paths=257,
    quotes=dal.CalibrationRiskRequest_(
        inputs=["quote:3", "quote:0"], report_factors=[0.01, 0.5],
    ),
    valuation=dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 9, 12)),
)
plan = dal.DupireScriptRiskPlan_New(product, hybrid, calibration, "Z_LOCAL", request)
result = dal.DupireScriptRiskResult_New(plan)
price = result.valuation.values[0]
spread_risk = result.quote_risk.reported_jacobian.to_rows()
```

Request fields are keyword-only. `num_paths` is required and accepts integers in
`1..INT_MAX`, excluding bool and enums. Optional `quotes`, `valuation` and
`simulation` accept their native bound types or `None`; omitted simulation
enables AAD. Explicit price-only settings reject during planning.

Use `direct_bindings=[dal.DupireQuoteBinding_(constant_ordinal=0,
quote_id="quote:3")]` for an explicit script constant dependency. The ordinal
identifies the prepared script constant, and its value must equal the selected
source quote. Bindings require a copied list/tuple of typed values. Alternatively,
`direct` accepts a common `CalibrationDirectQuoteAdjoints_` containing raw PV
partials with the local-vol surface fixed. These two forms are mutually exclusive;
the native planner validates source identity and adds direct risk once.

Plans expose `component`, `quote_plan`, `complete_input_axis`,
`required_input_axis`, `direct_bindings`, `num_paths`, `valuation_settings`,
`simulation_settings` and `numeric_payload_bytes`. They seal native product/model
content and capture the evaluation date. Explicit fixing snapshots remain owned;
global history is resolved at execution. Quote subset/empty selection retains all
mandatory surface/direct gradients and the native smoothed estimator.

The common quote request's budget now covers the retained valuation value and
required gradients plus all three complete quote contribution matrices. It
excludes source/metadata, getter copies and temporary worker/tape storage.
Results expose `valuation`, `quote_risk`, `component`, `method` and
`numeric_payload_bytes`; `quote_risk` is a `CalibrationRiskResult_`. All properties
are read-only and detached. Copy/deepcopy retain owning passive values. Planning
and execution release the GIL after copying typed inputs. See the
[native automatic request boundaries](../methodology/aad.md#automatic-c-dupire-risk-requests).

## Dupire quote Gamma and Hessian products

`DupireScriptCurvaturePlan_New` plans finite-step differences of complete native
quote gradients, recalibrating the surface at every perturbed quote point.
`DupireScriptCurvatureResult_New(plan)` executes the sealed plan on common paths.
Use an existing `DupireScriptRiskRequest_` to specify paths, valuation, simulation
and any direct script-constant quote bindings:

```python
directions = dal.DoubleMatrix_(1, 6, 0.0)
directions[0, 3] = 1.0
bumps = dal.BumpOverAADRequest_(directions=directions, steps=[0.0002])
request = dal.DupireScriptCurvatureRequest_(risk=risk_request, bumps=bumps)
plan = dal.DupireScriptCurvaturePlan_New(
    product, hybrid, calibration, "Z_LOCAL", request,
)
result = dal.DupireScriptCurvatureResult_New(plan)
gamma = result.hessian_products[0, 3]
gradient = result.gradient
```

Direction columns follow the complete raw strike-major decimal-vol spread axis,
available as `result.input_axis`. Unit rows select Gamma/cross-Gamma; signed rows
select Hessian-vector products. First-order input subsets and report factors
affect `result.base.quote_risk` only; `gradient` and `hessian_products` retain
the complete raw axis. Steps are finite, positive and explicit, one per direction
row. All-zero direction rows reject. A `DoubleMatrix_(0, quote_count)` with
`steps=[]` requests the base gradient without curvature rows.

The bump constructor is keyword-only and copies a native `DoubleMatrix_` and a
list/tuple of real steps. It excludes bool/enums and implicit matrix conversion.
Optional `numeric_payload_budget_bytes` and `recording_capacity_budget_bytes`
accept nonnegative size_t integers or `None`; zero remains an explicit cap.
The Dupire planner rejects recording-cap requests because a caller-thread tape
cap cannot constrain parallel worker tapes. The numeric cap covers the combined
bump/base result payload described in the
[native financial contract](../methodology/aad.md#common-path-c-monte-carlo-quote-curvature).
External first-order direct seeds and exercise policies also reject.

Requests, plans and results support copy/deepcopy. All container, matrix and
nested getters return detached values. Plans expose `base_plan`, `point`,
`directions`, `steps` and `numeric_payload_bytes`; results add `base`, `gradient`,
`input_axis`, `hessian_products` and `execution`. Execution reports `method`,
`quote_gradient_evaluations` (`1+2*direction_count`), `paths_per_evaluation` and
`numeric_payload_bytes`. Planning freezes the evaluation date and fixing snapshot;
native planning/execution release the GIL after owning typed Python inputs.
The method is a finite-step estimator, with the native first-order smoothing and
calibration stencil constraints. This entry does not select the smooth C++
forward-over-reverse prototype.

## Rate quote Gamma and Hessian products

`RateCalibration_New(spec)` captures an owning immutable calibration snapshot.
It accepts native single-curve, joint multi-curve, staged XCCY and joint XCCY
specifications, restricted to supported exact square systems. `point` contains
raw decimal quotes in `provenance.axis.quotes` order; `parameters` contains the
solved free coordinates. Staged XCCY exposes basis quotes only; its other curves
remain fixed. `RateCalibration_Recalibrate(calibration, quotes)` returns a new
snapshot from a copied list/tuple of finite real quotes with the same axis.

Use existing `RateTradeDefinition_` values and the shared bump request:

```python
calibration = dal.RateCalibration_New(spec)
directions = dal.DoubleMatrix_(1, len(calibration.point), 0.0)
directions[0, 0] = 1.0
bumps = dal.BumpOverAADRequest_(directions=directions, steps=[0.0002])
settings = dal.RateTradeQuoteCurvatureSettings_(weights=[1.0, -0.5])
result = dal.RateTradeQuoteCurvature([trade_a, trade_b], calibration, bumps, settings=settings)
value = result.curvature.value
gradient = result.curvature.gradient
gamma = result.curvature.hessian_products[0, 0]
```

Trades support the native deposit, FRA, future, IRS, basis-swap, OIS and XCCY
families. Use curve component keys from calibration provenance for free curves;
joint declarations retain distinct keys even when display names repeat. All
trades must have the same actual PV currency, returned as `result.currency`.
This entry performs no currency conversion. Missing or empty weights mean unit
weights; supplied finite weights must match the trade count. Negative and zero
weights are allowed, and every trade is validated even at zero weight.

`RateTradeQuoteCurvatureSettings_(*, weights=None, fixings=None)` optionally
retains an immutable `MarketFixingSnapshot_` for additional trade history.
It must agree with saved calibration observations. Without an explicit snapshot,
missing required history is captured once from global fixings. Calibration
sources, valuation inputs and historical observations stay fixed during bumps.

The finite-step method `BumpOverRecalibratedNativeRateAAD` recalibrates and
recomputes the full quote gradient at every point. With M directions, execution
reports `1+2*M` calibrations, quote gradients and objective reverse sweeps.
`DoubleMatrix_(0, quote_count)` and `steps=[]` request the base gradient only.
Products retain raw quote units and signs; choose explicit positive steps using
convergence checks. These estimates do not enable general native higher order.

The numeric budget covers `8*(1+2*N+2*N*M+M)` bytes for N quotes and M directions,
excluding solver/source storage, getter copies and allocator overhead. The
separate recording cap constrains caller-thread recalibration and objective
tapes, including cleanup reserve. Both optional caps preserve zero. Execution
also reports `numeric_payload_bytes`, `peak_tape_bytes` and `cleanup_reserve_bytes`.
Vectors, matrices, nested results and provenance getters are detached; snapshots,
settings and results support copy/deepcopy. Factories release the GIL after
copying typed inputs. See the
[native financial contract](../methodology/aad.md#native-rate-trade-quote-curvature).

## Segmented Black–Scholes Monte Carlo risk

Prepare a closed native Black–Scholes script once, then reuse its frozen contract
and historical observations for explicit parameter points:

```python
valuation = dal.ScriptValuationSettings_(evaluation_date=dal.Date_(2026, 1, 2))
product = dal.Product_New(
    ["SCALE", dal.Date_(2027, 1, 2)],
    ["2", "pay PAYS SCALE * FIX(EQ[UNDERLYING]) ^ 2"],
)
plan = dal.BlackScholesMonteCarloPlan_New(product, valuation=valuation)
point = [100.0, 0.2, 0.03, 0.01, *plan.script_constants]
settings = dal.SegmentedMonteCarloSettings_(first_path=7, segment_steps=64)
mean = dal.BlackScholesMonteCarlo_Get_Risk(plan, point, 129, settings=settings)
bumps = dal.BumpOverAADRequest_(
    directions=dal.DoubleMatrix_([[1.0, 0.0, 0.0, 0.0, 0.0]]), steps=[0.25]
)
risk = dal.BlackScholesMonteCarlo_Get_Curvature(
    plan, point, 129, bumps, settings=settings
)
spot_gamma = risk.hessian_products[0, 0]
```

Columns follow `plan.parameter_labels`: spot, volatility, continuously compounded
rate, dividend yield, then script constants. Points must be lists or tuples of
finite real numbers; no coordinate scaling is applied. The path count follows
the positive integer contract above. Plan creation resolves history; subsequent
evaluations replay the sealed observations and do not read global history again.
`valuation`, `smoothing`, `contract_dates`, `contract_events`, `contract_settings`
and `observations` identify the preparation. An optional `smoothing` factory
argument selects the compiled native first-order smoothing width. EXERCISE
requires the separate policy estimator and is rejected by this preparation.

Keyword-only settings expose `rsg` (sobol/mrg32/irn), `use_bb`, absolute
`first_path`, optional Sobol-only uint64 `scramble_key`, `normal_precision`
(Default/Fast/Precise), positive `segment_steps` and optional
`checkpoint_capacity_budget_bytes` / `recording_capacity_budget_bytes`.
Explicit zero caps remain zero. Fixed 32-path batches preserve native reduction
order. Defaults select Sobol, offset zero, no bridge and 64-step segments.

Mean results expose `value`, `gradient`, `point`, `parameter_labels`, `settings`,
`execution` and the owning `plan`. Curvature exposes `base` with the mean and
gradient, plus point, directions, actual steps, products, settings, execution
and plan. Containers/settings are detached copies; copy/deepcopy may safely
share immutable native preparation. Numerical calls release the Python GIL.

The curvature method is `BumpOverSegmentedNativeAAD`: each row differences
complete native mean gradients on the same absolute paths. Its `1+2M`
`gradient_evaluations` count is distinct from segment reverse sweeps. Empty
directions retain the base gradient and a `0 x N` product. Numeric payload uses
the shared bump request's owning-double formula and excludes preparation,
tasks and random buffers. The smaller path/bump recording limit applies;
execution reports that effective limit and per-path tape/checkpoint/cleanup
maxima, excluding aggregate memory and RSS. These are finite-step secants;
nonsmooth payoffs have separate step and sampling errors. See the
[native methodology](../methodology/aad.md#common-path-segmented-monte-carlo-curvature).

## Matrix and local-volatility surface input

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

Rows must be rectangular numeric sequences. `LocalVolSurfaceData_New` expects a
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
