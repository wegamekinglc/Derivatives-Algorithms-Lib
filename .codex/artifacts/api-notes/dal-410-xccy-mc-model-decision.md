# DAL-410: domestic-collateral XCCY Gaussian model decision

Status: proposed implementation contract, ready for focused review. No stochastic
XCCY implementation or numerical acceptance is delivered by this note.
`MCCrossCurrencyUnsupported` remains the current capability. This decision
controls the model, integration and independent acceptance tasks in section 10;
DAL-409's deterministic delivery proceeds independently.

## 1. Audience, baseline and decision

The audiences are model implementers, script/provider and staged-calibration
maintainers, numerical validators, and C++/Python/Excel API consumers. The aim is
to revalue the **original immutable script instrument** after curve calibration,
with matching FX direction, basis, collateral, fixings and cashflows.

Source inspected: `master@d3bf3043b2385b70a1fcd4fb32723a2b86c549bd`, fetched on
2026-10-23; task branch started there and was identical to fetched master. This
includes DAL-408 / PR #620, actually merged at 02:15:57 UTC, checked head
`ad9f2bea58705ab87dd54033afa3b5410e9c90fa`. The linked PR reports merged and
36 successful checks. These are accepted predecessor records, not fresh tests
by this design task. Historical research baseline
`0f1feabee193d0976ab7a5336c9e9e29cb2aa28d` is not the implementation base.

Inputs are DAL-396 attachment `01a15c20-3723-7ddd-8ef1-c0ac706cdc16`, sections
5, 7.2, 8.3 and 11; the frozen [DAL-398 contract](dal-398-script-contract.md);
the [DAL-408 record](../implementation/dal-408-xccy-script-cashflows.md); and
the current [methodology](../../../docs/methodology/script_calibration_contract.md).
DAL-396's current dispatch authorizes this one document and a base-master PR;
it does not authorize implementation, product tests, other experts or downstream
dispatch from this issue. The parent retains ownership of those actions.

**Decision:** implement one Gaussian short-rate factor for each currency and
lognormal FX with all three Brownian correlations, under the domestic collateral
bank-account measure. Keep cross-currency basis and tenor projection spreads
deterministic, explicit curves. Simulate rate endpoints **and integrals** jointly
and exactly for piecewise-constant volatilities. Add a same-source analytic
Gaussian-moment evaluator for the admitted cashflows, including reset/MTM and
payment-lag convexity. Use it for stochastic-model staged basis residuals and
MC validation; keep legacy deterministic native prices as their own convention.

This is the smallest selected model with stochastic FX, both rates, an explicit
basis term and a defensible independent expectation oracle. Deterministic basis
means zero *basis-spread volatility*, not zero fitted basis. It does not support
stochastic funding spreads, collateral switching, FX smile or tenor-basis risk.
No volatility/correlation estimation or joint rates/FX calibration is implied:
the caller supplies and freezes these dynamics. Fitting them from vanilla XCCY
par quotes alone would be underidentified.

### Current source evidence and reuse boundary

All code paths below are relative to the repository root at the source SHA above.

| Source / symbol | Established behavior; consequence |
|---|---|
| `dal-cpp/dal/curve/xccyscript.cpp`: `ExportRows`, `Notional`, `Principal`, `NewXccyScriptInstrument` | Both legs, N/K, notional ratio, FX reset and MTM deltas are source expressions. Foreign payment conversion is `SPOT()`. Reuse this instrument; do not retain or reconstruct an independent native trade for MC. |
| `dal-cpp/dal/curve/xccyscriptlowering.cpp`: `Lower`, `Verify`, `Capture` | Reconstructs a descriptor from complete bound AST and payment dates, verifies regeneration, and captures strict-past unsettled history once. Reuse admission geometry/history; add stochastic execution separately. |
| `dal-cpp/dal/curve/xccypricing.cpp`: `FxForwardValue`, `ForeignConversionFactor`, `ResolveObservedValue`, `ResolveXccyNotionals` | `F=S*Pf/(Pd*B)`, domestic collateral, domestic-denominated basis, exact timestamp policy, domestic reset `Nf*FX`, MTM `new-old`. Native forward substitution supplies no stochastic convexity guarantee. |
| `dal-cpp/dal/curve/xccyscriptprovider.cpp`: `ContextIdentity`, `Admit`, `Value` | Dedicated timestamp context excludes generic model/valuation fields; early MC refusal; signed `Price=B+K*A`, `BasisSpread=-B/A`, finite coefficient and annuity checks. No existing context may simply accept a foreign GSR. |
| `dal-cpp/dal/model/gsr.hpp`: `SupportsIndex`, `AdvancePath`, `GeneratePathWithState` | Single-currency DF/Libor/Swap, Gaussian kernels and integral/numeraire techniques. Reuse mathematics and suitable helpers only after proving the representation map; GSR `g/H` are not these short-rate `sigma/a` coordinates. |
| `dal-cpp/dal/model/hybrid.hpp`: `ValidateComponentIdentity`, `ValidateAndOrderComponents` | Every component must use the domestic currency; exactly one numeraire provider. Existing hybrid GSR/equity composition is not a two-currency quanto model. Do not loosen these guards globally. |
| `dal-cpp/dal/model/factory.hpp`: `CreateModel` | BS, correlated BS, domestic hybrid, GSR and SLV dispatch; no matching XCCY model. Add a dedicated model type and factory case. |
| `dal-cpp/dal/script/preparation.cpp`: `IsHistorical`, `Collector_::Add`, `Collector_::Collect` | Generic history is date/today-policy based, key is canonical index + midnight, SPOT may become default-index FIX. This would merge payment SPOT and reset FIX incorrectly at valuation. A dedicated XCCY observation planner is necessary. |
| `dal-cpp/dal/math/aad/sample.hpp`, `dal-cpp/dal/script/observationplan.hpp` | Existing sample observations, numeraire, date/slot mapping and evaluator interfaces are reusable. `script/detail/simulationobserver.hpp` is a submission/testing hook, not a financial observation provider. |
| `dal-cpp/dal/script/calibrationmc.cpp`: `MakeMapping`, `FactorNames`, `PreparedCalibrationMc_::RefreshG`; `calibrationmc.hpp` | Existing batching, ratio covariance, native gradient statistics and recording ownership can be reused. Mapping assumes native GSR numeraire and sample/factor coordinates; RefreshG is GSR-specific. Neither admits the new model unchanged. |
| `dal-public/src/calibrationinstrument.cpp`: `McContext`, `ValueCalibrationInstrumentByMonteCarlo`; `calibrationinstrument.hpp` | Existing public MC request carries generic valuation, and its AAD parameter path expects Gaussian curves. A dedicated additive XCCY request must build its own context and label mapping. |
| `dal-public/src/xccycalibration.hpp`; `dal-python/src/bindings/curve.cpp`; `dal-excel/src/__xccycalibration.cpp` | Native staged/joint curve interfaces and market handles exist. Preserve their defaults, solver behavior and result contracts. Integrate with the merged DAL-409 same-source staged adapter, not a second solver. |
| `dal-python/src/bindings/calibrationinstrument.cpp`; `dal-excel/src/__calibrationinstrument.cpp` | Owned reports, typed handles and GIL-release patterns exist. New model/request bindings must preserve these boundaries and fail before workers on invalid inputs. |

The accepted DAL-408 deterministic oracle and refusal cases live in
`dal-cpp/tests/curve/test_xccy_script.cpp`; accepted GSR numerical methodology is
recorded in [DAL-413](../validation/dal-413-gsr-script-numerical-validation.md).
Neither is evidence of stochastic XCCY correctness. Core reset example
`dal-cpp/examples/curves/xccy_reset_pricing/xccy_reset_pricing.cpp` deliberately
uses custom/intraday fixings; Python `examples/006.xccy_calibration.py` uses 12M
native conventions. Those examples are not admissible shared-script fixtures.
Preserve their locations and purpose. A new facade example belongs separately
under `dal-public/examples/xccy_script_monte_carlo/`, depending only in the allowed
`dal-cpp <- dal-public <- {dal-python, dal-excel}` direction.

