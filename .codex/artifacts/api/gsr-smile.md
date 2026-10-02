# GSR public surfaces

Active API design for the implementation in `specs/gsr-smile.md`.

`MultiFactorGSRVolData_` stores factorNames, gKnotDates, gValues, hKnotDates,
hValues and correlations. Matrices are factor rows by date columns; correlation
uses factorNames order. Factor names are nonempty and unique case-insensitively.
Dates start at the valuation date when joined to a curve. H can be signed.

`MultiFactorGSRModelData_` combines an existing `GSRCurveData_` snapshot and the
new volatility object. The legacy GSR data types and factories stay unchanged.

Public C++ factories are `NewMultiFactorGSRVolData(name, settings)` and
`NewMultiFactorGSRModelData(name, curve, vol)`. A settings overload avoids a long
positional C++ signature. Python/Excel use `MultiFactorGSRVolData_New` and
`MultiFactorGSRModelData_New`, matching the existing model factory convention.
Binding conversions delegate construction and validation to dal-public.

New model errors use `InvalidGSR...` prefixes with a concrete failed constraint;
the shared PSD factorization reports `InvalidCovariance...` errors.
Reject empty/duplicate names, nonfinite numbers, unordered dates, shape mismatch,
negative g, nonunit/asymmetric/indefinite correlation, null model inputs, and
curve/volatility anchor mismatch. Preserve old one-factor error contracts.

Calibration will have typed quote/config/result objects. Results contain fitted
model data, per-quote model prices/residuals, convergence status, numerical error,
and sensitivity provenance. Model-input AAD is not labeled as market quote risk.
