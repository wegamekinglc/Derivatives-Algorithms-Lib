# Native solve diagnostic API

Audience: core C++ users composing recorded solves with scalar/native expressions.
The new optional header declares `DiagnosedLinearSolveResult_` and three
`LinearSolveWithDiagnostics` overloads. Required recording, matrix and RHS
arguments precede the unchanged optional pivot tolerance.

```cpp
#include <dal/math/aad/linearsolvediagnostics.hpp>

auto result = Dal::AAD::LinearSolveWithDiagnostics(&scope, matrix, rhs);
Dal::AAD::Number_ objective = result.solution_(0, 0) * result.solution_(0, 0);
const auto reciprocal = result.diagnostics_.reciprocalConditionInfinity_;
```

The owned report is passive, stable after close and independent of caller input
containers. Output Numbers follow the existing recording lifetime. The report
uses the same numeric diagnostic definition as `DiagnosedLinearSolve_` and can
be projected into a later public/binding result without exposing tape internals.

Keep the existing `LinearSolve` signature and header unchanged. Automatically
adding diagnostics would burden ordinary calls; using a raw reference to the
event's report would make it expire on restore/close. A boolean mode with a
variant result would complicate the return contract. The explicit function and
owning result provide a fixed C++ return type and caller-visible cost.

Invalid recordings/inputs retain existing errors. Diagnostic numerical-range,
tape-capacity or caller-buffer rejection throws a DAL exception and invalidates
the affected graph. No partial outputs or event are published on report admission
failure. A subsequent independent scope recovers. No user choice remains open.
