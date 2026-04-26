# Underdetermined Search Method

Documentation of the underdetermined optimization solver in `dal/math/optimization/underdetermined.hpp` and `dal/math/optimization/underdetermined.cpp`.

## Purpose

The underdetermined solver handles problems with:

- more parameters than residual equations
- a need to satisfy pricing equations exactly or approximately
- a preference for the "smallest" or smoothest parameter move under a chosen weight matrix

This is the solver used by yield-curve calibration, but it is written as a general optimization utility.

## File Map

| File                                               | Purpose                                                                |
|----------------------------------------------------|------------------------------------------------------------------------|
| `dal/math/optimization/underdetermined.hpp`        | Core solver API declarations for `Find()` and `Approximate()`          |
| `dal/math/optimization/underdetermined.cpp`        | Core solver implementation and Jacobian handling                       |
| `dal/math/optimization/underdeterminedutils.hpp`   | Utility helpers for building smoothness weights such as `WeightsPWC()` |
| `dal/curve/yccalibration.hpp`                      | Yield-curve calibration declarations using the underdetermined solver  |
| `dal/curve/yccalibration.cpp`                      | Yield-curve calibration implementation using the solver                |
| `examples/underdetermined/underdetermined.cpp`     | End-to-end demonstration using curve calibration                       |
| `tests/math/optimization/test_underdetermined.cpp` | Direct solver coverage                                                 |
| `tests/curve/test_yccalibration.cpp`               | Integration coverage through yield-curve calibration                   |

## Core API

Two public entry points are exposed under `Dal::Underdetermined`:

```cpp
Vector_<> Find(
    const Function_& func,
    const Vector_<>& guess,
    const Vector_<>& tol,
    const Sparse::SymmetricDecomposition_& w,
    const Controls_& controls,
    Matrix_<>* eff_j_inv = nullptr
);

Vector_<> Approximate(
    const Function_& func,
    const Vector_<>& guess,
    const Vector_<>& func_tol,
    double fit_tol,
    const Sparse::Square_& w,
    const Controls_& controls
);
```

### `Function_`

Callers provide a residual function by deriving from `Function_`:

```cpp
class Function_ {
    virtual Vector_<> F(const Vector_<>& x) const = 0;
    virtual Jacobian_* Gradient(const Vector_<>& x, const Vector_<>& f) const;
    virtual void Gradient(const Vector_<>& x, const Vector_<>& f, Matrix_<>* j) const;
};
```

Key points:

- `F(x)` returns the residual vector.
- `Gradient(x, f)` may return a custom sparse or structured `Jacobian_`.
- If no custom Jacobian is supplied, the dense `Gradient(..., Matrix_<>*)` path falls back to finite differences.
- `FFast()` and `BumpSize()` are protected customization hooks used by the finite-difference path.

### `Jacobian_`

A custom Jacobian implementation must support:

- row scaling via `DivideRows()`
- products `J dx` and `Jᵀ t`
- quadratic-form construction `Jᵀ W⁻¹ J`
- secant updates through `SecantUpdate()`

This allows the solver to work with more than a plain dense matrix when the caller has better structure information.

## Controls

`Controls_` is an alias of the generated `UnderdeterminedControls_` settings object.

| Parameter             | Default  | Meaning                                                         |
|-----------------------|----------|-----------------------------------------------------------------|
| `maxEvaluations_`     | required | Total residual evaluations allowed                              |
| `maxRestarts_`        | required | Total fresh Jacobian builds allowed                             |
| `maxBacktrackTries_`  | `5`      | Backtracking iterations per step in `Find()`                    |
| `restartTolerance_`   | `0.4`    | Restart with a fresh Jacobian if the fitted `kMin` exceeds this |
| `backtrackTolerance_` | `0.1`    | Accept a step if `kMin` is below this                           |
| `maxBacktrack_`       | `0.8`    | Maximum fraction by which a step can be reduced                 |

The generated settings enforce:

- `maxEvaluations_ > 0`
- `maxRestarts_ > 0`
- `restartTolerance_` in `[0, 1]`
- `maxBacktrack_ > backtrackTolerance_` and `< 1`

## Residual Scaling

Both solver entry points internally scale residuals by user-provided tolerances.

For `Find()`:

```text
f_scaled[i] = f_raw[i] / tol[i]
```

