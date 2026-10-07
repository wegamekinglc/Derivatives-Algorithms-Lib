# Native solve-coordinate API decision

Status: active, paired with the [specification](../specs/aad-native-solve-coordinates.md).
Audience: C++ callers already using native recording scopes.

## Surface

An optional `dal/math/aad/linearsolvecoordinates.hpp` declares
`AAD::LinearSolve(recording, coordinates, parameters, rhs, tolerance)` for the
three activity combinations. Parameters are `Vector_<Number_>` or `Vector_<>`;
RHS is `Matrix_<Number_>` or `Matrix_<>`. The result is `Matrix_<Number_>`.
The optional tolerance remains 64 machine epsilons and is the final argument.
Fully passive callers use `CoordinateLinearSolvePullback_` directly.

```cpp
auto layout = Dal::LinearSolveCoordinates_::Symmetric(2);
Dal::Vector_<Dal::AAD::Number_> parameters{a00, a10, a11};
auto x = Dal::AAD::LinearSolve(&recording, layout, parameters, rhs);
```

The layout precedes the ordered parameter vector, exposing its meaning at the
call site. Packing belongs to the immutable numeric layout. Do not add boolean
packing flags, dense Number expansion, callback hooks or per-node metadata.
Lifetime and seed operations continue through RecordingScope_/NativeOperations_.

## Errors and compatibility

Parameter count errors identify coordinates; RHS errors identify dimensions.
Input-slot and recording errors reuse the accepted native contract. Numerical
errors retain the numeric solve constraints. New overloads preserve all existing
dense signatures and headers; the ordinary aad/native headers need no new include.
Native outputs require their originating graph to remain live; the numeric cache
is owned internally and cannot be borrowed by callers.

Owning diagnostic coordinate results and transpose residual reports are not
claimed by these overloads. No new binding names or serialization surface is
introduced by this C++ increment.

## Open questions

None within the specified activity and layout boundary.
