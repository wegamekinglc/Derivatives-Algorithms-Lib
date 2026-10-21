# Core script calibration contracts

The core headers `dal/script/instrument.hpp` and `dal/calibration/protocol.hpp`
provide immutable contract data, typed parameter binding and ordered calibration
observations. They do not implement a calibration solver or certify a script for
a fast evaluator. Existing Product, curve and GSR entry points keep their own
behavior.

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
The returned raw core tree is not an admitted calibration execution plan. Model,
route, output-receiver and full template admission remain necessary before a
future evaluator can price or calibrate it.

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

`CalibrationError_` owns structured diagnostics and a nonnull partial
`CalibrationReport_`, including field paths and available object/source
context. Input and Load stages are distinguished. Reports currently provide
the owned axes and errors needed by these data boundaries; evaluator/solver
metrics are not populated as fabricated results. Their archive Write fails
with CalibrationPersistenceUnsupported. Saving an instrument does not save
a market snapshot, prepared plan, calibration session or quote-risk record.
