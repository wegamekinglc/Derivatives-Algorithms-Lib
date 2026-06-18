# Log-Discount Curve

This note describes the `LOG_DISCOUNT` curve parameterization: how a discount curve is
represented as a set of node log-discount-factors, how it is interpolated, and why it is
the parameterization on which the analytic Jacobian for calibration is built. It ties
together [Yield curve construction](yield_curve.md) and
[Interpolation](interpolation.md).

## Representation

A `LOG_DISCOUNT` curve stores the discount curve at a fixed set of **node dates**
$t_0 < t_1 < \dots < t_{N-1}$ as the **log discount factors** relative to the anchor
$t_0$,

$$
\ell_i \equiv \ln P(t_0, t_i), \qquad \ell_0 = \ln P(t_0,t_0) = 0.
$$

The anchor node is pinned at $\ell_0 = 0$ (today's discount factor is 1), so the curve
has $N-1$ free parameters — the quantities the calibration solver drives. Between nodes
the log discount factor is obtained by interpolation in the year-fraction abscissa
$\tau = \mathrm{YearFrac}(t_0, t;\,\text{dayCount})$, and the discount factor itself is

$$
P(t_0, t) = \exp\!\big( \ell(\tau) \big).
$$

Because the parameter is $\ell = \ln P$ rather than $P$ or the forward rate, every
interpolated discount factor is strictly positive by construction — the exponential can
never cross zero — which makes the curve numerically robust to bump during calibration.
The implementation is `Tape::DiscountLogDF_<T_>` (alias `DiscountLogDF_` for `T_ = double`),
declared in `dal-cpp/dal/curve/yclogdf.hpp` and constructed via the
`NewDiscountLogDF(...)` factory.

## Interpolation Schemes: `LogDfScheme_`

The shape of $\ell(\tau)$ between nodes is selected by the `LogDfScheme_` enumeration
(`dal-cpp/dal/curve/logdfscheme.hpp`), carried on the calibration spec as
`CurveCalibrationSpec_::logDfScheme_` (default `LOG_LINEAR`). Each scheme is a concrete
interpolator from [Interpolation](interpolation.md) applied to the $(\tau_i, \ell_i)$
knots:

| `LogDfScheme_`        | Interpolator on $\ell(\tau)$                         | Properties                                                       |
|-----------------------|------------------------------------------------------|------------------------------------------------------------------|
| `LOG_LINEAR`          | linear in $\ell$ (i.e. log-linear in $P$)            | $C^0$, positivity-preserving, cheapest; default                  |
| `LOG_CUBIC_NATURAL`   | natural cubic spline in $\ell$ (`Boundary_(2, 0.0)`) | $C^2$ smooth, curved between knots                               |
| `MIXED`               | cubic to a cutoff year fraction, linear beyond       | spline smoothness at the short end, log-linear tail at long end  |

The mapping lives in `BuildLogDfInterpFromYf` in `dal-cpp/dal/curve/yclogdf.cpp`.

- **`LOG_LINEAR`** is the cheapest and most robust scheme; linear interpolation of $\ell$
  is exactly log-linear interpolation of $P$ and is the default.
- **`LOG_CUBIC_NATURAL`** fits a natural cubic spline (zero end curvature,
  `Boundary_(2, 0.0)` on both sides) through the knot $\ell$ values, giving a $C^2$ curve.
  Smoothness is valuable when second-derivative-dependent risk is computed off the curve.
- **`MIXED`** uses a cubic spline out to a cutoff year fraction and log-linear beyond it,
  combining spline smoothness where the term structure is data-rich with the robustness
  of log-linear extrapolation at the long end (where knots are sparse and a spline would
  oscillate). The cutoff is placed at the $(N-4)$-th knot so that the cubic tail retains
  at least three knots; the two cubic `Boundary_` conditions default to natural
  (`MixedSchemeSpec_`, `dal-cpp/dal/math/interp/interpmixed.hpp`).

## Why Log-Discount is the Analytic-Jacobian Parameterization

The analytic Jacobian shipped for curve calibration (see
[AAD analytic Jacobian](../experimental/aad-analytic-jacobian-curve-calibration.md))
is scoped to `LOG_DISCOUNT`. The reason is mechanical. The calibration residuals are
smooth functions of the node $\ell_i$ through the pricing machinery, so when the node
$\ell_i$ are placed on the AAD tape as independents the per-residual adjoints produced by
one reverse sweep per row are exactly the Jacobian entries
$\partial r_j / \partial \ell_i$ — no bump noise, exact structural zeros at nodes an
instrument does not touch. Because the anchor $\ell_0$ is pinned at zero, the solver's $x$
has exactly $N-1$ entries and matches the free-node count.

The `Tape::DiscountLogDF_<Number_>` specialization routes its log-DF evaluation through
basis weights (one per interpolation scheme) rather than the `Interp1_` handle, so the
tape records the dependence of every queried $\ell(\tau)$ on the node $\ell_i$ values
directly. The same machinery supports all three schemes; eligibility for the analytic path
is independent of which `LogDfScheme_` is chosen.

## Relationship to the Forward-Rate Parameterizations

The [yield curve methodology](yield_curve.md) describes the curve through the
instantaneous forward rate $f(t)$, parameterized as piecewise-constant or piecewise-linear
across knots. `LOG_DISCOUNT` is an alternative parameterization of the *same* underlying
discount curve: the node $\ell_i$ are the cumulative integrals of the forward rate up to
each node,

$$
\ell_i = -\tfrac{1}{365}\int_{t_0}^{t_i} f(t)\,dt,
$$

and interpolation in $\ell$ induces a particular between-node shape of $f$. The three views
(forward rate, node $\ell$, node $P$) are equivalent descriptions of one curve object;
`LOG_DISCOUNT` is preferred when analytic risk is wanted because its parameter is the
natural input to the AAD tape.

## See Also

- [Interpolation](interpolation.md) — the linear, log-linear, cubic, and mixed
  interpolators that `LogDfScheme_` selects between.
- [Yield curve construction](yield_curve.md) — the forward-rate parameterizations and the
  calibration pipeline this curve plugs into.
- [AAD analytic Jacobian](../experimental/aad-analytic-jacobian-curve-calibration.md) —
  the calibration enhancement scoped to this parameterization.
