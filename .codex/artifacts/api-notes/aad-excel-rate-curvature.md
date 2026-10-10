# Excel rate curvature API

Status: active. The audience is worksheet users of existing calibration and
native trade handles. Reuse the immutable common bump request and existing
calibration-risk axis formatter.

## Surface

`RateCalibration_New(name, source)` returns a snapshot from one of four existing
calibration result types. `RateCalibration_Recalibrate(name, calibration, quotes)`
returns a new snapshot. `RateCalibration_Get_Point`, `_Get_Parameters` and
`_Get_QuotePlan` return copies. The quote plan feeds the existing
`CalibrationRiskPlan_Get_Inputs(plan, complete=true)` formatter.

`RateTradeQuoteCurvatureSettings_New(name, weights=blank, fixings=blank)` owns
copied weights and optional fixing observations. `_Get_Weights` returns a numeric
column. `_Get_Fixings` returns a detached three-column record table: row zero is
`explicit_snapshot`, presence Boolean, blank; subsequent rows are index, fixing
time, value. Absence and an explicit empty snapshot remain distinguishable;
Excel cannot serialize a null output handle.

`RateTradeQuoteCurvatureResult_New(name, trades, calibration, bumps, settings=blank)`
returns an owning native financial result. Eleven functions comprise this
result family: the factory and `_Get_Value`, `_Get_Currency`, `_Get_Point`,
`_Get_Gradient`, `_Get_Directions`, `_Get_Steps`, `_Get_HessianProducts`,
`_Get_Shape`, `_Get_Execution` and `_Get_BaseCalibration`.

There are 19 new functions in total. Each input remains a typed handle except
the four-way source factory and the trade vector, which require checked dynamic
admission. Full quote coordinates and signed products are never report-scaled.
The execution spill has method, quote-gradient evaluations, calibrations,
objective reverse sweeps, numeric payload bytes, peak tape bytes and cleanup
reserve bytes. Shape is one row `[direction_count, quote_count]`.
Rate quote metadata retains the existing blank value column; Point is the
separate authoritative quote-value query.

## Worksheet flow

1. Use the existing calibration builders and calibration functions.
2. `RATECALIBRATION.NEW("snapshot", result)` seals the retained specification
   and solves with native default options; it does not reuse the result's
   parameters or solver-option overrides.
3. Query the quote plan and complete input metadata to build direction rows.
4. Build `BUMPOVERAADREQUEST.NEW` with one positive step per direction.
5. Optionally build portfolio weights/fixings, then call
   `RATETRADEQUOTECURVATURERESULT.NEW` with existing trade handles.
6. Query value, currency, gradient, products, shape and execution evidence.

## Decisions and errors

Accept retained result specifications to make all four native calibration
families reachable without adding a separate joint-XCCY spec wrapper or another
calibration algorithm. Reject specs in this worksheet factory rather than
silently accepting a partial set; Python's overloaded spec factory remains
unchanged. Use a single templated four-way dispatcher rather than repeated
factory branches. Reuse existing immutable storable and numeric spill helpers.

Errors identify null/wrong source or trade row, malformed vector location,
missing fixing value, or the native base/direction/sign and solve/objective
stage. Keep absent caps distinct from zero. Getter copies are detached and
allowed during caller recording; financial calls retain native nested-entry
rejection. Serialization remains explicitly unsupported.

Compatibility: no change to existing functions, solver options or first-order
quote-risk boundaries. Open questions: none.
