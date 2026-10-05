# F01 Excel quote-risk boundary

Status: implemented with complete local verification; publication-head CI
remains required. This extends the accepted
[core/Hybrid contract](aad-dupire-pullback.md) and [Python boundary](aad-dupire-bindings.md).
Excel acceptance and the common curve adapter remain separate requirements.

## Audience and current gap

Worksheet users can obtain structured scalar valuation risk, but cannot create
a Dupire calibration, a matching local-vol model, or a quote pullback. Merely
exposing a surface is insufficient: the worksheet lacks local-vol component
factories. Provide the complete calibration/model/valuation/quote-result chain.

## Shared C++ and Python decisions

`NewMertonIVS(spot, vol, intensity, averageJump, jumpStd)` validates finite
parameters, positive spot and nonnegative vol/intensity/jumpStd. Python retains
strict input-type checks and delegates physical validation to this public helper.
The existing unchecked native constructor is unchanged; Merton carry is zero.

`NewDupireModelData(name, calibration, index, currency, factor, maxStep=1/12)`
builds a Hybrid model with `equity` local-vol and `rate` deterministic-rate
components, one identity-correlated factor, and calibration spot/rate/dividend.
Copy the surface into the model; a mutable model must not alias the frozen
snapshot. Match the existing BS-local-vol factory's component convention.
Both factories delegate to one internal builder. Its temporary configuration
borrows input strings for the factory call; the resulting model owns its data.
The old BS factory preserves its surface-sharing convention and constructor
sequence. Only the new snapshot factory detaches its surface.
Python exposes `DupireModelData_New(calibration, index, currency, factor, *,
name="", max_step=1/12)` with strict numeric/string input validation.

## Excel factories

All factories assign their output only after success. Handles contain const
passive values and explicitly reject archive serialization; no new schema,
callback, tape, or active number is retained.

| Operation                        | Required inputs                                                            | Optional inputs |
|----------------------------------|----------------------------------------------------------------------------|-----------------|
| MertonIVS_New                    | name, settings                                                             | none            |
| DupireGrid_New                   | name, inclusion_spots, max_spot_spacing, inclusion_times, max_time_spacing | none            |
| DupireRiskInputs_New             | name, quote_strikes, quote_maturities, quote_spreads, grid                 | none            |
| DupireCalibration_New            | name, base, inputs                                                         | none            |
| DupireModelData_New              | name, calibration, index, currency, factor                                 | max_step=1/12   |
| DupireParameterAdjoints_New      | name, calibration, adjoints                                                | none            |
| DupireDirectQuoteAdjoints_New    | name, calibration, adjoints                                                | none            |
| DupireParameterAdjoints_FromRisk | name, valuation, calibration, component                                    | none            |
| DupireQuoteRisk_New              | name, calibration, parameters                                              | direct (blank)  |
| DupireScriptQuoteRisk_New        | name, valuation, calibration, component                                    | direct (blank)  |

Merton settings are strict two-column key/value rows requiring exactly the five
keys `spot`, `vol`, `intensity`, `average_jump`, `jump_std`. Reject duplicate,
unknown, missing, blank, text/bool and nonfinite values with row/column context.
`base` accepts an existing BS model or the new Merton handle. Generic handle
dispatch must reject other types and null. Grid splitting avoids seven positional
inputs. Core calibration remains authoritative for mathematical grid/quote
domains; seed factories reject wrong shapes and nonfinite values.

## Getters and units

`DupireCalibration_Get_Surface` returns a detached local-vol data handle.
`_Get_Spots`, `_Get_Times`, `_Get_Vols` copy completed grid axes and values.
`_Get_Quotes` returns a header plus spot-major/time-minor strike, maturity,
spread rows. `_Get_Provenance` returns method boundary, algorithm, carry,
grid spacings and inclusion-grid rows so the frozen input is inspectable.
Both seed types provide `_Get_Adjoints`, returning copied numeric matrices.
`DupireQuoteRisk_Get_Adjoints(result, contribution="total")` accepts exactly
`total`, `calibration`, or `direct`; raw decimal-vol derivatives are never
renormalized or report-scaled. `_Get_Provenance` reports method, unit,
boundary and algorithm. Composite getters expose copied valuation/quote-risk
handles and component/method provenance. Getters do no history lookup,
simulation submission, calibration, recording or reverse work.

Excel markup normalizes integer numeric cells before generic converters using
the existing scoped input adapter; do not change global converters. Validate
original text cells for embedded NUL before native dispatch; settings keys are
checked before matrix conversion. Native entry points also reject NUL.
Optional direct handles unwrap one-cell ranges before blank handling. Generate
all registration stubs and HTML from their markup; Windows DLL tests import
the native entry points through a guarded export header.

## Typical worksheet chain

1. Create grid and quote-input handles from numeric ranges.
2. Calibrate a BS or Merton base with `DupireCalibration_New`.
3. Create `DupireModelData_New` using index `EQ[LOCAL]`, currency `USD`, factor
   `F_LOCAL`; value the product with existing `MonteCarlo_ValueWithRisk`.
4. Create `DupireScriptQuoteRisk_New` for component `equity`, optionally adding
   a direct quote-adjoint handle, then obtain separated contributions.

## Compatibility and rejected alternatives

Existing factories, estimators, methods and legacy risk remain unchanged. The
new calls are opt-in. Requiring a dummy BS model for a Merton surface obscures
carry provenance; constructing from the calibration avoids that ambiguity.
Retaining a mutable surface alias, accepting arbitrary models as flat IVS,
silently treating absent surface risk as zero, or scaling already averaged risk
would break the established mathematical contract. Full archive support and
custom worksheet IVS callbacks are outside this increment.

## Evidence and remaining questions

Establish missing-factory RED before implementation. Verify the actual Excel
chain for flat/Merton bases and tree/compiled modes with one scoped worker,
257 common paths, independent legacy calibration/prices and the unchanged
2e-4/1e-4/5e-5 quote steps, abs/rel 1e-3 and two adjacent passing steps.
Retain every row. Exercise negative/zero/direct seeds, mismatch, detached
mutation, strict settings, null/wrong-type errors, output preservation,
serialization rejection and success/failure/success recovery. Inspect generated
Windows signatures and run portable, diagnostics, sanitizer, installed consumer,
Python parity and exact-head CI separately. Common curve integration remains
open; no unresolved API decision blocks this Excel scope.