Gitlinks are Eigen `3147391d`, Google Test `d72f9c8a`, Machinist `e76b0ef2`,
pybind11 `8a099e44`, RapidJSON `ea152ded`. They are uninitialized in this clean
design checkout. No build, dependency update, product test or performance run is
needed or claimed here.

## 2. Curves, units and the collateral interpretation

Set time zero to the exact midnight valuation V and use ACT/365F for **model**
time, with no intraday support. Coupon accrual alpha remains the exact canonical
currency/index day basis from the source, normally ACT/360 in the fixture.
Normalize every input discount to V. Write domestic OIS `Pd(0,T)`, foreign OIS
`Pf(0,T)`, basis discount `B(0,T)` and tenor projections `Q_{c,k}(0,T)`.
All are positive finite, anchored at one; no missing-route fallback.

Define the effective foreign discount under domestic collateral as

```text
Pc_d(0,T) = Pd(0,T)
Pc_f(0,T) = Pf(0,T) / B(0,T)
F(0,T)   = S0 * Pc_f(0,T) / Pc_d(0,T)
b(t)     = -d log B(0,t)/dt
c_f(t)   = r_f_own(t) - b(t)
```

S is domestic currency per one foreign currency: USD per EUR for EUR/USD.
Positive b raises F in the existing convention; using `Pf*B` reverses its sign.
The original foreign OIS remains available as
`P_f_own(t,T)=Pc_f(t,T)*B(0,T)/B(0,t)`. It is not another independently
simulated discount process. These are effective collateral prices in the chosen
funding model, not a claim that differently collateralized bonds are identical.

First implementation accepts losslessly represented log-linear log-discount
curves, including piecewise-constant forwards with all their breakpoints.
Union nodes and explicit coverage extend through every observation maturity,
payment and reset. No extrapolation, arbitrary-interpolation resampling or
replacement of projection by OIS is allowed. A nonlinear curve not representable
by this snapshot is explicitly outside the first stochastic capability, even if
the deterministic provider supports it. Basis is an explicit required handle;
zero basis uses an explicit unit curve, not an inferred absent market field.

For each live supported tenor define the deterministic projection multiplier

```text
q_{c,k}(T) = Q_{c,k}(0,T) / Pc_c(0,T)
Q_{c,k}(t,T) = Pc_c(t,T) * q_{c,k}(T)/q_{c,k}(t)
L_{c,k}(u;S,T) = (Pc_c(u,S)/Pc_c(u,T) * q_{c,k}(S)/q_{c,k}(T) - 1)/alpha
```

The model realizes this L at its fixing u, with `u <= S < T`; the realized value
is retained until payment. This deterministic multiplicative projection model
fixes the **initial** native forward exactly and gives a T-forward martingale
rate up to fixing. It does not model stochastic OIS/IBOR spread or claim arbitrary
observed tenor dynamics. An all-historical tenor requires no unused projection
route. Current canonical 3M/6M identity/geometry admission remains in force.

| Input / output | Native unit and constraint |
|---|---|
| `S0`, FX FIX/SPOT | Positive finite domestic/foreign currency ratio |
| `a_d`, `a_f` | Constant nonnegative mean reversion, year^-1; zero uses continuous limits |
| `sigma_d(t)`, `sigma_f(t)` | Nonnegative decimal short-rate volatility, year^-3/2; equivalently decimal annual rate per sqrt(year) |
| `sigma_X(t)` | Nonnegative log-FX volatility, year^-1/2; .12 means 12% per sqrt(year) |
| `R` | Constant 3x3 Brownian correlation, ordered `rate:USD`, `rate:EUR`, `fx:EUR/USD`; dimensionless, symmetric, unit diagonal, positive definite |
| curves / basis | Dimensionless discounts, original currency/route and native coordinate provenance retained |
| `N`, imported ratio, `K` | Domestic currency amount, foreign/domestic notional ratio, signed decimal spread; no bp rescaling inside pricing |
| Price / BasisSpread | Domestic currency / decimal rate; raw Jacobians in native parameter coordinates |

Require correlation minimum eigenvalue > 1e-10; refuse singular/perfect-correlation
configurations initially. No automatic PSD repair or correlation calibration.
Zero volatilities remain valid value limits; derivative admission is separate.

## 3. Dynamics, measure and mathematical checks

Use the domestic collateral bank account
`Nd(t)=exp(integral_0^t c_d(s) ds)`, Nd(0)=1, and its measure Qd. With
`c_i(t)=phi_i(t)+x_i(t)`, x_i(0)=0, specify

```text
dx_d = -a_d*x_d dt + sigma_d(t) dW_d^d
dx_f = (-a_f*x_f - rho_fX*sigma_f(t)*sigma_X(t)) dt + sigma_f(t) dW_f^d
d log S = (c_d - c_f - .5*sigma_X(t)^2) dt + sigma_X(t) dW_X^d
d<W_i^d,W_j^d> = R_ij dt
```

Under the effective foreign measure Qf, x_f is the zero-mean OU process:
the density is `S(t)*Nf(t)/(S0*Nd(t))`, with `Nf=exp(integral c_f)`.
Girsanov gives `dW_f^f=dW_f^d-rho_fX*sigma_X dt`, proving the **negative**
foreign-rate quanto term above. If implementing a Qf oracle, the domestic-rate
drift becomes `-a_d*x_d + rho_dX*sigma_d*sigma_X`; the foreign drift is `-a_f*x_f`.
The log-S drift under Qf is `c_d-c_f+.5*sigma_X^2`, because
`dW_X^f=dW_X^d-sigma_X dt`. A domestic payoff H at P has the independent
representation `Vd=S0*E_Qf[H/(S(P)*Nf(P))]`; a foreign payoff Hf has
`Vd=S0*E_Qf[Hf/Nf(P)]`. The same S quote is retained under measure change.
Reciprocal FX is computed
on each path; it is not an independent GBM with a copied drift.

Fit the initial curves without numerical differentiation at log-DF knots. For
each currency's zero-mean OU under its own effective measure define

```text
H_i(u,T) = (1-exp(-a_i*(T-u)))/a_i      # T-u at a_i=0
v_i(t,T) = integral_t^T sigma_i(u)^2 * H_i(u,T)^2 du
Phi_i(t) = integral_0^t phi_i(s) ds = -log Pc_i(0,t) + .5*v_i(0,t)
Pc_i(t,T) = exp(-(Phi_i(T)-Phi_i(t)) - H_i(t,T)*x_i(t) + .5*v_i(t,T))
```

Use stable expm1/series at small a*dt. Foreign bond prices use the **Qf**
conditional formula at the Qd-simulated x_f, not a Qd discount expectation
without a density. Model weights are `1/Nd(payment)`, once only in PAYS;
foreign amounts are first multiplied by `S(payment)`. Do not additionally
multiply them by Pf or B in the script executor.

The selected system implies these independently checkable identities:

```text
E_Qd[1/Nd(T)]              = Pd(0,T)
E_Qd[S(T)/Nd(T)]           = S0 * Pc_f(0,T)
F(t,T)                    = S(t) * Pc_f(t,T)/Pd(t,T)
E_Qd[S(T)*Nf(T)/Nd(T)]     = S0
```

The forward is an expectation under the domestic **T-forward** measure;
`E_Qd[S(T)]` need not equal F(0,T). Nor is `E[1/S(T)]` the reciprocal of an
expected S. Quoting the reciprocal forward does not switch the collateral or
numeraire of this model. The first product matcher admits the configured FX
direction only; reciprocal observation arithmetic can be tested internally.

