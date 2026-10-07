# F03 explicit linear-solve coordinates

Status: active. Numeric delivery first; native recording is a subsequent PR.
Source: the full AAD improvement plan and the user's native-only, no-regression
and scoped-performance amendments. Previous solves/diagnostics are accepted in
#490/#491/#493/#494. This document controls the remaining coordinate boundary.

## Problem and scope

The dense pullback treats each A(i,j) as independent. A shared symmetric entry
requires two contributions, while a banded matrix has fixed zeros outside its
band. Callers currently have to reconstruct these parameter gradients from a
dense matrix adjoint. That wastes storage and leaves parameter ordering implicit.

Provide immutable layout metadata, owning numeric solve caches and gradients
in independent parameter order. Retain general pivoted LU, including indefinite
symmetric systems. Native bindings, aliases and lifecycle follow after numeric
acceptance. Sparse factorization, Cholesky substitution, higher-order derivatives,
new Python/Excel functions and implicit/PDE operators are outside this increment.

## Requirements

1. `LinearSolveCoordinates_::Symmetric(n)` enumerates lower-triangle entries in
   row order: (0,0), (1,0), (1,1), ... . A parameter at (i,j), i>j, sets both
   A(i,j) and A(j,i); diagonals occur once. Its count is n(n+1)/2.
2. `Banded(n, below, above)` enumerates actual in-band entries by row, then
   increasing column. The row interval is [max(0,i-below), min(n,i+above+1)).
   There are no padding parameters. Outside entries are fixed zero.
3. Reject n<=0, widths outside [0,n-1], dense storage beyond the allocator's
   representable double-buffer range,
   invalid row/parameter indices and mismatched parameter counts before dense
   allocation. Count calculations must not overflow signed arithmetic.
4. Layouts own only constant-size metadata; copying/reading them is independent
   of source lifetime. Expansion owns its matrix, checks finite parameters and
   preserves negative/zero entries and symmetric indefinite matrices.
5. `CoordinateLinearSolvePullback_` owns the layout, one existing normalized LU
   and the solution X of AX=B. It retains no caller references. RHS, pivot and
   numerical-range checks have the same contract as `LinearSolvePullback_`.
6. For seed W, solve A^T Lambda=W once. Return RHS adjoints Lambda and, for each
   parameter k, the contraction of -Lambda X^T with dA/dp_k. A symmetric
   off-diagonal derivative is the SUM of both entries, without averaging.
7. Compute coordinate adjoints directly in O(p*m); allocate p coordinate values
   and n*m RHS values. Do not allocate a dense matrix adjoint to discard fixed
   entries. Dense LU's O(n^3) work and O(n^2) storage remain explicit.
8. Reject invalid/non-finite seeds, non-finite coordinate accumulation and
   unrepresentable substitution/scaling. A failed const reverse leaves the cache
   reusable. `ReverseRhs` avoids computing unused coordinate gradients.
9. Multiple RHS, repeated positive/negative/zero seeds and concurrent const
   reverse must agree with independent directional/difference references.
10. Numeric allocation follows existing buffer-capacity admission/refund rules.
    Demonstrate that reverse accepts a budget too small for a dense adjoint,
    and that one-byte-short failure refunds all temporary storage.
11. Leave ordinary solve APIs, Number/node/tape layouts and existing library
    objects unchanged. Reuse immutable accepted performance evidence only after
    proving archive-member and affected ordinary executable identity.

## Inputs and outputs

The numeric constructor takes a layout, `Vector_<>` parameter values,
`Matrix_<>` RHS and optional relative pivot tolerance (64 machine epsilons).
`Solution()` returns a const view owned by the cache. `Reverse(W)` returns an
owning `CoordinateLinearSolveAdjoints_` with `coordinates_` and `rhs_`.
`ReverseRhs(W)` returns an owning RHS-only adjoint. `Coordinates()` exposes the
owned layout; `ScaledMinimumPivot()` exposes the existing passive pivot metric.

## Executable acceptance

- Missing-API RED then analytic 2x2 symmetric GREEN; test diagonal factor one
  and off-diagonal paired sum explicitly.
- Enumerate asymmetric lower/upper, diagonal and full bands; exhaustive small
  layout indices, large allocation-free metadata and INT_MAX size requests
  fail safely where the dense extent is unrepresentable.
- Independent scalar elimination/Cramer, central differences at three step
  sizes and the directional identity cover multiple RHS and indefinite/pivoted A.
- Snapshot mutation, seed linearity, failure/recovery and concurrent const
  calls have focused tests, with exact buffer peak/admission/refund checks.
- Compile and run only changed/affected solve tests locally, plus an installed
  consumer. Inspect actual execution under applicable exact-head platform and
  sanitizer jobs before merge. Fix every actionable CI/Codacy/review finding.
- Select only changed coordinate size/band/RHS boundaries for informational
  cost measurements; existing ordinary acceptance requires identity proof or
  affected paired measurements. No portfolio/MC/PDE/full-matrix reruns.

## Remaining native requirements

The subsequent native PR records only p active coordinate bindings, never
dense Number placeholders for fixed zeros. It must support active coordinates,
active RHS and their three useful combinations, scalar/vector seeds, aliases,
snapshots, checkpoints, exception recovery and exact owned/scratch budgets.
It must share existing publication/lifecycle machinery while preserving ordinary
dense payload storage and hot paths. This numeric PR does not satisfy that gate.

## Open questions

None for the numeric boundary. Final native representation and optional
diagnostic composition require review against the accepted numeric surface.
