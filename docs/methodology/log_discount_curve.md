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

## Internal Algorithms

This section describes the numerical methods inside `Tape::DiscountLogDF_<T_>`, implemented
in `dal-cpp/dal/curve/yclogdf.cpp`. The `T_ = double` path uses the `Interp1_` handle for
evaluation; the `T_ = Dal::AAD::Number_` path accumulates storage-node basis weights
directly against the tape-registered `logDF_` vector, so the tape records the dependence of
every queried $\ell(\tau)$ on the node $\ell_i$.

### Anchor and Free-Node Convention

Storage node $0$ (the anchor, $\ell_0 = 0$) is pinned and excluded from the unknown vector.
The solver column mapping is `solver col j = storage node j+1`, so the parameter dimension
$N-1$ matches the instrument count in a square calibration. In the `Number_` path, the
anchor is deliberately omitted from the weighted sum in `LogDfAt` — its contribution is
zero by construction, and including it would register a spurious tape dependence on
$\ell_0$ that the calibration overrides anyway. Excluding it keeps the column map clean
without exception.

### Basis Weights by Interpolation Scheme

`StorageBasisWeightsAt(yf)` returns a sparse vector of `(storageNode, weight)` pairs so
that $\ell(\tau) = \sum_{(j,w)} w \cdot \ell_j$. The weights are functions of the knot
positions only, so they are identical for any `T_`.

**LOG_LINEAR (log-linear / linear-in-$\ell$).** On segment $[\tau_k, \tau_{k+1}]$ with
step $h = \tau_{k+1} - \tau_k$, the interpolation is

$$
\ell(\tau) = \ell_k + \frac{\tau - \tau_k}{h} (\ell_{k+1} - \ell_k)
           = (1 - g)\,\ell_k + g\,\ell_{k+1},
\qquad g = \frac{\tau - \tau_k}{h}.
$$

The basis weights are $(k,\; 1-g)$ and $(k+1,\; g)$. The same weights serve the `MIXED`
head (the linear portion before the cutoff).

**LOG_CUBIC_NATURAL (natural cubic spline).** On segment $[\tau_k, \tau_{k+1}]$ with
$h = \tau_{k+1} - \tau_k$, the cubic Hermite representation in terms of the unknown
second derivatives $\ell''_k$ (known as `fpp` in the code) gives the basis weight at
storage node $j$ as

$$
b_j(\tau) = a\,\delta_{j,k} + b\,\delta_{j,k+1}
          - \frac{a\,b\,h^2}{6}\Big((1+a)\,\mathrm{fppCoef}_{k,j} + (1+b)\,\mathrm{fppCoef}_{k+1,j}\Big),
$$

where $b = (\tau - \tau_k)/h$, $a = 1 - b$, and $\mathrm{fppCoef}_{k,j} = \partial\,
\ell''_k / \partial\,\ell_j$ is the second-derivative sensitivity matrix, computed once
at construction. `BuildNaturalCubicFppCoef` solves the interior tridiagonal system for
each unit source $\ell_j$ column to populate this matrix.

**MIXED.** The cutoff is at the $(N-4)$-th knot (ensuring $\ge 3$ knots in the cubic
tail). `LOG_LINEAR` basis applies for $\tau \le \tau_{\text{cutoff}}$; the cubic portion
(`LOG_CUBIC_NATURAL` basis) applies beyond. The `fppCoef_` matrix is computed on the
cubic tail subarray and expanded back to full storage-node indices, with
`mixedCutoffIndex_` and `mixedCutoffYf_` stored for dispatch at evaluation.

### Extrapolation

Beyond $\tau_{N-1}$, every scheme uses the secant slope of the last segment:

$$
\ell(\tau) = \ell_{N-1} + \frac{\ell_{N-1} - \ell_{N-2}}{\tau_{N-1} - \tau_{N-2}}
             (\tau - \tau_{N-1}).
$$

The weighting is linear in the last two storage nodes — the cubic derivative at the
boundary is deliberately not used, even for `LOG_CUBIC_NATURAL`. This keeps the tail
simple, monotone, and consistent with the linear head.

### Thomas Algorithm for the Natural-Cubic System

The natural-cubic spline interior system for $\ell''_1,\dots,\ell''_{N-2}$ is
tridiagonal and diagonally dominant:

$$
h_{i-1}\,\ell''_{i-1} + 2(h_{i-1}+h_i)\,\ell''_i + h_i\,\ell''_{i+1}
= 6\!\left( \frac{\ell_{i+1}-\ell_i}{h_i} - \frac{\ell_i-\ell_{i-1}}{h_{i-1}} \right),
\qquad i = 1,\dots,N-2,
$$

with $h_i = \tau_{i+1} - \tau_i$. The solver `SolveTriDiagonal(a, d, c, b)` in
`dal-cpp/dal/curve/yclogdf.cpp` uses the standard Thomas algorithm: forward elimination
with the modified diagonal $d'_i = d_i - a_{i-1}c_{i-1}/d'_{i-1}$, then back-substitution.
`NaturalCubicRhsEntry(j, i, h)` returns the four-term Kronecker-delta dispatch
$\partial b_i / \partial \ell_j$ that feeds the column-solve loop in
`BuildNaturalCubicFppCoef`.

### Rebuild and ApplyDX

`RebuildInterp()` constructs the double-valued `interp_` handle from the (possibly
`Number_`-typed) `logDF_` vector, extracting primals via `Dal::AAD::Value`. The
`Number_` `LogDfAt` path does **not** consult `interp_` — it uses the basis weights
above directly — but `RebuildInterp` still exists so the handle is available for any
code that defensively calls it.

`ApplyDX(dx, leverage)` bumps only the free nodes `logDF_[1..]` and calls
`RebuildInterp` so the bumped curve re-synchronises the interpolator. This method is
only exercised for `T_ = double`; the `Number_` factory never calls it (the AAD path
constructs the curve directly with the tape-registered `logDF` vector). A stale
`interp_` after a bump would silently desync the curve from its node values.

### Double vs Number_ Dispatch

The `DiscountLogDF_<T_>::operator()(from, to)` method uses `if constexpr` to select the
`double` path (reads `LogDfAt` + `std::exp` + base multiplication as pure arithmetic)
or the `Number_` path where the base curve is treated as a constant `double` multiplier.
Both specializations are explicitly instantiated so the linker finds them under every
AAD backend.

### Serialization: v1 vs v2

- **v1** (`DiscountLogDF_v1`) stored a built `Interp1_` handle and did **not** persist
  `LogDfScheme_`. The scheme cannot be recovered from the deserialised handle (all
  interpolator subtypes report the same `Storable_::type_` as `"Interp1"`, and RTTI
  cannot tell them apart because the concrete classes live in anonymous namespaces). v1
  therefore always reconstructs as `LOG_LINEAR`, the only scheme honestly rebuildable
  from `(nodeDates, logDF)` alone.
- **v2** (`DiscountLogDF_v2`) is the canonical format: it carries the scheme by name
  (`LogDfScheme_` serialised as a string) and fully reconstructs the curve including
  the `fppCoef_` matrix for cubic and mixed schemes.

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
