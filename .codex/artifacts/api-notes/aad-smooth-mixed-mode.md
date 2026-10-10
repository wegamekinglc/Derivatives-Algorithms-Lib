# Smooth mixed-mode C++ surface

Status: active API decision.

Add `dal/math/aad/forwardoverreverse.hpp`. Required arguments are the typed
callback, numeric point and `ForwardOverReverseRequest_`, in that order.
`EvaluateForwardOverReverse` returns an owning `ForwardOverReverseResult_`.

`ForwardOverReverseNumber_` owns two native active components privately. Public
`Value()` and `DirectionalDerivative()` are numeric inspection methods; they
must not be used to bypass active arithmetic. Double construction is explicit.
Arithmetic and supported math overloads create the directional component through
the chain rule. No conversion from native `Number_` or implicit conversion to
double is available. Constants, unary signs and compound assignment are supported.

The callback accepts `RecordingScope_*` and
`const Vector_<ForwardOverReverseNumber_>&`. Its recording argument permits
consistent lifecycle rejection; checkpoints and reverse events are unsupported.
The callback must be deterministic across directional recordings at the same point.

Request fields are `directions_`, `numericPayloadBudgetBytes_` and
`recordingCapacityBudgetBytes_`. There is no step field. Result getters are
`Value()`, `Gradient()`, `Point()`, `Directions()`, `DirectionalDerivatives()`,
`HessianProducts()` and `Execution()`. Matrix rows follow direction order;
columns follow point order. No labels or parameter maps reorder coordinates.

Execution exposes method `NativeForwardOverReversePrototype`, callback/recording
and reverse-sweep counts, numeric payload and peak tape/cleanup capacities.
`ForwardOverReverseCapabilities()` reports only this type's smooth directional
composition. Ordinary native higher-order and independent nesting remain disabled.

Errors identify admission, primitive domain, current direction or reverse output.
The result contains only owning numeric values, so it survives recording cleanup.
Python/Excel projection and general simulation/calibration support are separate
increments. This opt-in surface preserves all existing native callers.

Typical usage supplies a spot direction to a smooth European option kernel and
reads its spot-column product as an AD Gamma, without selecting an outer step.
