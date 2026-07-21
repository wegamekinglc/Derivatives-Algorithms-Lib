# PDE Framework

DAL's PDE framework lives under `dal-cpp/dal/math/pde/`. It provides coordinate maps,
grid construction, finite-difference operator builders, coefficient adapters, and a
one-dimensional theta rollback scheme. The public C++ surface is intentionally small:

- `pde.hpp` defines coordinate maps, coefficient interfaces, coefficient factories, and
  the abstract `Rollback_` interface.
- `pdegrid.hpp` materializes `CoordinateVector_` grids into physical node locations.
- `pdeoperators.hpp` builds tridiagonal first- and second-derivative operators from node
  locations.
- `thetascheme.hpp` implements `ThetaScheme_`, a one-dimensional `Rollback_`.

The framework is plain `double`; it does not use active AAD types.

## Coordinate Maps

`CoordinateMap_` maps a computational coordinate `y` to a physical coordinate `x`:

```cpp
// from dal-cpp/dal/math/pde/pde.hpp
virtual double operator()(double y, double* dxDy, double* d2xDy2) const = 0;
virtual double Y(double x) const = 0;
```

The derivative out-pointers may be null. Implementations use the DAL `ASSIGN` convention:
when a pointer is null, the value is simply not written.

### Identity And Sinh Maps

`NewIdentityMap()` returns the uniform map `x = y`, with `dx/dy = 1` and
`d2x/dy2 = 0`.

`NewSinhMap(xWidth, dxdyRange)` returns the stretch

$$
x(y) = \lambda \sinh(y / \lambda),
$$

where

$$
\lambda = \frac{xWidth}{\sqrt{dxdyRange^2 - 1}}.
$$

The factory requires `xWidth > 0` and `dxdyRange >= 1`. When `dxdyRange == 1`, the
factory degenerates to the identity map.

### Concentrating Map

`NewConcentratingMap(xLow, xHigh, cPoint, density)` returns a sinh/asinh stretch on
`y in [0, 1]`:

$$
x(y) = \mu + \rho \sinh(c_1(1-y) + c_2y),
$$

with

$$
\mu = cPoint, \qquad
\rho = density \cdot (xHigh - xLow),
$$

$$
c_1 = \mathrm{asinh}\left(\frac{xLow-\mu}{\rho}\right), \qquad
c_2 = \mathrm{asinh}\left(\frac{xHigh-\mu}{\rho}\right).
$$

The map concentrates nodes near `cPoint`; smaller `density` gives tighter local
concentration. The factory requires `xHigh > xLow`, `cPoint` in `[xLow, xHigh]`, and
`density > 0`.

The concentrating map is endpoint-exact. Calling `operator()` with `y == 0.0` returns
`xLow` bitwise, and `y == 1.0` returns `xHigh` bitwise. The inverse is endpoint-exact as
well: `Y(xLow) == 0.0` and `Y(xHigh) == 1.0`. This endpoint snap applies only to exact
endpoint arguments; derivative out-values at endpoints still use the analytic formulas.

The analytic derivatives are

$$
\frac{dx}{dy} = \rho(c_2-c_1)\cosh(c_1(1-y)+c_2y),
$$

$$
\frac{d^2x}{dy^2} = \rho(c_2-c_1)^2\sinh(c_1(1-y)+c_2y).
$$

## Grid Construction

`CoordinateVector_` describes a one-dimensional computational grid:

```cpp
// from dal-cpp/dal/math/pde/pde.hpp
struct CoordinateVector_ {
    double yLow_;
    double yHigh_;
    int n_;
    Handle_<CoordinateMap_> yToX_;
};
```

`GridLocations(points)` samples `n_` computational nodes uniformly on
`[yLow_, yHigh_]` and maps them through `yToX_`. It computes

$$
dy = \frac{yHigh-yLow}{n-1}
$$

once, assigns the first and last computational nodes verbatim to `yLow_` and `yHigh_`,
and uses `yLow_ + i*dy` for interior nodes. This keeps the sampled endpoints exact in
`y`-space; endpoint-exact maps then keep the physical grid endpoints exact as well.

`GridLocations` requires:

- at least three points,
- `yHigh_ > yLow_`,
- a non-empty coordinate-map handle,
- strictly increasing mapped physical locations.

Use `MakeUniformGrid(xLow, xHigh, n)` for a uniform physical grid. It pairs the identity
map with `yLow_ = xLow` and `yHigh_ = xHigh`.

Use `MakeConcentratingGrid(xLow, xHigh, n, cPoint, density)` for a concentrated physical
grid. It pairs the concentrating map with `yLow_ = 0.0` and `yHigh_ = 1.0`, so callers do
not have to remember the concentrating map's computational domain.

## Difference Operators

`NewDx` and `NewDxx` build `Sparse::TriDiagonal_` operators from physical node locations.
They require at least three strictly increasing locations. Boundary rows are zero; the
time-stepping scheme owns boundary policy.

