# Underdetermined Search

This note describes the optimisation method used to calibrate curves: a
constrained least-change solver for nonlinear systems that have more unknowns than
equations. The focus is the mathematics of the step and the iteration, not the
solver's code.

## The Problem

Given a residual map $r : \mathbb{R}^n \to \mathbb{R}^m$ (for example, model rate
minus market rate for each instrument), we seek parameters $x$ that make the
residuals vanish,

$$
r(x) = 0 .
$$

The defining feature is that the system is **underdetermined**: $n > m$. There are
more parameters than equations, so the solution set is (generically) an
$(n-m)$-dimensional manifold rather than a point. To pick a single, well-behaved
solution we impose a **least-change** preference under a metric defined by a
symmetric positive-definite weight matrix $W$.

Two solver modes serve different needs:

- **Exact fit** — drive every residual inside its tolerance band.
- **Approximate fit** — when an exact fit is impossible or undesirable (noisy or
  inconsistent quotes), minimise the residual norm while staying close to a
  reference parameter set.

## Residual Scaling

Residuals carry different units and magnitudes, so each is divided by a
user-supplied tolerance $\tau_j$ before the solver sees it:

$$
\tilde r_j(x) = \frac{r_j(x)}{\tau_j}.
$$

The tolerances therefore *define the units of convergence*: a scaled residual of
magnitude $\le 1$ means the instrument is fit to within its tolerance. The
Jacobian rows are scaled the same way.

## Exact Fit: Constrained Least-Change Step

Near the current iterate $x$ with residual $f = \tilde r(x)$ and Jacobian
$J = \partial \tilde r/\partial x$ (an $m \times n$ matrix), linearise:
$\tilde r(x+s) \approx f + Js$. We want a step $s$ that drives the linear model to
zero while moving as little as possible in the $W$-metric:

$$
\min_s \; \tfrac{1}{2}\, s^{\mathsf T} W s
\qquad \text{subject to} \qquad J s = -f .
$$

This is a quadratic program with linear equality constraints. Introducing
Lagrange multipliers $\lambda \in \mathbb{R}^m$,

$$
\mathcal{L}(s,\lambda) = \tfrac{1}{2} s^{\mathsf T} W s + \lambda^{\mathsf T}(Js + f),
$$

the stationarity condition $W s + J^{\mathsf T}\lambda = 0$ gives
$s = -W^{-1} J^{\mathsf T}\lambda$. Substituting into the constraint $Js=-f$:

$$
\big(J W^{-1} J^{\mathsf T}\big)\,\lambda = f
\qquad\Longrightarrow\qquad
\boxed{\,s = -\,W^{-1} J^{\mathsf T}\big(J W^{-1} J^{\mathsf T}\big)^{-1} f\,}.
$$

The matrix $J W^{-1} J^{\mathsf T}$ is the small $m \times m$ reduced system; it is
symmetric positive-definite (for full-rank $J$) and is solved by Cholesky
factorisation. The mapping $W^{-1} J^{\mathsf T}(JW^{-1}J^{\mathsf T})^{-1}$ is the
**$W$-weighted pseudoinverse** of $J$ — among all steps satisfying the linearised
equations it returns the one of minimum $W$-norm. The same weighted pseudoinverse,
evaluated at the solution, is the **effective Jacobian inverse** that maps
instrument bumps to parameter changes and is retained for risk.

## Exact Fit: Iteration

The solver is a scaled quasi-Newton loop with a backtracking line search:

1. Evaluate scaled residuals $f = \tilde r(x)$.
2. Build or refresh the Jacobian $J$ (see below).
3. Compute the least-change step $s$ from the boxed formula.
4. Trial point $x_{\text{new}} = x + s$.
5. **Convergence test (componentwise):** stop if every scaled residual lies in
   $[-1, 1]$. This is a per-instrument satisfaction test, not a norm — each
   instrument must be fit to its own tolerance.