For `Approximate()`:

```text
f_scaled[i] = f_raw[i] / func_tol[i]
```

This means the tolerances define the units in which the solver judges convergence.

## Exact Solve: `Find()`

`Find()` targets an exact fit in scaled-residual space.

### Optimization View

At each iteration it solves the minimum-weight-norm linearized step:

```text
minimize    1/2 sᵀ W s
subject to  J s = -f
```

which leads to:

```text
Q = Jᵀ W⁻¹ J
s = W⁻¹ Jᵀ solve(Q, -f)
```

In the implementation this is done by:

1. building `Q` through `j.QForm(w, &q)`
2. Cholesky solving the reduced system
3. mapping back to parameter space with `Jᵀ`
4. solving with the weight decomposition `W`

### Iteration Flow

`Find()` uses a scaled quasi-Newton loop:

1. start from `guess`
2. evaluate scaled residuals
3. build or refresh the Jacobian
4. compute the QP step
5. try `xNew = xOld + s`
6. if all scaled residuals are in `[-1, 1]`, stop
7. otherwise use backtracking / restart logic
8. after an accepted step, update the Jacobian with a secant update unless a restart was requested

### Convergence Test

The exact solve stops when every scaled residual is within one tolerance band:

```cpp
if (*MaxElement(fNew) < 1.0 && *MinElement(fNew) > -1.0)
    return xNew;
```

So `Find()` is not checking a norm; it checks componentwise satisfaction of the scaled equations.

### Backtracking and Restart Logic

For a candidate step, the solver computes:

```text
oldOld = fOld · fOld
oldNew = fOld · fNew
newNew = fNew · fNew
kMin   = (newNew - 0.5 * oldNew) / (newNew - oldNew + oldOld)
```

Interpretation in the current code:

- `kMin < backtrackTolerance_` → accept the step
- `kMin > restartTolerance_` → mark the Jacobian as stale and restart with a fresh one
- otherwise shrink the step and retry

The shrunken step uses:

```text
k = min(maxBacktrack_, min(kMin, 2 * (kMin - backtrackTolerance_)))
s *= 1 - k
```

## Approximate Solve: `Approximate()`

`Approximate()` is for problems where an exact scaled fit is not required or may be undesirable.

### Optimization View

The code solves a penalized quadratic step of the form:

```text
minimize  ||x + s - x0||²_W + jWeight ||f + J s||²
```

with:

```text
jWeight = ||func_tol||² / fit_tol²
```

This leads to an effective system:

```text
(W + jWeight Jᵀ J) s = W (x0 - x) - jWeight Jᵀ f
```

where:

- `x0` is the original guess
- `x` is the current iterate
- `W` keeps the solution close to the reference point in weighted norm
- `Jᵀ J` penalizes residual misfit

### Convergence Test

`Approximate()` stops when the Euclidean norm of the scaled residual vector is small enough:

```cpp
if (sqrt(InnerProduct(fNew, fNew)) <= fit_tol)
    return xNew;
```

Unlike `Find()`, this is a norm-based test.

### Linear Algebra Path

The approximate solve wraps the caller's weight matrix in `XPenaltyWeight_`, which represents:

```text
W_eff = W + jWeight Jᵀ J
```

`W_eff` is decomposed through a conjugate-gradient-backed `SymmetricDecomposition_` implementation (`XDecompByCG_`).

## Jacobian Paths

The solver tries Jacobians in this order:

1. **Custom Jacobian path** via `Function_::Gradient(x, f)` returning `Jacobian_*`
2. **Dense matrix path** via `Function_::Gradient(x, f, Matrix_<>*)`
3. **Finite-difference fallback** in the base implementation

The finite-difference implementation uses:

```cpp
dx = BumpSize();   // default 1e-4
xBumped[ix] += dx;
F(xBumped) - F(xBase)
```

and then divides by `dx` column by column.

Between restarts, the solver uses a Broyden-style secant update:

```text
J_new = J_old + ((df - J_old dx) / ||dx||²) dxᵀ
```

## Weight Matrix Semantics

The weight matrix `W` expresses which solutions are preferred when many parameter vectors fit the same equations.

Typical interpretation:

- large weight on a component → moving that component is expensive
- low weight on a component → that component is easier to move
- off-diagonal couplings → encourage neighboring parameters to move together or penalize roughness