The collateral/measure distinction and MTM corrections are consistent with the
primary treatment in Moreni and Pallavicini, [FX Modelling in Collateralized
Markets](https://arxiv.org/abs/1508.04321v2), sections 2, 3.2 and 4. The equations
above are this note's explicit specialization and derivation, not formulas
copied from that paper or proof that an existing DAL model already implements it.

### Exact transition and error contract

Maintain `Y=(x_d,x_f,I_d,I_f,Z)`, where `I_i=integral x_i` and
`Z=integral sigma_X dW_X^d`. Its drift matrix has diagonal `(-a_d,-a_f,0,0,0)`,
entries `A[I_d,x_d]=A[I_f,x_f]=1`, and deterministic drift
`q[x_f]=-rho_fX*sigma_f*sigma_X`. Diffusion loads sigma_d on x_d,
sigma_f on x_f, sigma_X on Z with covariance R. On each constant-volatility
interval h, sample the exact Gaussian transition

```text
m = exp(A*h)*Y + integral_0^h exp(A*(h-u))*q du
C = integral_0^h exp(A*u)*G*R*G^T*exp(A^T*u) du
Y_next = m + L_C * z
log Nd(t) = Phi_d(t) + I_d(t)
log S(t) = log S0 + Phi_d(t)-Phi_f(t)+I_d(t)-I_f(t)
           - .5*integral_0^t sigma_X^2 + Z(t)
```

Use closed-form exponential integrals, with constant-size matrix exponential
as an independently checked alternative, not Euler or trapezoidal short-rate
integration. Exact endpoints alone are insufficient: the joint endpoint/integral
covariances, including FX, are required. There are three economic Brownian
drivers but **five Gaussian innovation slots per advancing interval** in the
general exact joint transition. Do not infer SimDim from three driver names.
Slots are fixed in order `xd,xf,Id,If,Z`, with interval-major lower-triangular
factorization; zero-variance pivots consume their reserved slots and contribute
zero. Publish this layout as `dal.xccy-gaussian-exact/1`. The runtime reports
`NumFactors()=3`, `SimDim()=5*advancingIntervals`, and no Brownian bridge;
its dedicated mapping adapter enumerates the five innovation coordinates
directly. It must not use the current GSR `MakeMapping` dimension assumption.

R is strictly positive definite; C can be semidefinite at zero vol or limiting
a. Implement deterministic rank-aware lower-triangular factorization in the
fixed slot order. **Every reliably computed strictly positive residual pivot
is retained**, taking its square root and dividing subsequent cross residuals
by it, however small relative to the largest variance. No positive-volatility
cutoff, positive-pivot clipping, jitter or eigenvalue clipping is allowed.
`tau=64*machine_epsilon*max_diagonal(C)` is only a roundoff allowance for
symmetry, nonpositive residuals in `[-tau,0]`, and residual-row compatibility
after a zero pivot has been established. It is never a positive-pivot/rank test.
A nonpositive pivot may become zero only when the kernel/roundoff check supports
a null direction; materially negative pivots or incompatible null rows fail.
An uncertain cancellation must be recomputed with scaling or wider arithmetic,
or fail `CalibrationNumericalFailure`, rather than silently erase a direction.
An all-zero kernel covariance produces five zero innovations but consumes all
five slots. AAD through a genuine changing-rank or zero-vol boundary remains
unavailable; a small positive sigma alone creates no new derivative boundary.

The existing `dal-cpp/dal/math/matrix/covariance.hpp:30` retains strictly positive
residuals; `dal-cpp/tests/model/test_gsr_multifactor.cpp:141` checks a positive
variance of 1e-30. These are source precedents, not proof of this new transition.
The independent kernel/reconstruction control in section 8 must retain, together,
`C[xd,xd]` approximately 9.7059110693e-17 and `C[xd,Z]` approximately
-2.3643573161e-10 for sigma_d=1e-8, h=1 and the other base dynamics. Here tau is
approximately 2.0463630790e-16; treating that positive pivot as zero is forbidden.

Grid = V + all live payment events + all live rate/FX fixings + dynamics
bucket boundaries strictly before the last required live sample. Accrual
endpoints and requested bond maturities additionally enter the curve-coverage
set; they need not be random sample events unless used
as observations. Reset effective-date MTM payments are explicit events.
Deduplicate by exact Date, sort, and preserve source-to-observation slots.
Internal innovation coordinates and observation/event coordinates are distinct.
No Brownian bridge, Sobol, intraday rounding or synthetic midnight fixings.

For this discrete-cashflow domain, exact transitions remove time-stepping bias;
they do not remove roundoff, covariance factorization error, ratio bias or
sampling error. Splitting a constant-coefficient interval must preserve its
analytic transition moments and expected cashflows. Refined-grid paths need not
be bitwise identical: refinement changes the normal mapping. Compare analytic
moments and independent estimates, or explicitly couple Brownian integrals;
do not claim CRN just because the seed is unchanged.

## 4. Observation provider and historical-time policy

Prepare one dedicated XCCY observation plan from the admitted instrument's
bound AST, event dates and header. Do not run generic date/today classification
then overwrite its results. Each request needs canonical and original identity,
exact fixing timestamp, source row/statement/node, economic kind, currency,
tenor/start/end/accrual, route, and sample/known-value slot. Internally distinguish
`PaymentFxSpot` from `ResetFxFixing` even when pair/date coincide.

| Request | Resolution in this stochastic model |
|---|---|
| `FIX` strictly before V | Exact frozen supplied historical rate/FX value, passive under all bumps; missing fails before workers |
| `FIX` exactly at V | Supplied snapshot value first; otherwise active initial projected L or S0. The selected source is frozen for all candidates |
| `FIX` after V | Realized model L or S at the fixing sample, saved for later uses; ignore future snapshot entries |
| Payment `SPOT()` at P >= V | Always model S(P), including S0 at P=V; never replace with an optional reset fixing |
| Event/payment before V | Settled payments excluded. Only exact history needed for remaining payments/resets is captured |
| Explicit empty snapshot | Authoritative: missing strict-past dependencies fail, at-V FIX remains active; never supplement from globals |
| Omitted snapshot | Capture strict-past unsettled dependency closure once, including previous notionals needed for unpaid MTM deltas; do not capture at-V global values |

V, native rate fixings and reset fixings must be exactly midnight. Intraday V
or identities fail `UnsupportedCalibrationFixingTime` with actual time and
required constraint. Generic Model/RequireHistorical override fails
`InvalidCalibrationFixingPolicy`. Distinct time keys are never aliased.
Historical values and optional at-V supplied fixings are passive numbers,
including when they disagree with active S0/projection. Payment spot stays
active. At-V omitted and explicit-empty snapshots can give the same value but
retain distinct provenance identities, as in DAL-408.

Repeated FIX nodes share one realized slot. Rate fixing dates before payment
must enter the simulation grid even when absent from source event rows. Keeping
the rate at its payment-date forward instead of its fixing-date realization is
an error. The dedicated planner deduplicates by `(economic kind, canonical
identity, exact timestamp)`, so PaymentFxSpot and ResetFxFixing may resolve to
different slots at V. This internal key does not change the fixing archive's
canonical identity/timestamp keys. Match and capture history before compilation
or worker submission and freeze conventions once. The existing tree/compiled evaluators receive the same
sealed ObservationPlan and sample definitions; no financial logic belongs in a
simulation observer hook. Settled XCCY PAYS rows do not require global fixings
that cannot affect outstanding cashflows.

## 5. Product and capability matrix

| Product/feature | First complete selected scope | Explicit limit |
|---|---|---|
| Both floating legs | Canonical 3M/6M LCH, exact existing geometry; both spread legs and receive/pay directions | No inferred custom index, 12M, overnight compounding, arbitrary stubs or rescheduling |
| Principal | Existing initial/final exchange switches and signs, foreign SPOT on actual exchange date | No arbitrary amortization; never add an omitted exchange |
| FIXED | Original N and imported foreign/domestic ratio | No extra strike/notional market role |
| RESETTABLE | Period zero retains source N; later domestic `Nf*S(resetFixing)`, historical/at-V rules above | Reset lag allowed only as represented by exact source dates; no change to direction |
| MARK_TO_MARKET | RESETTABLE coupons/final principal plus **source-signed new-minus-old** domestic payments on effective dates | Do not replace the source's delta convention with a familiar native market convention |
| Different frequencies/payment lag | Both legs retain their actual source dates; include rate/reset/discount covariance | Payment at or after accrual end; earlier-payment products outside the initial capability |
| Started trades | Outstanding coupon/principal/MTM dependency closure; historical-only projection routes may be absent | All-expired Price can be zero; par fails its annuity floor rather than returning fabricated zero |
| Measures | Price, signed BasisSpread, expected A/B and their native first partials | No new vol quotation, PVAtQuote, callable/nonlinear payoff or quote-risk promise |
| Market/model | Domestic collateral, explicit nonzero/zero deterministic basis; two one-factor rates, lognormal FX, fixed R | No switching/third collateral, stochastic basis, extra rate factors, joint or SLV |
| Execution | Analytic Gaussian fast and tree/compiled MC of the same instrument; hard smooth-free arithmetic | No generic arbitrary-script or fallback-forward executor |

All three notional modes are the complete first target because their finite
Gaussian-moment closure is the same. Implementing fixed notionals first is an
engineering checkpoint, not completion of reset/MTM promises. If cost forces a
partial release, explicitly expose FIXED-only, with reset/MTM and any unverified
lag cases still `MCCrossCurrencyUnsupported`; keep the overall XCCY MC work open.
Deterministic-rates FX-only simulation is a useful zero-rate-vol control, not
the selected general production model.

### Why native deterministic pricing is not the stochastic fast oracle

Native `XccyContractValues` uses initial forwards for future fixings and notionals.
In general `E[D(P)*S(R)*L(u)]` is not
`Pd(0,P)*F(0,R)*L(0;S,T)`: reset/fixing/payment covariance changes the price.
Even fixed notionals with P different from the index end T can have rate
convexity. The correct guarantee has three explicitly different comparisons:

1. **Zero dynamics, common support only:** native deterministic, current
   same-source deterministic fast, independent cashflow oracle, new Gaussian
   fast and MC agree where **all** admit the same product/history/market/measure.
   In particular both remaining leg spread annuities must be positive for the
   legacy calls, even for Price; their other admission checks still apply.
2. **Nonzero dynamics, eligible FIXED/no-lag coupons:** P=T, canonical fixing
   u<=S, deterministic projection multipliers, plus deterministic or already
   known notionals. Rate T-forward martingales and converted foreign T-forward
   valuation give native/analytic/MC price agreement. This is a useful genuine
   three-way nonzero-volatility case within the same legacy common support.
3. **General nonzero reset/MTM/payment-lag domain:** new same-source Gaussian
   fast agrees with an independent Gaussian cashflow oracle and script MC.
   Report `convexityAdjustment=GaussianFast-NativeDeterministic` as a comparison,
   not a numerical failure, and only publish that comparison where native is
   supported. Native is a sign/date/zero-vol oracle in its own admitted domain.

This restriction follows `dal-cpp/dal/curve/xccypricing.cpp:419-420`:
`XccyContractValues` requires **both** positive remaining spread annuities before
returning, including for Price. The old script fast provider calls it at
`dal-cpp/dal/curve/xccyscriptprovider.cpp:145`; a native assertion remains a native
failure, while this provider owns the wrapped `CalibrationNumericalFailure`
at `/market`. Do not widen either gate or pretend a refusal is a price.

For the delayed-payment fixture moved to V=2027-11-05, domestic coupons and
principal are settled but the foreign 2027-11-08 coupon remains. New Gaussian
Price includes that coupon using its exact historical rate fixing, active
payment FX and discounting. Its domestic-spread A is zero, so domestic par
fails `InvalidCalibrationAnnuity`; foreign-spread par is available only if its
declared sign/floor (and MC denominator confidence gate) passes. Old native/fast
reject **both** views because the domestic annuity is zero. With V after the last
payment, new A=B=Price=0, par fails its floor, and the legacy paths still reject.
These cases use independent outstanding-cashflow oracle -> new moment fast ->
same-instrument script MC, including zero dynamics. They preserve outstanding
history dependencies and require no settled-leg history or unused projection
route. Their Price capability does not imply par or legacy-native capability.

For case 3, a deterministic basis calibration followed by stochastic MC need
not remain at par. Full same-model **calibration -> MC** closure therefore also
requires staged basis fitting with the analytic Gaussian evaluator at the
supplied fixed dynamics. Reuse DAL-409's observations, scales, curve layout and
solver; only select the model-matched residual/Jacobian provider explicitly.
Refresh basis -> effective foreign discount -> Phi_f and projection multipliers
for every candidate. Never move K, inject a trade-specific drift, or adjust
the frozen input basis solely to force an MC validation price to par.

The current deterministic staged result may be snapshotted for stochastic
revaluation with its convexity discrepancy clearly reported. It cannot be
relabeled a stochastic-model calibration result. The case-3 staged extension
is necessary follow-up work, not an optional hidden replacement of the original
overall promise. No new optimizer or joint volatility calibration is required.

## 6. Same-source analytic evaluator and independent oracle

After complete AST/date matching, lower the **discounted** source cashflows
to a sum `sum_j w_j*exp(k_j + ell_j^T U)`. U stacks the necessary Gaussian
states/integrals at fixing and payment dates. A canonical Libor is exponential
affine minus a constant; FX is exponential affine; reset notionals multiply FX;
MTM deltas subtract two such terms. These operations preserve finite closure.
Historical/supplied observations are passive constants, and K remains affine.

```text
E[exp(k+ell^T U)] = exp(k + ell^T mean(U) + .5*ell^T Cov(U)*ell)
Price(K) = B + K*A
ParBasisSpread = -B/A
```

The analytic evaluator derives w/k/ell from the same admitted bound source;
no native product is stored alongside it. It reuses source notional **ratios**
exactly, including their existing double normalization limit. It does not
silently improve them with independent original notionals. Price derivatives
can use templated AD through these moments; quotient derivatives use expected
A/B. A fallback FD must be explicitly reported, never labeled AAD.

The independent validator must not call `XccyContractValues`, the new moment
lowerer, model transition builder or its Cholesky. For the fixed schedule
fixtures, explicitly list each signed domestic/foreign cashflow, its amount
formula, fixing/reset/payment dates and weight. Compute joint moments with
independent Brownian kernels:

```text
K_xd(t,u) = sigma_d(u)*exp(-a_d*(t-u)) in driver d
K_xf(t,u) = sigma_f(u)*exp(-a_f*(t-u)) in driver f
K_Id(t,u) = sigma_d(u)*H_d(u,t) in driver d
K_If(t,u) = sigma_f(u)*H_f(u,t) in driver f
K_Z (t,u) = sigma_X(u) in driver X
Cov(Y_a(t),Y_b(s)) = integral_0^min(t,s) K_a(t,u)*R*K_b(s,u)^T du
mean(x_f(t)) = -integral_0^t exp(-a_f*(t-u))*rho_fX*sigma_f(u)*sigma_X(u) du
mean(I_f(t)) = -integral_0^t H_f(u,t)*rho_fX*sigma_f(u)*sigma_X(u) du
```

Use independent high-precision scalar integration split at every volatility
knot (refine it to bound its numerical indicator) and the Gaussian exponential
identity. Separately inspect exported source against this cashflow table, so a
shared matcher/exporter error cannot pass both references. Keep pathwise small
fixtures that compare the script receiver against explicit realized cashflows
before comparing expectations. Section 8's direct deterministic gate compares
the two references before MC; neither MC agreement nor an integration indicator
alone establishes their agreement. The research's one-payment FX-forward example
is an additional diagnostic/oracle; do not broaden the first complete swap
matcher to arbitrary FX scripts merely to run it.

## 7. Proposed API and binding contract

**All names in this section are proposed, not callable on the inspected master.**
They are additive; the immutable instrument/quote/observation schemas and old
native APIs remain unchanged. Required inputs precede defaults; configuration
structures replace long positional constructors.

Core types, under `dal-cpp/dal/model/` and `dal-cpp/dal/script/`:

```cpp
struct XccyGaussianMarketSpec_;     // V, pair, collateral, S0, blocks, basis, route
struct XccyGaussianDynamicsSpec_;   // a_d/a_f, bucket end dates and 3 vol vectors, R
struct XccyGaussianModelSpec_ { XccyGaussianMarketSpec_ market_; XccyGaussianDynamicsSpec_ dynamics_; };
struct XccyGaussianModelData_;      // immutable ModelData_, owned normalized snapshots
struct XccyGaussianExecution_;      // optional snapshot, omission has section-4 meaning
struct PreparedXccyGaussian_;      // owning source, descriptor, requests, identities

PreparedXccyGaussian_ PrepareXccyGaussianInstrument(
    const Handle_<CalibrationInstrument_>& instrument, const String_& measureId,
    const Handle_<XccyGaussianModelData_>& model, const XccyGaussianExecution_& execution,
    const CalibrationEvaluationOptions_& options = {});
// Evaluate analytic moments or ordinary prepared script MC against this one instrument.
```

Market spec has required exact `valuationTime_`, `pair_`, `collateralCurrency_`,
`fxSpot_`, `domesticBlock_`, `foreignBlock_`, `basis_`, `discountRoute_`.
Model construction copies/normalizes required routes into immutable snapshots
and checks collateral==domestic and both source currencies. It never borrows
a candidate solver's mutable pointers. Dynamics has required `domesticA_`,
`foreignA_`, strictly increasing `bucketEndDates_` after V, three equally sized
volatility vectors, and named R. Vector element i applies from the previous end
(V for i=0) through end i; final end must cover the requested horizon, with no
implicit last-bucket extrapolation. Constants still use one explicit bucket.

Public facade and cross-language names:

| C++ | Python / Excel exported logical name |
|---|---|
| `NewXccyGaussianModel(name, spec)` -> typed model handle | `XccyGaussianModel_New(name, spec)` |
| `ValueXccyInstrumentByMonteCarlo(instrument, model, request)` -> existing owning `CalibrationMonteCarloResult_` | `XccyInstrument_ValueByMonteCarlo(instrument, model, request)` |
| `XccyMonteCarloRequest_` | typed settings handle; required measure, XCCY phase and existing typed sampling, optional snapshot, derivative labels/method and simulation defaults |
| Existing staged instrument spec + explicit model-evaluation settings | Additive DAL-409 integration after its merge; preserve deterministic default and frozen solver/observation contracts |

Xccy request intentionally has no generic today policy or independent valuation
date: V/pair/route/numeraire come from the model, checked against the instrument.
Snapshot supplied/empty/omitted remains expressible. Default derivative method
is None; AAD requires explicit ordered native labels; FD is explicit. Simulation
defaults are Precise normals, no bridge, compiled as existing default, smoothing
zero. Nonzero smoothing fails. Standalone MC is explicit and needs no solver
opt-in; using MC *inside calibration* still requires explicit opt-in and budgets.
The selected staged implementation uses analytic residuals, so does not add an
MC optimizer. Existing generic MC calls retain current admission unless an
explicit XCCY-aware dispatch is subsequently added with equivalent tests.

### Explicit XCCY sampling phase

`XccyMonteCarloRequest_` has required `measureId_`, `phase_` and `sampling_`,
followed by optional fields. `phase_` is the additive domain enum
`XccyMonteCarloPhase_`, with canonical tokens `Fit`, `Validation`, `HeldOut`.
There is **no valid default**: an unset C++ value, omitted Python argument or
missing/blank Excel field fails before preparation/workers. Generate the enum
through Machinist with `_NOT_SET` as the invalid C++ default; its three
`Value_` alternatives use the canonical tokens above. Do not add a field or
token to the existing shared `CalibrationMonteCarloSampling_`. Existing
mode/count/seed/version constraints remain; this domain uses
`dal.mrg32-normal-paths/1`, Precise and no bridge.

The domain range layout is `dal.xccy-gaussian-phase-ranges/1`. Its immutable
reserved table is in section 8. There is no public offset/range override in /1:
phase selects the reservation start, requested batch count times paths per
batch selects its prefix, and each batch is the next contiguous subrange.
Analytic staged fitting consumes no paths but retains the Fit reservation;
an explicit standalone MC diagnostic may select Fit. Every result reports the
phase, range-layout version, actual path ranges and scalar-coordinate ranges.

Checked preparation validates positive integer counts (at least two batches
of two paths), their product, phase capacity, and every add/multiply for path
endpoints and `p*D` before allocation/seeking. Require endpoints to fit the
actual RNG/index types as well as size_t; overflow is
`InvalidCalibrationPlan` at `/sampling`, capacity exhaustion is
`CalibrationBudgetExceeded` at `/sampling`. Invalid/unset phase is
`InvalidCalibrationPlan` at `/phase`, with actual token and accepted tokens.
Prepared-range or mapping identity mismatch is `CalibrationContextMismatch`
at `/sampling/ranges` or `/sampling/mapping`; overlapping independent ranges
are `InvalidCalibrationPlan` at `/sampling/ranges`. No truncation, wrap, clamping,
hidden seed change or phase inference from call order/global state is allowed.
For an all-expired deterministic view with D=0, retain the requested phase and
reservation but consume/report zero random coordinates; do not divide by D or
claim independent draws from identical empty coordinate ranges.

Preparation owns a checked reservation ledger. Its sampling identity includes
phase/layout, counts, seed, generator/stream/normal versions, D, actual ranges
and the complete mapping hash. The enclosing prepared context also binds the
source/measure/history/model identities. Validation and HeldOut with identical
16x16384/seed1024 sampling therefore select different ranges and identities;
repeating one phase is deterministic. A/B, tree/compiled and qualified AAD/CRN FD
reuse the same range identity deliberately; independent phases may not overlap.
Numeric model refresh may retain a compatible mapping for CRN while recording
a new model context. A new grid/D requires a new checked mapping, not a reused
seek offset. Across a basket require the section-8 union mapping or an explicit
prepared disjoint coordinate allocation; if no such allocator is implemented,
reject mixed mappings. No second freeform offset input is introduced.

Python uses required keyword `phase="Validation"` and Excel's typed request
factory uses required `phase` beside `measure_id` and `sampling`. C++ unset,
unknown phase tokens (including generated enum parse failures), and semantic
range errors use the owning codes/paths above. Python preserves these through
the existing `CalibrationError` translator; omission of its required argument
is a binding `TypeError` naming `phase`, not an invented C++ diagnostic. Excel
maps missing/blank or invalid phase to `InvalidCalibrationPlan` at `/phase`,
adds the factory/argument name (and row/column for table errors), and preserves
range-error codes/paths. Phase is not also accepted in optional two-column
options; reject that duplicate input channel as an unknown option.
Phase/range identity must be exposed through a versioned XCCY result/report
extension; do not silently add columns to existing /1 tables.

Typical future Python use, assuming a previously imported/fitted instrument,
market spec and dynamics; this is an API sketch, not executed code:

```python
model = dal.XccyGaussianModel_New(
    "usd-eur", dal.XccyGaussianModelSpec_(market=market, dynamics=dynamics))
request = dal.XccyMonteCarloRequest_(
    measure_id="Price", phase="Validation", sampling=validation_sampling,
    fixings=frozen_fixings)
value = dal.XccyInstrument_ValueByMonteCarlo(instrument, model, request)
price, standard_error = value.mean, value.standard_error
```

Python constructors use named required fields and keyword-only optional settings;
copy/validate inputs before releasing the GIL, then execute without Python
callbacks, preserving owned exceptions and results. Excel uses typed model-spec,
dynamics, request and result handles; required curve handles stay out of freeform
settings. Dynamics tables have explicit `end_date,sigma_domestic,sigma_foreign,
sigma_fx` columns and named correlation rows/columns. Two-column options accept
only documented keys, reject normalized-key collisions, and preserve omitted vs
explicit-empty snapshot handles. No positional currency/vol arrays with implicit
ordering, new Product archive fields, or fake default FX/basis market values.

Parameter labels are stable and qualified, e.g. `fxspot:EUR/USD`,
`logdf:USD:OIS:<date>`, `logdf:EUR:OIS:<date>`,
`logdf:USD:BASIS:EUR/USD:<date>`, `logdf:EUR:6M:<date>`,
`sigma:EUR:<bucketEnd>`, `a:USD`. First release validates raw partials in spot,
domestic/foreign discount/projection log-DF nodes, basis log-DF nodes and interior
rate/FX sigmas. R/a are fixed dynamics, value-tested but their requested AAD
partials may fail `DerivativeUnavailable` initially; no fake zeros. Native curve
coordinates require the complete snapshot Jacobian chain, especially basis
through Pc_f/Phi_f/q. No claim that snapshot log-DF partials are calibrated quote
risk. Fixed geometry reuses preparation; new candidate numeric model content
gets a new evaluation context identity and refreshed moments, never stale caches.

Cache compatibility fingerprints include source/definition/binding/event dates,
measure, compiler rule, exact policy and supplied marker/snapshot values,
convention state, curve route/layout/content, dynamics/R, ACT/365F, numeraire
`dal.domestic-collateral-bank/1`, transition/mapping version and execution settings.
Changing K/N uses immutable rebind; changing payment/source/history/model creates
a checked new plan or an explicitly supported numeric refresh. Model and mapping
identities must be reported separately; economic Brownian registry has size 3,
exact innovation width is 5. The old GSR mapping identity is never reused.
Phase/range-layout and actual range identities additionally bind the sampling
execution as specified above; they are distinct from the economic model hash.
If report tables need additive columns, evolve the report schema explicitly
under DAL-398 rather than silently changing /1 columns or execution meanings.

Errors use existing owning CalibrationError/report conventions. Each includes
instrument/measure, stage, offending field, actual/required value, index/time and
source row/statement when applicable:

| Condition | Error contract |
|---|---|
| No matching model, unsupported notional/payoff/collateral/smile domain | `MCCrossCurrencyUnsupported` before workers; name the unsupported feature |
| Currency/route/date/snapshot/model compatibility mismatch | `CalibrationContextMismatch` at precise `/model/market/...` or `/context/...` |
| Bad vol/R/bucket dimensions/coverage | `InvalidCalibrationPlan`, e.g. `/model/dynamics/correlation`, expected named 3x3 PD matrix, actual minimum eigenvalue |
| Missing exact history / intraday / generic override | `MissingFixing` / `UnsupportedCalibrationFixingTime` / `InvalidCalibrationFixingPolicy` |
| Ill-conditioned A or wrong sign | `InvalidCalibrationAnnuity`, expected sign/floor and measured A/SE |
| Nonfinite A/B/path/transition | `CalibrationNumericalFailure`, coefficient/step/date/cause retained, no fabricated candidate |
| Unsupported derivative/rank boundary | `DerivativeUnavailable` with label/domain; value capability remains separate |
| Path budget insufficient | Existing budget/numerical-validation failure flags; preserve completed estimates/report, no automatic tolerance relaxation |

## 8. Fixed-input acceptance design

These are planned tests for later tasks, not tests run by DAL-410. Freeze inputs,
oracle code, mapping version, budgets and thresholds before the first MC result.
Use small pathwise/negative controls first; do not repeat unrelated product
matrices. Validate new numerical scope independently rather than inheriting GSR
or deterministic XCCY approvals.

### Fixtures and financial invariants

Base fixture is the admitted DAL-408 schedule in `test_xccy_script.cpp`:
V=2026-10-21 00:00, start=2027-05-04, maturity=2027-11-04, domestic USD 3M,
foreign EUR 6M, domestic N=110, foreign N=100, K=.001. Domestic coupon ends are
2027-08-04 and 2027-11-04; delayed foreign payment is 2027-11-08. Use the
frozen canonical fixing calendars/ACT360 and fixing lag 2, not unadjusted
guessed fixing dates. Freeze the resolved date table and compare it with the
independent calendar/index reference before stochastic comparisons. A no-lag
variant pays each coupon at its index end. Reset FX lag 0 at midnight; preserve
the imported signs and ratio. Both spread legs and receive/pay directions
and all three notional modes are coverage dimensions, not new independent
random streams per scalar comparison.

Flat normalized curves are Pd=exp(-.04*t), Pf=exp(-.02*t),
B=exp(-.003*t), domestic 3M projection exp(-.045*t), foreign 6M projection
exp(-.026*t), S0=1.10. A control makes projection=own OIS. Dynamics are
a_d=.03, a_f=.07, sigma_d=.01, sigma_f=.009, sigma_X=.12 with

```text
R = [[1, .30, -.20], [.30, 1, -.25], [-.20, -.25, 1]]
```

One explicit bucket ends V+1095 calendar days. A second scenario adds boundaries
at V+180 and V+365 with rate/FX sigmas respectively
(.008,.006,.10), (.012,.011,.14), (.010,.009,.12); same a/R, final end V+1095.
Add nonflat logDF nodes at V, V+180, V+365, V+730, V+1095 for discount,
projection and basis: at each node use `logDF=-r*t-.002*t*t` for Pd,
`-.02*t-.001*t*t` for Pf, `-.003*t+.0002*t*t` for B,
`-.045*t-.0015*t*t` and `-.026*t-.0008*t*t` for projections; interpolate logDF
linearly between these explicit nodes. This catches sign and interpolation
assumptions that flat curves cannot expose.

Required cases:

- FX unit payout and domestic unit payout at V+365/V+730 calendar days verify
  all section-3 discounted martingales. Cross-check a Qf independent valuation. Flip each
  correlation sign and basis sign separately; inspect conditional FX forwards.
- Zero all sigmas: every path and analytic coefficient equals the independent
  deterministic cashflow table. Compare native and existing fast only in
  section 5's common support (Price absolute 1e-10, spread 1e-12, raw partials
  1e-8 at ordinary fixture scale). One-leg-settled/all-expired Price and par
  refusal use the independent oracle/new-fast/MC chain and separately assert
  legacy refusal. Do not extend these tolerances to ill-conditioned ratios.
