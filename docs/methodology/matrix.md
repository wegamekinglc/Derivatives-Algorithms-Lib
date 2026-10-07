# Matrix and Linear Algebra

This note describes the matrix and linear-algebra methodology in `dal-cpp/dal/math/matrix/`:
the storage conventions for dense and band-diagonal matrices, the direct solvers
(Cholesky, tri-diagonal, band-Cholesky), and the iterative Krylov solvers
(preconditioned conjugate-gradient and bi-conjugate-gradient). The focus is the
mathematical definition of each scheme, the interface it satisfies, and when each is
appropriate. Sparse square-matrix operators implement `Sparse::Square_`
(`dal-cpp/dal/math/matrix/sparse.hpp`); the dense `SquareMatrix_` container is
separate. The decomposition interfaces are
`SquareMatrixDecomposition_` or its symmetric specialization
(`dal-cpp/dal/math/matrix/decompositions.hpp`).

## Common Interfaces

A `Sparse::Square_` is a square matrix that knows how to multiply, be tested for symmetry,
and produce a factorization. It exposes left/right matrix-vector products and a
`Decompose()` factory:

$$
b = A\,x \;\;\texttt{MultiplyLeft}, \qquad b = A^{\top}\!x \;\;\texttt{MultiplyRight}.
$$

`Decompose()` returns a `SquareMatrixDecomposition_` that supports forward/backward solves
against either $A$ or $A^{\top}$:

| Method                         | Meaning                                        |
|--------------------------------|------------------------------------------------|
| `SolveLeft(b, x)`              | solve $A\,x = b$                               |
| `SolveRight(b, x)`             | solve $A^{\top}\!x = b$                        |
| `MultiplyLeft(x, b)` / `Right` | apply $A$ / $A^{\top}$ using the factorization |

When the matrix is symmetric, `DecomposeSymmetric()` returns the narrower
`Sparse::SymmetricDecomposition_`, which additionally exposes `MakeCorrelated` (apply
$L$ to i.i.d. deviates) and `QForm` (form $J^{\top} A^{-1} J$ for a given $J$, used to
build Gauss-Newton Hessian proxies from a Jacobian).

Dense Cholesky solves resize their output vector to the decomposition size;
empty output vectors and in-place right-hand sides are supported. Dense
`Matrix_::Swap` also supports empty matrices. The adjacent `ArrayN_` and `Cube_`
containers preserve all overlapping coordinates during resize, including the
origin, and default-initialize new cells. Array dimensions must be nonnegative,
their strides and extent must fit in `int`, and resize preserves the dimension
count. A zero extent produces an empty array.

## Dense Storage and Numeric Kernels

`Matrix_` stores rows contiguously without padding. Row views are contiguous;
column views are strided. Double-precision vector and row inner products use
shared SIMD kernels, including mutable row views. Column views and other value
types retain the generic iterator implementation.

The SIMD implementation is compiled into the library. Its ISA selection and
reduction order are shared by all callers, including callers compiled with
different ISA flags. Portable x86-64 builds use SSE2; native builds can use AVX2
when the build CPU supports it. Other architectures use the scalar fallback.
No runtime CPU dispatch is performed.

Parallel partial sums change the addition order. Cancellation can therefore
change results beyond the low-order bits, even without fused multiply-add.
Results need not be bit-identical to a serial reduction or to a library built
for another ISA. Numerical checks use absolute error scaled by the sum of
absolute terms for cancellation, and scaled residuals for ill-conditioned
Cholesky systems; they do not impose a relative-error guarantee near zero.

Dense matrix-matrix products use zero-copy Eigen views by default, with Eigen's
internal parallelism disabled. `DAL_USE_EIGEN=OFF` selects the built-in SIMD
kernel. Both paths preserve output aliasing and empty-shape behavior.

## Dense Linear-Solve Pullback

`LinearSolvePullback_` in `dal-cpp/dal/math/matrix/linearsolvepullback.hpp`
solves $AX=B$ with one or more right-hand-side columns and retains its normalized,
row-pivoted LU factors and solution. `Reverse(W)` reuses those factors to return
fresh dense matrix and RHS contributions:

$$
A^{\mathsf T}\Lambda=W,\qquad
\bar B=\Lambda,\qquad \bar A=-\Lambda X^{\mathsf T}.
$$

