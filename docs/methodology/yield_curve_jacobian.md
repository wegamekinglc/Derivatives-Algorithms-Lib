# Yield-Curve Jacobian and Inverse-Jacobian Risk

This note explains two operations that sit on top of a calibrated yield curve
(see [Yield Curve Construction](yield_curve.md) and
[Underdetermined search](underdetermined_search.md) for the calibration itself):

1. the **residual Jacobian** $J = \partial\,\text{modelRate}_i / \partial\,x_k$,
   computed two independent ways — AAD reverse sweep and finite-difference bump —
   and shown to agree; and
2. the **inverse-Jacobian IR-risk transform**, which turns a parameter
   sensitivity vector $g$ into bucketed risk per market quote via the calibration
   inverse Jacobian `effJacobianInverse_`.

The runnable demonstration of both is the example program
`dal-cpp/examples/yield_curve_jacobian/yield_curve_jacobian.cpp`.

## The Forward Jacobian, Two Ways

At a solved point $x^\star$ the calibration residuals vanish by construction, but
the Jacobian of the residual map is still the useful linearisation of the curve.
For each instrument $i$ and each free curve parameter $x_k$ (a log-discount-factor
node value; the anchor node is pinned at $0$ and is not free),

$$
J_{ik} = \frac{\partial\, \text{modelRate}_i}{\partial\, x_k}(x^\star).
$$

**AAD reverse sweep.** Recording the free node values as independents on the AAD
tape, building a `Number_`-typed curve, and running one reverse sweep per
instrument output harvests a full row of $J$ at the cost of one forward pass plus
$n$ sweeps — the canonical AAD cost asymmetry. The example records the tape from
first principles (it does not call `TestOnly::AnalyticJacobianAt`) so the
mechanics are visible; the same tape sits behind the library's
`CurveJacobianMode_::ANALYTIC` flag (see
[Analytic Jacobian for curve calibration](../experimental/aad-analytic-jacobian-curve-calibration.md)).

**Bump oracle.** A two-sided central difference — perturb $x_k$ by $\pm h$,
rebuild the curve via `NewDiscountLogDF(...)`, reprice every instrument — gives
the same $J$ with a truncation error of $O(h^2)$ and a round-off floor of
roughly $\varepsilon/h$. At $h = 10^{-6}$ both are around $10^{-10}$ for the
well-conditioned Phase A residual map, so the two methods agree to $10^{-9}$
relative. The example asserts that bar element-wise and prints the worst
discrepancy.

The agreement is not a tautology: the AAD path is analytic in the tape, the bump
path rebuilds a fresh curve per perturbation, and a missed `RegisterIndependent`
or a wrong recording order would make the AAD row silently zero. That is exactly
the failure the two-way comparison is built to catch.

## The Inverse Jacobian and Bucketed IR Risk

Once the curve is calibrated with `solveMode_ = EXACT`, the diagnostics carry
`CurveCalibrationDiagnostics_::effJacobianInverse_` — the solver's effective
(weighted) inverse Jacobian, shape `nFreeParams × nInstruments`. It is the linear
map from a perturbation of the market quotes to the resulting perturbation of the
free parameters, modulo the solver's internal scaling (see below).

Given a portfolio parameter-sensitivity vector

$$
g_k = \frac{\partial\, \text{PV}}{\partial\, x_k}
$$

(length `nFreeParams`), the bucketed risk per calibration instrument is

$$
r = g^{\mathsf T} \cdot \texttt{effJacobianInverse\_} \;/\; \texttt{tolerance\_},
\qquad r_i = \frac{\partial\, \text{PV}}{\partial\, (\text{decimal quote}_i)} .
$$

A true DV01 (price change per $+1\text{bp} = +10^{-4}$ decimal) is $r_i \times 10^{-4}$.

### Why divide by `tolerance_`

The underdetermined solver does not operate on the raw residuals. It scales every
residual row by $1/\tau_j$ before forming the pseudoinverse (see
[Residual Scaling](underdetermined_search.md#residual-scaling)); equivalently,
`effJacobianInverse_` carries units

$$
\texttt{effJacobianInverse\_[}k,i\texttt{]} \sim
\frac{\partial\, x_k \cdot \texttt{tolerance\_}}{\partial\, (\text{decimal-rate perturbation}_i)} .
$$

A consumer that reads a sensitivity vector in honest price-per-decimal-rate units
must therefore divide by `tolerance_` when applying the transform. Omitting that
division leaves the result off by exactly the tolerance factor — silently, since
the magnitude looks plausible. The example applies the correction in
`TransformToQuoteRisk`; the regression test
`dal-cpp/tests/curve/test_inverse_jacobian_risk.cpp` gates it against a genuine
nonlinear re-solve (bump one market quote, re-run `CalibrateYieldCurve`, diff the
new `NodeLogDF()` against the linear prediction). That re-solve path is the
honest sanity check: the alternative "analytic forward map" prediction is
tautological and is deliberately not used.

### Parameter sensitivity $g$

The transform needs $g$ as a *portfolio* sensitivity, computed independently of
the AAD tape that produced the residual Jacobian. That tape records
$\partial(\text{modelRate} - \text{marketRate})/\partial x$ — a rate residual, not
a price. Reusing it for $g$ would conflate a residual sensitivity with a PV
sensitivity (different units, different sign). The example computes $g$ by its
own central-difference bump on the calibrated curve. In the public API the
`YCInstrument_` surface exposes only the par-rate model rate (float-PV over
annuity), not the leg PVs, so $g$ there is a par-rate sensitivity and $r$ is a
par-rate risk per decimal quote bump; the annuity scaling that would convert it
to a true price DV01 is not exposed.

## See Also

- [Yield Curve Construction](yield_curve.md) — the calibration that produces
  `effJacobianInverse_`.
- [Underdetermined search](underdetermined_search.md) — the solver whose residual
  scaling motivates the `/tolerance_` correction.
- [AAD methodology](aad.md) — the reverse-mode machinery behind the analytic
  Jacobian.
- [Analytic Jacobian for curve calibration](../experimental/aad-analytic-jacobian-curve-calibration.md)
  — the `CurveJacobianMode_::{BUMPED,ANALYTIC}` flag that selects the AAD path
  inside `CalibrateYieldCurve`.
- `dal-cpp/examples/yield_curve_jacobian/yield_curve_jacobian.cpp` — the
  runnable program demonstrating both arcs end to end.
