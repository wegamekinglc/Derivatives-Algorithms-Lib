# Yield Curve Construction Methods

Documentation of the yield curve framework in `dal/curve/`.

## File Map

| File | Purpose |
|------|---------|
| `yc.hpp/cpp` | `YieldCurve_` — top-level curve abstraction (currency, discount access, LIBOR forecast) |
| `yccomponent.hpp/cpp` | `YCComponent_` — base for all curve components with dependency tracking and cloning |
| `discount.hpp/cpp` | `DiscountCurve_` — abstract discount factor interface |
| `ycimp.hpp/cpp` | `DiscountPWLF_` — concrete discount curve built on piecewise-linear forward rates |
| `fittable.hpp` | `FittableCurve_` — interface for calibration (`NX()`, `ApplyDX()`) |
| `yccalibration.hpp/cpp` | `YCInstrument_`-based calibration, `CalibratedYieldCurve_`, and `CalibrateYieldCurve()` |
| `piecewiseconstant.hpp/cpp` | `PiecewiseConstant_` — step-function representation with precomputed integrals |
| `piecewiselinear.hpp/cpp` | `PiecewiseLinear_` — continuous piecewise-linear function with precomputed integrals |

## Class Hierarchy

```
Storable_
└── YCComponent_                        (dependency tracking, Poll/Clone)
    ├── DiscountCurve_                  (abstract: operator()(from, to) → df)
    │   └── DiscountPWLF_              (piecewise-linear forwards → discount factors)

Storable_
└── YieldCurve_                        (currency, discount access, LIBOR forecast interface)
    └── CalibratedYieldCurve_          (lightweight wrapper around a calibrated discount curve)

CurveWithBase_<T_, B_>                  (template mixin: optional base curve + substitution)
└── DiscountPWLF_                      (also inherits FittableCurve_ for calibration)
```

## Core Abstractions

### YieldCurve_

Top-level entry point. Holds a currency and provides the interface for:

- `Discount(CollateralType_)` → returns the `DiscountCurve_` for a given collateral type
- `FwdLibor(PeriodLength_, Date_)` → forward LIBOR rate for a given tenor and fixing date

Note: the current `CalibratedYieldCurve_` implementation wraps a discount curve for calibration/pricing, but deliberately leaves `FwdLibor()` unsupported.

### DiscountCurve_

Pure interface — a single method:

```cpp
double operator()(const Date_& from, const Date_& to) const;
```

Returns the discount factor from `from` to `to`.

### YCComponent_

Base class for anything that participates in the curve dependency graph:

- `Poll(Vector_<const YCComponent_*>*)` — collect all dependent components
- `Clone(name, substitutions)` — deep-copy with base-curve substitution (for bump-and-reprice risk)

## Piecewise Function Building Blocks

### PiecewiseConstant_

A step function defined by knot dates and right-continuous values.

**Members:**
- `knotDates_` — sorted dates where the function jumps
- `fRight_` — value effective on-or-after each knot
- `sofar_` — precomputed cumulative integrals at each knot

**Integral formula:**

```
sofar_[i] = sofar_[i-1] + (knotDates_[i] - knotDates_[i-1]) * fRight_[i-1]
```

For an arbitrary date between knots `[i-1, i)`:

```
IntegralTo(d) = sofar_[i-1] + (d - knotDates_[i-1]) * fRight_[i-1]
```

Before the first knot, extrapolates flat at `fRight_[0]`.

### PiecewiseLinear_

A continuous piecewise-linear function supporting left/right limits at knots (for discontinuities).

**Members:**
- `knotDates_` — sorted dates
- `fLeft_` — left-limit value at each knot
- `fRight_` — right-limit value at each knot
- `sofar_` — precomputed cumulative integrals at each knot

**Interpolation between knots `i` and `i+1`:**

```
elapsed     = d - knotDates_[i]
span        = knotDates_[i+1] - knotDates_[i]
elapsedFrac = elapsed / span
fStart      = fRight_[i]
fStop       = fLeft_[i+1]
value       = fStart + elapsedFrac * (fStop - fStart)
```

**Integral between knots (trapezoid rule):**

```
sofar_[i] = sofar_[i-1] + 0.5 * (fRight_[i-1] + fLeft_[i]) * (knotDates_[i] - knotDates_[i-1])
```

Extrapolation: flat at `fLeft_[0]` before the first knot, flat at `fRight_.back()` after the last.

## Discount Curve Construction: DiscountPWLF_

The only concrete discount curve implementation. Builds discount factors from piecewise-linear instantaneous forward rates.

### Construction

```cpp
DiscountCurve_* NewDiscountPWLF(
    const String_& name,
    const String_& ccy,
    const PiecewiseLinear_& fwds,                       // forward rate curve
    const Handle_<DiscountCurve_>& base = {}            // optional base curve
);
```

**Inputs:**
- A `PiecewiseLinear_` of instantaneous forward rates (knot dates + left/right values)
- A currency code stored on the resulting discount curve
- An optional base discount curve (for multi-curve / spread construction)