- Nonzero FIXED/no-lag: three-way native/analytic/MC comparison, at multiple
  correlations and nonzero basis. General lag/reset/MTM: independent moments vs
  analytic vs script MC, with native convexity discrepancy separately visible.
- Started valuation after the first coupon/reset: list only outstanding
  dependencies, including previous notional for an unpaid MTM delta. Strict-past
  missing, t=V supplied, t=V empty, t=V omitted with conflicting global same-time,
  future snapshot ignored, and payment-SPOT vs reset-FIX distinction all tested.
  Change globals after Prepare and between CRN bumps; results must stay frozen.
  Include V=2027-11-05 and a V after 2027-11-08, with Price/par and both spread
  legs distinguished as in section 5; do not demand an unavailable native value.
- Reverse orientation doubles as a sign oracle; toggling either principal,
  moving a payment, mutating a reset FX identity or rebinding K/N must change
  the one source and all applicable backends, or fail complete matching.
- Nonzero correlation/FX-only limits, a->0, zero basis, zero sigmas and covariance
  rank limits; invalid PD/currency/coverage/custom/intraday inputs fail before
  execution. Direct FX reciprocal is checked pathwise, not by reciprocal means.
- Small positive variance with cross terms: use section 3's sigma_d=1e-8, h=1
  control alongside ordinary/zero-vol limits. Independent high-precision kernels
  must agree with C and L*L^T entrywise after normalization by
  `sqrt(Ckernel[i,i]*Ckernel[j,j])`, within 1e-12. Exact kernel-zero rows must
  remain zero; do not divide them by zero or use the largest variance as a floor.
  Both the positive xd pivot and nonzero xd/Z cross term must survive. A mutant
  that zeroes positive pivots below global tau must fail reconstruction or
  admission, not redefine the fixture as unsupported. Keep all five slots;
  interior small-sigma AD/FD remains qualified where perturbations stay positive.
