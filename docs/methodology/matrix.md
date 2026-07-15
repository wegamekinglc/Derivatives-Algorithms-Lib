# Matrix and Linear Algebra

This note describes the matrix and linear-algebra methodology in `dal-cpp/dal/math/matrix/`:
the storage conventions for dense and band-diagonal matrices, the direct solvers
(Cholesky, tri-diagonal, band-Cholesky), and the iterative Krylov solvers
(preconditioned conjugate-gradient and bi-conjugate-gradient). The focus is the
mathematical definition of each scheme, the interface it satisfies, and when each is
appropriate. All square matrices implement `Sparse::Square_`
(`dal-cpp/dal/math/matrix/sparse.hpp`); all factorizations implement
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

| Method                          | Meaning                                            |
|---------------------------------|----------------------------------------------------|
| `SolveLeft(b, x)`               | solve $A\,x = b$                                   |
| `SolveRight(b, x)`              | solve $A^{\top}\!x = b$                            |
| `MultiplyLeft(x, b)` / `Right`  | apply $A$ / $A^{\top}$ using the factorization     |

When the matrix is symmetric, `DecomposeSymmetric()` returns the narrower
`Sparse::SymmetricDecomposition_`, which additionally exposes `MakeCorrelated` (apply
$L$ to i.i.d. deviates) and `QForm` (form $J^{\top} A^{-1} J$ for a given $J$, used to
build Gauss-Newton Hessian proxies from a Jacobian).

## Numerical-Recipes Band Storage

Band-diagonal matrices are stored in the compact form used throughout the
*Numerical Recipes* tri-diagonal and band-diagonal routines. An $n \times n$ matrix $A$
with $m_1$ sub-diagonals and $m_2$ super-diagonals is held in an
$n \times (m_1 + 1 + m_2)$ array `store`, where the row index is the matrix row $i$ and
the column index is the **offset** $j - i$ shifted so that the diagonal sits at column
$m_1$:

$$
\texttt{store}(i,\; m_1 + (j - i)) \;=\; A_{i,j}, \qquad |i - j| \le m_1 \text{ or } m_2.
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
nBelow)` returns a `TriDiagonal_` when both `nAbove` and `nBelow` are at most 1, and the
general `Banded_` otherwise, so callers always use the tightest available representation.

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
iteration builds a conjugate search direction $p_k$ and a corresponding residual $r_k$ that
are $A$-orthogonal:

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
$(\tilde{p}_k, A\,p_k) = (A^{\top}\tilde{p}_k, p_k) = 0$ for $k \ne l$. The update is

$$
\alpha_k = \frac{(\tilde{r}_{k-1}, z_{k-1})}{(\tilde{p}_k, A\,p_k)}, \quad
x_k = x_{k-1} + \alpha_k\,p_k, \quad
r_k = r_{k-1} - \alpha_k\,A\,p_k, \quad
\tilde{r}_k = \tilde{r}_{k-1} - \alpha_k\,A^{\top}\tilde{p}_k,
$$

with a right-preconditioned shadow direction and the same $\beta$-ratio structure as CG.
BCG does not minimise a norm and can exhibit irregular convergence, but it is the cheapest
Krylov option when $A$ is genuinely non-symmetric and a transpose-product is available.

### When to Use Which

CG requires symmetry and positive-definiteness and is preferable whenever those hold: it is
shorter-recurrence (one matrix-vector product and one preconditioner solve per iteration),
monotone in the error $A$-norm, and numerically well-behaved. BCG is the fallback when $A$
fails symmetry — for instance, a non-symmetric Jacobian-based system — at the cost of two
matrix-vector products per iteration (one against $A$, one against $A^{\top}$) and less
predictable convergence.

Both solvers accept the same parameter tuple: relative tolerance `tolRel`, absolute
tolerance `tolAbs`, and an iteration cap `maxIterations`. Convergence is declared when the
residual 2-norm drops below

$$
\|r_k\|_2 \;\le\; \texttt{tolRel}\cdot\|b\|_2 \;+\; \texttt{tolAbs},
$$

and exceeding `maxIterations` without meeting the threshold throws.

## See Also

- [Interpolation](interpolation.md) — the natural cubic-spline construction reduces to a
  tri-diagonal system solved by the Thomas algorithm.
- [Log-discount curve](log_discount_curve.md) — the `LOG_CUBIC_NATURAL` scheme uses the
  tri-diagonal solve to compute spline second derivatives.
- [Underdetermined search](underdetermined_search.md) — Gauss-Newton steps can call on the
  Cholesky factorization of the normal-equations Hessian $J^{\top} J$.
