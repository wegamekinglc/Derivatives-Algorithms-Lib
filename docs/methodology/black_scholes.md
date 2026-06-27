# Black / Bachelier Vanilla Pricing

This note describes the closed-form European option pricers that sit at the bottom of
DAL's vanilla analytics: the **Black** (lognormal) model and the **Bachelier** (normal)
model. The two share the same role in the library — they take a forward, an
*de-annualized* volatility (the standard deviation over the option's life), and a strike,
and return a price, greeks, or an implied volatility — but they differ in the underlying
distribution assumed for the forward at expiry. The load-bearing numerical content lives
in `dal-cpp/dal/math/distribution/black.hpp` (and its `.cpp`), with the annualized-vol
entry points `BlackScholesIVol` / `BachelierIVol` in `dal-cpp/dal/math/analytics/vanilla.hpp`.

## Notation and the Volatility Convention

Throughout, let $F$ be the forward, $K$ the strike, $\sigma$ the (annualized) volatility,
$T$ the time to expiry, and $\Phi$ / $\varphi$ the standard normal CDF / PDF. The library
works in the **de-annualized** parametrization, defining

$$
\nu \;=\; \sigma \sqrt{T} ,
$$

the standard deviation of $\ln(F_T/F)$ under the Black measure (or of $F_T$ under the
Bachelier measure). Every Black/Bachelier routine in `Distribution::` and in the
`DistributionBlack_` / `DistributionBachelier_` classes takes this $\nu$ directly. The
`Dal::AAD::BlackScholes` / `Dal::AAD::Bachelier` helpers in `vanilla.hpp` are the thin
annualized-to-de-annualized adapters: they fold the $\sqrt{T}$ in before dispatching to
`Distribution::BlackOpt` / `Distribution::BachelierOpt`, and the corresponding `*IVol`
helpers divide by $\sqrt{T}$ on the way out. This convention keeps the closed-form
kernels independent of any discounting, day-count, or term-structure concern — the
caller supplies $F$ and $\nu$, the kernel prices.

## The Black (Lognormal) Closed Form

Under the Black measure, $\ln F_T$ is normal with mean $\ln F - \tfrac{1}{2}\nu^{2}$ and
variance $\nu^{2}$. With

$$
d_{-} \;=\; \frac{\ln(F/K)}{\nu} - \frac{\nu}{2}, \qquad d_{+} \;=\; d_{-} + \nu ,
$$

the call and put prices are

$$
\begin{aligned}
C &\;=\; F \, \Phi(d_{+}) \;-\; K \, \Phi(d_{-}), \\
P &\;=\; K \, \Phi(-d_{-}) \;-\; F \, \Phi(-d_{+}).
\end{aligned}
$$

The library evaluates these via `Distribution::BlackOpt(F, \nu, K, type)`. The
`OptionType_` argument selects `CALL`, `PUT`, or `STRADDLE` (aliases `C`, `P`, `V`);
`STRADDLE` is priced as $C + P$, equivalently $F\,(1 - 2\Phi(-d_{+})) + K\,(1 - 2\Phi(d_{-}))$.
Two degenerate cases short-circuit the formula and return the intrinsic
$\max(0, F - K)$ / $\max(0, K - F)$ / $|F - K|$ directly: when $\nu = 0$ (no uncertainty),
and when $F \cdot K \le 0$ (a non-positive forward or strike, where the lognormal model is
undefined). This degenerate branch is shared with the Bachelier pricer.

## The Bachelier (Normal) Closed Form

Under the Bachelier measure, $F_T$ is normal with mean $F$ and variance $\nu^{2}$ (so
$\nu = \sigma\sqrt{T}$ is in *price* units here, not log-units). With

$$
d \;=\; \frac{F - K}{\nu} ,
$$

the prices are

$$
\begin{aligned}
C &\;=\; (F - K)\,\Phi(d) \;+\; \nu\,\varphi(d), \\
P &\;=\; (K - F)\,\Phi(-d) \;+\; \nu\,\varphi(d).
\end{aligned}
$$

These are evaluated by `Distribution::BachelierOpt(F, \nu, K, type)`, with the same
`OptionType_` dispatch and the same zero-vol / non-positive-argument short-circuit as the
Black pricer. Because $d$ is dimensionless and the formula is well defined for any real
$F$ and $K$, the only practical degenerate trigger is $\nu = 0$.

### Black vs. Bachelier: When to Use Which

The two models are the standard normal/lognormal pair for interest-rate and equity
vanillas:

- **Black** is the legacy default for lognormal-underlying markets (most equity FX, and
  historically LIBOR-rate caps/floors/swaptions). It is *undefined* for negative forwards
  or strikes — the degenerate branch returns intrinsic in that regime rather than
  producing a NaN.
- **Bachelier** is the natural choice for normal-underlying markets (most modern rates:
  SOFR/SONIA swaptions, caps/floors after the post-2016 rates moved through zero). Its
  $\nu$ has the units of the underlying (e.g. bp), so the same number is directly
  comparable across strikes — which is why the normal implied vol is the preferred quote
  type in modern rates trading.

The library keeps both kernels side by side behind the shared `DistributionNormalLike_`
base rather than picking one, so that downstream code (e.g. the IVS / Dupire machinery in
`dal-cpp/dal/model/ivs.hpp`) can be parameterized by which distribution it assumes.

## Greeks

The free functions `Distribution::BlackGreeks(F, \nu, K, type)` and
`Distribution::BachelierGreeks(F, \nu, K, type)` return a `Vector_<>` of length two,
$(\Delta, \Lambda)$, where both are sensitivities to the de-annualized inputs:

| Greek       | Black ($\partial/\partial F$, $\partial/\partial \nu$)                | Bachelier ($\partial/\partial F$, $\partial/\partial \nu$)      |
|-------------|----------------------------------------------------------------------|----------------------------------------------------------------|
| Forward delta $\Delta$ | CALL: $\Phi(d_{+})$; PUT: $-\Phi(-d_{+})$; STRADDLE: $\Phi(d_{+}) - \Phi(-d_{+})$ | CALL: $\Phi(d)$; PUT: $-\Phi(-d)$; STRADDLE: $2\Phi(d) - 1$ |
| Vega $\Lambda$ (per unit $\nu$) | CALL/PUT: $F\,\varphi(d_{+})$; STRADDLE: $2 F\,\varphi(d_{+})$     | CALL/PUT: $\varphi(d)$; STRADDLE: $2\,\varphi(d)$             |

Note that the **vega** returned here is $\partial V / \partial \nu$ — i.e. per unit of
*de-annualized* standard deviation, not per unit of annualized vol $\sigma$. The
`DistributionNormalLike_::VolVega` method rescales it: because the distribution stores
$\nu$ in its `vol_` member, `VolVega` returns $\nu \cdot \partial V/\partial \nu$, the
**vega notional** (equivalently $\sigma \cdot \partial V/\partial \sigma$, the same
elasticity-scaled quantity regardless of annualization). This is the form used by
calibration, which minimizes over a *relative* vol perturbation. The full greek set
exposed through
`DistributionNormalLike_::ParameterDerivatives` is:

- `delta` — the forward delta $\Delta$ above.
- `vega` — $\partial V/\partial \nu$ from the greeks vector.
- `volvega` — the rescaled $\nu \cdot \partial V/\partial \nu$.

Asking `ParameterDerivatives` for any other name throws.

## Implied Volatility by Brent Inversion

`Distribution::BlackIV(F, K, type, price, guess)` and
`Distribution::BachelierIV(...)` invert their respective pricers to recover the
de-annualized implied $\nu$ that re-prices a given market `price`. The inversion is a
scalar root-find on the residual $V(\nu) - \text{price}$, solved by the library's
bracketing-aware `Brent_` root finder (`dal-cpp/dal/math/rootfind.hpp`):

- **Bracketing variable.** Black solves for $\ln \nu$ (so the search is over the whole real
  line and $\nu = \exp(x)$ stays positive); Bachelier solves for $\nu$ directly (the
  normal model is well-defined for any real $\nu$, though only $\nu \ge 0$ is meaningful).
  The guess is mapped into the solver's variable via a model-specific transform: a Black
  guess $g > 0$ becomes $\ln g$, otherwise a fixed $-1.5$; a Bachelier guess $g > 0$ is
  used as-is, otherwise it falls back to $-1.5\,F$.
- **Convergence.** Both calls accept the residual as converged when either the residual
  magnitude drops below $10^{-10}\max(1, \text{price})$, or the bracket width drops below
  $10^{-10}\max(1, F)$. The iteration budget is 30 evaluations; exhausting it throws.
- **Intrinsic floor.** A `REQUIRE` enforces $\text{price} \ge \text{intrinsic}$ before the
  search starts. A price below intrinsic has no realizable $\nu \ge 0$ (the pricer is
  monotone increasing in $\nu$ from intrinsic upward), so the inversion would search an
  empty set; the `REQUIRE` fails fast with a "value below intrinsic value" message rather
  than letting Brent hunt forever.

Because the call/put pricers are monotone in $\nu$ for $\nu \ge 0$ and the residual is
continuous, Brent converges from the supplied guess (or the fallback) without needing an
explicit bracket. The annualized implied vol helpers in `vanilla.hpp`,
`Dal::AAD::BlackScholesIVol(spot, K, prem, T)` and `Dal::AAD::BachelierIVol(...)`, hard-code
the `CALL` type, dispatch to the corresponding `*IV`, and divide by $\sqrt{T}$ to return
the annualized $\sigma$.

## See Also

- [Yield-curve Jacobian](yield_curve_jacobian.md) — how greeks feed calibration risk.
- [Dupire local volatility](dupire.md) — Black-Scholes calls are the input to the IVS
  inversion that recovers the local-volatility surface.
