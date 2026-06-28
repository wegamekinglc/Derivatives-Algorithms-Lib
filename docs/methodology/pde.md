# PDE Finite-Difference Meshers and Coordinate Maps

This note documents the one-dimensional finite-difference mesh generators under
`dal-cpp/dal/math/pde/meshers/` and the coordinate-map abstraction in
`dal-cpp/dal/math/pde/pde.hpp` that the operator machinery uses to remap the spatial
coordinate. The meshers only lay out grid points and their forward/backward spacings; the
actual difference operators are built downstream by `Dal::PDE::Dx()` and `Dal::PDE::Dxx()`
from a mesher, and the one-dimensional finite-difference engine `Dal::PDE::FD1D_` holds a
mesher by const-reference and exposes its node locations through `FD1D_::X()`.

## The Mesher Interface

Every concrete mesher derives from `Dal::FDM1DMesher_`
(`dal-cpp/dal/math/pde/meshers/fdm1dmesher.hpp`), which fixes the contract a downstream
differencer relies on. The base class is constructed with a node count `size` and populates
three parallel vectors of that length:

- `locations_` — the physical grid abscissae $x_0, x_1, \dots, x_{n-1}$,
- `dplus_` — the forward spacing $\Delta^+_i = x_{i+1} - x_i$,
- `dminus_` — the backward spacing $\Delta^-_i = x_i - x_{i-1}$.

These are exposed through the getters `Size()`, `DPlus(int i)`, `DMinus(int i)`,
`Location(int i)`, and `Locations()`. The interior convention is consistent:
$\Delta^+_i = \Delta^-_{i+1} = x_{i+1} - x_i$, so a forward step out of node $i$ equals the
backward step into node $i+1$.

### Boundary-null convention

At the two boundaries there is no "next" node past the last and no "previous" node before the
first. The protected helper `FinalizeSpacings()` sets

$$
\Delta^+_{n-1} = \text{Null\_}<\text{double}>(), \qquad \Delta^-_0 = \text{Null\_}<\text{double}>().
$$

The `Null_<double>()` sentinel is intentional: downstream code that needs a one-sided
difference at the boundary must use the inward spacing only ($\Delta^-_{n-1}$ on the right,
$\Delta^+_0$ on the left). A differencer that accidentally reads the out-of-bounds slot gets
the null sentinel rather than a silently-stale neighbour value, making the bug detectable.
Every concrete mesher calls `FinalizeSpacings()` as its last step.

## Uniform Mesher

`Dal::Uniform1DMesher_` (`dal-cpp/dal/math/pde/meshers/uniform1dmesher.hpp`) lays out a
constant-spacing grid on $[\text{start}, \text{end}]$ with $n$ nodes. The constructor requires
$\text{end} > \text{start}$ and uses the spacing

$$
\Delta x = \frac{\text{end} - \text{start}}{n - 1}, \qquad x_i = \text{start} + i\,\Delta x.
$$

Each interior forward/backward pair is set to $\Delta x$.

The last node is **pinned** to `end` directly (`locations_.back() = end`) rather than computed
as $\text{start} + (n-1)\Delta x$. This is deliberate: the closed-form $x_{n-1}$ accumulates
floating-point round-off through the repeated-addition form, so a grid that computed the
endpoint would not close exactly on $[\text{start}, \text{end}]$. Assigning the endpoint
verbatim guarantees the domain closes to machine precision, and the trailing
`FinalizeSpacings()` nulls $\Delta^+_{n-1}$.

## Concentrating Mesher

`Dal::Concentrating1dMesher_`
(`dal-cpp/dal/math/pde/meshers/concentrating1dmesher.{hpp,cpp}`) lays out a grid on
$[\text{start}, \text{end}]$ that concentrates nodes around a chosen interior point. The
constructor takes

- `cPoints.first` — the concentration point $\mu$ (`cPoint`), required to lie in
  $[\text{start}, \text{end}]$,
- `cPoints.second` — a dimensionless density budget; the implementation sets
  $\rho = \texttt{cPoints.second} \cdot (\text{end} - \text{start})$ and requires $\rho > 0$,
- `requireCPoint` — when true, guarantees a node lands exactly on $\mu$.

### The sinh/asinh coordinate stretch

The grid is the image of a uniform sample on $y \in [0,1]$ under the transform