The matrix result sums over all RHS columns. Its coordinates are independent
dense entries; symmetric or shared coordinates require callers to combine the
corresponding entries. The operator owns its numeric state, accepts repeated
const reverse calls, and can be shared across concurrent readers. It does not
record native tape events or accumulate into caller adjoints.

Inputs must have positive compatible dimensions and finite values. A is scaled
by its largest absolute entry before partial pivoting. The relative pivot
tolerance defaults to 64 times double machine epsilon and must lie strictly
between zero and one; pivots at or below it are rejected. `ScaledMinimumPivot()`
reports the smallest accepted normalized pivot, which is not a condition number.
The solve does not regularize or truncate rank. Non-finite arithmetic, including
overflow in a reverse contribution, raises a DAL exception; a failed reverse
leaves the operator usable for a subsequent valid seed.

RHS columns whose early normalization would become subnormal use the original
RHS during substitution and divide the solved column by the matrix scale
afterward. This preserves representable tiny solutions and adjoints; each RHS
chooses its scaling order independently. A nonzero solved component that becomes
zero at final scaling raises an exception instead of publishing a truncated
contribution. Finite inputs may still be rejected if substitution overflows.

```cpp
Dal::LinearSolvePullback_ solve(matrix, rhs);
const auto& values = solve.Solution();
const auto contributions = solve.Reverse(solutionSeeds);
// contributions.matrix_ and contributions.rhs_ own their dense numeric values.
```

Construction costs $O(n^3+n^2m)$ for n rows and m RHS columns. Each reverse costs
$O(n^2m)$, with no inverse or repeated factorization; retained storage is
$O(n^2+nm)$.

### Optional Solve Diagnostics

`DiagnosedLinearSolve_` in `dal/math/matrix/linearsolvediagnostics.hpp`
owns a numeric pullback and its passive diagnostics. It reuses the same LU to
solve normalized inverse columns and reports
`reciprocalConditionInfinity_`, the floating-point value of
$1/(\lVert A\rVert_\infty\lVert A^{-1}\rVert_\infty)$.
Norm scaling avoids overflowing the product or constructing the physical
inverse of a uniformly tiny matrix. This diagnostic is independent of the
minimum pivot; an accepted system can still be ill-conditioned. A small
reciprocal signals sensitivity, and a value below representable range is zero.
No additional condition threshold changes the pivot policy.

`componentwiseBackwardErrors_` contains one value per RHS column:

$$
\operatorname{berr}_j = \max_i
\frac{|(AX-B)_{ij}|}{\sum_k |A_{ik}X_{kj}|+|B_{ij}|}.
$$

A zero denominator denotes an exact zero equation and contributes zero.
`LinearSolveBackwardErrors(A, B, X)` also evaluates this metric independently
for any finite candidate X, including a solution from another solver. Binary
exponent scaling prevents intermediate product overflow/underflow; explicit
FMA product compensation and compensated summation retain small residuals
through cancellation. Rounding below the final representable ratio can still
produce zero. A small backward error alone does not guarantee a small forward
error for an ill-conditioned matrix.

```cpp
#include <dal/math/matrix/linearsolvediagnostics.hpp>

Dal::DiagnosedLinearSolve_ result(matrix, rhs);
const auto& diagnostics = result.Diagnostics();
const auto& values = result.Solve().Solution();
const auto contributions = result.Solve().Reverse(solutionSeeds);
```

Diagnostics add $O(n^3+n^2m)$ work, $O(n^2)$ temporary scratch and $O(m)$
retained error values. They perform no second factorization and retain no
inverse. All numeric allocations obey the active buffer budget and refund
failed construction. Opt-in construction rejects unsupported diagnostic
inverse range even if the supplied RHS alone is solvable. Ordinary
`LinearSolvePullback_` and native recorded solves keep their existing work,
layout and caches. Native recording diagnostic results require separate API
support.

