# Explicit solve accuracy API

Status: active, paired with the [specification](../specs/aad-solve-accuracy.md).
Audience: numeric C++ callers requiring an explicit residual policy.

An optional `dal/math/matrix/linearsolveaccuracy.hpp` adds
`CheckedLinearSolve_(matrix, rhs, policy, relativePivotTolerance)`.
`LinearSolveAccuracyPolicy_` stores forwardBackwardErrorLimit_ and
transposeBackwardErrorLimit_; zero-initialized fields demand zero reported error.
The policy argument is required. Pivot tolerance retains its existing default.

`Solution()`, `Diagnostics()` and `Policy()` expose const owning state.
`Reverse(W)` and `ReverseRhs(W)` return `CheckedLinearSolveAdjoints_` containing
`adjoints_` and `transposeBackwardErrors_`. Full reverse has n-by-n matrix and
n-by-m RHS contributions. RHS-only uses the same result with an empty matrix,
omitting its storage/work and unused overflow checks. A single passive result
avoids duplicate wrappers and supports consistent future getters.

```cpp
Dal::LinearSolveAccuracyPolicy_ policy{1e-14, 1e-14};
Dal::CheckedLinearSolve_ solve(a, b, policy);
const auto risk = solve.ReverseRhs(w);
const auto& lambda = risk.adjoints_.rhs_;
const auto& errors = risk.transposeBackwardErrors_;
```

Errors identify the forward/transpose policy or failed numeric constraint.
An unmet policy publishes no partial result and leaves the numeric cache intact.
No new method is added to existing solver classes and their includes stay optional.
There is no mutable last-seed report, exposed raw factor cache, additional
factorization, user callback, native event handle or binding name in this increment.

Open questions: none within this numeric surface.