6. Otherwise apply the line-search / restart logic
   (see [Backtracking Line Search](#backtracking-line-search)) and iterate.

## Jacobian Construction and Maintenance

The Jacobian is obtained by the most informative route available:

1. **Analytic / structured.** If the residual function supplies its own Jacobian
   (sparse or banded), it is used directly — most efficient when the
   parameter-to-instrument coupling is local. In curve calibration this Jacobian
   comes from AAD.
2. **Dense.** If a dense gradient is supplied, it is used as an $m \times n$
   matrix.
3. **Finite differences.** As a fallback, bump each parameter by a small $\Delta x$
   and form difference quotients column by column.

Between restarts the Jacobian is refreshed cheaply by a **Broyden secant
update**, which is the least-change correction (in Frobenius norm) consistent with
the most recent step $\delta x$ and residual change $\delta f$:

$$
J \leftarrow J + \frac{(\delta f - J\,\delta x)\,\delta x^{\mathsf T}}{\delta x^{\mathsf T}\delta x}.
$$

This avoids recomputing a full Jacobian every iteration while keeping it
consistent with observed behaviour.

## Approximate Fit

When an exact fit is not warranted, the step trades residual reduction against
proximity to the reference point $x_0$:

$$
\min_s \; \big\| (x + s) - x_0 \big\|_W^2 \;+\; \gamma\,\big\| f + J s \big\|^2 ,
\qquad \gamma = \frac{\|\tau\|^2}{\text{fitTol}^2}.
$$

The penalty weight $\gamma$ is set so that the relative importance of fitting
versus staying near $x_0$ is governed by the ratio of the instrument tolerances to
the desired fit tolerance. Setting the gradient to zero gives the normal
equations

$$
\big(W + \gamma\,J^{\mathsf T} J\big)\, s = W(x_0 - x) - \gamma\,J^{\mathsf T} f .
$$

The effective operator $W_{\text{eff}} = W + \gamma\,J^{\mathsf T} J$ is symmetric
positive-definite; the system is solved iteratively (conjugate gradient), which is
efficient because $J^{\mathsf T} J$ need only be applied, not formed. The
**convergence test is norm-based**: stop when $\|f\| \le \text{fitTol}$.

## The Weight Matrix as a Smoothness Prior

$W$ encodes which solutions are preferred when many fit the data:

- a large weight on a component makes moving it expensive;
- a small weight makes it cheap to move;
- off-diagonal couplings tie neighbouring components together.

For parameters indexed along a time axis (curve knots), a **tridiagonal** weight
penalising differences between adjacent knots acts as a discrete smoothness
(roughness) penalty. Among all parameter vectors that reprice the instruments, the
solver then selects the smoothest — the least oscillatory curve consistent with the
market.

## Implementation Details

The solver is implemented as free functions `Underdetermined::Find` and
`Underdetermined::Approximate` in `dal-cpp/dal/math/optimization/underdetermined.cpp`.

### Scaled Residual Wrapper (`XScaledFunc_`)

Before the solver loop, the raw residual function `funcIn` is wrapped in
`XScaledFunc_`, which divides every residual and every Jacobian row by the per-instrument
tolerance $\tau_j$ returned by `funcIn.Tolerances()`. This normalises the convergence test
to $|f_j| \le 1$ per component (exact mode) or $\|f\| \le \texttt{fitTol}$ (approximate
mode). The wrapper also caps the evaluation budget and tracks `nEvals_`.

The wrapper's Jacobian factory `XScaledFunc_::J` first queries the raw function's
`Gradient` method. If it returns a sparse Jacobian, the tol-divided sparse is used
directly. If it returns a dense Jacobian (the `XCurveJacobian_` case), the wrapper
divides rows by tol and wraps the result in an `XJDense_`. If `Gradient` returns
`nullptr` (bump fallback, or the analytic path declined eligibility), the wrapper
allocates a dense `jDense_` member, calls `funcIn.Gradient(x, f, &jDense_)` to fill
it, and divides rows. This shared dense storage avoids repeated allocation across
bump iterations.

### Forward Jacobian at the Solution

When the caller requests `fwdJacobianAtSolution` (non-null pointer), the convergence
branch in `Find` captures the **unscaled** dense forward Jacobian at the solution:

1. The raw (unwrapped) `funcIn` is called: `funcIn.Gradient(xNew, fUnscaled, fwdJacobianAtSolution)`.
2. The residual `fUnscaled` is reconstructed by element-wise multiplication of the
   scaled residual by the tolerance vector — avoiding a redundant `funcIn.F()` call
   that would bypass the solver's evaluation budget.
3. If `Gradient` returns `nullptr` (no analytic Jacobian at this `x`), the output is
   cleared so a caller-reused matrix cannot leak stale contents. There is no dense-FD
   fallback on this branch.

`StoreForwardJacobianAtSolution` performs the column-by-column extraction: it probes
each unit column of the Jacobian via `MultiplyLeft` (which returns $J \cdot e_k =
\text{column } k$, length $m$) and stores it into the output. This works for any
`Jacobian_` subclass.

### Backtracking Line Search

The exact-mode line search minimises a one-dimensional quadratic model of the scaled
residual norm squared along the trial step $s$. Given $a = f^{\mathsf T} f$ (current),
$b = f^{\mathsf T} f_{\text{new}}$ (cross), and $c = f_{\text{new}}^{\mathsf T}
f_{\text{new}}$ (candidate), the quadratic model

$$
\tilde f^2(k) = a\,k^2 + 2b\,k(1-k) + c\,(1-k)^2
$$

has derivative $\partial\tilde f^2/\partial k = 2a k + 2b(1-2k) + 2c(k-1)$, which
vanishes at

$$
k_{\min} = \frac{c - \tfrac{1}{2} b}{c - b + a}.
$$

The interpretation of $k_{\min}$ relative to the backtrack and restart tolerances:

- $k_{\min} < \texttt{backtrackTolerance\_}$ — the full step is good; accept it and
  optionally capture the forward Jacobian at the solution.
- $k_{\min} > \texttt{restartTolerance\_}$ — the linear model is poor; discard the
  secant-updated Jacobian (or the analytic one) and restart from a freshly computed
  Jacobian.
- Between the two — shrink the step by factor $1 - k$, capped by `maxBacktrack_`, and
  retry with the same Jacobian.

The loop budgets `maxBacktrackTries_` attempts per iteration; exceeding it forces a
restart. The `approxJ` guard (Broyden-updated Jacobian) ensures at least one Jacobian
is freshly computed per restart cycle.

### Controls Structure

The `Underdetermined::Controls_` struct (default-constructed) carries:

| Field                   | Default        | Role                                                          |
|-------------------------|----------------|---------------------------------------------------------------|
| `maxEvaluations_`       | $150$          | Hard evaluation budget across all restarts                    |
| `maxRestarts_`          | $3$            | Maximum Broyden refreshes before failure                      |
| `maxBacktrackTries_`    | $3$            | Line-search tries per iteration                               |
| `maxBacktrack_`         | $0.5$          | Maximum fractional step shrinkage                             |
| `backtrackTolerance_`   | $0.1$          | $k_{\min}$ below which the full step is accepted              |
| `restartTolerance_`     | $0.5$          | $k_{\min}$ above which the linear model is considered invalid |

## Summary

The method poses calibration as a constrained least-change problem. The exact mode
takes minimum-$W$-norm Gauss–Newton steps,
$s = -W^{-1}J^{\mathsf T}(JW^{-1}J^{\mathsf T})^{-1} f$, with line search,
restarts, and Broyden updates, and converges on componentwise tolerance
satisfaction. The approximate mode solves a regularised normal-equation system and
converges on residual norm. In both, the weight matrix turns the surplus degrees of
freedom into a smoothness prior.

## See Also

- [Yield curve construction](yield_curve.md) — the primary consumer of this solver.
- [Cross-currency calibration](xccy_calibration.md) — applies the same solver to a
  basis curve.
- [AAD methodology](aad.md) — supplies the analytic Jacobian used by the solver.