$$
x(y) = \mu + \rho \sinh\!\big(\alpha(1-y) + \beta y\big),
$$

where the endpoint constraints $x(0) = \text{start}$ and $x(1) = \text{end}$ fix

$$
\alpha = \operatorname{asinh}\!\left(\frac{\text{start} - \mu}{\rho}\right), \qquad
\beta  = \operatorname{asinh}\!\left(\frac{\text{end} - \mu}{\rho}\right).
$$

Concentration arises because $\sinh$ has near-unit slope at the origin but grows like
$\tfrac{1}{2}e^{|t|}$ for large $|t|$. The local node density is governed by

$$
\frac{\mathrm{d}x}{\mathrm{d}y} = \rho(\beta - \alpha)\cosh\!\big(\alpha(1-y) + \beta y\big).
$$

Where $\mathrm{d}x/\mathrm{d}y$ is small the grid is fine; where it is large the grid is coarse.
Choosing $\rho$ small relative to $|\text{end} - \text{start}|$ drives $|\alpha|$ and $|\beta|$
to large magnitudes, sharpening the $\cosh$ peak around the $y$ where the argument crosses
zero — that is, around $x = \mu$. This is the concentration mechanism.

The scaling $\rho = \texttt{cPoints.second} \cdot (\text{end} - \text{start})$ makes the
user-supplied second element a dimensionless density budget: a larger value produces a coarser
grid near $\mu$ and a more uniform overall spacing; a smaller value tightens the concentration
near $\mu$.

### The snapped-knot device

With `requireCPoint = false` the implementation samples the transform directly at the uniform
pre-image $y_i = i/(n-1)$. The resulting grid concentrates around $\mu$ but, except by
coincidence, no node sits exactly on $\mu$.

With `requireCPoint = true` the mesher forces a node onto $\mu$. The continuous pre-image that
maps to $\mu$ solves $\alpha(1-z_0) + \beta z_0 = 0$, giving

$$
z_0 = \frac{-\alpha}{\beta - \alpha}.
$$

The implementation snaps $z_0$ to the nearest grid fraction $u_0 = \operatorname{round}(z_0
(n-1))/(n-1)$, clamped to the interior range $[1, n-2]$, then builds a piecewise-linear
`Interp1_` on the knots $(0,0)$, $(u_0, z_0)$, $(1,1)$. Each grid index $i$ is mapped through
this interpolant to its effective pre-image $y_i$ before being fed to the $\sinh$ transform.
The snap-and-clamp ensures $u_0$ is a valid interior node and that the $z_0 \mapsto \mu$ image
is realized exactly by the linear interpolation. (When $\mu$ coincides with `start` or `end`
the snapped knot is not inserted, since the endpoint pin already places a node there.)

After computing the interior `locations_`, the mesher pins
`locations_.front() = start` and `locations_.back() = end` (closing the domain exactly, as in
the uniform case), derives $\Delta^+_i = \Delta^-_{i+1} = x_{i+1} - x_i$ from the final
locations, and calls `FinalizeSpacings()`.

## Coordinate Maps for Operators

`Dal::PDE::CoordinateMap_` (`dal-cpp/dal/math/pde/pde.hpp`) is the operator-side counterpart
of the mesher. Where a mesher lays out physical grid points, a coordinate map reparametrizes
the spatial coordinate so that a uniform mesh in the computational coordinate $y$ corresponds
to a stretched mesh in the physical coordinate $x$. The abstract interface is

```cpp
virtual double operator()(double y, double* dxDy, double* d2xDy2) const = 0;
virtual double Y(double x) const = 0;
```

`operator()` maps $y \mapsto x$ and writes the first and second derivatives $\mathrm{d}x/\mathrm{d}y$,
$\mathrm{d}^2x/\mathrm{d}y^2$ through the out-parameters; `Y()` is the inverse $x \mapsto y$.

### `NewSinhMap(xWidth, dxdyRange)`

The free factory `Dal::PDE::NewSinhMap(double xWidth, double dxdyRange)` returns a
`SinhMap_` implementing

$$
x = \lambda \sinh(y/\lambda), \qquad
\frac{\mathrm{d}x}{\mathrm{d}y} = \cosh(y/\lambda), \qquad
\frac{\mathrm{d}^2x}{\mathrm{d}y^2} = \frac{1}{\lambda}\sinh(y/\lambda), \qquad
y(x) = \lambda\,\operatorname{asinh}(x/\lambda),
$$

