# Interpolation

This note describes the one- and two-dimensional interpolators exposed by the
`Dal::Interp` namespace (`dal-cpp/dal/math/interp/`). The focus is the mathematical
definition of each scheme, the boundary conditions it accepts, and where in the library
each is used. All one-dimensional interpolators inherit from `Interp1_` and are constructed
through factory functions rather than directly.

## Common Interface

Every one-dimensional interpolator is a `Storable_` subclass of `Interp1_`
(`dal-cpp/dal/math/interp/interp.hpp`) with

$$
\texttt{double operator()(double x) const}, \qquad \texttt{bool IsInBounds(double x) const}.
$$

The abscissae $x_1 \le x_2 \le \dots \le x_N$ must be non-decreasing (`IsMonotonic` is
checked at construction). Evaluation is by `LowerBound` on the knot vector followed by a
scheme-specific local formula; values exactly at a knot return the knot's $f$ value
without rounding error. The shared linear kernel used internally is

$$
f(x) = f_i + \frac{x - x_i}{x_{i+1} - x_i}\,(f_{i+1} - f_i), \qquad x \in [x_i, x_{i+1}],
$$

implemented by `InterpLinearImplX` in `dal-cpp/dal/math/interp/interp.hpp`.

| Scheme                         | Factory                          | Header                                                  |
|--------------------------------|----------------------------------|---------------------------------------------------------|
| Linear                         | `Interp::NewLinear`              | `dal-cpp/dal/math/interp/interplinear.hpp`              |
| Log-linear                     | `Interp::NewLogLinear`           | `dal-cpp/dal/math/interp/interploglinear.hpp`           |
| Cubic spline                   | `Interp::NewCubic`               | `dal-cpp/dal/math/interp/interpcubic.hpp`               |
| Mixed log-DF (linear + cubic)  | `NewMixedLogDF`                  | `dal-cpp/dal/math/interp/interpmixed.hpp`               |
| Bilinear (2D)                  | `Interp::NewLinear2`             | `dal-cpp/dal/math/interp/interp2d.hpp`                  |

## Passive Geometry and Typed Ordinates

Curve calibration also needs the same interpolation definition for passive `double`
values and tape-active `AAD::Number_` values. The interpolation geometry is independent
of the ordinate scalar type, so `dal-cpp/dal/math/interp/interpweights.hpp` separates the
two concerns:

- `LinearWeightGeometry_` and `NaturalCubicWeightGeometry_` store passive abscissae and
  return `(storage index, weight)` pairs for a query;
- `ApplyInterpWeights<T_>` applies those weights to `Vector_<T_>` ordinates.

Thus a value is always evaluated as

$$
f(x) = \sum_j w_j(x) f_j,
$$

where segment selection, knot positions, and weights remain passive while the ordinates
may be `double` or AAD-active. Exact-knot queries return a unit weight. Linear geometry
clamps outside its range. Natural-cubic geometry uses the first or last cubic segment for
extrapolation and generally returns a dense weight vector because a natural spline's
second derivatives depend globally on all ordinates.

The archive-backed `Interp1_` factories remain the public general-purpose interpolation
surface. The weight geometry is the scalar-generic evaluation primitive used by the
yield-curve layer; it prevents passive and AAD paths from maintaining separate formulas.

## Linear

Piecewise-linear interpolation between knots — the degree-one, piecewise-affine kernel
above. It is exact at knots, continuous, and has $O(h^2)$ interpolation error for a
sufficiently smooth scalar function. `IsInBounds` returns true on
$[x_1, x_N]$; outside that range the kernel clamps to the nearest endpoint value (flat
extrapolation via the `LowerBound` edge cases).

Factory: `Interp::NewLinear(name, x, f)` (`dal-cpp/dal/math/interp/interplinear.hpp`).

## Log-Linear

Interpolates linearly in $\ln f$ rather than in $f$:

$$
f(x) = \exp\!\left( \ln f_i + \frac{x - x_i}{x_{i+1} - x_i}\,(\ln f_{i+1} - \ln f_i) \right).
$$

This is the natural choice for strictly positive quantities that scale geometrically with
the abscissa — discount factors, forward rates, volatilities — because it preserves
positivity exactly: a convex combination of logs can never produce a negative value.

Factory: `Interp::NewLogLinear(name, x, f)` (`dal-cpp/dal/math/interp/interploglinear.hpp`).
Requires $N \ge 2$ and **every $f_i > 0$** (checked at construction). This scheme is the
default for the `LOG_DISCOUNT` curve parameterization — see
[Log-discount curve](log_discount_curve.md).

## Cubic Spline

A cubic spline through the knots: a piecewise-cubic polynomial on each
$[x_i, x_{i+1}]$ that is $C^2$ continuous, with second derivatives $f''_i$ at the knots
solved once at construction by the standard tri-diagonal elimination (the routine is based
on the *Numerical Recipes* `spline`/`splint` pair). Evaluation between knots uses the local
Hermite form