For an interior node `i`, define

$$
\Delta^- = x_i - x_{i-1}, \qquad \Delta^+ = x_{i+1} - x_i.
$$

The first-derivative row is

$$
\left[
-\frac{\Delta^+}{\Delta^-(\Delta^-+\Delta^+)},
\frac{\Delta^+-\Delta^-}{\Delta^-\Delta^+},
\frac{\Delta^-}{\Delta^+(\Delta^-+\Delta^+)}
\right].
$$

The second-derivative row is

$$
\left[
\frac{2}{\Delta^-(\Delta^-+\Delta^+)},
-\frac{2}{\Delta^-\Delta^+},
\frac{2}{\Delta^+(\Delta^-+\Delta^+)}
\right].
$$

The operators derive spacings from the final location vector, so their coefficients agree
with endpoint-pinned grids.

## Coefficients

`ScalarCoeff_`, `VectorCoeff_`, and `MatrixCoeff_` expose a `Value(x, out)` virtual and
an `XDependence()` declaration. Dependence flags are `std::bitset<MAX_DIMENSIONS>` values:
bit `i` means the coefficient depends on spatial coordinate `x[i]`. Time is not a
dependence bit; time-dependent coefficients are handled by calling `Prepare` again before
the next roll.

The factory functions return `std::unique_ptr`. Callers normally wrap the result in
`Handle_` through its converting constructor or keep the `unique_ptr`.

Constant factories:

```cpp
// from dal-cpp/dal/math/pde/pde.hpp
std::unique_ptr<ScalarCoeff_> NewConstCoeff(double val);
std::unique_ptr<VectorCoeff_> NewConstCoeff(const Vector_<>& val);
std::unique_ptr<MatrixCoeff_> NewConstCoeff(const Matrix_<>& val);
```

The matrix constant must be square. Constant coefficients report all-zero dependence.

Callable factories:

```cpp
// from dal-cpp/dal/math/pde/pde.hpp
std::unique_ptr<ScalarCoeff_> NewScalarCoeff(std::function<double(const Vector_<>&)> f, Coeff_::x_dep_t dep);
std::unique_ptr<VectorCoeff_> NewVectorCoeff(std::function<void(const Vector_<>&, Vector_<>*)> f,
                                             const Vector_<Coeff_::x_dep_t>& dep);
std::unique_ptr<MatrixCoeff_> NewMatrixCoeff(std::function<void(const Vector_<>&, SquareMatrix_<>*)> f,
                                             const Matrix_<Coeff_::x_dep_t>& dep);
```

For vector and matrix callables, `dep` declares both dependence and output shape. The
adapter resizes the output object to match `dep` before invoking the callable.

The one-dimensional convenience overloads infer axis-0 dependence and length/shape 1:

```cpp
// from dal-cpp/dal/math/pde/pde.hpp
std::unique_ptr<ScalarCoeff_> NewScalarCoeff(std::function<double(double)> f);
std::unique_ptr<VectorCoeff_> NewVectorCoeff(std::function<double(double)> f);
std::unique_ptr<MatrixCoeff_> NewMatrixCoeff(std::function<double(double)> f);
```

These make Black-Scholes coefficients concise:

```cpp
// from dal-cpp/examples/european_fd/european_fd.cpp
Handle_<ScalarCoeff_> disc(NewConstCoeff(rate));
Handle_<VectorCoeff_> mu(NewVectorCoeff([=](double s) { return (rate - div) * s; }));
Handle_<MatrixCoeff_> var(NewMatrixCoeff([=](double s) { return vol * vol * s * s; }));
```

The diffusion coefficient supplies variance; schemes apply the `0.5` factor.

## Value Layout

`Rollback_` passes values as `Vector_<std::shared_ptr<Cube_<>>>`. Each cube layer holds one
time level. For one-dimensional problems the shape is `(1, 1, nX)`:

- axis I is reserved and must be size 1,
- axis J is reserved for a future second spatial dimension and must be size 1 here,
- axis K is the first spatial dimension.

The last-axis layout makes each spatial row contiguous through
`SliceBegin(0, 0)` / `SliceEnd(0, 0)`.

## Theta Scheme

`ThetaScheme_` implements one backward time step of

$$
\frac{\partial V}{\partial t}
+ \mu(x)\frac{\partial V}{\partial x}
+ \frac{1}{2}\sigma^2(x)\frac{\partial^2 V}{\partial x^2}
- r(x)V = 0.
$$

`theta = 0` is explicit, `theta = 0.5` is Crank-Nicolson, and `theta = 1` is fully
implicit. The constructor requires `theta` in `[0, 1]`. The implementation supports
exactly one spatial dimension; a request with any other `xPoints` size throws.

### Prepare And Roll

