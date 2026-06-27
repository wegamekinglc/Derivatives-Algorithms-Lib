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
$n$ sweeps — the canonical AAD cost asymmetry. This is the sweep behind the
library's `CurveJacobianMode_::ANALYTIC` flag (see
[Analytic Jacobian for curve calibration](../experimental/aad-analytic-jacobian-curve-calibration.md)),
and `CalibrateYieldCurve` exposes its result on the public diagnostics struct:
`CurveCalibrationDiagnostics_::jacobian_` (shape `nInstruments × nFreeParams`) is a
fresh analytic reverse sweep evaluated at the solved $x^\star$. It is the plain
Jacobian before the solver's `DivideRows(tol_)` row-scaling, and it is populated
iff `jacobianMode_ = ANALYTIC && solveMode_ = EXACT` and the calibration is
eligible for the AAD-tape Jacobian (default-constructed empty otherwise). The
example reads $J$ straight from `result.diagnostics_.jacobian_`, so the AAD
recording contract and backend-portability rules live in the library rather than
in example code.

`jacobian_` is a different object from `effJacobianInverse_` below: it is unscaled
and evaluated at the solution, whereas `effJacobianInverse_` is a solver-weighted,
tolerance-scaled pseudoinverse formed at the solver's final iterate. They are not
inverses in their exposed form.

### How `jacobian_` is captured

The forward Jacobian is not re-derived by the consumer; it is captured **once,
inside the solver, on its convergence branch**. When the solve is eligible for
the analytic path, the convergence hook issues a single
`func.Gradient(xNew, fNew)` call at the solved $x^\star$, and the reverse sweep
that fills `jacobian_` is that one evaluation. The output is the plain Jacobian
before the solver's `DivideRows(tol_)` row-scaling, and an independent
finite-difference bump of the solved nodes reproduces it element-wise.

There is deliberately **no standalone "analytic Jacobian at a point" accessor**.
The forward Jacobian is obtainable only as a byproduct of an eligible
calibration, on the public diagnostics struct — never as a free-standing query
on a curve. A consumer that wants $J$ at an arbitrary point must run an
eligible calibration through `CalibrateYieldCurve` and read
`result.diagnostics_.jacobian_`, or perform the bump locally.

**Bump oracle.** A two-sided central difference — perturb $x_k$ by $\pm h$,
rebuild the curve via `NewDiscountLogDF(...)`, reprice every instrument — gives
the same $J$ with a truncation error of $O(h^2)$ and a round-off floor of
roughly $\varepsilon/h$. At $h = 10^{-6}$ both are around $10^{-10}$ for the
well-conditioned `LOG_DISCOUNT` residual map, so the two methods agree to $10^{-9}$
relative. The example asserts that bar element-wise and prints the worst
discrepancy.

The agreement is not a tautology: the AAD path is analytic in the tape, the bump
path rebuilds a fresh curve per perturbation, and a missed `RegisterIndependent`
or a wrong recording order would make the AAD row silently zero. That is exactly
the failure the two-way comparison is built to catch.

## Joint Multi-Curve Analytic Jacobian

The single-curve path calibrates one discount curve under the
$\text{forecast} = \text{discount}$ identity. The joint multi-curve path
(`CalibrateJointMultiCurve` in `dal-cpp/dal/curve/jointcalibration.cpp`)
calibrates several curves in **one** `Underdetermined::Find` / `Approximate`
solve over the concatenated free-parameter vector of every declaration, and
produces an AAD-derived dense residual Jacobian over that stacked vector. The
result is exposed as
`JointMultiCurveCalibrationResult_::jacobianAtSolution_`, shaped
`(totalResiduals) × (totalFreeParams)`, populated iff
`JointMultiCurveCalibrationOptions_::jacobianMode_ == ANALYTIC`,
`solveMode_ == EXACT`, and the spec is eligible; empty otherwise.

