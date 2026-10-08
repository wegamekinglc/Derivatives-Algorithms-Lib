# Native solve accuracy API

Status: active; C++ native recording users are the initial audience.

Use optional `dal/math/aad/linearsolveaccuracy.hpp`:

```cpp
const auto checked = AAD::LinearSolveWithAccuracy(&scope, matrix, rhs, policy);
// Compose and seed the ordinary native objective, then finish recording.
const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
const auto& report = reports.Report(checked.event_);
```

`CheckedLinearSolveResult_` owns solution_, passive diagnostics_ and opaque
event_. Required policy precedes optional pivot tolerance. The three activity
combinations match existing dense solve APIs; no passive/passive recording API.

`SolveAccuracyReport_` owns event_, invocationId_, isMulti_ and
transposeBackwardErrors_. The opaque event includes recording identity.
The matrix rows identify RHS columns; columns identify actual reverse channels.

`SolveAccuracyReports_` offers Entries(), Report(event), InvocationId(),
IsMulti() and Channels(). It owns entries through DAL budgeted storage, supports
detached copies/moves and never refers back to an event/cache/tape.

`ReverseWithSolveAccuracy(scope)`,
`ReverseSuffixWithSolveAccuracy(scope, checkpoint)` and
`ReversePrefixWithSolveAccuracy(scope, checkpoint)` return one collection after
success. A missing event throws, including an event discarded by restore or
outside the chosen window. Empty graphs return empty entries with invocation
metadata. Scalar mode and vector width one remain distinguishable.

Ordinary reverse checks the policy but returns no report. A last-report getter
is rejected because repeated seeds/window changes would silently expose stale
data. A report from a failed invocation is never returned. Previous detached
reports remain historical values after a later failure.

Existing recording/numeric APIs and Python/Excel bindings retain their surfaces.
Public binding expansion follows the complete F03/F04 semantics separately.