`Prepare(dt, xPoints, discounting, advection, diffusion)` is the assembly phase. It
requires `dt > 0`, materializes the grid, samples coefficients, builds the explicit and
implicit tridiagonal operators with `dt` baked in, and factors the implicit operator when
`theta > 0`.

The const `operator()` is the roll phase. It does not assemble or factor. It checks that
the `dt`, grid, and coefficient objects match the prepared state, revalidates coefficient
probe values at the first, middle, and last nodes, applies the prepared explicit
operator, and solves the prepared implicit operator when needed.

`Decompositions()` returns the number of implicit factorizations since construction.
Each `Prepare` with `theta > 0` increments the count once. `Prepare` with `theta == 0`
does not factor and leaves the count unchanged. `operator()` never changes the count.

### Boundary Policy

Both prepared operators use identity rows at the two spatial boundaries. The discount term
does not alter boundary diagonals.

For full theta-scheme treatment of Dirichlet boundaries, `operator()` reads the target
layer's two boundary values from `newVals` before writing output. Those target-time
boundary values become the right-hand side for the implicit boundary rows, while the
source layer's boundary values remain part of the explicit half-step for interior rows.
This gives Crank-Nicolson (`theta = 0.5`) the usual old-boundary/new-boundary averaging.

If a target layer is null or has the wrong shape, the scheme falls back to the source
layer's boundary values. This preserves the pass-through behavior for constant-boundary
or in-place rolls. For time-dependent boundaries, use a separate target layer, seed its
end nodes for the target time level, call the scheme, and then swap the source and target
vectors.

### Aliasing And Output Layers

`oldVals` must be non-empty and all layers must be non-null with shape `(1, 1, n)`.
`newVals` must be non-null.

Whole-vector aliasing is supported:

```cpp
// from dal-cpp/dal/math/pde/thetascheme.hpp
scheme(dt, grids, vals, *disc, *mu, *var, &vals);
```

Whole-vector aliasing cannot supply boundary values that differ between source and target
time levels. Use the separate-target pattern below when boundary values are
time-dependent.

Same-index layer aliasing is also supported. The scheme copies the source slice into local
scratch before writing target values. A null or mis-shaped target layer is replaced with a
fresh `Cube_<>(1, 1, n)`; the implementation does not resize cubes in place.

## Example Roll Loop

The European finite-difference example in
[`dal-cpp/examples/european_fd/`](../../dal-cpp/examples/european_fd/) runs explicit,
Crank-Nicolson, and fully implicit scheme configurations through the same rollback helper.
The explicit configuration uses a finer time grid because explicit rollback is
conditionally stable. After the base scheme comparison, the example continues the
Crank-Nicolson convergence sweep by increasing `spaceSteps` and `timeSteps` together. For
each selected run, the helper uses:

```cpp
// from dal-cpp/examples/european_fd/european_fd.cpp
const SchemeRun_ schemeRuns[] = {
    {"Explicit", 0.0, kBaseSteps, kExplicitTimeSteps},
    {"Crank-Nicolson", 0.5, kBaseSteps, kBaseSteps},
    {"Implicit", 1.0, kBaseSteps, kBaseSteps},
};

const int numX = run.spaceSteps + 1;
const int numT = run.timeSteps;
const CoordinateVector_ x = MakeUniformGrid(0.0, 500.0, numX);
const Vector_<CoordinateVector_> grids(1, x);
const Vector_<> loc = GridLocations(x);

Handle_<ScalarCoeff_> disc(NewConstCoeff(rate));
Handle_<VectorCoeff_> mu(NewVectorCoeff([=](double s) { return (rate - div) * s; }));
Handle_<MatrixCoeff_> var(NewMatrixCoeff([=](double s) { return vol * vol * s * s; }));

Vector_<std::shared_ptr<Cube_<>>> vals(1, std::make_shared<Cube_<>>(1, 1, numX));
for (int k = 0; k < numX; ++k)
    (*vals[0])(0, 0, k) = std::max(loc[k] - strike, 0.0);
Vector_<std::shared_ptr<Cube_<>>> next(1, std::make_shared<Cube_<>>(1, 1, numX));

ThetaScheme_ scheme(run.theta);
const double dt = t / numT;
scheme.Prepare(dt, grids, *disc, *mu, *var);
for (int n = 0; n < numT; ++n) {
    (*next[0])(0, 0, 0) = 0.0;
    (*next[0])(0, 0, numX - 1) =
        500.0 * std::exp(-div * (n + 1) * dt) - std::exp(-rate * (n + 1) * dt) * strike;
    scheme(dt, grids, vals, *disc, *mu, *var, &next);
    vals.Swap(&next);
}
```

## See Also

- [Matrix and linear algebra](matrix.md) — tridiagonal storage and decomposition.
- [Black / Bachelier Vanilla Pricing](black_scholes.md) — analytic benchmarks used by
  the PDE tests and examples.
