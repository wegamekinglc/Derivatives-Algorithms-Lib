# GSR public surfaces

Active API design for the [GSR implementation](../specs/gsr-smile.md).

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

The implemented stochastic-volatility inputs and binding conventions are in the
[GSR guide](../../../docs/models/gaussian-short-rate.md#stochastic-local-volatility).
Smile calibration should return GSRSLVModelData through the same public factories.

New model errors use `InvalidGSR...` prefixes with a concrete failed constraint;
the shared PSD factorization reports `InvalidCovariance...` errors.
Reject empty/duplicate names, nonfinite numbers, unordered dates, shape mismatch,
negative g, nonunit/asymmetric/indefinite correlation, null model inputs, and
curve/volatility anchor mismatch. Preserve old one-factor error contracts.

Gaussian calibration uses typed quote/config/result objects exposed through
dal-public, Python and Excel. It fits selected g entries with H/R fixed and reports
fit, numerical refinement and rank diagnostics. New analytic APIs use doubles;
the existing simulation APIs supply model-input AAD. Stochastic-volatility and
local-leverage calibration still need calibrated quote-risk provenance and optimum
derivatives. Model-input AAD is not labeled as market quote risk.
