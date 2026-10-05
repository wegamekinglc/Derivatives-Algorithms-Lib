# F01 Dupire binding boundary

Status: Python calibration/pullback is accepted through `95f0706d` with all
35 CI checks. The [Excel extension](aad-dupire-excel.md), including the shared
model factory, is locally verified and needs publication-head CI. Common curve
adaptation remains required. The [binding review](../reviews/aad-dupire-python-bindings.md)
retains acceptance evidence. This extends the accepted [core/Hybrid API](aad-dupire-pullback.md),
without changing its frozen-input derivative or estimator.

## Audience and operations

Python users need to create a reusable calibration, feed its surface into an
existing Hybrid model, and map the resulting scalar model adjoints to quotes.
Expose the same passive snapshots, explicit parameter/direct seeds and separated
quote contributions as C++. Existing valuation/model factories remain compatible.

Add a public C++ `CalibrateDupireWithRisk(const BSModelData_&, inputs, name={})`
overload. A private constant-vol IVS adapter copies spot, vol and deterministic
carry and delegates to the checked core. This lets both language bindings use
the existing BS factory without introducing a public competing flat-IVS type.

Python registers `IVS_` with keyword-only spot/rate/dividend_yield and an
`implied_vol(strike, maturity)` override, and the existing `MertonIVS_` with
keyword-only spot/vol/intensity/average_jump/jump_std. Merton retains its existing
zero deterministic carry. Freeze under the GIL so Python callbacks are safe;
no callback survives in the calibration and native pullbacks release the GIL.

`DupireRiskInputs_` takes seven keyword-only fields: quote_strikes,
quote_maturities, quote_spreads, inclusion_spots, max_spot_spacing,
inclusion_times, max_time_spacing. Axis inputs accept list, tuple or DAL numeric
vector; numbers exclude bool and enums. Matrices require `DoubleMatrix_`.
Core validation remains authoritative for ordered axes, sizes and finite values.

Factories and extraction:

```python
calibration = dal.DupireCalibration_New(base, inputs, name="surface")
parameters = dal.DupireParameterAdjoints_FromRisk(valuation, calibration, "equity")
quotes = dal.DupireQuoteRisk_New(calibration, parameters, direct=None)
combined = dal.DupireScriptQuoteRisk_New(valuation, calibration, "equity", direct=None)
```

`base` must be an existing BS model or IVS. Required arguments precede
keyword-only name/direct. Seed constructors take calibration and numeric matrix.
Readonly getters return copied axes/matrices/configuration; the surface getter
returns a detached surface usable by existing Hybrid factories. Snapshot also
exposes spots/times/vols, carry, algorithm and full-content `matches`.
Core and composite results expose the C++ contributions, source valuation,
component and method. Copy/deepcopy retains passive immutable ownership while
returned numeric values remain detached. These objects introduce no archive
schema, active-number exposure or implicit valuation.

## Errors and compatibility

Type failures identify the function/field and expected type. Required handles
reject None; only optional direct accepts it. String inputs reject embedded
NUL and enums. Invalid numeric domains, incompatible snapshot identity, missing
surface columns and unsupported methods retain the existing core/public errors.
Python callback exceptions propagate unchanged; a later valid calibration and
pullback must recover, preserving earlier results.

Rejected alternatives: accepting any model as a flat IVS loses typed identity;
retaining a Python callback breaks frozen provenance; returning mutable aliases
breaks snapshot validation. A flat-only API would exclude the accepted Merton
and custom-IVS mathematical scope.

## Required evidence

Confirm missing APIs fail before implementation. Verify flat convenience and
custom IVS produce identical snapshots; retain all existing core/Hybrid oracles.
Exercise Python callback mutation/destruction/failure, readonly detached values,
strict input types, mismatch/error recovery, direct contributions and complete
Hybrid mapping for both evaluator modes. Fixed-path quote differences retain
the existing three steps and tolerance, using one worker initialized before
import. Native and Python results also receive an independent installed-consumer
comparison. Exact new-head CI is separate from previous accepted checks.

Excel's [separate API decision](aad-dupire-excel.md) defines immutable
nonserializable `_New`/`_Get_` handles, split grid configuration, detached
model/surface getters and separated contributions. It is implemented and
locally verified; this boundary does not close publication-head CI or common
curve adaptation.
