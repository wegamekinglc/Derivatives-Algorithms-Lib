# Native coordinate accuracy API

Status: active API contract after #499 merge. Audience: C++ native AAD
callers that opt into physical solve accuracy and invocation reports. Current
ordinary coordinate APIs and dense checked signatures stay compatible.

## Public surface

Header: dal/math/aad/linearsolvecoordinateaccuracy.hpp. Include the existing
checked result/report types plus coordinate layout declarations. Do not expose
collector mutation, slot-validation helpers or event payload types.

AAD::LinearSolveWithAccuracy(recording, coordinates, parameters, rhs, policy,
relativePivotTolerance = 64*epsilon) returns CheckedLinearSolveResult_. Required
arguments precede the optional tolerance. There are exactly three overloads:
Vector_<Number_>/Matrix_<Number_>, Vector_<Number_>/Matrix_<>, and
Vector_<>/Matrix_<Number_>. Passive/passive callers use the numeric checked API.

Result solution_, owning diagnostics_ and opaque event_ match the dense checked
surface. ReverseWithSolveAccuracy, ReverseSuffixWithSolveAccuracy and
ReversePrefixWithSolveAccuracy collect coordinate and dense events together.
Report(event) returns actual m-by-channel transpose errors. No new packed-risk
return type is needed: native input adjoints accumulate into registered slots.

## Typical use

```cpp
AAD::RecordingScope_ scope;
AAD::Number_ offDiagonal;
scope.RegisterInput(offDiagonal, 1.0);
scope.StartRecording();
Vector_<AAD::Number_> parameters{AAD::Number_(3.0), offDiagonal, AAD::Number_(2.0)};
Matrix_<> rhs(2, 1);
rhs(0, 0) = 1.0;
rhs(1, 0) = 2.0;
auto checked = AAD::LinearSolveWithAccuracy(
    &scope, LinearSolveCoordinates_::Symmetric(2), parameters, rhs,
    LinearSolveAccuracyPolicy_{0.0, 0.0});
AAD::Number_ objective = 2.0 * checked.solution_(0, 0) - checked.solution_(1, 0);
scope.FinishRecording();
scope.ClearAdjoints();
AAD::NativeOperations_::SetSeed(objective, 1.0);
const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
const auto error = reports.Report(checked.event_).transposeBackwardErrors_(0, 0);
// offDiagonal risk is -1.0; error is 0.0.
scope.Close();
```

## Errors and compatibility

Reject wrong parameter count, incompatible/empty RHS, invalid recording or
active slots, invalid numerical inputs/policy/pivot, and forward failure before
publishing outputs. Reverse failures use the existing failed-recording rule;
partial adjoints are unusable and no new partial collection escapes. Invalid
owner/checkpoint admission retains the existing pre-reverse behavior.

Rejected alternatives: a second result/report family (ambiguous collection),
dense active expansion (wrong memory scaling), returning one construction-time
transpose error (no actual-seed provenance), or exposing a mutable TLS collector
(unnecessary lifetime/API surface). Binding wrappers are later work and must
preserve owning values and explicit report axes.