with

$$
\lambda = \frac{\text{xWidth}}{y_{\max}}, \qquad y_{\max} = \sqrt{\text{dxdyRange}^2 - 1}.
$$

Because $\mathrm{d}x/\mathrm{d}y = \cosh(y/\lambda) \ge 1$, the map is monotone
non-decreasing and stretches $y$-space outward: near the origin $y/x \to 1/\lambda$ (dense
sampling in $x$), while for large $|x|$ the tails compress as
$|y| \sim \lambda \ln(2|x|/\lambda)$. The `dxdyRange` parameter caps the stretching at the
boundary — the maximum value of $\mathrm{d}x/\mathrm{d}y$ is exactly `dxdyRange`, attained
where $y/\lambda = y_{\max}$. `xWidth` sets $\lambda$ and hence the physical half-width of the
central high-resolution band. The factory requires `xWidth > 0` and `dxdyRange >= 1`.

### Degeneration to identity

When $\text{dxdyRange} = 1$ we have $y_{\max} = 0$, so $\sinh(y/\lambda) = y/\lambda$ collapses
to the identity $x = y$ and the parametrization above degenerates. The factory detects this and
returns an `IdentityMap_` instead. `NewIdentityMap()` is the explicit convenience for this
case — it simply calls `NewSinhMap(1.0, 1.0)`.

## Downstream Consumers

The difference operators and the one-dimensional engine that consume a mesher live in
`dal-cpp/dal/math/pde/`:

- `Dal::PDE::Dx(const FDM1DMesher_&)` and `Dal::PDE::Dxx(const FDM1DMesher_&)`
  (`dal-cpp/dal/math/pde/finitedifference.hpp`) build tridiagonal first- and
  second-difference operators from the mesher's `dplus_`, `dminus_`, and `locations_` arrays.
  The non-uniform spacing is exactly why those arrays exist: a one-sided first difference at
  node $i$ uses $\Delta^\pm_i$, and the second-difference stencil weights its three nodes by
  the local $\Delta^+ / \Delta^-$ ratio.
- `Dal::PDE::FD1D_` (`dal-cpp/dal/math/pde/fd1d.hpp`) holds a `const FDM1DMesher_&` and
  exposes the node locations through `FD1D_::X()`, which returns `Locations()`. The engine's
  drift, variance, and result vectors are all indexed against this mesh.

### Cached implicit-operator decomposition

`FD1D_::RollBwd` solves a $\theta$-scheme step that combines the explicit
($1-\theta$) and implicit ($\theta$) applications of the drift-plus-diffusion
operator assembled by `CalcAx` from the mesher-derived `Dx` / `Dxx` stencils
and the per-node `mu_`, `var_`, `r_` coefficient vectors. The implicit solve
factorises the operator $A$ and calls `SolveLeft` against it. For a
time-homogeneous problem the step `dt`, the weight `theta`, and the
coefficients (`mu_`, `var_`, `r_`) do not change between rolls, so the same
factorisation is valid roll-to-roll. `FD1D_` caches the
`SquareMatrixDecomposition_` of $A$ and reuses it across rolls as long as
`CacheHit(dt, theta)` confirms that `dt`, `theta`, and the three coefficient
vectors all still match the cached values; any of them changing rebuilds the
cache (and the explicit-operator product is recomputed unconditionally, since
it is not factorised). The `DecompositionsSinceInit()` counter exposes how many
fresh factorisations the engine has performed since `Init()`, so a caller can
confirm that a time-homogeneous sweep is reusing a single decomposition rather
than re-factoring every step.

The boundary-null convention is what makes these consumers safe: a stencil that would read
$\Delta^+_{n-1}$ or $\Delta^-_0$ at a boundary instead gets the null sentinel, forcing the
boundary to be handled by its one-sided inward difference.

## See Also

- [Interpolation](interpolation.md) — the snapped-knot device in the concentrating
  mesher builds a piecewise-linear `Interp1_` over a small knot set.
- [Matrix and linear algebra](matrix.md) — the finite-difference operators are
  tri-diagonal and solved by the Thomas algorithm documented there.
