# Excel Interface

The Windows XLL exposes worksheet functions and stores constructed objects in an
Excel-side repository. Constructors return handles; pass those handles into later
functions rather than attempting to unpack native objects in cells.
See [Excel add-in setup](../../dal-excel/README.md) for the build and loading
workflow, and [FIX settings](script-settings.md) for complete worksheet
matrices and the executable workbook. The numerical methods have C++ examples
in the [method chapters](../README.md#quantitative-methods).

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