`tests/math/optimization/test_underdetermined.cpp` shows this directly:

- in `TestFindRespectsWeights`, a one-equation two-unknown system with diagonal weights `(1, 4)` produces the lower-cost solution `x = (2.4, 0.6)` for `x0 + x1 = 3`

## Smoothness Helpers

`dal/math/optimization/underdeterminedutils.hpp` exposes helpers for building smoothness penalties. The main public helper is:

```cpp
Sparse::TriDiagonal_* WeightsPWC(const Vector_<DateTime_>& knots, double tau_s);
```

It uses `SelfCouplePWC()` to add nearest-neighbor couplings so adjacent parameters are penalized when they separate too sharply.

This is useful when the unknowns represent values along time buckets or knot points.

## Curve Calibration Integration

The solver is used directly in `dal/curve/yccalibration.cpp`.

### Current Calibration Setup

- unknowns: left/right instantaneous forward values at each knot
- parameter count: `2 * knotDates.size()`
- residuals: model rate minus market rate for each `YCInstrument_`
- initial guess: every parameter starts at `0.05`
- weights: a tridiagonal smoothing matrix built inline by `BuildSmoothingWeights()`
- solve path: `Underdetermined::Find(...)`

The calibration function in `dal/curve/yccalibration.cpp` builds a `PiecewiseLinear_` from the parameter vector, wraps it in `DiscountPWLF_` from `dal/curve/ycimp.cpp`, then reprices all instruments through `CalibratedYieldCurve_` declared in `dal/curve/yccalibration.hpp`.

### High-Level Pipeline

```text
YC instruments
  -> residual function F(x)
  -> Underdetermined::Find()
  -> fitted left/right forward values
  -> PiecewiseLinear_
  -> DiscountPWLF_
  -> calibrated discount curve
```

### Relation to `FittableCurve_`

The generic curve-fitting abstraction in `dal/curve/fittable.hpp` exists as:

```cpp
class FittableCurve_ {
    virtual int NX() const = 0;
    virtual void ApplyDX(Vector_<>::const_iterator dx, double leverage) = 0;
};
```

`DiscountPWLF_` in `dal/curve/ycimp.cpp` implements that interface, but the current `CalibrateYieldCurve()` path in `dal/curve/yccalibration.cpp` does not drive calibration through `FittableCurve_` directly. Instead it rebuilds a temporary `PiecewiseLinear_` from the candidate parameter vector inside the residual function.

## Example and Tests

### Example Program

`examples/underdetermined/underdetermined.cpp` demonstrates:

- a yield curve with more parameters than calibration instruments
- reporting degrees of freedom as `2 * knots - instruments`
- calibration through `CalibrateYieldCurve()`
- repricing checks after the solve

### Direct Solver Tests

`tests/math/optimization/test_underdetermined.cpp` covers:

- weighted exact solve on a linear one-constraint system
- approximate solve behavior and the fit-vs-distance balance
- the custom `Jacobian_` path on a multi-residual system
- failure behavior when evaluation/restart budgets are exhausted

### Integration Tests

`tests/curve/test_yccalibration.cpp` checks successful repricing for:

- a flat curve
- an upward-sloping curve
- a round-trip style setup
- calibration including `STIR_` instruments

## Current Code Realities and Limitations

These points reflect the code as it exists today:

1. `Find()` accepts `Matrix_<>* eff_j_inv`, but the current implementation does **not** populate it.
2. `Approximate()` returns the last iterate if it runs out of evaluation budget without meeting `fit_tol`; it does not throw on non-convergence by default.
3. `CalibrateYieldCurve()` currently builds its smoothing matrix inline instead of reusing `WeightsPWC()`.
4. `CalibratedYieldCurve_` supports discounting for repricing, but `FwdLibor()` currently throws.
5. The exact solver throws if it cannot find a descent direction and does not have a fresh-approximation path available:
   `REQUIRE(tookStep || approxJ, "Could not find a descent direction in underdetermined search")`.

## See Also

- `yield_curve.md` for the surrounding curve-construction framework
- `tests/math/optimization/test_underdetermined.cpp` for concrete solver behavior
- `examples/underdetermined/underdetermined.cpp` for a runnable integration example