### Discount Factor Calculation

```cpp
double operator()(const Date_& from, const Date_& to) const {
    double integral = fwds_.IntegralTo(to) - fwds_.IntegralTo(from);
    return exp(-integral / 365.0) * (base_ ? (*base_)(from, to) : 1.0);
}
```

Mathematically:

```
DF(from, to) = exp( -1/365 * ∫[from,to] f(t) dt ) × base_DF(from, to)
```

Where `f(t)` is the piecewise-linear instantaneous forward rate and the 365 divisor converts from day-count to annualized rates.

### LIBOR Forecast

Forward LIBOR rates are derived from discount factors:

```cpp
double LiborForecastFromDiscounts(
    const DiscountCurve_& dc,
    const Date_& fix_date,
    int tenor_months, int tenor_weeks,
    const DayBasis_& daycount
) {
    auto end = fix_date.AddDays((365 * tenor_months) / 12 + 7 * tenor_weeks);
    double df = dc(fix_date, end);
    return (1.0 / df - 1.0) / daycount(fix_date, end, nullptr);
}
```

Standard simple-rate formula: `L = (1/DF - 1) / τ`.

## Multi-Curve Framework

The library supports multi-curve construction through:

1. **Base curve layering** — `DiscountPWLF_` accepts an optional `Handle_<DiscountCurve_>` base. The final discount factor is the product of the spread curve's own DF and the base DF.
2. **Dependency tracking** — `CurveWithBase_<T_>` mixin implements `Poll()` to traverse the base-curve chain, enabling consistent bumping across the full dependency graph.
3. **Clone with substitution** — `Clone(name, substitutions)` deep-copies a curve while replacing base curves, used for bump-and-reprice risk scenarios.

Typical setup: an OIS discount curve as the base, with tenor-specific spread curves layered on top.

## Calibration Support

`DiscountPWLF_` implements `FittableCurve_`:

- `NX()` — returns `2 * knotDates_.size()` because each knot has left/right forward values
- `ApplyDX(Vector_<>::const_iterator dx, double leverage)` — perturb forward rate parameters and refresh the cached `PiecewiseLinear_` integrals

This enables iterative solvers (Newton, Levenberg-Marquardt) to bootstrap forward rates from market instrument prices.

## Curve Building Pipeline (High-Level)

```
`YCInstrument_` implementations (`Deposit_`, `Swap_`, `STIR_`, ...)
        │
        ▼
Underdetermined::Find()                         ← scaled quasi-Newton solver
        │  evaluates pricing residuals f(x)
        │  computes Jacobian (sparse / dense / finite-diff)
        │  solves QP: s = W⁻¹ J · solve(Jᵀ W⁻¹ J, -f)
        │  calls FittableCurve_::ApplyDX() + Update()
        ▼
PiecewiseLinear_ of instantaneous forward rates
        │  (2K parameters: left/right values at K knots)
        ▼
DiscountPWLF_ (discount factors via integration)
        │  DF = exp(-∫f(t)dt / 365) × base_DF
        ▼
CalibratedYieldCurve_ (wraps the calibrated discount curve; `FwdLibor()` is currently unsupported)
```

## Underdetermined Search for Curve Calibration

The solver that actually bootstraps forward rates from market instruments lives in `dal/math/optimization/underdetermined.hpp/cpp`, with utility helpers in `underdeterminedutils.hpp`.

For a solver-focused description of the optimization method itself, see `underdetermined_search.md`.

### Problem Statement

Yield curve calibration is typically **underdetermined**: there are more forward-rate parameters (unknowns) than market instruments (equations). For a `PiecewiseLinear_` with `K` knots, there are `2K` parameters (left + right values at each knot), but often fewer than `2K` instruments.

The solver finds `x` that satisfies `f(x) ≈ 0` (pricing errors vanish) while preferring smooth, well-behaved solutions:

```
minimize: ||f(x) / tol||² + λ ||x - x₀||²_W
```

Where:
- `f(x)` — vector of pricing residuals (market price − model price)
- `tol` — per-instrument tolerance (scales each residual to dimensionless units)
- `W` — symmetric positive-definite weight matrix encoding smoothness preference
- `x₀` — initial guess / reference point

### API

Two entry points:

```cpp
// Exact solve: find x such that all scaled residuals < 1
Vector_<> Underdetermined::Find(
    const Function_& func,
    const Vector_<>& guess,
    const Vector_<>& tol,
    const Sparse::SymmetricDecomposition_& w,
    const Controls_& controls,
    Matrix_<>* eff_j_inv = nullptr          // optional: effective Jacobian inverse
);

// Approximate solve: fit within a looser tolerance
Vector_<> Underdetermined::Approximate(
    const Function_& func,
    const Vector_<>& guess,
    const Vector_<>& func_tol,
    double fit_tol,
    const Sparse::Square_& w,
    const Controls_& controls
);
```

**Control parameters** (`UnderdeterminedControls_`):