- Same-model staged synthetic calibration: generate three maturities' quotes
  from independent moments at frozen dynamics and a three-free-node log-basis
  target at 2027-11-08, 2028-05-08 and 2028-11-08, anchored at V, with target
  logB=-.003*t at each free node; initial basis differs by +.001 in each free
  logDF. No other basis nodes are free. Use exactly the
  same instruments/observations in analytic fit and held-out MC, preserve
  EXACT/APPROXIMATE scaled norms and native row axes, then compare recovered
  basis, Price/par and raw basis partials. Report rank; do not hide an
  underdetermined fit or compare MC against deterministic par by decree.

The staged fixture must freeze its three admitted resolved maturity schedules
(2027-11-04, 2028-05-04, 2028-11-06, with full canonical periods/no stubs) and
actual basis knot/payment coverage before use. If a named maturity fails
canonical matching, record and replace the invalid fixture **before** observing
fit/MC results; do not weaken geometry admission. This is fixture qualification,
not a financial open question or permission to move acceptance thresholds.

### Direct deterministic reference gate

Before any MC comparison, compare same-source GaussianFast with the independent
cashflow/Brownian-kernel oracle for **A and B separately**, Price at the source
K, eligible par, and the selected native partials below. This applies to both
spread legs, directions and all three modes, zero/nonzero dynamics and admitted
history/lag cases. It does not require legacy pricing outside common support.
For the ordinary N=110 USD / Nf=100 EUR fixtures, freeze these absolute budgets:

| Quantity X | Unit                   | Tdet_X | epsilonDet_X |
|------------|------------------------|--------|--------------|
| B, Price   | USD                    | 1e-10  | 1e-11        |
| A          | USD per decimal spread | 1e-10  | 1e-11        |
| par        | Decimal spread         | 1e-12  | 1e-13        |

These are fixed fixture tolerances, not a general notional-scaling rule.
For each quantity retain both computed values and independently measurable
numeric indicators in that quantity's unit:

- `eOracle_X`: independently integrate each Brownian kernel at 45, 65, 85
  decimal digits, with per-entry quadrature targets 1e-30, 1e-45, 1e-60 times
  `max(1,abs(entry))` in that entry's native units, respectively. Bisect each
  knot interval at each refinement. Use four times the larger successive
  final-X difference. Record precision, targets, subdivision and all three values. Oracle
  cashflow formulas remain independent of the exporter/lowerer/model/factorizer.
- `eFastNumeric_X`: evaluate the fast route's source-derived terms and its own
  closed-form kernels in wider arithmetic (50 then 80 digits). Use four times
  the larger of double-vs-50 and 50-vs-80 final-X differences. This diagnostic
  may reuse its lowering; it must not replace the independent cashflow oracle
  or use the oracle difference as its numeric allowance. Cancellation belongs
  in this indicator, not a larger MC tolerance. Wider replay/differentiation is
  validation tooling for the fast algebra, not a public multiprecision API.

