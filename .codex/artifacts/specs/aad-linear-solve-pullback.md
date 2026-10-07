# F03 independent dense linear-solve pullback

Status: active first F03 increment, based on merged P03 `97567d6e`.
Source: the detailed AAD plan, F03 and appendix C.8, and the user's native-only,
no-regression requirement. The implementation ledger controls subsequent work.

## Problem and delivery boundary

DAL's matrix decompositions support left and right solves, but do not expose an
owning reverse operator for a complete solve. Recording elimination as ordinary
scalar AAD operations retains algorithm intermediates rather than using the
linear system's mathematical derivative. This increment establishes an independent
numeric operator and independent oracles before modifying recording internals.

Deliver a core C++ dense square solve with one or more right-hand sides, an owning
cached factorization, and a reusable reverse operation. Do not change existing
decomposition factories, native scalar nodes, simulation paths, public valuation
interfaces, or bindings. Tape events, checkpoint cache ownership, implicit
calibration, PDE boundaries and structured independent coordinates remain later
F03 increments. This increment does not complete F03.

## Mathematical function and coordinates

For a nonsingular real matrix A of size n by n and B of size n by m, solve
AX = B. Columns of B are independent right-hand sides. For a seed W of size
n by m, reverse returns fresh contributions for the scalar objective
sum over i,k of W(i,k) X(i,k):

$$
A^{\mathsf T}\Lambda=W,\qquad
\bar B=\Lambda,\qquad
\bar A=-\Lambda X^{\mathsf T}.
$$

The matrix gradient uses all n squared independent dense entries. A symmetric
storage convention must sum its mirrored off-diagonal contributions; banded and
shared coordinates need their own mapping. This operator does not silently apply
such mappings. Callers own accumulation with other operations and alias mapping.

## Proposed core surface

`LinearSolvePullback_` lives in the matrix module. Construct it with a
`SquareMatrix_<>`, a `Matrix_<>` right-hand side, and an optional dimensionless
relative pivot tolerance. `Solution()` returns a const reference to the owned
forward result. `Reverse(solutionAdjoints)` returns `LinearSolveAdjoints_` with
owning `matrix_` and `rhs_` numeric matrices. `ScaledMinimumPivot()` exposes the
minimum accepted absolute pivot after normalizing A by its largest absolute entry.

The constructor owns factors and X; it retains no pointers into caller storage.
There is no setter or hidden mutable cache. Const reverse calls allocate their
own scratch/results and may safely share one completed operator across threads.
No Python or Excel function is added for this internal first increment.

## Numbered requirements

1. Accept n >= 1, m >= 1 and B.Rows() == n. Reject empty dimensions,
   incompatible rows and all non-finite A/B values before publishing an operator.
2. Require a finite relative pivot tolerance strictly between zero and one.
   Default to 64 times double machine epsilon. This is a pivot rejection policy,
   not a condition-number estimate or a guarantee about forward error.
3. Normalize A by its maximum absolute entry, then use deterministic partial
   row pivoting. Reject an all-zero matrix and any normalized pivot whose absolute
   value is at most the tolerance. Reject non-finite factor intermediates.
   Do not regularize, truncate rank, choose a pseudoinverse or change the function.
4. Factor once per construction. Both forward and transpose solves reuse these
   same LU factors and row permutations. Do not construct an inverse or refactor
   during reverse. Complexity is O(n cubed + n squared times m) forward and
   O(n squared times m) reverse; retained numeric storage is O(n squared + n m).
5. Solve every RHS column; preserve column order. Validate finite forward solve
   intermediates and X. Choose scaling independently per RHS: normalize before
   substitution for ordinary columns, but normalize afterward when early scaling
   would produce subnormal/zero nonzero entries. Preserve representable subnormal
   solves and transpose adjoints. Extreme finite inputs may be rejected if
   substitution overflows or a solved nonzero component cannot survive final
   scaling; do not silently publish a scaling-truncated gradient.
6. Reverse accepts exactly X's shape, validates finite seeds and applies the
   transpose permutation correctly for a nonsymmetric matrix with row swaps.
7. Reverse returns finite new matrices. Sum matrix contributions over every RHS
   column. Reject non-finite solve or outer-product intermediates rather than
   returning partial gradients.
8. Zero seed returns zero contributions; positive/negative seeds obey linearity.
   Repeating the same seed gives the same result. A failed reverse leaves the
   operator usable for the next valid call and leaves earlier returned results intact.
9. Mutating or destroying original A/B after construction cannot change X or
   later reverse results. Reverse does not consume seed input or mutate X/factors.
10. Keep diagnostics operation-specific: matrix/RHS/seed shape, pivot policy,
    factorization, forward solve, transpose solve and gradient overflow failures
    must identify the offending constraint. Use DAL runtime exceptions.
11. New behavior is opt-in. Existing hot-path source and layout remain unchanged.
    Build without Eigen as well as with Eigen; use only the built-in numeric
    kernel and native AAD for reference tests. No external AAD dependencies.

## Executable acceptance

- Save a focused RED showing the missing analytic solve/reverse behavior, then
  GREEN against a two-by-two hand-computed result. Use absolute tolerance 1e-10.
- Nonsymmetric three-by-three matrices requiring more than one row permutation
  verify both AX = B and A-transpose Lambda = W independently by multiplication.
- Multiple RHS compare against independent individual solves and sum their
  matrix contributions. Seed linearity, zero seeds and repeated reverse are tested.
- Compare small smooth systems with an independently recorded native scalar AAD
  elimination reference and central differences at h, h/2 and 2h. Predeclare
  abs/rel tolerances and steps; do not loosen them after failure.
- Check the directional identity
  sum W*dX = sum matrixAdjoint*dA + sum rhsAdjoint*dB, where dX is obtained
  independently from A*dX = dB - dA*X. Include nonsymmetric dA and multiple RHS.
- Test scales 1e-6, 1 and 1e6 with consistently scaled A/B, one-by-one systems,
  singular and near-pivot-threshold rejection, tolerance boundaries, non-finite
  inputs/seeds, incompatible shapes, arithmetic overflow and failure recovery.
- Verify ownership by changing original matrices and preserving old returned
  gradients. A concurrent read-only reverse test runs under TSan in final CI.
- Compare extreme-scale diagonal solves and transpose adjoints against direct
  component-wise division. A mixed-RHS request must preserve tiny representable
  values while another column uses early scaling to avoid substitution overflow.
  Reject final nonzero-to-zero scaling and verify the next valid reverse succeeds.
- Run only affected local tests as behavior grows. At a stable implementation
  head, inspect all applicable CI, Codacy annotations and review threads; fix
  failures, repeat publication audits and use a SHA-guarded merge.
- Measure complete construction plus reverse, and cached repeated reverse,
  against the scalar reference for predetermined sizes/RHS counts. Report time
  and retained/scratch storage. Existing formal performance gates remain required;
  unchanged production object identities may support unaffected workload reuse.

## Open questions and later gates

No user decision blocks this numeric increment. The default pivot threshold is
explicit and testable; an actual condition estimator is a separate diagnostic.
The next increment must settle tape event ordering, aliases, multiple adjoint
channels, exception invalidation and checkpoint release before integrating this
operator into recording. Current `SquareMatrixDecomposition_` has no general
dense pivot/rank diagnostics, so this opt-in operator owns its normalized LU;
existing decomposition semantics are preserved.