The reciprocal definition and componentwise metric correspond to the
[LAPACK condition interface](https://www.netlib.org/lapack/explore-html/d4/daf/group__gecon_ga4f9b830e19e12c7f082ddb497a57af18.html)
and [backward-error interface](https://www.netlib.org/lapack/explore-html/d5/da4/group__gerfs_gaf9908a6db85a278e5756cbded5f49819.html).
The diagnostic computes inverse columns directly; it supplies neither a norm
estimator nor iterative refinement or certified forward-error bounds.

## Numerical-Recipes Band Storage

Band-diagonal matrices are stored in the compact form used throughout the
*Numerical Recipes* tri-diagonal and band-diagonal routines. An $n \times n$ matrix $A$
with $m_1$ sub-diagonals and $m_2$ super-diagonals is held in an
$n \times (m_1 + 1 + m_2)$ array `store`, where the row index is the matrix row $i$ and
the column index is the **offset** $j - i$ shifted so that the diagonal sits at column
$m_1$:

$$
\texttt{store}(i,\; m_1 + (j - i)) \;=\; A_{i,j}, \qquad -m_1 \le j-i \le m_2.
$$

Entries outside the band are not stored and read as zero. The mapping is implemented by the
`BandElements_` helper in `dal-cpp/dal/math/matrix/banded.cpp`, parameterised by `nBelow_`
(the $m_1$ shift). The same layout is reused for both the symmetric band-Cholesky
factorization (where the band has $m_1$ columns below the diagonal plus the diagonal, i.e.
$m_2 = 0$ on the stored lower factor) and the general band matrix
($m_1 = $ `nBelow`, $m_2 = $ `nAbove`).

The tri-diagonal case ($m_1 = m_2 = 1$) is special-cased by `Sparse::TriDiagonal_`, which
does not use the offset array at all. Instead it stores the three diagonals directly as
`diag_` (length $n$), `above_` (length $n-1$, the first super-diagonal), and `below_`
(length $n-1$, the first sub-diagonal). The factory `Sparse::NewBandDiagonal(size, nAbove,
nBelow)` returns a `TriDiagonal_` when both `nAbove` and `nBelow` are at most 1, and a
wider banded matrix otherwise, so callers always use the tightest available representation.

## Tri-Diagonal Solve (Thomas Algorithm)

For a tri-diagonal $A$, the LU factorization without pivoting can be written compactly.
Write $A = L\,U$ with

$$
L = \begin{pmatrix} 1 & & & \\ \ell_1 & 1 & & \\ & \ddots & \ddots & \\ & & \ell_{n-1} & 1 \end{pmatrix}, \qquad
U = \begin{pmatrix} \beta_0 & u_0 & & \\ & \beta_1 & u_1 & \\ & & \ddots & \ddots \\ & & & \beta_{n-1} \end{pmatrix},
$$

where $u_i = \texttt{above}\_i$ (the original super-diagonal is preserved on $U$) and the
recurrence

$$
\beta_0 = d_0, \qquad \beta_i = d_i - \frac{a_{i-1}\,c_{i-1}}{\beta_{i-1}}, \qquad
\ell_i = \frac{c_{i-1}}{\beta_{i-1}}
$$

with $d$ the diagonal, $a$ the super-diagonal, and $c$ the sub-diagonal. The implementation
in `TridagBetaInverse` (`dal-cpp/dal/math/matrix/banded.cpp`) stores the inverses
$1/\beta_i$ rather than the $\ell_i$ directly. The forward substitution starts with
$x_0=b_0/\beta_0$; subsequent forward and backward steps are

$$
x_i=\frac{b_i-c_{i-1}x_{i-1}}{\beta_i},
\qquad
x_{i-1}\leftarrow x_{i-1}-\frac{a_{i-1}}{\beta_{i-1}}x_i
$$

These are the `SolveLeft` recurrences for $A x=b$. `TriDecomp_::XSolveLeft_af`
passes the stored `below_` diagonal first and `above_` second to the internal
`TriSolve`, so forward substitution uses $c$ and backward substitution uses
$a$. `SolveRight` reverses those diagonals to solve against $A^{\top}$.

This is the Thomas algorithm. It is $O(n)$ in time and $O(n)$ in memory. Pivoting is not
used: the factorization is valid only when every $\beta_i$ is non-zero,
which holds in particular for **strictly diagonally dominant** and for **symmetric
positive-definite** tri-diagonal systems — the
two cases that dominate finite-difference PDE discretisations and natural-spline
construction. The `TriDecomp_` factorization wraps the asymmetric case; `TriDecompSymm_`
collapses `above_` and `below_` to one vector for the symmetric case, where left and right
solve coincide.

## Cholesky Factorization

For a symmetric positive-definite $A$, the Cholesky factor $L$ with $A = L\,L^{\top}$ is
computed by `CholeskyDecomposition` (`dal-cpp/dal/math/matrix/cholesky.cpp`). The dense
implementation works row-by-row, subtracting the inner product of previously computed row
entries before taking the square root:

$$
L_{i,j} = \frac{1}{L_{j,j}}\!\left( A_{i,j} - \sum_{k<j} L_{i,k}\,L_{j,k} \right), \quad j < i, \qquad
L_{i,i} = \sqrt{ A_{i,i} - \sum_{k<i} L_{i,k}^2 } .
$$

The dense path clips a negative pivot residual to zero in `CholeskyImpl`, then regularizes
the reciprocal diagonal stored for subsequent solves using the running mean diagonal and
the `regularization` argument (default `Dal::EPSILON`). It does **not** form or factor an
explicitly shifted matrix $A + \lambda I$. The decomposition then supports $A\,x = b$ via
forward and backward substitution, and exposes `MakeCorrelated` for converting i.i.d.
deviates into correlated ones by applying the retained lower factor.

The **band-Cholesky** factorization in `dal-cpp/dal/math/matrix/banded.cpp` applies the
same recurrence but restricts the inner-product sum to the band. For lower bandwidth $m$
its factorization cost is $O(n m^2)$; band triangular solves and multiplies cost
$O(n m)$. The resulting lower factor is stored back into the band layout, and the same
`BandedLSolve` / `BandedLTransposeSolve` pair handles forward and backward substitution.
Because the band layout places the diagonal at column $m$, both the dense and band forms
implement the same `SymmetricDecomposition_` interface and are interchangeable from the
caller's perspective.

## Krylov Solvers: CG and BCG

When $A$ is sparse or available only through its matrix-vector products, a direct
factorization is wasteful. The two iterative solvers in `dal-cpp/dal/math/matrix/bcg.cpp`
build the solution from the Krylov subspace $\mathcal{K}_k(A, r_0) = \text{span}\{r_0, A\,r_0, \dots, A^{k-1}\,r_0\}$.

### Preconditioned Conjugate Gradient (CG)

`Sparse::CGSolve` solves $A\,x = b$ for a **symmetric positive-definite** $A$. Each
iteration updates the residual $r_k$ along a search direction $p_k$. In exact
arithmetic the search directions are mutually $A$-conjugate:

$$
\alpha_k = \frac{(r_{k-1}, z_{k-1})}{(p_k, A\,p_k)}, \quad
x_k = x_{k-1} + \alpha_k\,p_k, \quad
r_k = r_{k-1} - \alpha_k\,A\,p_k,
$$

where $z_{k-1} = M^{-1}\,r_{k-1}$ for a symmetric preconditioner $M$ supplied by the matrix
through `HasPreConditioner_::PreConditionerSolveLeft`. The next direction is

$$
\beta_k = \frac{(r_k, z_k)}{(r_{k-1}, z_{k-1})}, \qquad p_{k+1} = z_k + \beta_k\,p_k .
$$

CG minimises the $A$-norm of the error over the Krylov subspace and, in exact arithmetic,
converges in at most $n$ iterations. It is the right choice whenever $A$ is SPD — for
example, the normal-equations Hessian $J^{\top} J$ that appears in
Gauss-Newton calibration.

### Bi-Conjugate Gradient (BCG)

`Sparse::BCGSolve` handles **non-symmetric** $A$. It maintains two residual sequences — a
forward one $r_k$ in the original space and a shadow one $\tilde{r}_k$ in the transposed
space — and constructs search directions $p_k$, $\tilde{p}_k$ that are bi-conjugate:
$(\tilde{p}_l, A\,p_k) = (A^{\top}\tilde{p}_l, p_k) = 0$ for $k \ne l$. The update is

$$
\alpha_k = \frac{(\tilde{r}_{k-1}, z_{k-1})}{(\tilde{p}_k, A\,p_k)}, \quad
x_k = x_{k-1} + \alpha_k\,p_k, \quad
r_k = r_{k-1} - \alpha_k\,A\,p_k, \quad
\tilde{r}_k = \tilde{r}_{k-1} - \alpha_k\,A^{\top}\tilde{p}_k,
$$

with a right-preconditioned shadow direction and the same $\beta$-ratio structure as CG.
BCG does not minimise a norm and can exhibit irregular convergence. It supports
non-symmetric systems when both matrix and transpose products are available.

### When to Use Which

CG requires symmetry and positive-definiteness and is preferable whenever those hold: it is
shorter-recurrence (one matrix-vector product and one preconditioner solve per iteration),
monotone in the error $A$-norm, and numerically well-behaved. BCG is the fallback when $A$
fails symmetry — for instance, a non-symmetric Jacobian-based system — at the cost of two
matrix-vector products per iteration (one against $A$, one against $A^{\top}$) and less
predictable convergence.

Both solvers accept the same parameter tuple: relative tolerance `tolRel`, absolute
tolerance `tolAbs`, and an iteration cap `maxIterations`. Convergence is declared when the
residual 2-norm is at or below

$$
\|r_k\|_2 \;\le\; \texttt{tolRel}\cdot\|b\|_2 \;+\; \texttt{tolAbs},
$$

and exceeding `maxIterations` without meeting the threshold throws. An exact-zero initial
residual returns immediately for either method. BCG also accepts a non-zero initial residual
that already meets the tolerance; CG enters the iteration loop in that case.

The implementation evaluates norms, tolerance bounds, and potentially canceling dot
products without relying on an overflowing or underflowing intermediate `double`. Ordinary
dot-product reduction remains the fast path when a conservative error bound proves its sign
and non-zero classification reliable. Ambiguous cases use an order-independent exact
accumulation of the binary64 products followed by one round-to-nearest, ties-to-even
conversion. Borderline convergence comparisons likewise use the exact squared binary64
quantities, so the inclusive boundary above is preserved across the full finite input range.

Scale-aware recurrence handling is deliberately limited to the `alpha` candidate
combinations. Before materializing `alpha` as binary64, an integer classifier proves that
the existing division and exponent placement are safe. Otherwise the stored numerator and
denominator remain an exact rational while each complete `alpha * value + base` expression
for the solution, residual, and BCG shadow residual is combined and rounded once to
binary64. This admits finite cancellation and subnormal results even when standalone
`alpha` would overflow or underflow. A complete expression that rounds to a non-finite
candidate still fails closed.

The later `beta/betaPrev` direction ratio remains on the existing binary64 `ScaledRatio`
and fused-combination path; the exact `alpha` evaluator does not change or cover it. For
fixed input bits, that evaluator is independent of flush-to-zero (FTZ) and preserves the
caller's FP mode. Solver-level FTZ validation is limited to the S3/S5 first-iteration
acceptance cases (minimum-subnormal formation and cancellation to $\pm 2^{-1074}$);
it does not extend to other iterations, arbitrary callbacks, convergence or
direct-residual arithmetic, or the `beta/betaPrev` path.

Inputs and every matrix or preconditioner result must have the expected size and contain
only finite values. Each update is assembled in private candidate buffers; a genuine
non-finite candidate fails before publication. If the recursive residual appears converged,
the solver recomputes $b-Ax$ from the candidate and publishes the candidate atomically only
when that direct residual independently meets the same tolerance. Malformed callback
results, non-finite arithmetic, recurrence breakdown, or failed direct confirmation throw
without exposing a partial candidate, leaving `x` at the entry state or the last fully
committed finite iterate.

### Exact-Alpha Differential Probe

`MatrixTest.TestBcgExactAlphaProbe` is an external differential probe rather than a normal
CTest assertion. It consumes a 53-row, tab-separated corpus supplied by the comparison
harness and writes a bit-exact trace for comparison with an independent implementation.
The corpus and expected trace deliberately remain owned by that external harness, so CTest
does not register the probe or claim it as repository-local coverage.

Build and run the seam executable explicitly:

```bash
cmake --build build/Release-linux --target dal_cpp_bcg_workspace_boundary_tests
BCG_EXACT_ALPHA_CORPUS=/absolute/path/to/exact-alpha.tsv \
BCG_EXACT_ALPHA_OUTPUT=/tmp/dal-exact-alpha.txt \
  ./build/Release-linux/dal-cpp/dal_cpp_bcg_workspace_boundary_tests \
  --gtest_filter=MatrixTest.TestBcgExactAlphaProbe
```

The corpus header must be
`id kind solver fp in0 in1 in2 in3 alpha_num alpha_den alpha_exp alpha_neg`, with fields
separated by tabs. A missing path, a different header, or any row count other than 53 fails
the probe before it writes a usable trace.

## Examples

No dedicated example program exercises the matrix layer in isolation; the
snippets below are drawn from the public headers under `dal-cpp/dal/math/matrix/`.

A band-diagonal system is built through `Sparse::NewBandDiagonal`, which returns
the tightest available representation — a public `TriDiagonal_` when both
bandwidths are at most one, otherwise a wider banded matrix reached only through
the `Sparse::Square_` interface — and solved through the common
`SquareMatrixDecomposition_` interface:

```cpp
// from dal-cpp/dal/math/matrix/banded.hpp
#include <dal/math/matrix/banded.hpp>
#include <dal/math/vectors.hpp>

using namespace Dal;

const int n = 8;
std::unique_ptr<Sparse::Square_> a(Sparse::NewBandDiagonal(n, 1, 1));   // tri-diagonal
for (int i = 0; i < n; ++i)
    a->Set(i, i, 2.0);
for (int i = 0; i < n - 1; ++i) {
    a->Set(i, i + 1, -1.0);   // above
    a->Set(i + 1, i, -1.0);   // below
}

const Vector_<> b(n, 1.0);
Vector_<> x(n);
std::unique_ptr<SquareMatrixDecomposition_> decomp(a->Decompose());
decomp->SolveLeft(b, &x);   // solve A x = b by the Thomas algorithm
```

For a dense symmetric positive-definite system, `CholeskyDecomposition` returns a
`SymmetricDecomposition_` whose `Solve` handles both forward and backward
substitution, and whose `MakeCorrelated` applies the retained lower factor to
i.i.d. deviates:

```cpp
// from dal-cpp/dal/math/matrix/cholesky.hpp
#include <dal/math/matrix/cholesky.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/math/vectors.hpp>

using namespace Dal;

SquareMatrix_<> a(3);
a(0, 0) = 4.0; a(1, 1) = 1.0; a(2, 2) = 9.0;
a(0, 1) = a(1, 0) = 1.0;
a(0, 2) = a(2, 0) = 2.0;
a(1, 2) = a(2, 1) = 0.0;

std::unique_ptr<Sparse::SymmetricDecomposition_> chol(CholeskyDecomposition(a));
const Vector_<> b = {1.0, 2.0, 3.0};
Vector_<> x(3);
chol->Solve(b, &x);   // forward/backward substitution against L L^T
```

When $A$ is available only through its matrix-vector products, the Krylov solvers
take the matrix by its `Sparse::Square_` base and converge under the combined
tolerance of the *When to Use Which* section. `CGSolve` is the right call for a
symmetric positive-definite $A$, such as $J^{\top}J$ when $J$ has full column rank:

```cpp
// from dal-cpp/dal/math/matrix/bcg.hpp
#include <dal/math/matrix/bcg.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/vectors.hpp>

using namespace Dal;

const int n = 64;
std::unique_ptr<Sparse::Square_> a(Sparse::NewBandDiagonal(n, 1, 1));
// ... fill a SPD band matrix ...
const Vector_<> b(n, 1.0);
Vector_<> x(n, 0.0);
Sparse::CGSolve(*a, b, /*tolRel=*/1e-8, /*tolAbs=*/1e-10, /*maxIterations=*/200, &x);
// Sparse::BCGSolve(*a, b, 1e-8, 1e-10, 200, &x);   // non-symmetric fallback
```

## See Also

- [Interpolation](interpolation.md) — the natural cubic-spline construction reduces to a
  tri-diagonal system solved by the Thomas algorithm.
- [Log-discount curve](../yield-curves/log-discount.md) — the `LOG_CUBIC_NATURAL` scheme uses the
  tri-diagonal solve to compute spline second derivatives.
- [Underdetermined search](underdetermined_search.md) — Gauss-Newton steps can call on the
  Cholesky factorization of the normal-equations Hessian $J^{\top} J$.
