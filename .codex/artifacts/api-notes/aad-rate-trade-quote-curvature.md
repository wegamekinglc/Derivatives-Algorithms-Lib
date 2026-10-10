# Rate-trade curvature API

Status: active; controlled by the [specification](../specs/aad-rate-trade-quote-curvature.md).

Add to `dal-public/src/ratecurvature.hpp`:

```cpp
struct RateTradeQuoteCurvatureSettings_ {
    Vector_<> weights_;
    Handle_<MarketFixingSnapshot_> fixings_;
};

class RateTradeQuoteCurvatureResult_ {
public:
    const Ccy_& Currency() const;
    const RateQuoteCurvatureResult_& Curvature() const;
};

RateTradeQuoteCurvatureResult_ EvaluateRateTradeQuoteCurvature(
    const Vector_<RateTradeDefinition_>& trades,
    const RateCalibrationSnapshot_& calibration,
    const AAD::BumpOverAADRequest_& request,
    const RateTradeQuoteCurvatureSettings_& settings = {});
```

Use the existing `RateTradeDefinition_` financial terms. Component keys are the
snapshot provenance's `ComponentKeyByParameterBlock()` values. The native XCCY
contract chooses its actual tenor/collateral routes from the saved market. Read
the exact quote axis from `Provenance().Axis()`.

```cpp
auto calibration = NewRateCalibration(jointSpec);
RateTradeQuoteCurvatureSettings_ settings;
settings.weights_ = {1.0, -0.5};
auto result = EvaluateRateTradeQuoteCurvature({payerSwap, receiverSwap},
                                             calibration, request, settings);
const auto& risk = result.Curvature();
// result.Currency() is the actual common PV currency.
```

The nested result avoids duplicating the existing value/gradient/products,
calibration ownership and execution metadata accessors. Weights are dimensionless
fixed portfolio coefficients. Derivatives use raw decimal quote units. No
implicit PV conversion or market-label relabeling is available.

An explicit fixing snapshot supplements saved calibration observations and
disables global fallback. With no explicit snapshot, missing required historical
trade observations are captured once at request start. Saved observations cannot
be overridden. The result retains the calibration's immutable source; extra
trade fixing data belongs to this request's pricing objective.

Keep the scalar objective builder internal to core pricing. Rejected alternatives
are public mutable market access, duplicated public pricing formulas, and a full
trade Jacobian followed by weighted reduction. Only the final alternative would
perform one reverse direction per trade unnecessarily.

Errors identify the operation and the offending weights, row/instrument, currency,
fixing or request constraint. Existing replay failures retain base/direction/sign
and calibration/objective/pullback stages. C++ binding projection is deferred to
the later Python/Excel work; the API shape has four arguments with optional
settings last and an explicit result currency.
