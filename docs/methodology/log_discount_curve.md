# Log-Discount Curve

This note describes the `LOG_DISCOUNT` curve parameterization: its stored node
log-discount factors, scalar-generic interpolation, boundary policy, solver layout, and
AAD behavior. It complements [Yield curve construction](yield_curve.md) and
[Interpolation](interpolation.md).

## Representation

A log-discount curve stores node dates
$t_0 < t_1 < \dots < t_{N-1}$ and log discount factors relative to the anchor
$t_0$,

$$
\ell_i = \ln P(t_0,t_i), \qquad \ell_0 = 0.
$$

The anchor is pinned, so only $\ell_1,\dots,\ell_{N-1}$ are calibration
parameters. At a date $t$, the curve maps the date to a passive year fraction

$$
\tau(t) = \operatorname{YearFrac}(t_0,t;\text{dayCount})
$$

and returns

$$
P(t_a,t_b) = \exp\!\left(\ell(\tau(t_b))-\ell(\tau(t_a))\right)
               P_{\mathrm{base}}(t_a,t_b),
$$

with the base factor omitted when no base curve is attached. Positivity follows from the
exponential for every finite interpolated value.

The implementation is `Tape::DiscountLogDF_<T_, B_>` in
`dal-cpp/dal/curve/yclogdf.hpp`; `DiscountLogDF_` aliases the passive
`<double, DiscountCurve_<double>>` specialization. `NewDiscountLogDF(...)` remains the
public passive factory.

## Unified Scalar-Generic Interpolation

`LogDfInterpolation_` in `dal-cpp/dal/curve/logdfinterp.hpp` owns the passive
year-fraction geometry and returns interpolation weights over storage nodes. Its
`Evaluate<T_>` method applies those weights to `Vector_<T_>` ordinates:

$$
\ell(\tau)=\sum_{j=0}^{N-1}w_j(\tau)\ell_j.
$$

The same object and the same weights are used for `double` and `AAD::Number_`. Knot
positions, segment choices, mixed cutoffs, and extrapolation weights remain passive;
only the stored ordinates carry adjoints. There is no separate hand-maintained AAD
interpolation formula and no passive interpolator to rebuild after a node bump.

The weight primitives live in `dal-cpp/dal/math/interp/interpweights.hpp`:

- `LinearWeightGeometry_` returns the two local affine weights, or one unit weight at a
  knot or clamped endpoint;
- `NaturalCubicWeightGeometry_` precomputes the dependence of every knot second
  derivative on every ordinate, then returns the resulting natural-spline weights;
- `ApplyInterpWeights<T_>` performs the typed ordinate accumulation.

Natural-cubic weights are generally global: changing one node can alter second
derivatives, and therefore interpolated values, on distant segments. Calibration
Jacobians are harvested at full width for every scheme; nominal instrument maturity is
not a safe truncation boundary when payment dates may be adjusted or lagged.

## `LogDfScheme_`

| Scheme | Definition on $\ell(\tau)$ | Minimum storage nodes | Sensitivity support |
|--------|------------------------------|-----------------------|---------------------|
| `LOG_LINEAR` | Piecewise linear | 2 | Local segment, except tail secant |
| `LOG_CUBIC_NATURAL` | Natural cubic with zero endpoint curvature | 3 | Global |
| `MIXED` | Linear head and natural-cubic tail | 4 | Local in head, global over tail |

### Log-linear

On $[\tau_i,\tau_{i+1}]$,

$$
\ell(\tau)=(1-g)\ell_i+g\ell_{i+1},\qquad
g=\frac{\tau-\tau_i}{\tau_{i+1}-\tau_i}.
$$

This is linear interpolation in log discount factor, equivalently log-linear
interpolation in discount factor.

### Natural cubic

The natural spline solves the tridiagonal interior system

$$
h_{i-1}\ell''_{i-1}+2(h_{i-1}+h_i)\ell''_i+h_i\ell''_{i+1}
=6\left(\frac{\ell_{i+1}-\ell_i}{h_i}
-\frac{\ell_i-\ell_{i-1}}{h_{i-1}}\right),
$$

with $h_i=\tau_{i+1}-\tau_i$ and $\ell''_0=\ell''_{N-1}=0$. Geometry
construction solves this system once for each unit ordinate. The resulting dense
second-derivative weight map depends only on the passive abscissae and can be reused
after every ordinate change.

### Mixed

For $N$ storage nodes, the cutoff is the zero-based storage index

$$
c=\max(1,N-5).
$$