Require separately `eOracle_X <= Tdet_X/4`,
`eFastNumeric_X <= Tdet_X/4`, and

```text
abs(Xfast-Xoracle) <= eOracle_X + eFastNumeric_X + epsilonDet_X <= Tdet_X
```

These refinement indicators are observed numerical controls, not rigorous
probability/error bounds. Failure to obtain stable indicators fails the gate;
neither missing indicators nor an implementation without the diagnostic earns
a pass. Freeze the oracle code, refinement policy and thresholds before running
the implementation. Price must also equal B+K*A within its deterministic
budget. Par uses -B/A with the same sign/floor; if either reference fails the
annuity condition, compare that refusal and A/B/Price, not a fabricated ratio.
An ill-conditioned par cannot borrow a looser gate from its Price budget.

For each selected interior spot, discount/projection/basis logDF node and
rate/FX-sigma coordinate, use section 8's parameterScale in its native unit.
For X in A, B, Price and eligible par set
`Tdet_gX=Tdet_X/parameterScale` and
`epsilonDet_gX=epsilonDet_X/parameterScale`, with units X per native parameter.
Compare analytic fast AD directly with independent high-precision oracle FD at
h, h/2, h/4 (`h=1e-3*parameterScale`); its indicator includes oracle precision
refinement and four times the larger successive FD difference. The fast
gradient numeric indicator compares its AD at double/50/80 digits as above.
If an interior coordinate's h crosses a domain bound, first halve h until
both perturbations are admissible, recording that deterministic rule and h;
keep parameterScale and thresholds fixed. Small positive sigma is not a cutoff.
Each gradient indicator must be <=Tdet_gX/4 and the same direct inequality
holds for gradients. Apply the complete snapshot Jacobian chain. At zero vol,
genuine rank change, bounds or a failed annuity gate, qualify one-sided FD or
report DerivativeUnavailable; do not assert interior two-sided equivalence.
R/a and calibrated quote-risk are outside this selected partial qualification.

The required **payment-lag-only negative control** preserves source dates,
discounting, quanto and all other terms, but omits only the lag covariance in
the candidate expectation. For the fixed foreign coupon, let q be its log
projection-bond ratio at fixing and w(t)=log(S(t)/Nd(t)); replace only
`Cov(q,w(P))` by `Cov(q,w(T))`, where P is actual payment and T the index end.
All means, variances and other cross terms stay intact. It must fail this direct
gate on the base foreign coupon/Price (and B at K=0). The independent design
calculation reports about -2.3401625053e-5 USD for the 100 EUR coupon's lag
convexity, far above 1e-10 USD but below the MC .0025 USD budget. V1 must
qualify this mutant against the
actual normalized source fixture; that design calculation is not a product
test. Passing two separate MC comparisons cannot replace this assertion.

### Sampling, parity, precision and derivatives

Use the existing MRG32 pseudorandom/Precise normal stream, seed 1024 and its
versioned stream implementation. The request's explicit phase selects the
maximum reserved range in **normal-vector path ordinals** before any run:

| Phase token | Reserved half-open range | Initial prefix    | Initial batches x paths |
|-------------|--------------------------|-------------------|-------------------------|
| Fit         | [0,262144)               | [0,65536)         | 16 x 4096               |
| Validation  | [262144,1310720)         | [262144,524288)   | 16 x 16384              |
| HeldOut     | [1310720,2359296)        | [1310720,1572864) | 16 x 16384              |

Analytic fit consumes no random paths but reserves its range. For a mapping of
dimension D, path p occupies coordinates [pD,(p+1)D). Same instrument/mapping
uses the same normals for A/B, tree/compiled, AAD and CRN FD; never seek by just
path ordinal with a different D. Across a basket use one prepared union grid
and five-slot layout, or allocate disjoint **coordinate** ranges per mapping;
reject mixed-D reuse if neither is implemented. Report all ranges and hashes.
First run deterministic selector tests: identical sampling with Validation and
HeldOut must yield their different prefixes; repeats and reversed call order
must yield identical phase-specific traces. Assert checked refusal for unknown
phase, reservation overrun, count/coordinate overflow, overlapping independent
ranges, and stale mapping/D. Permit only declared CRN reuse. These assertions
operate before RNG draws and apply equally through C++, Python and Excel.

Predeclare refinement with twice then four times the per-batch paths, maximum
2,359,296 paths per mapping across all three reserved phases. Enlarging fit
cannot invade validation. Keep each phase's samples as prefixes of its reserved
range and publish the actual offsets. Do not adapt fit to validation draws.
These are candidate budgets, not a claim they already achieve precision.

At N=110 freeze `sPrice=.01 USD`, `sSpread=1e-4`, k=3, nu=nuG=.25,
epsilonPrice=1e-10, epsilonSpread=1e-12. Only after the direct deterministic gate
passes, set eFast to the relevant reference's measured **value** numeric indicator
above, additionally <=.01*s_measure. Gradient comparisons use their separate
eFD indicator in gradient units below. Neither is the fast-vs-oracle discrepancy
or a model-error allowance. For each MC comparison to analytic or independent
reference enforce separately:

```text
SE = sampleSD(equal-sized batch means)/sqrt(16)
parity:    abs(meanMC-reference) <= 3*SE_delta + eFast + epsilon
precision: 3*SE_delta + eFast <= .25*s_measure
combined:  abs(meanMC-reference) + 3*SE_delta + eFast <= .25*s_measure
```

Use hypot of genuinely independent estimator SEs, or paired-difference SE for
an explicitly coupled comparison. Analytic reference SE is zero, but eFast and
roundoff floor are not a probability bound. Budget exhaustion fails numerical
validation even when parity alone passes. Path doubling checks sampling/ratio
bias separately from exact time-grid split/merge moment checks. Do not increase
s_measure or epsilon after seeing a failure.

MC par is `-mean(B)/mean(A)`, never mean(pathwise par). Compute its SE from the
full paired (B,A) batch covariance and require signed A plus
`abs(mean(A))-3*SE_A>A_min`. For par gradients, use quotient derivatives of
overall coefficient means and the full (B,A,dB,dA) covariance, as frozen in
DAL-396 section 8.3; record delta-method status and path-refinement bias. Price
and spread have separate scales and acceptance flags. Report no stable par
when the denominator confidence condition fails.

