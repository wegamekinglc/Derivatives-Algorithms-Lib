# Cross-Currency Pricing and Calibration

DAL prices fixed-notional, resettable-notional, and mark-to-market (MTM)
cross-currency swaps, and calibrates their basis either after the two currency
markets are known or simultaneously with both currency curve blocks. This note
defines the current pricing, fixing, collateral, and calibration contracts.

## Market and FX-Forward Convention

For a currency pair `(domestic, foreign)`, FX spot $S$ is quoted in domestic
currency per unit of foreign currency. With domestic and foreign discount
factors $P_d(t,T)$ and $P_f(t,T)$ and a domestic-currency basis discount factor
$P_b(t,T)$, the FX forward is

$$
F(t,T)=S\frac{P_f(t,T)}{P_d(t,T)P_b(t,T)}.
$$

The current implementation supports domestic-currency collateral only. The
domestic and foreign discount curves used by an XCCY instrument come from the
collateral selectors on its two rate-index conventions. Projection-enabled
indices route to the matching tenor forward curves; non-projection indices use
their discount curves for forecasting.

## Cashflow and Notional Modes

Let $N_f$ be the constant foreign notional, $N_{d,0}$ the configured domestic
notional, and $X_i$ the observed or forward FX fixing for the reset effective
date of domestic coupon period $i$. DAL creates FX resets from the second
domestic period onward. The three `XccyNotionalMode_` values are:

- `FIXED`: $N_{d,i}=N_{d,0}$ for every domestic period.
- `RESETTABLE`: $N_{d,0}$ applies to the first period and
  $N_{d,i}=N_fX_i$ for $i\ge1$.
- `MARK_TO_MARKET`: notionals follow the same formula as `RESETTABLE`, and each
  reset effective date exchanges the domestic notional change
  $\Delta N_{d,i}=N_{d,i}-N_{d,i-1}$. Under DAL's domestic-leg PV sign
  convention, the adjustment is $-\Delta N_{d,i}P_d(t,T_i)$.

Coupon PVs use the period notional, accrual fraction, projected or observed
rate, and the payment-date discount factor. Foreign coupons and notional
exchanges are converted to domestic PV with $S P_f(t,T)/P_b(t,T)$. Initial
notional exchanges are included only when the leg start date is on or after the
valuation date. Final exchanges are included when maturity is on or after the
valuation date.

A cashflow whose payment date is strictly before the valuation date is omitted.
A cashflow on the valuation date remains in the PV, regardless of the valuation
time. The same date rule applies to MTM notional adjustments: an adjustment is
omitted only when its effective date is strictly before the valuation date.

The model quote is the par spread. If the spread is on the foreign leg,

$$
s=\frac{\mathrm{PV}_d-\mathrm{PV}_f}{A_f};
$$

if it is on the domestic leg,

$$
s=\frac{\mathrm{PV}_f-\mathrm{PV}_d}{A_d}.
$$

Here $A_d$ and $A_f$ are the remaining positive spread annuities in domestic
PV units. Calibration rejects an instrument with no remaining annuity on its
quoted leg.

## Reset and Fixing Timing

`FxResetConvention_` defines a non-negative business-day fixing lag, fixing
holiday calendar, business-day convention, and fixing hour/minute. The reset
for domestic period $i\ge1$ is effective on that period's accrual start; its FX
fixing date is the lagged and adjusted effective date. `FixingIdentity_` gives
the index name and hour/minute for each domestic and foreign floating-rate
fixing. `FxIndexName(pair)` gives the FX fixing index name.

At valuation time $v$:

- a fixing time before $v$ must exist in the supplied snapshot;
- a fixing exactly at $v$ uses the snapshot value when present, otherwise the
  active forward value; and
- a fixing after $v$ uses the active forward value.

Only a historical fixing attached to a non-settled cashflow or reset is
required. Missing required observations fail with the index, timestamp, and
pricing context.

## Immutable Market-Fixing Snapshot

