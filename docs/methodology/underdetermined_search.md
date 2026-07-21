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

## How AAD Supplies the Jacobian

AAD is a derivative provider for the search, not the search algorithm itself. The
underdetermined solver stores parameters, residuals, steps, and matrix operations as
`double`. It knows nothing about curves or AAD tapes; it only asks the caller's
`Underdetermined::Function_` for two operations:

- `F(x)`, which evaluates the residual vector with passive `double` arithmetic;
- `Gradient(x, f)`, which may return a `Jacobian_` implementation.

Curve-calibration residual functions override `Gradient` and, when analytic mode is
selected and the calibration route supports active evaluation, return an
`XCurveJacobian_` built by reverse-mode AAD. Returning `nullptr` selects the solver's
dense finite-difference fallback. Some higher-level calibration APIs instead reject an
unsupported analytic request before entering the solver; that policy belongs to the
caller rather than `Underdetermined::Find` or `Underdetermined::Approximate`.

This separation keeps the nonlinear iteration unchanged when the Jacobian source changes:

```text
passive F(x) evaluations
        |
        v
Function_::Gradient request
        |
        +-- AAD-supported route --> XCurveJacobian_
        |
        +-- nullptr -------------> dense parameter bumps
                                      |
                                      v
                         tolerance-scale Jacobian rows
                                      |
                                      v
                    weighted step / line search / Broyden update
```

### One AAD Jacobian Evaluation

For a calibration residual

$$
r_i(x) = q_i^{\mathrm{model}}(x) - q_i^{\mathrm{market}},
$$

the market quote is passive and the solver parameters are the independent AAD
variables. A current curve-calibration `Gradient` evaluation follows this sequence:

1. Obtain the thread-local AAD tape and enter `TapeGuard_`. The guard rewinds the tape
   on entry and exit, including exceptional exit, so allocated tape blocks can be reused
   without allowing a recording to escape the gradient call.
2. Convert every component $x_j$ to an `AAD::Number_` and register it as an independent
   variable in the curve parameter layout. Registration occurs before `NewRecording`,
   which is required by the supported backends.
3. Start the recording, construct the scalar-templated curve or curve block from the
   active parameters, and reprice all instruments with active arithmetic. This records
   the graph from $x$ through discount factors, forecast/discount routing, and instrument
   formulas to the active residuals $r_i$.
4. For each residual row $i$, seed its adjoint with one, propagate adjoints back to the
   start of the recording, and read the adjoint of every independent parameter:

   $$
   \bar r_i = 1
   \quad\Longrightarrow\quad
   \bar x_j = \frac{\partial r_i}{\partial x_j} = J_{ij}.
   $$

   The current `HarvestCurveJacobian` implementation performs one single-output reverse
   sweep per residual row. Its backend-neutral zeroing logic prevents adjoints from one
   row leaking into the next.
5. Store the harvested $m \times n$ matrix in `XCurveJacobian_` and leave the tape guard.
   No live `AAD::Number_` crosses this boundary; the solver receives ordinary doubles.