$$
f(x)=a f_i+b f_{i+1}-\frac{a b h^2}{6}\left[(1+a)f_i''+(1+b)f_{i+1}''\right]
$$

with $h = x_{i+1}-x_i$, $b = (x-x_i)/h$, $a = 1-b$.

The two end conditions are supplied as a `Boundary_(order, value)` pair, where `order`
selects which derivative is pinned at the boundary:

- **Order 1:** `lhs` pins $f'(x_1)$ and `rhs` pins $f'(x_N)$ to its respective
  `value_`.
- **Order 2:** `lhs` pins $f''(x_1)$ and `rhs` pins $f''(x_N)$ to its respective
  `value_`.
- **Order 3:** each endpoint segment's third derivative is pinned to the
  respective `lhs`/`rhs` `value_`.

`Boundary_(2, 0.0)` on both ends gives the classic natural spline (zero end curvature).

Factory: `Interp::NewCubic(name, x, f, lhs, rhs)` (`dal-cpp/dal/math/interp/interpcubic.hpp`).
Requires $N > 2$ and strictly increasing $x$. `IsInBounds` forbids extrapolation.
The curve-specific natural-cubic weight geometry uses the same polynomial definition but
allows first- and last-segment polynomial extrapolation as part of the log-DF curve's
explicit boundary policy.

## Mixed Log-DF

A composite scheme used by the log-discount curve. The abscissa is the year fraction and
the ordinate is $\ln P$. For compatibility with the existing `MIXED` curve contract, the
scheme is **linear in $\ln P$ through the cutoff** `cutoffYf_` and a cubic spline beyond
the cutoff. The cubic tail uses the configurable `Boundary_` conditions carried by
`MixedSchemeSpec_`; the curve builder supplies natural conditions by default.

The cutoff must coincide with one of the knot abscissae so that both sub-interpolators
reproduce its value exactly. The linear head and cubic tail are built on overlapping
sub-arrays that both include the cutoff knot; because each sub-interpolator is exact at its
own knots, the two pieces agree at the cutoff and the composite curve is $C^0$ continuous
across the join. First- and second-derivative continuity are not imposed. Construction
therefore searches the knot vector for the abscissa equal to `cutoffYf_` rather than
silently snapping an in-between value to a knot.

Factory: `NewMixedLogDF(name, yf, logDF, spec)` where `spec` is a `MixedSchemeSpec_`
carrying `cutoffYf_` and the two cubic `Boundary_` conditions
(`dal-cpp/dal/math/interp/interpmixed.hpp`). This scheme backs the `MIXED` value of
`LogDfScheme_` — see [Log-discount curve](log_discount_curve.md).

`Tape::DiscountLogDF_` uses the scalar-generic `LogDfInterpolation_` rather than this
archive-backed composite. Its compatibility cutoff is storage index
$\max(1,N-5)$; the cubic tail's local weight indices are translated back to the full
storage-node indices before evaluation. Both implementations therefore retain the
linear-head/natural-cubic-tail contract, while the curve implementation can apply the
identical weights to passive and AAD-active ordinates.

## Bilinear (2D)

Tensor-product linear interpolation on a rectilinear grid: linear in $x$ along each row,
then linear in $y$ across the two bracketing rows,

$$
f(x, y) = (1-t)\,f(x, y_{\text{lo}}) + t\,f(x, y_{\text{hi}}), \qquad
t = \frac{y - y_{\text{lo}}}{y_{\text{hi}} - y_{\text{lo}}},
$$

with the per-row $f(x, y_{\text{lo}})$ and $f(x, y_{\text{hi}})$ obtained by the same
linear kernel as the 1D case. It is implemented by `Interp2Linear_` and constructed through
`Interp::NewLinear2(name, x, y, f)` where `f` is a row-major `Matrix_<>` of shape
$(N_x, N_y)$ (`dal-cpp/dal/math/interp/interp2d.hpp`).

## Selection Guidance

- Use **linear** for noisy or sparse data where higher-order smoothness would amplify
  noise, and for piecewise-linear forwards in calibration (see
  [Yield curve construction](yield_curve.md)).
- Use **log-linear** for any strictly positive, geometrically-scaling quantity — discount
  factors and forward rates are the canonical cases.
- Use **cubic** when $C^2$ smoothness matters (e.g. second-derivative-dependent risk) and
  the data is dense enough to support it; pick end conditions deliberately.
- Use **mixed log-DF** when compatibility with DAL's linear-head/natural-cubic-tail
  log-discount convention is required. Use a pure scheme when one interpolation shape is
  desired across the entire curve.
- Use **bilinear** for surfaces quoted on a regular grid (e.g. option volatility by
  expiry and strike).

## See Also

- [Log-discount curve](log_discount_curve.md) — uses log-linear, cubic, and mixed
  interpolation on $\ln P$ as its `LogDfScheme_` parameterization.
- [Yield curve construction](yield_curve.md) — the piecewise-linear and
  piecewise-constant forward parameterizations are built on the linear kernel.
