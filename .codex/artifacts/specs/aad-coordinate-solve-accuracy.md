# Checked coordinate solve acceptance

Status: active numerical F03 increment after merged #498.

Source: the structured-operator requirements in
[the implementation plan](../plans/aad-implementation.md), the physical coordinate
cache in `dal-cpp/dal/math/matrix/linearsolvecoordinates.hpp`, and the checked
dense cache in `dal-cpp/dal/math/matrix/linearsolveaccuracy.hpp`.

Problem: packed symmetric/banded risks currently have no declared forward or
transpose accuracy policy. This increment supplies owning checked numeric
results, while preserving ordinary caller behavior and cost.

## Public behavior

`CheckedCoordinateLinearSolve_` accepts an existing symmetric or banded layout,
one passive physical parameter vector, a passive multiple-RHS matrix, a required
`LinearSolveAccuracyPolicy_`, and the existing optional relative pivot tolerance.
It owns the layout metadata and one `CheckedLinearSolve_` of the expanded physical
matrix. The parameter vector and RHS need not survive construction.

`Solution`, `Diagnostics`, and `Policy` are immutable owning-cache views.
`Reverse` returns an owning `CheckedCoordinateLinearSolveAdjoints_`: packed
parameter risks, RHS risks, and one transpose componentwise backward error per
RHS column. `ReverseRhs` returns the same result with an empty parameter-risk
vector. Repeated and concurrent reverse calls are pure const and independent.

The forward and transpose limits have the existing checked dense meaning,
including inclusive comparison, validation, zero seeds and finite-range errors.
The physical residual uses every expanded matrix entry. Packed symmetric
off-diagonal risks add both physical entry contributions. In-band zero values
remain differentiable parameters; entries outside the declared band remain
passive zeros. No clipping, averaging, regularization or matrix-size-dependent
accuracy relaxation is allowed.

## Implementation constraints

1. Exactly one checked numeric cache and its one LU are retained. Physical
   expansion is a construction temporary; no second coordinate pullback/cache
   is composed around the checked dense solver.
2. Reverse calls checked `ReverseRhs` once. It contracts `-Lambda * X^T` directly
   into a vector of the layout's parameter count. It never calls dense full
   `Reverse` and never allocates a dense matrix gradient.
3. Extract the existing packed contraction into one private inline helper.
   Preserve row-end hoisting, accumulation order, finite checks and inlining.
   Ordinary coordinate cache layout and public behavior stay unchanged.
4. Every owning numeric buffer remains under the existing caller capacity
   accounting. Construction and unpublished reverse failures refund allocations;
   an unsuccessful reverse does not invalidate this immutable numeric cache.
5. This representation still uses dense LU and a dense physical transpose.
   It makes no sparse/tridiagonal complexity or PDE solver claim.

## Independent correctness cases

| Case                        | Required observation                                                                                                                                |
|-----------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------|
| Symmetric two-by-two        | `A=[[3,1],[1,2]], B=[1,2], W=[2,-1]`: `X=[0,1]`, `Lambda=[1,-1]`, packed risks `[0,-1,1]`                                                           |
| Zero symmetric parameter    | `A=diag(3,2), B=[3,2], W=[3,2]`: zero off-diagonal still has risk `-2`                                                                              |
| Upper band                  | `A=[[2,s],[0,4]], B=[s,8]`, objective `3*X0-X1`: matrix-parameter and RHS contributions together give `-1.5` for `s`                                |
| Independent Cramer oracle   | Evaluate a closed-form two-by-two objective separately, check every packed parameter and RHS with three finite-difference steps                     |
| Multiple RHS                | Nonzero and zero seed columns retain independent RHS/error entries and contribute correctly to one packed gradient                                  |
| Rounding boundary           | `A=1+2^-27`: forward and transpose error `2^-55`; equality accepts and `nextafter` below rejects                                                    |
| Paired overflow             | Identity symmetric matrix, solution `[1,1]`, finite seeds `0.75*max`: RHS-only accepts; summed off-diagonal risk overflows and full reverse rejects |
| Omitted dense overflow      | Tiny diagonal/RHS construction with finite RHS risk and unused overflowing parameter risk: RHS-only accepts                                         |
| Ill-conditioned legal pivot | Explicit admissible tolerance preserves finite large risk and reports physical condition; default tolerance rejects                                 |
| Ownership/concurrency       | Mutated/destroyed inputs and policy do not affect captures; independent const readers and detached results remain valid                             |
| Rejections                  | Count/shape mismatch, nonfinite values/seeds/policy, singular matrix and invalid pivots reject; a subsequent valid reverse recovers                 |

## Exact resource proof

Use a four-by-four diagonal band with four parameters, two RHS columns and exact
power-of-two coefficients. Input fixture buffers live outside the measured scope.

- Checked dense retained buffers: `(2*16 + 8 + 2)*sizeof(double) + 4*sizeof(int)`.
- Coordinate construction has one additional temporary physical expansion of
  `16*sizeof(double)`. Peak is retained plus `3*16*sizeof(double)`; retained
  storage after construction equals the checked dense cache.
- RHS-only reverse retains and peaks at `(8 + 2)*sizeof(double)`.
- Full packed reverse retains and peaks at `(8 + 2 + 4)*sizeof(double)`.
- Exact limits accept; one-byte-short limits reject and refund. These bounds
  fail if a full sixteen-entry matrix gradient is accidentally allocated.

Verify the live allocation ordering before sealing these formulas into tests;
derive any necessary correction from source ownership, never from loosening the
budget until a failing test passes.

## Changed-path verification and publication

Use the existing `LinearSolvePullbackTest` suite so all existing sanitizer filters
actually run the new cases without unrelated workflow expansion. Run the new
numeric cases plus existing numeric coordinate cases and the native coordinate
callers of the shared contraction. Reuse unchanged dense/MC/tape evidence.

Freeze affected ordinary packed-contraction performance rows before sampling.
If archive/caller identities are unchanged, retain identity evidence and reuse
their accepted timings. Otherwise time only the changed coordinate callers with
the existing paired protocol; report the optional checked path's added costs
separately. No complete benchmark matrix is required for this increment.

Require strict production compilation, current-state documentation, installed
`find_package(dal-cpp)` use, exact-head CI/Codacy/review audits, and logs proving
every new numeric case ran in all fourteen platform/diagnostic profiles. Merge
this numeric increment before starting the separate native integration PR.