The curve is log-linear through $\tau_c$ and natural-cubic on the overlapping tail
$\tau_c,\dots,\tau_{N-1}$. Tail-local weight indices are translated back to full
storage indices before typed evaluation. Both pieces reproduce the cutoff node exactly;
the join is $C^0$, while first- and second-derivative continuity are not imposed.

## Boundary and Extrapolation Policy

The curve owns its boundary policy rather than inheriting whichever behavior a generic
interpolator happens to provide:

- before the anchor, `LOG_LINEAR` and `MIXED` clamp to $\ell_0=0$;
- before the anchor, `LOG_CUBIC_NATURAL` extends the first cubic segment polynomial;
- at every storage knot, every scheme returns the stored ordinate exactly;
- beyond the last node, every scheme uses the final two-node secant,

  $$
  \ell(\tau)=\ell_{N-1}
  +\frac{\ell_{N-1}-\ell_{N-2}}{\tau_{N-1}-\tau_{N-2}}
   (\tau-\tau_{N-1}).
  $$

The right-tail policy deliberately does not use the cubic endpoint derivative. Passive
and AAD curves therefore agree at the anchor boundaries and outside the node range.

## Definitions, Layout, and Column Order

`CurveDefinition_` and `CurveParameterLayout_` in
`dal-cpp/dal/curve/curveparameterization.hpp` provide the common construction contract
used by passive pricing, single calibration, and joint calibration.

For `LOG_DISCOUNT`, `MakeCurveDefinition` prepends the anchor when the declared knots do
not already contain it. The joint public declaration remains unchanged: its knot list
contains only future free nodes, while the internal storage definition contains the
pinned anchor exactly once. The single-curve validation surface continues to require
the caller's first input knot to equal `today_`.

The stable layout is:

| Quantity | Count or mapping |
|----------|------------------|
| Storage nodes | anchor plus declared future nodes |
| Free parameters | storage node count minus one |
| Solver column $j$ | storage node $j+1$ |
| Anchor | storage node 0, fixed at zero, never registered as an independent |

Joint calibration concatenates each declaration's columns in declaration order. A
log-DF declaration contributes its future-node log discount factors in date order.

## AAD and Base Composition

Calibration registers the flat free-parameter vector, opens the recording, and builds
the typed curve through the common `BuildDiscountCurveT` factory. Every queried log
discount factor is the typed weighted sum above, so a reverse sweep returns
$\partial r_i/\partial\ell_j$ directly. Exact structural zeros are retained for
log-linear queries that do not touch later nodes; natural-cubic and mixed-tail rows may
be dense because their mathematical support is global.

The base type is independently templated:

- `B_ = DiscountCurve_<double>` treats an absent or pre-existing base as passive;
- `B_ = DiscountCurve_<AAD::Number_>` propagates adjoints through a base curve built in
  the same joint solve.

Consequently a base-layered log-DF forward declaration carries cross-curve sensitivities
to its discount declaration just like PWC and PWL forward declarations.

`ApplyDX` changes only free ordinates `logDF_[1..]`. Interpolation geometry is unchanged,
so no coefficient or interpolator rebuild is required.

## Persistence

The passive, passive-base specialization writes the `DiscountLogDF` v2 schema, including
node dates, node log discount factors, day count, interpolation scheme, and optional
base. Tape-active or active-base specializations are transient calibration objects and
are not serializable.

The v1 reader remains supported. Because v1 stored an `Interp1_` handle without an
explicit `LogDfScheme_`, it reconstructs as `LOG_LINEAR`; v2 is the canonical format for
scheme-preserving persistence.

## Relationship to Forward-Rate Parameterizations

PWC and PWL curves parameterize the instantaneous forward rate and integrate it to a log
discount factor. `LOG_DISCOUNT` instead parameterizes cumulative log discount factors and
interpolates them directly:

$$
\ell_i=-\frac{1}{365}\int_{t_0}^{t_i}f(t)\,dt.
$$

All three representations implement the same `DiscountCurve_` contract, use the same
curve-definition/factory layer during calibration, and support the AAD-derived analytic
residual Jacobian. Their solver column layouts and between-node shapes differ.

## See Also

- [Interpolation](interpolation.md) — passive interpolation geometry and typed ordinate
  evaluation.
- [Yield curve construction](yield_curve.md) — PWC/PWL integration, calibration, and
  multi-curve routing.
- [Yield-curve Jacobian](yield_curve_jacobian.md) — analytic support, column layouts, and
  inverse-Jacobian risk.