The recording sequence is implemented by the curve calibration adapters and
`HarvestCurveJacobian` under `dal-cpp/dal/curve/`. Backend-specific recording and
adjoint-zeroing details for the native, Adept, XAD, and CoDiPack implementations are
covered in [AAD methodology](aad.md#backends).

### From the AAD Matrix to a Solver Step

AAD produces the unscaled forward Jacobian $J_{ij}=\partial r_i/\partial x_j$.
`XScaledFunc_` then divides row $i$ by its residual tolerance $\tau_i$:

$$
\tilde J_{ij} = \frac{1}{\tau_i}\frac{\partial r_i}{\partial x_j}
              = \frac{\partial \tilde r_i}{\partial x_j}.
$$

The resulting `Jacobian_` supplies the four operations needed by the solver:

- `MultiplyLeft(dx)` computes $\tilde J\,dx$;
- `MultiplyRight(t)` computes $\tilde J^{\mathsf T}t$;
- `QForm(w, ...)` forms the reduced matrix
  $\tilde J W^{-1}\tilde J^{\mathsf T}$;
- `SecantUpdate(dx, df)` applies the Broyden correction after an accepted step.

Thus AAD determines the local residual geometry, while $W$ still determines which of
the infinitely many feasible parameter moves is preferred. AAD does not change the
least-change objective, convergence test, line search, or regularisation.

### Where AAD Runs During the Iteration

The solver does not record a tape for every trial residual evaluation. Trial points and
line-search candidates use the passive `F(x)` path. In exact mode, a fresh AAD Jacobian
is requested at the initial restart and whenever the line-search logic decides that the
current linear model must be restarted. Between those points, accepted steps update the
stored AAD-derived matrix with Broyden's secant formula. Approximate mode consumes the
same analytic `Jacobian_` interface and likewise uses secant updates between scheduled
gradient refreshes.

This hybrid is deliberate: AAD periodically restores an accurate derivative of the full
pricing graph, while inexpensive rank-one updates avoid rebuilding that graph at every
iteration.

At an exact solution, optional diagnostics can request fresh derivatives:

- the effective inverse is built from the tolerance-scaled Jacobian at the solution;
- the forward Jacobian is requested directly from the raw function and remains unscaled.
  It is populated only when `Gradient` returns a non-null analytic `Jacobian_`; this
  diagnostics path has no finite-difference fallback and clears the output otherwise.

These are separate outputs with different units. Requesting both may require separate
gradient evaluations. Approximate mode does not expose these exact-solution diagnostics.

### Why Reverse Mode Fits an Underdetermined System

A dense bump Jacobian requires a baseline residual evaluation plus one forward
evaluation for each of the $n$ parameters. The AAD path records the residual calculation
once and performs one reverse sweep for each of the $m$ residuals. Since the defining
case is $n>m$, differentiating from the smaller output side is a natural fit. It also
avoids bump-size selection and subtractive cancellation, so the local model is normally
more accurate near a tight calibration tolerance.

The comparison is not simply $m$ versus $n$ function calls: a reverse sweep traverses
the recorded graph and the tape consumes memory. The practical benefit depends on graph
size and sparsity, but the dimensional advantage and derivative accuracy are why AAD is
the default analytic route for eligible curve calibrations.

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
tolerance $\tau_j$ passed to `Find` as `tol` (or to `Approximate` as `funcTol`). This normalises
the convergence test
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

1. The raw (unwrapped) `funcIn` is called: `funcIn.Gradient(xNew, fUnscaled)` returns a sparse
   `Jacobian_` by `std::unique_ptr` (the dense overload is not used on this branch).
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
residual norm squared along the trial step $s$. Let $k$ be the fraction backtracked
from the full trial: $k=0$ is the candidate $x+s$, while $k=1$ is the current point
$x$. Given $a = f^{\mathsf T} f$ (current),
$b = f^{\mathsf T} f_{\text{new}}$ (cross), and $c = f_{\text{new}}^{\mathsf T}
f_{\text{new}}$ (candidate), linear interpolation of the two residual vectors gives
the quadratic model

$$
\tilde f^2(k) = a\,k^2 + 2b\,k(1-k) + c\,(1-k)^2
$$

Its derivative is $\partial\tilde f^2/\partial k = 2a k + 2b(1-2k) + 2c(k-1)$, which
vanishes at

$$
k_{\min} = \frac{c-b}{a-2b+c}.
$$

If the denominator is not positive, the implementation uses $k_{\min}=1$ and
therefore treats the quadratic model as unsuitable for accepting the full trial.

The interpretation of $k_{\min}$ relative to the backtrack and restart tolerances:

- $k_{\min} < \texttt{backtrackTolerance\_}$ — little or no backtracking is indicated;
  accept the full step.
- $k_{\min} > \texttt{restartTolerance\_}$ — the linear model is poor; discard the
  secant-updated Jacobian (or the analytic one) and restart from a freshly computed
  Jacobian.
- Between the two — discard a fraction $k$ of the full trial, retain the step factor
  $1-k$, cap the discarded fraction by `maxBacktrack_`, and retry with the same
  Jacobian.

The loop budgets `maxBacktrackTries_` attempts per iteration; exceeding it forces a
restart. The `approxJ` guard (Broyden-updated Jacobian) ensures at least one Jacobian
is freshly computed per restart cycle.

### Controls Structure

The `Underdetermined::Controls_` struct is a Machinist `settings` storable built from a
`Dictionary_`; omitted keys take the defaults below, and `maxEvaluations_` / `maxRestarts_`
are required (no default). It carries:

| Field                 | Default  | Role                                                          |
|-----------------------|----------|---------------------------------------------------------------|
| `maxEvaluations_`     | required | Point evaluations before giving up                            |
| `maxRestarts_`        | required | Gradient calculations before giving up                        |
| `maxBacktrackTries_`  | $5$      | Line-search tries per iteration                               |
| `maxBacktrack_`       | $0.8$    | Maximum fraction discarded from a full trial step             |
| `backtrackTolerance_` | $0.1$    | $k_{\min}$ below which the full trial step is accepted        |
| `restartTolerance_`   | $0.4$    | $k_{\min}$ above which the linear model is considered invalid |

## Summary

The method poses calibration as a constrained least-change problem. The exact mode
takes minimum-$W$-norm Gauss–Newton steps,
$s = -W^{-1}J^{\mathsf T}(JW^{-1}J^{\mathsf T})^{-1} f$, with line search,
restarts, and Broyden updates, and converges on componentwise tolerance
satisfaction. The approximate mode solves a regularised normal-equation system and
converges on residual norm. In both, the weight matrix turns the surplus degrees of
freedom into a smoothness prior.

## Examples

The underdetermined solver is consumed by curve calibration rather than called
directly. The example program constructs an instrument set and a denser knot
grid, then calls the positional `CalibrateYieldCurve` overload from
`dal-cpp/dal/curve/curveblock.hpp`, which assembles a `CurveCalibrationSpec_`,
runs the residual function through `Underdetermined::Find`, and returns the
fitted discount curve. See [`dal-cpp/examples/underdetermined/`](../../dal-cpp/examples/underdetermined/)
for a runnable version; its calibration entry point is:

```cpp
// from dal-cpp/examples/underdetermined/underdetermined.cpp
#include <dal/platform/platform.hpp>
#include <dal/curve/curveblock.hpp>

using namespace Dal;

const Date_ today(2024, 1, 15);
const String_& ccy = "USD";
const DayBasis_ basis("ACT_365F");

Vector_<Handle_<YCInstrument_>> instruments;
instruments.push_back(Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 1), 0.0450, basis)));
instruments.push_back(Handle_<YCInstrument_>(new STIR_(today, Date::AddMonths(today, 3), Date::AddMonths(today, 6), 0.0470, basis)));
instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 12), 0.0490, 6, basis)));
instruments.push_back(Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 60), 0.0510, 6, basis)));

Vector_<Date_> knotDates = {
    Date::AddMonths(today, 1),
    Date::AddMonths(today, 3),
    Date::AddMonths(today, 6),
    Date::AddMonths(today, 12),
    Date::AddMonths(today, 24),
    Date::AddMonths(today, 36),
    Date::AddMonths(today, 60),
};

// The system is underdetermined: with the default piecewise-linear-forward
// parameterization there are two parameters per knot, so n > m and the
// tridiagonal smoothing weight W picks the least-rough solution
std::unique_ptr<DiscountCurve_> dc(CalibrateYieldCurve(today, ccy, instruments, knotDates));
```

The example then wraps the calibrated curve in a `CurveBlock_`, reprices each
instrument through its `Precompute` rate, and prints the model-minus-market
error in basis points to confirm the componentwise tolerance test converged.
The solver controls (`maxEvaluations_`, `maxRestarts_`, line-search tolerances)
flow in through the `CurveCalibrationSpec_` fields when the spec-form overload
`CalibrateYieldCurve(spec)` is used instead of the positional convenience
overload shown above.

## See Also

- [Yield curve construction](yield_curve.md) — the primary consumer of this solver.
- [Cross-currency calibration](xccy_calibration.md) — applies the same solver to a
  basis curve.
- [AAD methodology](aad.md) — supplies the analytic Jacobian used by the solver.
