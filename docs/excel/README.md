# Excel Interface

The Windows XLL exposes worksheet functions and stores constructed objects in an
Excel-side repository. Constructors return handles; pass those handles into later
functions rather than attempting to unpack native objects in cells.
See [Excel add-in setup](../../dal-excel/README.md) for the build and loading
workflow, and [FIX settings](script-settings.md) for complete worksheet
matrices and the executable workbook. The numerical methods have C++ examples
in the [method chapters](../README.md#quantitative-methods).

## European PDE risk

Use immutable settings, request and result handles for a fixed-grid European
call/put calculation. Place `grid_points`/9 and `ordinary_steps`/8 in the two
columns of A1:B2, without a header:

```text
D1 = EUROPEANPDERISKSETTINGS.NEW("small", A1:B2)
D2 = EUROPEANPDERISKREQUEST.NEW("request", 0.05, 0.20, 110, D1)
D3 = EUROPEANPDERISKRESULT.NEW("risk", D2)
F1 = EUROPEANPDERISKRESULT.GET.PRICES(D3)
I1 = EUROPEANPDERISKRESULT.GET.JACOBIAN(D3)
```

Omitting settings selects 61 nodes, 120 ordinary time intervals, upper spot
400, expiry 1 and decimal dividend yield 0.02. Optional `spot_index` is
zero-based; when omitted, the upper/4 spot must be a mesh node. The rate and
volatility arguments use decimal units. The strike must lie strictly inside
the grid and away from a node, where its payoff derivative is not unique.
The method uses four implicit half-steps followed by Crank–Nicolson; actual
step count is `ordinary_steps + 2`. See the
[financial PDE contract](../methodology/pde/aad.md#owning-financial-request).

Settings keys match case-insensitively and may also specify `upper`,
`spot_index`, `expiry`, `dividend_yield`, `forward_backward_error_limit`,
`transpose_backward_error_limit`, `numeric_payload_budget_bytes` and
`recording_capacity_budget_bytes`. Both error limits default to 1e-12 and must
be finite in [0,1]. Numeric inputs reject text, booleans and Excel errors;
integer fields require exact nonnegative integers. Unknown/repeated keys and
malformed ranges fail. Blank optional index/budget cells mean unset; explicit
zero is a limit. Blank required numeric settings fail. Settings and requests
are passive; point-domain and budget admission occur when creating the result.
The worksheet row ceiling reserves a header: at most 1,048,575 grid nodes and
1,048,573 ordinary intervals.

`EUROPEANPDERISKSETTINGS.GET.CONFIGURATION(settings)` returns all ten key/value
rows. `EUROPEANPDERISKREQUEST.GET.SETTINGS(request)` and `GET.POINT(request)`
copy the settings and three parameters. Result `GET.REQUEST` returns the
actually resolved request. All result getters take only the result handle:

| Getter              | Spill                                                 |
|---------------------|-------------------------------------------------------|
| GET.PRICES          | Payoff/Price header and Call/Put rows                 |
| GET.JACOBIAN        | Six Payoff/Parameter/Unit/Derivative rows plus header |
| GET.GRID            | NodeIndex/Spot header and zero-based nodes            |
| GET.FORWARDERRORS   | Step/Call/Put header and chronological actual steps   |
| GET.TRANSPOSEERRORS | Step plus four exact layer/seed labels                |
| GET.EXECUTION       | Method and five native execution/resource counters    |

Step ordinals are one-based. Derivatives are raw price per decimal rate,
decimal volatility or strike-price unit; there is no per-bp scaling, Delta,
Gamma or higher-order claim. Getters return detached copies without pricing.
Handles do not support serialization. Result execution requires the XLL's
native caller tape to be empty and free of an independent recording scope.

The retained numeric payload is `8*(17+N+6*(ordinary_steps+2))` bytes.
Its optional cap is separate from actual recording capacity. Neither cap
counts worksheet cells, labels, handles or additional getter copies. Budget
cells must be exact nonnegative integers at most 2^53-1 and representable as
size_t; these limits do not bound the complete Excel heap.

## Script valuation

```text
=PRODUCT.NEW("call", dates, events)
=BSMODELDATA.NEW("bs", 100, 0.20, 0.05, 0.02)
=MONTECARLO.VALUE(product_handle, model_handle, 65536, "sobol", FALSE, TRUE, 0.01)
```

For local-volatility pricing, create a frozen Dupire calibration and a matching
model with `DUPIRECALIBRATION.NEW` and `DUPIREMODELDATA.NEW`, as below.
Both Excel Value functions require a finite integer path count in
`1..2147483647`.

The seven-input `MONTECARLO.VALUE` retains its argument order and has no
compiled or script-settings argument. For explicit FIX settings, construct
`SCRIPTPRODUCTSETTINGS.NEW(name, [settings])`,
`SCRIPTVALUATIONSETTINGS.NEW(name, [settings], [fixings])`,
and `MONTECARLOSETTINGS.NEW(name, [settings])` handles. Settings
are strict two-column ranges with physical row/column errors. Use
`PRODUCT.NEWWITHSETTINGS(name, dates, events, settings)` for a product default,
then `MONTECARLO.VALUEWITHSETTINGS(product, modelData, n_paths, [valuation], [simulation])`.
Square brackets mark optional arguments; omitted valuation/simulation handles
select fresh native defaults. The product settings handle is required by
`PRODUCT.NEWWITHSETTINGS`; `PRODUCT.NEW` remains available without it.

Write unquoted `FIX(EQ[AAPL])` in event text. Model-sourced named FIX binds the
model's `spot` output to one ordinary EQ, taken from the script's own index by
name; a product default only gives legacy
`SPOT()` an identity. Valuation settings accept an integral evaluation date,
case-sensitive `Model` or `RequireHistorical` today-policy text (settings keys
match case-insensitively), and an immutable snapshot.
`MARKETFIXINGSNAPSHOT.NEW(,,)` creates an explicit empty snapshot, which never
falls back to global history. Snapshot timestamps retain intraday fractions;
daily FIX requires exact midnight. Old and new Value return the same headerless
N×2 `PV`/`d_` key/value table, with already-normalized risks.

`PRODUCT.DESCRIBE(product)` inspects contract syntax without history or date
access. `SCRIPTVALUATION.EXPLAIN(product, modelData, [valuation])` performs
fresh default price preparation without paths, workers, or a subsequent Value
cache. `SCRIPTSIMULATION.EXPLAIN(product, modelData, n_paths, [valuation],
[simulation])` runs the full double valuation on exercise products (plain
products skip the run) and returns the
`dal.script-simulation/1` exercise diagnostics. Their `dal.script-product/2`,
`dal.script-valuation/1`, and `dal.script-simulation/1` JSON is returned
as one column of text chunks: concatenate in order without separators before
parsing. Functions are nonvolatile; explicitly recalculate after global-state
changes. See the [Excel FIX guide](script-settings.md) for exact defaults,
matrix/handle rules, diagnostics, and the executable workbook with PV/AAD oracles.

`SOBOLRSG.NEW(name, i_path, n_dim, precise, polish)` uses the same independent
normal-draw flags as C++ and Python. Pass `TRUE, TRUE` for the precise-CDF Newton
correction; leaving both optional flags `FALSE` selects the Acklam-only default.

## Weighted script risk

Use `PRODUCT.GET.RISKOUTPUTS(product)` to list scalar output IDs, labels and
slots without valuation. The receiver is `payoff`; other scalar slots are
`output:<ordinal>`. Vector storage is excluded.

Create `WEIGHTEDRISKREQUEST.NEW(name, [settings])` from a two-column range:

| Key                           | Example value           |
|-------------------------------|-------------------------|
| outputs                       | output:0;payoff          |
| weights                       | 2;-0.5                  |
| inputs                        | constant:0;model:1       |
| report_factors                | 0.5;0.01                |
| numeric_payload_budget_bytes  | 56                      |

List values are text, including a single ID/number. Missing keys select the
default payoff, unit weights and all native inputs; a blank list explicitly
selects none. Signed/zero weights are valid, but every selected component must
be finite. Empty/repeated/unknown outputs and incompatible weights fail.
Report factors remain finite and positive. The numeric budget must be an exact
nonnegative integer and covers `8 * (1 + inputs + 2 * outputs)` bytes; metadata,
worker/tape storage, snapshots and getter copies are excluded.

```text
=WEIGHTEDRISKREQUEST.NEW("objective", request_settings)
=MONTECARLO.VALUEWITHWEIGHTEDRISK(product, modelData, 65536, request, valuation)
=WEIGHTEDRISKRESULT.GET.WEIGHTEDVALUE(result)
=WEIGHTEDRISKRESULT.GET.COMPONENTS(result)
=WEIGHTEDRISKRESULT.GET.JACOBIAN(result, TRUE)
```

Valuation accepts required product/model/path arguments and optional
request/valuation/simulation handles. Omitted simulation enables native AAD;
explicit `enable_aad=FALSE` selects price only. Exercise and fully expired
products are unsupported. Variables retain their actual script values and the
payoff retains its existing discounting; weights are held fixed.

The immutable result's component table has headers `id`, `label`, `slot`,
`weight`, `mean` in requested order. `GET.JACOBIAN` returns the one-row raw
gradient, or applies report factors when `reported=TRUE`. Zero selected inputs
spill one blank cell; `GET.SHAPE` reports `(1,0)`. Empty native inputs retain
AAD smoothing. `GET.INPUTS(result, [complete])`, `GET.PROVENANCE`, `GET.HISTORY`,
`GET.PRODUCT` and `GET.MODELSNAPSHOT` copy retained passive data without history
access or valuation. Requests/results do not support archive serialization.
See the [weighted AAD methodology](../methodology/aad.md#weighted-script-risk-results).

## Budgeted script Jacobians

`JACOBIANRISKREQUEST.NEW(name, settings)` creates an immutable request from a
two-column settings table. The supported keys are `inputs`, `outputs`,
`report_factors`, `max_block_width`, `numeric_payload_budget_bytes`,
`recording_capacity_budget_bytes` and `scratch_capacity_budget_bytes`. IDs and
factors use semicolon-separated text. Missing keys use defaults; a present
blank input list explicitly selects no columns. Width defaults to one and must
be a positive bounded integer. Budgets are nonnegative numeric integers no
larger than `2^53-1` or `size_t`; booleans, numeric text and fractions are rejected.

`MONTECARLO.VALUEWITHJACOBIANRISK(product, modelData, n_paths, request, valuation,
simulation)` returns a completed immutable result. Optional blank handles
select defaults. Paths must be a positive integer at most `INT_MAX`.

| Getter suffix under `JACOBIANRISKRESULT`                            | Returned table                                                      |
|---------------------------------------------------------------------|---------------------------------------------------------------------|
| `GET.VALUES`                                                        | ID, label, slot and output mean                                     |
| `GET.JACOBIAN(result, reported)`                                    | Selected outputs by selected inputs; `reported=true` scales columns |
| `GET.SHAPE`                                                         | Exact output and input counts                                       |
| `GET.OUTPUTS(result, complete)`                                     | Selected or complete output coordinates                             |
| `GET.INPUTS(result, complete)`                                      | Selected or complete input coordinates and reporting scales         |
| `GET.EXECUTION`                                                     | Actual widths, replay attempts, executed paths and capacity peaks   |
| `GET.PROVENANCE`, `GET.HISTORY`, `GET.PRODUCT`, `GET.MODELSNAPSHOT` | Frozen preparation data                                             |

Flag inputs must be actual booleans or blank. Matrix/table getters copy stored
data and perform no valuation. A zero-column Jacobian spills one blank cell;
`GET.SHAPE` retains `(m, 0)`. Execution widths are semicolon-separated; counts
above `2^53-1` are text to preserve their exact integer values. Native blocks
replay the full path range, while passive mode selects no risk columns and
performs one forward replay. See the
[AAD capacity contract](../methodology/aad.md#budgeted-script-jacobians).

## Compatible script portfolios

`SCRIPTPORTFOLIO.NEW(name, trades)` seals an ordered physical three-column
table. Each row contains a unique trade ID, a product handle and a model handle:

| Trade ID | Product handle | Model handle |
|----------|----------------|--------------|
| A        | product_a      | shared_model |
| B        | product_b      | shared_model |

Use the same model handle to establish shared ownership. Distinct model handles
retain separate owners even when their parameter values agree. Product and model
execution data are frozen. IDs and handles must be nonempty text cells; numeric,
boolean, error and blank cells are rejected without coercion. Trades share
scenarios only when their original sampling contracts agree.

Create `PORTFOLIOWEIGHTEDRISKREQUEST.NEW(name, [settings])` for one weighted
objective or `PORTFOLIOJACOBIANRISKREQUEST.NEW(name, [settings])` for output
attribution. Both accept strict two-column tables with `inputs`, `outputs`,
`report_factors`, `numeric_payload_budget_bytes`,
`recording_capacity_budget_bytes` and `scratch_capacity_budget_bytes`. Weighted
requests add `weights`; attribution requests add `max_block_width` (default one).
IDs and numeric lists are semicolon-separated text. Missing selections use
defaults; a present blank `inputs` value selects zero risk columns. Weights are
fixed, may be signed or zero, and default to one per selected output. Budgets
must be nonnegative numeric integers at most `2^53-1`; width must be in
`1..32768`. Boolean, numeric-text and fractional budgets/widths are rejected.

For products A=`2*SPOT()+X` with X=5 and B=`3*SPOT()+X` with X=7, use a shared
BS model with spot one, zero volatility and zero carry. Select their payoffs and
these three inputs with a settings table:

| Key | Value |
|-----|-------|
| inputs | model:0:parameter:0;trade:0:constant:0;trade:1:constant:0 |
| weights | 2;-1 |

```text
=PORTFOLIOMONTECARLO.VALUEWITHWEIGHTEDRISK(portfolio, 17, weighted_request, valuation, simulation)
=PORTFOLIORISKRESULT.GET.OBJECTIVE(weighted_result)
=PORTFOLIORISKRESULT.GET.JACOBIAN(weighted_result, FALSE)
```

With an evaluation date before the product dates and native simulation settings,
the weighted objective is `2*7-10=4`; its raw gradient is `[1,2,-1]`. Replace the
`weights` row with `max_block_width | 2` to construct an attribution request:

```text
=PORTFOLIOMONTECARLO.VALUEWITHJACOBIANRISK(portfolio, 17, jacobian_request, valuation, simulation)
=PORTFOLIORISKRESULT.GET.VALUES(jacobian_result)
=PORTFOLIORISKRESULT.GET.JACOBIAN(jacobian_result, FALSE)
```

The values are `[7,10]`, and the raw rows are `[2,1,0]` and `[3,0,1]`. The optional
request, valuation and simulation handles select fresh defaults when omitted.
Paths must be numeric positive integers at most `2147483647`. Native simulation
is the default. Passive simulation accepts omitted or empty inputs, retains
sharp prices and zero columns, and performs one forward replay per selected
group. Recording budgets do not apply to passive execution.

Both result kinds use the same checked `PORTFOLIORISKRESULT` getters:

| Getter suffix | Returned data |
|---------------|---------------|
| `GET.OBJECTIVE` | Weighted objective; requires a weighted result |
| `GET.VALUES` | ID, label, slot, mean and weight; attribution weights are blank |
| `GET.JACOBIAN(result, reported)` | Raw or column-scaled gradient/Jacobian |
| `GET.SHAPE` | Exact `(1,n)` weighted or `(m,n)` attribution dimensions |
| `GET.OUTPUTS(result, complete)`, `GET.INPUTS(result, complete)` | Selected or complete coordinates |
| `GET.EXECUTION` | Group/owner positions, dimensions, work, requested/actual widths, attempts and whole-request peaks |
| `GET.SAMPLING` | Original group/sample definitions as field/ordinal/subordinal rows |
| `GET.PROVENANCE`, `GET.TRADES` | Frozen request and original trade/owner metadata |
| `GET.TRADEPROVENANCE(result, trade)`, `GET.HISTORY(result, trade)`, `GET.PRODUCT(result, trade)`, `GET.MODELSNAPSHOT(result, trade)` | Frozen data for a zero-based original trade ordinal |

Flags require actual booleans or blank cells. Trade ordinals require numeric
integers; text and booleans are rejected. Getters copy retained data and perform
no history access or valuation. Zero-column matrices spill one blank cell, while
`GET.SHAPE` retains their exact dimensions. Execution integer counts above
`2^53-1` are text to preserve their values. Requests/results do not support
archive serialization. See [portfolio risk semantics](../methodology/aad.md#script-portfolio-jacobians).

## Dupire quote risk

The calibration holds the base IVS, deterministic carry and grid choices fixed;
quotes are additive absolute decimal-volatility spreads. Quote matrix rows are
strikes and columns are maturities. For a BS base, use this worksheet sequence
with `spreads` a 3×2 numeric range and all surface inputs selected in valuation:

```text
=DUPIREGRID.NEW("grid", {60;100;140}, 10, {0.5;1}, 0.5)
=DUPIRERISKINPUTS.NEW("quotes", {75;105;135}, {0.4;1.2}, spreads, grid_handle)
=BSMODELDATA.NEW("base", 100, 0.20, 0.05, 0.02)
=DUPIRECALIBRATION.NEW("calibration", base_handle, inputs_handle)
=DUPIREMODELDATA.NEW("local", calibration_handle, "EQ[LOCAL]", "USD", "F_LOCAL", 0.25)
=MONTECARLO.VALUEWITHRISK(product_handle, model_handle, 65536, , valuation_handle, simulation_handle)
=DUPIRESCRIPTQUOTERISK.NEW("quotes", valuation_risk_handle, calibration_handle, "equity", )
=DUPIRESCRIPTQUOTERISK.GET.QUOTERISK(combined_handle)
=DUPIREQUOTERISK.GET.ADJOINTS(quote_risk_handle, "total")
```

`DUPIREMODELDATA.NEW` copies the surface and uses the calibration's spot, rate
and dividend yield; its components are named `equity` and `rate`. The optional
maximum model time step defaults to `1/12`. A Merton base uses
`MERTONIVS.NEW(name, settings)` instead of the BS handle, with these five required
numeric key/value rows:

| Key          | Example |
|--------------|---------|
| spot         | 100     |
| vol          | 0.20    |
| intensity    | 0.08    |
| average_jump | -0.10   |
| jump_std     | 0.15    |

Merton carry is zero. Settings reject duplicate/unknown/missing keys, bool/text
values and invalid numeric domains. Calibration rejects invalid grids, quotes,
discrete curvature or local variance rather than regularizing them.

`DUPIREPARAMETERADJOINTS.FROMRISK(name, valuation, calibration, component)`
extracts raw surface seeds for compatible portfolio accumulation. To provide
seeds explicitly, use `DUPIREPARAMETERADJOINTS.NEW(name, calibration, adjoints)`
with spot rows/time columns, then
`DUPIREQUOTERISK.NEW(name, calibration, parameters, [direct])`.
`DUPIREDIRECTQUOTEADJOINTS.NEW(name, calibration, adjoints)` takes strike
rows/maturity columns. Direct seeds are PV derivatives, including any cashflow
discount already applied. Quote results separate `calibration`, `direct`, and
`total` contributions; omitting the getter's contribution selects `total`.
Neither path averaging nor report factors are applied again.

Calibration getters copy the completed `SPOTS`, `TIMES`, `VOLS`, quote rows
(`QUOTES`) and field/value `PROVENANCE`. `GET.SURFACE` returns detached data.
Composite getters return passive `VALUATION`, `QUOTERISK` and `PROVENANCE`;
they run no valuation or history lookup. Seeds and results are immutable
handles and do not support archive serialization. Snapshot mismatches and
missing selected surface risks fail explicitly. See the
[discrete calibration method](../methodology/aad.md#discrete-dupire-calibration-pullback)
for identity, units, rounding and estimator boundaries.

## Dupire quote Gamma and Hessian products

Create a common finite-step request, combine it with the first-order Dupire
request, freeze a plan and evaluate it. The calibration and model handles use
the construction above. `directions` has one row per direction and one column
per **complete raw quote**, in strike-major order; `steps` contains one positive
number per row, as either a row or column vector.

```text
=DUPIRESCRIPTRISKSETTINGS.NEW("execution", 17, valuation_handle, simulation_handle)
=DUPIRESCRIPTRISKREQUEST.NEW("first", execution_handle, , bindings, )
=BUMPOVERAADREQUEST.NEW("bumps", directions, steps, )
=DUPIRESCRIPTCURVATUREREQUEST.NEW("request", first_handle, bumps_handle)
=DUPIRESCRIPTCURVATUREPLAN.NEW("plan", product_handle, model_handle, calibration_handle, "equity", request_handle)
=DUPIRESCRIPTCURVATURERESULT.NEW("result", plan_handle)
=DUPIRESCRIPTCURVATURERESULT.GET.GRADIENT(result_handle)
=DUPIRESCRIPTCURVATURERESULT.GET.HESSIANPRODUCTS(result_handle)
```

Bindings are optional two-column rows containing a zero-based scalar constant
ordinal and a quote ID, for example `0 | quote:3`. Each perturbed quote point
rebuilds the bound scalar, recalibrates the complete surface, and evaluates its
native AAD gradient using common paths. For the script `QUOTE=0.001` followed
by `pay PAYS QUOTE * QUOTE`, bind constant zero to `quote:3`. At rate 0.05 and
expiry one year, the raw quote-3 gradient is `0.002*exp(-0.05)`; a unit quote-3
direction gives Gamma `2*exp(-0.05)`. A direction of minus two gives
`-4*exp(-0.05)`. Other raw coordinates are zero for this payoff.

The gradient is a full quote column. Hessian products have direction rows and
full quote columns. They estimate `H*v` with explicit central step sizes in
decimal-volatility quote units. Selection and report factors in the first-order
request affect only the retained base report; they never shrink or scale the
raw gradient and products. This finite-step calculation does not enable general
higher-order AD. EXERCISE products and external direct-adjoint seeds reject.

| Handle prefix                  | Getter suffixes                                                                                                                              | Returned data                                                                        |
|--------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------|
| `BUMPOVERAADREQUEST`           | `GET.DIRECTIONS`, `GET.STEPS`, `GET.SETTINGS`, `GET.SHAPE`                                                                                   | Copied common request and logical dimensions                                         |
| `DUPIRESCRIPTCURVATUREREQUEST` | `GET.RISK`, `GET.BUMPS`                                                                                                                      | Detached first-order and bump requests                                               |
| `DUPIRESCRIPTCURVATUREPLAN`    | `GET.BASEPLAN`, `GET.POINT`, `GET.DIRECTIONS`, `GET.STEPS`, `GET.SHAPE`, `GET.PAYLOAD`                                                       | Frozen first-order plan, raw point, directions, steps, dimensions and admitted bytes |
| `DUPIRESCRIPTCURVATURERESULT`  | `GET.BASE`, `GET.QUOTEPLAN`, `GET.POINT`, `GET.GRADIENT`, `GET.DIRECTIONS`, `GET.STEPS`, `GET.HESSIANPRODUCTS`, `GET.SHAPE`, `GET.EXECUTION` | Owning base result, quote plan, raw numeric copies and work counters                 |

All getters take just their owning handle. To obtain quote IDs, coordinates,
units and scales, pass `GET.QUOTEPLAN(result)` into
`CALIBRATIONRISKPLAN.GET.INPUTS(quote_plan, TRUE)`. This returns the complete
twelve-column quote metadata, including quotes omitted from the base report.
Execution returns four key/value rows: `method`, `quote_gradient_evaluations`,
`paths_per_evaluation`, and `numeric_payload_bytes`. M directions require
exactly `1+2*M` quote-gradient evaluations.

Blank direction/step ranges mean zero directions. Supply `input_count | Q` in
the bump settings to retain a logical zero-by-Q shape. Planning requires Q to
match the complete calibration axis. Zero directions perform one base
evaluation. Empty numeric spills contain one blank cell; `GET.SHAPE` always
returns one row containing the actual direction and quote counts. Input count
must be in `0..1048575`, reserving a metadata header row; nonempty direction
matrices must fit the worksheet's row and column limits.

Common bump settings also accept `numeric_payload_budget_bytes` and
`recording_capacity_budget_bytes`. Blank means unset; zero is an actual limit.
Both caps require exactly represented nonnegative numeric integers no larger
than 2^53-1 and size_t. Dupire rejects every supplied bump recording cap,
including zero, because this interface does not enforce worker tape limits.
Its combined native numeric cap covers `8*(2+S+B+5*Q+2*M*Q+M)` bytes, where S
counts mandatory surface derivatives and B bound scalar constants. The existing
first-order quote budget remains separate. These caps exclude worksheet cells,
labels, handles, getter copies, calibration/model snapshots and worker memory.

Names and settings keys reject embedded NUL; numeric cells reject bool, text,
errors and nonfinite values. Steps must be positive and match the number
of direction rows. Financial planning checks nonzero directions, representable
perturbations and valid calibration points. Constructors copy passive inputs;
queries do no simulation or history access. Plans retain their date, history,
grids, carry and execution settings. Financial planning and execution reject
active outer recording scopes. All new handles reject archive serialization.
See [the curvature method](../methodology/aad.md#common-path-c-monte-carlo-quote-curvature).

## Rate quote Gamma and Hessian products

The rate curvature worksheet interface uses existing calibration result and
native rate-trade handles. It solves the calibration again at the base and
every perturbed quote point, then takes central secants of native quote
gradients. These are finite-step estimates; native higher-order differentiation
remains unavailable.

```text
A1: =RATECALIBRATION.NEW("snapshot", calibration_result)
B1: =RATECALIBRATION.GET.POINT(A1)
C1: =RATECALIBRATION.GET.QUOTEPLAN(A1)
D1: =CALIBRATIONRISKPLAN.GET.INPUTS(C1, TRUE)
E1: =BUMPOVERAADREQUEST.NEW("bumps", direction_rows, positive_steps)
F1: =RATETRADEQUOTECURVATURESETTINGS.NEW("portfolio", signed_weights, fixing_snapshot)
G1: =RATETRADEQUOTECURVATURERESULT.NEW("gamma", trade_handles, A1, E1, F1)
H1: =RATETRADEQUOTECURVATURERESULT.GET.GRADIENT(G1)
I1: =RATETRADEQUOTECURVATURERESULT.GET.HESSIANPRODUCTS(G1)
J1: =RATETRADEQUOTECURVATURERESULT.GET.EXECUTION(G1)
```

The source may be a single-curve, generic joint multi-curve, staged-XCCY or
joint-XCCY result. The factory seals its retained final specification and
performs a new solve with native default options; it does not reuse the old
parameters or solver-option overrides. Only native EXACT square systems are
supported. Legacy staged multi-curve results without a retained full
specification and arbitrary handles reject. `RATECALIBRATION.RECALIBRATE`
takes name, snapshot and a complete finite raw quote row/column and returns a
new snapshot. `GET.PARAMETERS` copies solved free parameters. Quote metadata
uses the existing twelve-column formatter; its rate value column remains
blank, with actual quote values available separately from `GET.POINT`.

Blank weights or a blank settings handle mean unit weights. Supplied weights
must be finite and match the trade count; signed and zero weights are allowed.
Every trade is validated, including zero-weight rows, and duplicate rows remain
distinct. Trade ranges reject leading, interrupted and trailing blank cells
before conversion, with trade ordinal and worksheet position. Actual PV
currencies must agree; there is no portfolio FX conversion.
An explicit fixing snapshot supplies all additional historical observations,
with native conflict checks against calibration history. If absent, the native
adapter captures missing required trade history once. Settings getters copy
weights and fixing records. `GET.FIXINGS` returns three columns: the first row
is `explicit_snapshot`, a presence Boolean and blank; subsequent rows contain
index, fixing time and value. This distinguishes absent and explicit empty
history.

Result getters copy value, currency, point, raw gradient, directions, steps,
Hessian products, shape, execution evidence and the base calibration snapshot.
Products have one row per direction and one column per full raw decimal quote,
without DV01/report scaling. Zero directions retain logical 0-by-Q shape and
perform one base calibration; empty numeric outputs spill one blank cell.
`GET.SHAPE` returns direction count then quote count. With M directions there
are exactly 1+2M calibrations, gradient evaluations and objective reverses.

Common request settings accept separate numeric and recording capacity limits.
The numeric budget is `8*(1+2Q+2MQ+M)` bytes and excludes solver/preparation
storage, handles and worksheet cells. Rate recording caps apply to caller-thread
recalibration and objective recording, including retained capacity and cleanup
reserve. Blank caps are unset; zero is an actual limit. Execution reports peak
tape capacity and cleanup reserve separately, without an RSS claim.

Numeric quote/weight ranges reject bool, text, date, blank, error and nonfinite
elements before coercion; integers are normalized. Names reject embedded NUL.
Constructors and getters copy passive data; financial entries reject active
outer recordings. Failed financial calls preserve prior output handles and
caller adjoint mode. New handles reject archive serialization. See the
[native rate curvature contract](../methodology/aad.md#native-rate-trade-quote-curvature).

## Curve workflows

Primary worksheet families are:

| Purpose         | Worksheet functions                                                                                                                                                                                                                                                                       |
|-----------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Conventions     | `PERIODLENGTH.NEW`, `DAYBASIS.NEW`, `RATELEGCONVENTION.NEW`, `RATEINDEXCONVENTION.NEW`, `COLLATERALTYPE.*`                                                                                                                                                                                |
| XCCY reset data | `XCCYRESETCONVENTION.NEW`, `MARKETFIXINGSNAPSHOT.NEW`                                                                                                                                                                                                                                     |
| Instruments     | `DEPOSIT.NEW`, `FRA.NEW`, `FUTURE.NEW`, `SWAP.NEW`, `OISSWAP.NEW`, `BASISSWAP.NEW`, `CROSSCURRENCYSWAP.NEW`, `CROSSCURRENCYSWAPCONFIG.NEW`, `CROSSCURRENCYSWAP.CONFIG.NEW`                                                                                                                |
| Direct curves   | `DISCOUNTPWLF.NEW`, `DISCOUNTZERORATE.NEW`, `CURVEBLOCK.NEW.SIMPLE`                                                                                                                                                                                                                       |
| Calibration     | `CALIBRATE.SINGLECURVE`, `CALIBRATE.XCCYMARKET`, `CALIBRATE.JOINTXCCY`                                                                                                                                                                                                                    |
| Results         | `CALIBRATIONRESULT.GET`, `CALIBRATIONRESULT.GET.CURVE`, `XCCYCALIBRATIONRESULT.*`, `JOINTXCCYCALIBRATIONRESULT.GET*`                                                                                                                                                                      |
| Rate risk       | `RATETRADEHEADER.NEW`, `RATEFIXINGIDENTITY.NEW`, `RATEDEPOSITTRADE.NEW`, `RATEFRATRADE.NEW`, `RATEFUTURETRADE.NEW`, `RATEFIXEDFLOATTRADE.NEW`, `RATEBASISTRADE.NEW`, `RATEXCCYTRADE.NEW`, `RATEPRICINGMARKET.NEW`, `RATETRADENODESENSITIVITIESBATCH.SPILL`, `RATEPORTFOLIONODERISK.SPILL` |
| Repository      | `REPOSITORY.FIND`, `REPOSITORY.ERASE`, `REPOSITORY.SIZE`                                                                                                                                                                                                                                  |

`DISCOUNTZERORATE.NEW` takes name, currency, anchor, future dates, and continuously
compounded decimal zero rates, with optional day count, log-DF scheme, and base handle.
`CALIBRATE.SINGLECURVE` accepts a two-column optional settings range. Supported keys
include curve name, target, solve mode, parameterization (`ZERO_RATE` included), log-DF
scheme, smoothing/tolerances, scalar initial guess, and evaluation budgets. Its optional
`baseCurve` input is the curve multiplied under the calibrated curve; it is distinct from
the `discountCurve` used to price a forward-curve calibration.

`CALIBRATE.XCCYMARKET` accepts `jacobianMode`,
`computeForwardJacobian`, and `computeEffJacobianInverse` in its optional
two-column settings range. Omitting them preserves the `ANALYTIC`, `TRUE`,
`TRUE` defaults. `XCCYCALIBRATIONRESULT.GET` exposes `instrumentNames`,
`parameterKnotDates`, `jacobian`, `effJacobianInverse`,
`residualTolerance`, both scaling labels, and both availability states in
addition to the fit vectors and scalars. The staged matrix axes and scaling
contract match C++ and Python.

`CALIBRATE.JOINTXCCY` accepts one domestic discount-instrument/knot group, one
foreign discount-instrument/knot group, configured XCCY instruments, basis
knots, an optional immutable snapshot handle, and two-column settings. Dedicated
result functions return the domestic block, foreign block, and basis curve
handles.
`JOINTXCCYCALIBRATIONRESULT.GET` returns `fxForwards`, `marketRates`,
`modelRates`, `residuals`, `jacobian`, `effJacobianInverse`,
`parameterRanges`, or `residualRanges`. Joint settings can request both matrix
computations independently.

## Rate risk

The rate-risk family is handle-based: build the index convention, trade header,
and trade with their constructors, assemble the market, then spill the results.
A minimal deposit sequence:

```text
C1: =RATEINDEXCONVENTION.NEW("3M", "ACT_365F", "OIS")
D1: =RATETRADEHEADER.NEW("deposit-1", DATE(2026,1,15), DATE(2026,1,15), DATE(2027,1,15), "USD")
E1: =RATEDEPOSITTRADE.NEW(D1, 100, 0.05, TRUE, C1, "discount")
F1: =DISCOUNTPWLF.NEW("flat-discount", "USD", DATE(2027,1,15), 0.04)
F2: =DISCOUNTPWLF.NEW("flat-forecast", "USD", DATE(2027,1,15), 0.04)

' the index convention's first three arguments are plain strings, not handles;
' the component-key array and the curve-handle range must be equal-length
' parallel arrays; the six trailing market arguments (fixings, domestic block,
' foreign block, fxSpot, collateral currency, basis curve) stay empty for a
' single-currency market, while an XCCY market requires both blocks, a positive
' fxSpot, and a collateral currency
G1: =RATEPRICINGMARKET.NEW(NOW(), "USD", {"discount","forecast"}, F1:F2, , , , , , )

H1: =RATETRADENODESENSITIVITIESBATCH.SPILL(E1, {"discount","forecast"}, G1)
I1: =RATEPORTFOLIONODERISK.SPILL(E1, {"discount"}, G1)
```

Both spill functions take `(trades, componentKeys, market)` and return long-form
spills rather than node-gridded columns, since components can carry different
node counts. `RATETRADENODESENSITIVITIESBATCH.SPILL` emits the six columns
`trade, component, reason, pv, node, value` — one row per node of each eligible
(trade, component) entry plus a reason row per failed entry.
`RATEPORTFOLIONODERISK.SPILL` emits the same columns plus a trailing
`currency`, with one aggregate row per actual PV currency. Only trades with
past fixing dates need a fixing-snapshot handle, and XCCY additionally needs
the domestic/foreign blocks and the basis curve.

Quote-space DV01 uses provenance handles and a separate fixed-width spill:

```text
J1: =SINGLECURVEQUOTERISKPROVENANCE.NEW(calibrationResult, "usd-ois", parameterBlockKeys, componentKeys, G1)
K1: =RATEPORTFOLIOQUOTERISK.SPILL(E1, G1, J1)
```

`JOINTXCCYQUOTERISKPROVENANCE.NEW` and
`STAGEDXCCYBASISQUOTERISKPROVENANCE.NEW` cover the other supported calibration
domains. Generic joint calibration has the dedicated
`JOINTMULTICURVEQUOTERISKPROVENANCE.NEW` factory and a
[complete worksheet construction path](../../dal-excel/examples/009.generic_joint_quote_risk.md).
The legacy `RATEQUOTERISKPROVENANCE.NEW(result, calibrationId,
parameterBlockKeys, componentKeys, market)` dispatches from a result handle;
ordinary staged chains and generic joint calibration produce explicit rows with
`QUOTE_RISK_NOT_AVAILABLE_FOR_STAGED_CHAIN_RULE` and
`QUOTE_RISK_EFFECTIVE_INVERSE_UNAVAILABLE` under its unchanged v1 contract.

The quote-risk spill columns are `calibration`, `axis_fingerprint`,
`quote_key`, `quote_name`, `block`, `currency`, `quote_sensitivity`, `dv01`,
`availability`, and `reason`. Quote sensitivity is price per decimal quote;
DV01 is price per `+1 bp`. Rows remain separated by actual PV currency under
`UnconvertedByActualPvCcy`, with no FX conversion. A paste-ready worksheet
recipe is in [dal-excel/examples/008.quote_risk.md](../../dal-excel/examples/008.quote_risk.md).

## Common calibration pullback

`CALIBRATIONPULLBACK.NEW` accepts a frozen Dupire calibration or captured native
curve provenance. To capture a curve record, pass `TRUE` as the optional final
argument of `SINGLECURVEQUOTERISKPROVENANCE.NEW`,
`JOINTMULTICURVEQUOTERISKPROVENANCE.NEW`,
`JOINTXCCYQUOTERISKPROVENANCE.NEW` or
`STAGEDXCCYBASISQUOTERISKPROVENANCE.NEW`. Blank and `FALSE` preserve the default
without retaining the record. The legacy dispatcher accepts the same option and
keeps its generic-joint exclusion. Capture may rebuild provenance from a retained
calibration result; it does not rerun calibration.

```text
=CALIBRATIONPULLBACK.NEW("boundary", captured_provenance_handle)
=CALIBRATIONPARAMETERADJOINTS.NEW("parameters", boundary_handle, node_adjoints)
=CALIBRATIONDIRECTQUOTEADJOINTS.NEW("direct", boundary_handle, direct_adjoints)
=CALIBRATIONQUOTERISK.NEW("risk", boundary_handle, parameters_handle, direct_handle)
=CALIBRATIONQUOTERISK.GET.ADJOINTS(risk_handle, "total")
```

The optional direct handle may be blank. Curve seeds use one column in the
captured global parameter/quote order; Dupire uses its native spot/time and
strike/maturity layouts. Seeds are finite raw PV derivatives. Numeric ranges
accept integer and floating-point cells; bool, text, errors, blanks and
nonfinite values fail with cell locations. The capture option requires a
Boolean or blank. Complete source checks reject mismatched or uncaptured input.

`CALIBRATIONQUOTERISK.GET.ADJOINTS` copies `calibration`, `direct` or `total`;
blank selects total. Seed `GET.ADJOINTS` functions also return detached matrices.
Seed/result `GET.CALIBRATION` functions retain the owning boundary, and
`CALIBRATIONPULLBACK.GET.SOURCE` returns its typed Dupire or native curve source.
Boundary/result `GET.PROVENANCE` return method, units, dimensions and source
identity metadata; the source handle retains the complete record and axes.
All four common handle types reject archive serialization.

Curve mapping reuses the retained inverse without pricing, recording, history
lookup or FX conversion. Apply it separately per actual PV currency and multiply
decimal-quote derivatives by `1e-4` for DV01. Dupire keeps its independent native
recording and nested-use rejection. Getters read stored passive values. See the
[common calibration contract](../yield-curves/jacobian-risk.md#common-passive-c-calibration-pullback)
for source matching and numerical boundaries.

Generated function help under `dal-excel/auto/*.htm` is the argument-level
catalog used by Excel registration.

See [dal-excel/README.md](../../dal-excel/README.md) for build and add-in guidance.
