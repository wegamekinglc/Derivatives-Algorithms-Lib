# Native directional curvature API

Audience: C++ kernel authors needing selected second-order columns or HVPs from
the built-in tape. Later public/binding adapters provide financial axes and units.

## Proposed surface

`dal-cpp/dal/math/aad/bumpoveraad.hpp` owns:

```cpp
using NativeScalarFunction_ =
    std::function<Number_(RecordingScope_*, const Vector_<Number_>&)>;
struct BumpOverAADRequest_ {
    Matrix_<> directions_;
    Vector_<> steps_;
    std::optional<size_t> numericPayloadBudgetBytes_;
    std::optional<size_t> recordingCapacityBudgetBytes_;
};
BumpOverAADResult_ EvaluateBumpOverAAD(
    const NativeScalarFunction_& function, const Vector_<>& point,
    const BumpOverAADRequest_& request);
size_t BumpOverAADPayloadBytes(size_t inputs, size_t directions);
```

The owning result exposes `Value()`, `Gradient()`, `HessianProducts()`, `Point()`,
`Directions()`, `Steps()` and `Execution()`. Execution stores method, gradient
evaluation/reverse counts, numeric payload bytes, peak tape bytes and cleanup
reserve. Three required arguments avoid positional settings proliferation.
Peak tape payload excludes unused cleanup reserve. Admission retains cleanup
headroom alongside live tape capacity; neither number describes total process memory.

## Orientation and units

Directions are M rows of N native-coordinate components. Product `(k,i)` is the
i-th component of H v[k]. With direction e[k], `(k,k)` is Gamma and `(k,i)` is
the corresponding cross-Gamma column entry. Numerical central columns need not
be exactly symmetric at finite steps. No factor-of-half price Taylor convention
or report scaling is applied. The caller owns direction units and step choice;
the API uses exactly the numeric points x±hv.

```cpp
BumpOverAADRequest_ request;
request.directions_ = Matrix_<>(1, 2, 0.0);
request.directions_(0, 0) = 1.0;
request.steps_ = {1e-3};
auto result = EvaluateBumpOverAAD(
    [](RecordingScope_*, const Vector_<Number_>& x) -> Number_ {
        return x[0]*x[0] + x[0]*x[1];
    },
    {2.0, 3.0}, request);
// Gradient = {7,2}; product row = {2,1}.
```

## Errors and lifetime

Use operation-prefixed runtime errors for malformed shape, nonfinite values,
zero directions, nonpositive steps, ineffective bumps and unsupported numeric
range. Admission completes before any callback. Payload/tape limits are separate;
neither is a total process-memory limit. No partial result survives failure.
The callback rebuilds from each supplied active point, keeps external state fixed,
and must not retain active numbers. Snapshotting does not deep-copy reference
captures. Independent recording nesting remains unsupported.
The supplied scope pointer permits existing `LinearSolve`/`ImplicitRoot` and
other recorded operators. The driver owns its lifecycle: callbacks must not
finish, close, checkpoint, rewind or change modes. Validate recording state again
after callback return, before inspecting or extending the returned root.

## Alternatives and compatibility

Dense Hessian construction would force quadratic storage/work for HVP users.
Automatic relative steps would obscure units and boundary decisions. Native
mixed mode requires separate implementation and capability validation. This
explicit additive driver changes none of the existing entry signatures, tape
layout or capability flags. Financial APIs and bindings remain later work.

Open API questions: none for the generic increment.
