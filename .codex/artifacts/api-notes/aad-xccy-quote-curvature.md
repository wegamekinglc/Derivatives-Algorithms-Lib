# Cross-currency rate snapshot API

Status: active; controlled by the [specification](../specs/aad-xccy-quote-curvature.md).

Add two overloads to `dal-public/src/ratecurvature.hpp`:

```cpp
RateCalibrationSnapshot_ NewRateCalibration(const CrossCurrencyCalibrationSpec_& spec);
RateCalibrationSnapshot_ NewRateCalibration(const JointXccyCalibrationSpec_& spec);
```

Both use the existing `RecalibrateRateWithRisk` and
`EvaluateRateQuoteCurvature` entry points. Results retain owning provenance and
the native solver's free-parameter order. Use `Provenance().Axis()` for block
identity instead of inferring counts or assuming a 3M projection tenor.

For staged snapshots, `Point()` contains basis quotes only. For joint snapshots,
it concatenates domestic YC quotes, foreign YC quotes and XCCY basis quotes.
YC groups are sorted once into native solver order; XCCY groups preserve input
order. FX spot and resolved fixing data are fixed dependencies. Caller mutation
does not change replay. Objective inputs concatenate parameters and raw quotes.
Cross-currency snapshots retain a fresh native inverse after one residual refinement
against the at-solution analytic Jacobian; the unchanged strict identity check
must pass before the matrix is admitted to provenance and quote pullback.

```cpp
auto snapshot = NewRateCalibration(jointXccySpec);
auto risk = EvaluateRateQuoteCurvature(objective, snapshot, request);
```

Malformed shapes, approximate/rectangular/singular systems, unsupported fixed
graph types and unavailable analytic solves fail explicitly. Existing error
contexts identify the base/direction/sign and calibration/objective/pullback
stage. Numeric payload is unchanged and excludes solver matrices/retained data.

Do not add a callback-based generic calibration provider or expose the saved
mutable market graph. Keep Python/Excel projection in the later binding work;
this increment makes no binding capability claim.
