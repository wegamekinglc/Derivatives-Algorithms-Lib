# Checked coordinate linear solve API

Status: active numerical F03 increment. The
[specification](../specs/aad-coordinate-solve-accuracy.md) controls acceptance.

## Audience and existing surface

C++ numerical callers currently use `CoordinateLinearSolvePullback_` for owning
physical symmetric/banded solves and packed risks. `CheckedLinearSolve_` supplies
accuracy admission only for dense matrix coordinates. Neither surface requires
a recording. Public Python/Excel risk APIs do not currently bind these caches;
native recording and later binding integration remain separate deliveries.

## Proposed surface and rationale

`CheckedCoordinateLinearSolve_` accepts layout, parameters, RHS, required policy
and optional pivot tolerance, in that order. It exposes `Coordinates`,
`Solution`, `Diagnostics`, `Policy`, `Reverse` and `ReverseRhs` as const views or
pure const calls. `CheckedCoordinateLinearSolveAdjoints_` owns packed/RHS risks
and one transpose error per RHS. RHS-only returns an empty packed-risk vector.

One checked dense cache retains the physical LU/transpose. Direct packed
contraction avoids a full dense matrix gradient, and the physical policy avoids
inventing a residual formula on packed parameter storage. Composing another
coordinate cache would duplicate factors; projecting dense full reverse would
allocate and compute unused matrix risks. Both alternatives are excluded.

```cpp
const auto layout = LinearSolveCoordinates_::Symmetric(2);
const Vector_<> parameters{3.0, 1.0, 2.0};
const CheckedCoordinateLinearSolve_ solve(layout, parameters, rhs, {1e-14, 1e-14});
const auto risk = solve.Reverse(seed);
const auto rhsOnly = solve.ReverseRhs(seed);
```

## Errors and compatibility

Count/shape, nonfinite values, singular or rejected pivots, invalid policies,
physical accuracy violations, contribution overflow and capacity admission
throw `Exception_`. Failed reverse leaves the immutable numeric cache usable.
The existing dense policy defines inclusive limits and zero-error semantics.
No promise is made to validate an invalid policy before coordinate expansion.

Ordinary numeric/native coordinate APIs remain available, with unchanged
layouts, ordering and risk definitions. The checked representation retains
dense factorization/condition work; it supplies no sparse/PDE complexity claim.
No open public-interface decision remains for this numerical increment.