| Parameter | Default | Meaning |
|-----------|---------|---------|
| `maxEvaluations_` | — | Total function calls allowed |
| `maxRestarts_` | — | Total fresh Jacobian computations |
| `maxBacktrackTries_` | 5 | Linesearch iterations per step |
| `restartTolerance_` | 0.4 | Restart Jacobian when `kMin` exceeds this |
| `backtrackTolerance_` | 0.1 | Accept step when `kMin` is below this |
| `maxBacktrack_` | 0.8 | Maximum step reduction fraction |

### Algorithm: Scaled Quasi-Newton with Backtracking

Each iteration:

1. **Compute or update Jacobian** — fresh finite-difference Jacobian on first iteration and after restarts; Broyden secant update otherwise
2. **QP step** — solve the quadratic subproblem for the search direction:
   ```
   Q = Jᵀ W⁻¹ J
   s = W⁻¹ J · solve(Q, -f)
   ```
   This is the minimum-`W`-norm solution to the linearized system.
3. **Backtracking linesearch** — evaluate `f(x + s)`, compute optimal step fraction `k` via quadratic interpolation:
   ```
   k_min = (newNew - 0.5 * oldNew) / (newNew - oldNew + oldOld)
   ```
   - `k_min < 0.1` → accept step
   - `k_min > 0.4` → restart with fresh Jacobian
   - otherwise → reduce step by `s *= (1 - k)` and retry
4. **Convergence** — all scaled residuals in `[-1, 1]`:
   ```cpp
   if (*MaxElement(fNew) < 1.0 && *MinElement(fNew) > -1.0)
       return xNew;
   ```

### Jacobian Computation

Three modes, tried in order:

1. **Sparse Jacobian** — if `Function_::Gradient()` returns a `Jacobian_*`, use it directly (most efficient for banded/sparse structures)
2. **Dense Jacobian** — if `Function_::Gradient()` populates a `Matrix_<>*`
3. **Finite difference** (default fallback) — bump each parameter by `dx = 1e-4`:
   ```cpp
   for (int ix = 0; ix < nx; ++ix) {
       xBumped[ix] += dx;
       FFast(xBumped, &fBumped);
       j.Col(ix) = (fBumped - fBase) / dx;
       xBumped[ix] = x[ix];
   }
   ```

Between restarts, the Jacobian is maintained cheaply via **Broyden secant update**:

```
J_new = J_old + ((df - J_old · dx) / ||dx||²) · dxᵀ
```

### Regularization: Smoothness Penalty

The weight matrix `W` controls what "preferred" solutions look like. Utility code such as `WeightsPWC()` (in `underdeterminedutils.hpp`) builds a **tridiagonal matrix** that penalizes jumps between adjacent knots for piecewise-constant parameterizations:

```cpp
Sparse::TriDiagonal_* WeightsPWC(const Vector_<DateTime_>& knots, double tau_s);
```

The effect: among all parameter vectors that reprice the instruments, the solver picks the smoothest one (smallest weighted norm of parameter changes).

For the `Approximate` variant, an additional **Jacobian-based penalty** blends in:

```
W_eff = W + jWeight · Jᵀ J
```

where `jWeight = ||func_tol||² / fit_tol²`. This balances fitting accuracy against parameter stability.

### Integration with Curve Building

The solver connects to `DiscountPWLF_` through the `FittableCurve_` interface. In the current `CalibrateYieldCurve()` implementation, `yccalibration.cpp` builds a simple tridiagonal smoothing matrix directly and solves for the `2K` left/right forward parameters.

```cpp
class FittableCurve_ {
    virtual int NX() const = 0;
    virtual void ApplyDX(Vector_<>::const_iterator dx, double leverage) = 0;
};
```

`DiscountPWLF_` exposes `2K` parameters (left + right forward rate at each of `K` knots):

```cpp
int NX() const override { return 2 * fwds_.knotDates_.size(); }

void ApplyDX(Vector_<>::const_iterator dx, double leverage) override {
    auto pl = fwds_.fLeft_.begin(), pr = fwds_.fRight_.begin();
    while (pl != fwds_.fLeft_.end()) {
        *pl++ += leverage * *dx++;
        *pr++ += leverage * *dx++;
    }
    fwds_.Update();   // recompute cached integrals
}
```

**Calibration loop:**

```
1. Solver proposes dx (parameter perturbation)
2. ApplyDX(dx) updates forward rates in PiecewiseLinear_
3. Update() recomputes sofar_ (cached integrals)
4. Reprice all instruments → compute residuals f(x)
5. Solver checks convergence; if not, compute next dx
```

The `eff_j_inv` output from `Find()` gives the effective Jacobian inverse, which can be reused for risk (bump-and-reprice sensitivities) without re-solving.

## Serialization

`DiscountPWLF_` serializes via auto-generated code (`dal/auto/MG_DiscountPWLF_v1_*.inc`):

- Stored fields: name, currency, knot dates, left values, right values, base curve handle
- Round-trips through the `Archive_` framework for persistence and transport