`MarketFixingSnapshot_` is an immutable nested map
`index name -> DateTime_ -> value`. The same snapshot can contain rate and FX
observations and is retained by the calibration result. Supplying a snapshot
makes it authoritative, including when it is explicitly empty. When a snapshot
is omitted, staged and joint XCCY calibration first collect all historical
requests across all instruments and copy those values from the process-wide
fixing store once. Later global mutations cannot change the captured solve.

## Staged Basis Calibration

`CalibrateCrossCurrencyMarket` takes pre-calibrated domestic and foreign
`CurveBlock_` objects and solves only for the basis-curve parameters. For XCCY
instrument $j$ and basis parameters $b$,

$$
r_j(b)=s_j^{\text{model}}(b)-s_j^{\text{market}}.
$$

The basis curve is piecewise-constant forward in this path. Exact mode solves
the tolerance-scaled system and may retain the effective inverse Jacobian;
approximate mode minimizes the fit subject to smoothness regularization. The
result contains the basis-bearing `CrossCurrencyMarket_`, FX forwards at the
basis knots, fit diagnostics, and optional matrices.

## Joint Domestic, Foreign, and Basis Calibration

`CalibrateJointXccyMarket` solves one residual system over three ordered groups:

1. every domestic `JointCurveDeclaration_`, in declaration order;
2. every foreign declaration, in declaration order; and
3. the `XccyBasisCurveDeclaration_`.

Within each declaration, parameter columns follow its representation:

| Parameterization | Columns per declaration |
|------------------|-------------------------|
| `PIECEWISE_CONSTANT_FWD` | right-hand forwards in knot order |
| `PIECEWISE_LINEAR_FWD` | left/right forward pair at each knot |
| `LOG_DISCOUNT` | future-knot log discount factors |
| `ZERO_RATE` | future-knot continuously compounded zero rates |

Residual rows use the same group order: domestic instrument groups, foreign
instrument groups, then XCCY quotes. `parameterRanges_` and `residualRanges_`
name every contiguous slice and give its zero-based `offset_` and `size_`.
`jacobianAtSolution_` therefore has shape `total residuals x total parameters`.
The smoother is block diagonal by declaration; cross-block Jacobian entries
come from pricing and curve routing, not regularization.

The result retains both solved currency blocks, the basis curve, FX forwards,
the fixing snapshot, group and joint diagnostics, market/model/residual vectors,
ranges, and optional forward/effective-inverse Jacobian matrices.

## Analytic and Bumped Jacobians

Both staged and joint calibration accept `ANALYTIC` and `BUMPED`. In exact mode,
analytic calibration can populate the forward Jacobian at the solution; either
mode can populate the solver's effective inverse when requested. Approximate
mode does not expose either matrix.

Joint XCCY analytic calibration is fail-fast. Every domestic and foreign
declaration must satisfy the joint curve AAD gates, including `ACT_365F`, a
supported curve representation and templated instrument route, and consistent
discount/forward slot usage. Every XCCY plan mode (`FIXED`, `RESETTABLE`, and
`MARK_TO_MARKET`) is supported when its plan and fixing identities are valid.
If any analytic gate fails, the exception identifies the ineligible currency,
declaration, instrument, or reset. Select `BUMPED` explicitly to run the same
valid residual system without the analytic eligibility requirement.

## End-to-End Example

`dal-cpp/examples/xccy_mtm_calibration/xccy_mtm_calibration.cpp` builds known
domestic, foreign, and basis curves, derives self-consistent quotes, supplies one
immutable snapshot containing historical domestic-rate, foreign-rate, and FX
observations for an already-started MTM swap, and recovers all three parameter
blocks in one call. It prints the convergence residual, Jacobian shape, block
ranges, and parameter-recovery errors.

## See Also

- [Yield-curve construction](yield_curve.md) — curve declarations and single or
  multi-curve calibration.
- [Yield-curve Jacobian](yield_curve_jacobian.md) — Jacobian layouts and
  inverse-Jacobian risk.
- [Underdetermined search](underdetermined_search.md) — exact and approximate
  solver behavior.
