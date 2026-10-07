# Explicit solve-coordinate API

Status: numeric API approved for implementation under the active
[specification](../specs/aad-solve-coordinates.md).

Audience: C++ callers with independent symmetric or banded matrix parameters,
and the subsequent native event implementation. Existing dense APIs stay intact.

## Proposed surface

`LinearSolveCoordinates_` has static `Symmetric(n)` and `Banded(n, below, above)`
factories. It is a value with no default invalid state. `Size()`, `Count()` and
`IsSymmetric()` are constant-time metadata queries. `RowBegin(row)` and
`RowEnd(row)` identify the half-open physical column interval. `Location(index)`
returns the representative (row,column), using logarithmic row lookup without
allocating an index table. `Expand(values)` returns an owning dense matrix.

`CoordinateLinearSolvePullback_(layout, values, rhs, tolerance)` owns one LU and
X. `Solution()`, `Coordinates()`, `ScaledMinimumPivot()`, `Reverse(seed)` and
`ReverseRhs(seed)` parallel the existing numeric operator. Returned coordinate
adjoints use exactly the factory's order. A const view remains valid only while
the cache lives; reverse results are independently owned.

```cpp
const auto layout = Dal::LinearSolveCoordinates_::Symmetric(2);
const Dal::Vector_<> parameters = {2.0, 1.0, 3.0};
Dal::Matrix_<> rhs(2, 1), seed(2, 1);
rhs(0, 0) = 1.0; rhs(1, 0) = 2.0;
seed(0, 0) = 1.0; seed(1, 0) = -2.0;
const Dal::CoordinateLinearSolvePullback_ solve(layout, parameters, rhs);
const auto gradient = solve.Reverse(seed);
// coordinates: dA00, d(A10=A01), dA11; rhs: dB.
```

Factories avoid a new enum and an invalid combination of symmetry/band options.
A generic callback/map API would introduce lifetimes, duplicated entries and
arbitrary complexity that these two established layouts do not require. An
allocated location table would add resource ownership without improving the
rowwise reverse algorithm. Filtering a dense adjoint is rejected by the storage
requirement. Sparse factorization is deferred as a separately measurable change.

Errors name coordinates, width/count/index constraints, seed shape or numerical
range. Fully passive callers use the numeric cache; native recording is a later
surface. Binding exposure will require typed coordinate metadata and matching
order descriptions, rather than positional booleans or silently flattened bands.

Open questions: native diagnostic composition remains outside this delivery.