The mechanics mirror the single-curve path — register the stacked free
parameters on the tape, build `Number_`-typed curves, compute `Number_`-typed
residuals, run one reverse sweep per residual row — but the routing of a
discount-factor read now has to choose between multiple curves, and the curve
types themselves have to carry their parameter dependence through that routing.
The library encodes this in three templated primitives under `namespace Dal::Tape`
(see [AAD methodology — Tape-Layer Primitives for Curve Calibration](aad.md#tape-layer-primitives-for-curve-calibration)
for the type-by-type detail). The methodology that is *specific* to the joint
path — the routing rules, the eligibility gate, the smoothing — is what this
section covers.

### Eligibility

`JointSpecEligibleForAnalyticJacobian` admits a spec only when **all** of the
following hold (each failing condition emits a `NOTICE` naming it, then the
solver dense-bumps):

- every declaration is `PIECEWISE_LINEAR_FWD` — the independents are the
  per-knot forward parameters `fLeftT_[k]`, `fRightT_[k]` ($2 \cdot n_{\text{knots}}$
  per declaration, no anchor exclusion);
- `liborBasis_ == ACT_365F` — the basis-year fraction in the simple-rate
  arithmetic must match the basis the templated rates assume;
- every instrument is a vanilla `Deposit_`, `FRA_`, `Future_`, or `Swap_`
  (`OISSwap_` rides the inherited `Swap_::PrecomputeT<T_>`, since its overnight
  index has `useProjectionCurve_ == false`, so forecast == discount == OIS and
  both the AAD and bumped paths share the identical simple-rate arithmetic);
  instruments without a templated rate (e.g. `BasisSwap_`) reject the whole
  calibration;
- on a **discount** declaration, no instrument projects
  (`useProjectionCurve_ == false`) so every fixing routes to a single curve on
  the tape; on a **forward** declaration, every instrument projects
  (`useProjectionCurve_ == true`) so the forward curve is actually constrained.

The verdict is evaluated once per `CalibrateJointMultiCurve` call and cached, so
the `NOTICE`s fire at most once even though `Gradient` is invoked per solver
iteration. `ANALYTIC` never throws — an ineligible spec falls back to `BUMPED`
byte-for-byte.

The supported-parameterization set is narrower still, and **not** gated by
eligibility: the joint path supports `PIECEWISE_LINEAR_FWD` and
`PIECEWISE_CONSTANT_FWD` only. A declaration with `parameterization_` of
`LOG_DISCOUNT` or `ZERO_RATE` is rejected at validation with a hard `REQUIRE`
(it throws `Dal::Exception_`) on **both** the `BUMPED` and `ANALYTIC` paths —
the joint residual function has no log-DF or zero-rate machinery, and there is
no fallback. `CalibrateJointMultiCurve` is single-threaded: the AAD tape is
thread-local and a `TapeGuard_` clears it on entry and exit (also under
exception unwind), so concurrent calls would corrupt the tape.

### Discount-vs-forward routing, and the OIS post-2008 fallback

A joint spec has two slot kinds. A **discount** declaration
(`calibrateDiscountCurve_ == true`) produces one discount curve per
`targetCollateral_`. A **forward** declaration
(`calibrateDiscountCurve_ == false`, with a non-default `targetTenor_`) produces
one forward curve per tenor. The IBOR leg of a forward-declaration instrument is
discounted at the collateral of another declaration's discount curve —
canonically OIS. The capability wires that routing internally by assembling a
`CurveBlock_` from every declaration's curves; the caller never names "which
curve discounts which other curve."

On the AAD tape this routing is reproduced by `Tape::JointCurveBlock_<T_>` (in
`dal-cpp/dal/curve/jointycctx.hpp`), whose `Discount(collateral)` and
`Forward(tenor, collateral)` reads mirror `CurveBlock_::Discount` /
`CurveBlock_::Forward` exactly. Two fallbacks are load-bearing:

1. **OIS fallback on discount.** `Discount(collateral)` first looks for an
   exact-collateral match; if none is registered it falls back to the OIS
   discount curve. This is the post-2008 single-curve convention preserved on
   the tape: an OIS knot perturbation must flow into an IBOR leg's discounting
   when the leg's collateral is not OIS. Dropping this fallback on the tape
   would silently zero the OIS-to-IBOR discounting sensitivity — exactly the
   cross-curve coupling the joint solve exists to capture.
2. **Forward-to-discount fallback.** `Forward(tenor, collateral)` first looks
   for an exact-tenor forward curve; if none is registered it routes to
   `Discount(collateral)`. This admits the discount-slice / baseless-forward
   case where forecast == discount (e.g. an OIS-discount slice), and the
   eligibility predicate treats it as supported rather than as a missing curve.

### Base layering over discount

A forward declaration may optionally set `baseLayeredOverDiscount_ == true`. The
forward curve is then built with its base set to the discount curve at
`targetCollateral_` produced in the **same** solve, so the curve the smoother
acts on is the **spread forward** $f_{\text{abs}} - f_{\text{ois}}$ rather than
the absolute forward $f_{\text{abs}}$. This matches the staged calibration's
base layering and is the form in which a LIBOR-OIS forward is naturally smooth.

On the AAD tape this is realised by giving `Tape::DiscountPWLF_<T_, B_>` a
second template parameter `B_`:

- `B_ = DiscountCurve_<double>` — baseless / constant-base (the base is passive;
  its parameters carry no adjoints);
- `B_ = DiscountCurve_<T_>` — base-layered (the base's own parameters carry
  adjoints, and the reverse sweep propagates OIS sensitivities through the base
  multiplication into the discount-curve free nodes).

The base-layered form is required for the joint solve to see the OIS → forward
coupling on the tape; the baseless form remains supported for representations
that do not layer. `PIECEWISE_CONSTANT_FWD` declarations cannot be base-layered
— base layering requires `PIECEWISE_LINEAR_FWD`.

### PWL-forward → log-DF integration

`Tape::DiscountPWLF_<T_, B_>` interpolates forward rates piecewise-linearly on
`T_` and integrates them to log-discount factors. The running integral
`sofarT_[k] = ∫_{knot_0}^{knot_k} f(τ) dτ` is `T_`-typed (mirroring the
`PiecewiseLinear_::sofar_` member but on `T_`), so the dependence of every
discount-factor read on `fLeftT_` / `fRightT_` records on the tape. The
year-fraction weights (`dt`) and the knot abscissae themselves
(`knotAbscissae_`, serial-day offsets from knot 0) stay `double` — they are
functions of the knot positions only, computed once at construction and
identical for any `T_`. `UpdateT()` recomputes `sofarT_` whenever the forward
parameters change. The `double` specialization of the templated class is
byte-for-byte identical in arithmetic to the non-templated `DiscountPWLF_` the
bumped path uses, so the AAD-vs-bump agreement bar is defined against the same
residual function.

The `Number_`-typed curve is constructed directly from tape-registered forward
parameters — the `AnalyticJacobian` override registers `fLeftT_` / `fRightT_`
as independents and passes them straight to the `Tape::DiscountPWLF_<T_, B_>`
constructor. `ApplyDX` is never invoked on the AAD path (it would overwrite a
tape leaf with a non-typed `double` increment and break the recording). This is
why the class holds flat `Vector_<T_>` members rather than a templated
interpolator: the joint path is the only consumer of the templated curve, the
parameters are always supplied at construction, and flat members minimise the
surface the tape has to traverse.

The templated `Tape::DiscountPWLF_<T_, B_>` is **non-storable** — its `Write()`
override is a hard `REQUIRE(false)`. Only the anonymous-namespace `double`
`DiscountPWLF_` in `dal-cpp/dal/curve/ycimp.cpp` is serializable, and that is
the one the bumped path and any persistent result curve use; the templated
curve exists only for the duration of one `Gradient` sweep and is discarded
with the analytic-Jacobian frame.

### Why assembly is sparse-by-row

The reverse sweep produces one row of $J$ at a time. AAD produces **exact
structural zeros** at every parameter a residual does not touch by any routing
path. Which cross-declaration entries are nonzero is determined entirely by the
residual map's routing — a forward-declaration residual touches the OIS
discount declaration's parameters through the discount leg (OIS fallback) or
through base layering, and those entries are genuinely nonzero; parameters no
residual reaches by any route are exactly zero. Storage is dense (in
`XCurveJacobian_`, `dal-cpp/dal/curve/curvejacobian.hpp`, shared with the
single-curve path), so the zeros are stored explicitly rather than compressed
away; the structural sparsity is exploited only at assembly time, by sweeping
each row independently. The dense storage is what the underdetermined solver's
`MultiplyLeft` / `MultiplyRight` / `QForm` virtual interface reads; the row-wise
AAD sweep is what guarantees the untouched entries are exactly zero and not
numerical noise.

The pointers in `Tape::JointCurveBlock_<T_>` are non-owning `const
DiscountCurve_<T_>*` (not `Handle_<...>`) because the curves they reference live
on the `shared_ptr` storage of the analytic-Jacobian frame for the duration of
the single sweep and are destroyed when that frame goes out of scope — wrapping
them in `Handle_<...>` would imply shared ownership the frame does not need.

The joint smoother is **block-diagonal**: `BuildJointSmoothing` assembles a
single tridiagonal weight matrix over the stacked parameter vector, but each
declaration's self-coupling is written independently into the diagonal block at
that declaration's `paramOffset` (one per-slot `SelfCouplePWC` call, using only
that declaration's own knots and `smoothingWeight_`). No cross-declaration
smoothing entries are ever introduced. The smoother therefore adds **no**
cross-declaration coupling to the Jacobian beyond what the residual map's
routing already establishes: the only cross-declaration nonzeros in $J$ come
from a forward-declaration residual reading a discount declaration's parameters
(via the OIS discount-fallback on the leg, or via base layering), not from the
smoother. The exact structural zeros the AAD sweep produces sit at parameters
no residual touches by any route — the block-diagonal smoother simply ensures
the smoothing pass does not smear those zeros into small non-zero entries.

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

### Nonlinear re-solve tolerance

The re-solve sanity check compares the linear inverse-Jacobian prediction
`effJacobianInverse_ · Δquote / tolerance_` against the true rebumped parameter
delta from a fresh `CalibrateYieldCurve`. The two diverge at a relative error
that **grows with the number of knots**: bumping a long-end quote propagates
through every intervening LOG_DISCOUNT knot, accumulating second-order terms the
linear map cannot capture. The observed worst-case relative error scales roughly
as $7\times10^{-7}$ at 5 instruments, $1.8\times10^{-5}$ at 10, and
$1.3\times10^{-4}$ at 16. A bar of $10^{-4}$ relative at the 10-instrument size
leaves about $5\times$ headroom over the observed worst case; it should be
re-measured (and tightened back toward $10^{-6}$ for shorter ladders, or relaxed
for longer ones) whenever the ladder length changes. This is looser than the
$10^{-9}$ forward-Jacobian agreement bar by design, because the re-solve is a
genuine nonlinear operation rather than a finite-difference check of an analytic
derivative.

## Timing: BUMPED vs ANALYTIC

The example also times `CurveJacobianMode_::{BUMPED,ANALYTIC}` calibrations on
the same EXACT solve. Both modes run the identical Newton solve; the only
difference is how the forward Jacobian is obtained — $n$ serial finite-difference
re-calibrations for `BUMPED` versus one AAD reverse sweep per residual row for
`ANALYTIC`. Each `CalibrateYieldCurve` call resets its own tape internally, so
repeated calls are independent and safe to time.

The comparison is not pure like-for-like. The `ANALYTIC` time includes the
single at-solution forward-Jacobian evaluation the solver makes on its
convergence branch to populate `CurveCalibrationDiagnostics_::jacobian_`, which
`BUMPED` does not perform. The honest reading of the ratio is therefore
"`ANALYTIC` solve-with-Jacobian vs `BUMPED` solve-without-Jacobian". A truly
matched comparison would give `BUMPED` its own separate finite-difference
Jacobian pass to produce the same diagnostic, which the example does not do.

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