For supported raw native partials compare independent-oracle FD at h and h/2,
analytic AD, script pathwise AAD, and same-mapping CRN FD. Freeze scales:
spot .01, each logDF node .001, each rate sigma .001, FX sigma .01 in their
native units. Use `h=1e-3*parameterScale`, h/2, and an additional h/4 stability
check if required; eFD cannot exceed .01*GScale, `GScale=s_measure/parameterScale`.
Require gradient parity `abs(gMC-gRef)<=3*SE_g+eFD+1e-8`, gradient precision
`3*SE_g+eFD<=.25*GScale`, and the analogous combined indicator. SE_g comes from
batch gradients, never price dispersion. Check chain rules for source N/K,
spot, projection, discount and basis explicitly. At zero vol, rank changes or
bounds use qualified one-sided FD or DerivativeUnavailable; do not assert a
two-sided AAD pass. Quote-risk, correlation and mean-reversion gradients are
not smuggled into this partial-risk qualification.

Required negative controls remove the quanto drift, change Pf/B to Pf*B, merge
payment SPOT with supplied at-V reset FIX, omit a fixing-grid date, consume an
extra normal vector, and substitute Price SE for gradient SE. The extra-normal
mutant must fail deterministic interval/slot/path scalar-coordinate consumption,
range and mapping-hash trace assertions: unchanged marginal means/SE cannot
reliably detect it. The wrong-SE mutant uses a prequalified batch-gradient
summary whose price and gradient dispersions differ. Each must fail its intended
independent assertion without changing the real implementation. Include the
positive-pivot and payment-lag-only controls specified above.
An insufficient-budget case must retain estimates and fail precision; source
mutation tests must not manually synchronize a second product to help pass.

## 9. Rejected alternatives and compatibility

- **Independent FX GBM / current BS renamed FX:** can fit one forward but lacks
  stochastic rates/quanto and cannot establish reset/lag or collateral identity.
- **Two existing GSR instances plus generic hybrid correlation:** different
  single-currency measure/numeraire assumptions; domestic-only hybrid guards;
  correlated draws alone do not supply foreign drift or integral covariances.
- **Stochastic basis as a fourth factor immediately:** legitimate later extension
  requiring basis volatility/correlation data and additional drift/oracles; not
  necessary for the selected deterministic-basis scope. Never claim it supported.
- **Euler log-FX with exact rate endpoints:** loses joint integrated-rate
  covariance and discounting; sampling SE does not cover its weak bias.
- **Force native forward-substitution price to equal stochastic MC everywhere:**
  hides real convexity with trade-dependent adjustments. Keep genuine differences
  and add model-matched analytic staged fitting instead.
- **A separate native MC swap object or hand-built second script:** can agree
  only because two sets of terms were synchronized; fails source authority.
- **Full multi-currency HJM/LMM, stochastic volatility, SLV or FX smile model:**
  larger parameter/data/validation requirements, outside the bounded current ask.

Legacy native APIs, source/definition/binding archives, existing rule
`dal.xccy-cashflows/1`, deterministic staged defaults and accepted GSR behavior
remain intact. New analytic stochastic capability should use
`dal.xccy-gaussian-moments/1` and MC `dal.xccy-gaussian-script-mc/1`, both gated
by model and product support. AutoFast under an explicit stochastic context
must select the matching moment evaluator or fail; it must not return the old
forward-substitution result. RequireFast and derivative requirements never fall
back silently. Changing existing report schemas requires explicit versioning.

## 10. Executable follow-up plan, estimate and open questions

Coordinator dispatch is required; the following are task proposals, not created
issues or authorization to begin code in this run. Estimates are focused engineer
days including targeted implementation checks, excluding queue/CI/review delays;
they are planning ranges, not measured performance or fixed commitments.

| Task / dependencies | Safe write area and acceptance | Estimate |
|---|---|---|
| M1: frozen market/dynamics and exact model; this decision accepted | New `dal-cpp/dal/model/xccygaussian*`, narrow `model/factory.hpp` case, owning archive registration and model tests. Exact transition/bond/martingale tests, zero-vol/rank, small positive variance plus cross-term reconstruction and a=0; no global hybrid guard change. | 5-8 days |
| M2: XCCY observations and same-source execution; M1 | Dedicated script XCCY planner/provider, minimal shared preparation hooks and tests; all section-4 policies, complete matcher/event authority, tree/compiled pathwise cashflow identity. Coordinate shared files serially with other model/script work. | 3-5 days |
| M3: Gaussian moments and model-matched staged basis; M1, merged DAL-409; execution parity after M2 | Source-derived moment lowerer/evaluator and additive staged evaluation selection; no second solver. A/B/native basis chain, direct deterministic oracle budgets/diagnostics, synthetic fit and convexity disclosure. Native comparisons only in common support. New product files first; coordinator arranges narrow shared adapter window. | 4-6 days |
| M4: MC mapping/statistics/native partials; M2, analytic reference from M3 | Five-slot innovation registry, explicit phase/range layout and checked identity/coordinate ledger, owning results, ratio covariance and selected AAD. Same-sampling different-phase and overflow/overlap/stale-mapping controls. Reuse statistics, not GSR RefreshG/labels or identity. No MC optimizer expansion. | 3-5 days |
| M5: public C++/Python/Excel and examples; M1-M4 contracts stable, DAL-409 merged | New typed model/spec/request with required phase and no offset override, snapshot semantics, owning cross-language phase/range errors and versioned reports, GIL/handle checks, explicit stochastic staged selection, separate public example and necessary current-state docs. No changes to existing core example locations/purpose. | 2-4 days |
| V1: independent numerical acceptance; M2-M4 available, M5 for boundary checks | Independent cashflow/moment/Qf oracle; direct deterministic A/B/Price/par/partial gates before MC, small-positive and lag-only mutants; phase/coordinate trace controls; common-support native checks plus one-leg-settled/all-expired Price/par/refusal cases; fixed-input MC parity+precision+combined gates, budget and historical failures. Record exact sources/dependencies/config/executable identities; no self-oracle acceptance. | 4-6 days |

Total roughly 21-34 engineer days, with M2/M3 able to proceed in separate files
after M1 and final numerical validation dependent on both. M4's shared MC hooks
and M5's public hooks require coordinator ownership windows, not simultaneous
uncontrolled edits. A model-only first checkpoint is not the full deliverable.
Future performance cases should map only changed callers to small model/path,
XCCY view/basket and script MC cases with prepare/evaluate separated; this design
does not request a full benchmark matrix or claim a speedup.

Completion of the original XCCY model gap requires: M1/M2 model/provider,
M3 same-model fast staged fitting, M4 same-instrument MC/partials, necessary M5
language access, and V1's independent numerical evidence for the advertised
matrix. Until then retain unsupported for the unavailable paths; after partial
delivery report capabilities per model/product/measure rather than one blanket
XCCY-success flag. DAL-396 remains open until its actual overall criteria hold.

No blocking financial question remains for the explicitly selected bounded
model: deterministic basis/tenor spreads, domestic collateral, supplied dynamics,
midnight and the source's existing reset/delta signs are decisions here. Whether
users later need stochastic basis, intraday identity, smile or a wider collateral
set is an extension question, not permission to guess their data or delay this
scope. Formal model-market suitability and supplied sigma/R provenance remain
the caller's responsibility; synthetic acceptance cannot claim empirical FX-smile
calibration. Necessary documentation/schema details should be settled in the
scoped follow-up PR before publishing their interfaces, without reopening the
frozen instrument/quote/observation contract.

### Design-level checks performed in DAL-410

Static source/contract review and independent scalar algebra only; no product,
statistical or performance tests were run. With S0=1.1, rd=.04, rf=.02, b=.003,
the 1Y forward is 1.125593193501939, reciprocal .8884204397938881 and discounted
foreign unit 1.081458053098401. R's leading principal minors are 1, .91, .8375.
For a_f=.07, sigma_f=.009, sigma_X=.12, rho_fX=-.25 and T=2,
`Cov(I_f,Z)=-.0005156578689137898` and `mean(I_f)=+.0005156578689137898`.
Their cancellation gives `E[S(T)/Nd(T)]=1.063228655101257`, matching
`1.1*exp(-(.02-.003)*2)`. Removing the foreign quanto drift instead multiplies
that identity by `exp(-Cov(I_f,Z))`, a relative error .0005157908432881214.
These checks confirm sign/measure algebra and qualify a future negative control;
they are not empirical model acceptance or evidence that XCCY MC exists.
