# Black / Bachelier Vanilla Pricing

This note describes the closed-form European option pricers that sit at the bottom of
DAL's vanilla analytics: the **Black** (lognormal) model and the **Bachelier** (normal)
model. The two share the same role in the library — they take a forward, a
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
and when $F \cdot K \le 0$ (either input is zero or the inputs have opposite signs).
The supported lognormal domain is $F>0$ and $K>0$; both-negative inputs are outside
that domain even though their positive product does not trigger the fallback. This
domain restriction belongs to Black only; it does not apply to Bachelier.

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
`OptionType_` dispatch. The normal model is well defined for every real $F$ and $K$,
including zero, negative, and opposite-sign pairs. Its only degenerate branch is
$\nu=0$, which returns intrinsic value.

### Black vs. Bachelier: When to Use Which

The two models are the standard normal/lognormal pair for interest-rate and equity
vanillas:

- **Black** is the legacy default for lognormal-underlying markets (most equity FX, and
  historically LIBOR-rate caps/floors/swaptions). Use it with $F>0$ and $K>0$.
  The kernel returns intrinsic when $F K \le 0$, but both-negative inputs remain
  outside the supported lognormal domain.
- **Bachelier** is the natural choice for normal-underlying markets (most modern rates:
  SOFR/SONIA swaptions, caps/floors after the post-2016 rates moved through zero). Its
  $\nu$ has the units of the underlying (e.g. bp), so the same number is directly
  comparable across strikes and remains usable when forwards or strikes are non-positive.

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
de-annualized implied $\nu$ that re-prices a given market `price`. Both solve the
scalar residual $V(\nu) - \text{price}$, but their coordinates and bracketing
contracts differ.

### Black inversion

Black solves in the log-volatility coordinate $x=\ln\nu$, so
$\nu=\exp(x)$ remains positive. A positive guess becomes $\ln g$; otherwise the
initial coordinate is $-1.5$. `Brent_` accepts either a residual magnitude below
$10^{-10}\max(1,\text{price})$ or a coordinate-bracket width below
$10^{-10}\max(1,|F|)$. The solve has a 30-evaluation budget. A price below the
option's intrinsic value is rejected before iteration.

### Bachelier inversion

Bachelier solves directly in the price-unit volatility $\nu$ on an explicit
finite nonnegative bracket. `BachelierIV` first requires finite forward, strike,
price, guess, represented moneyness $F-K$, and intrinsic value. A price below
intrinsic is rejected; a price exactly at intrinsic returns $\nu=0$.

The lower endpoint is $\nu=0$. The initial positive upper endpoint is

$$
\max\left(0.01,\,|F-K|,\,\text{price},\,g\;\text{when }g>0\right).
$$

If this endpoint does not bracket the root, the implementation doubles it for at
most 1024 expansions while checking the endpoint, option price, and residual for
finiteness. Expansion stops when the residual changes sign or when a finite
bracket cannot be formed; overflow or bracket exhaustion raises a DAL exception.
`BracketedBrent_` then performs at most 100 solve iterations without evaluating a
negative volatility.

The Bachelier volatility-coordinate tolerance is

$$
10^{-10}\max(1,|F-K|,\text{price}),
$$

and the price-residual tolerance is
$10^{-10}\max(1,\text{price})$. These scales depend on represented moneyness and
price, not on the absolute level of $F$ or $K$. Consequently a common shift of
forward and strike leaves the inversion unchanged whenever floating-point
representation preserves $F-K$.

The annualized implied-volatility helpers in `vanilla.hpp`,
`Dal::AAD::BlackScholesIVol(spot, K, prem, T)` and `Dal::AAD::BachelierIVol(...)`, hard-code
the `CALL` type, dispatch to the corresponding `*IV`, and divide by $\sqrt{T}$ to return
the annualized $\sigma$.

## Examples

The closed-form kernels above are the analytical benchmark that the pricing
example programs converge to. The vanilla program prices a European call by
Monte Carlo on a `BSModelData_`-driven `BlackScholes_<T_>` model and by AAD
against an inlined closed-form payoff. The canonical kernel those runs measure
against is `Distribution::BlackOpt`, defined in the header below; the European
Monte Carlo and finite-difference examples call it directly as their benchmark.
See [`dal-cpp/examples/vanilla/`](../../dal-cpp/examples/vanilla/) for a
runnable version; the closed-form kernel is:

```cpp
// from dal-cpp/dal/math/distribution/black.hpp
#include <dal/platform/platform.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;

RegisterAll_::Init();
Global::Dates_::SetEvaluationDate(Date_(2022, 9, 25));

const double spot = 100.0;
const double vol = 0.15;
const double rate = 0.05;
const double div = 0.03;
const double strike = 120.0;
const Date_ maturity(2025, 9, 24);
const double t = (maturity - Global::Dates_::EvaluationDate()) / 365.0;

const double discount = std::exp(-rate * t);
const double fwd = std::exp((rate - div) * t) * spot;
const double volStd = std::sqrt(t) * vol;
// de-annualized kernel: caller supplies forward and sigma * sqrt(T)
const double price = discount * Distribution::BlackOpt(fwd, volStd, strike, OptionType_::Value_::CALL);
```

The `OptionType_::Value_::CALL` argument is the dispatch described in *The Black
(Lognormal) Closed Form*; `PUT` and `STRADDLE` go through the same template. The
European Monte Carlo program reuses this closed form as the convergence target
of a quasi-Monte Carlo sweep on a `BSModelData_` model. See
[`dal-cpp/examples/european_mc/`](../../dal-cpp/examples/european_mc/); its
per-path pricing loop is:

```cpp
// from dal-cpp/examples/european_mc/euorpean_mc.cpp
#include <dal/platform/platform.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/script/event.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;
using namespace Dal::Script;
using Dal::AAD::BlackScholes_;

// Closed-form benchmark (de-annualized inputs), as in euorpean_mc.cpp.
double discounts = std::exp(-rate * t);      // P(0, T), t = time to expiry in years
double fwd       = std::exp((rate - div) * t) * spot;
double volStd    = std::sqrt(t) * vol;
const auto benchmark = discounts * Distribution::BlackOpt(fwd, volStd, strike, OptionType_::Value_::CALL);

Handle_<ModelData_> modelData(new BSModelData_("bsmodel", spot, vol, rate, div));
ScriptProduct_ product(eventDates, events, "call");
int maxNested = product.PreProcess(true, true);

for (int i = 12; i <= 30; ++i) {
    const int numPaths = std::pow(2, i);
    SimResults_ results = MCSimulation<Real_>(product, modelData, numPaths, rsg, false, compiled, maxNested);
    const double calculated = results.aggregated_ / static_cast<double>(numPaths);
    // calculated -> benchmark as numPaths grows
}
```

A finite-difference European pricer exercises the same closed form through
explicit, implicit, and Crank-Nicolson $\theta$-schemes on the Black-Scholes
PDE, and checks the recovered price against `Distribution::BlackOpt`. See
[`dal-cpp/examples/european_fd/`](../../dal-cpp/examples/european_fd/); its PDE
coefficient setup is:

```cpp
// from dal-cpp/examples/european_fd/european_fd.cpp
#include <dal/platform/platform.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/math/pde/pdegrid.hpp>
#include <dal/math/pde/thetascheme.hpp>

using namespace Dal;

const double fwd = std::exp((kRate - kDiv) * kT) * kSpot;
const double volStd = std::sqrt(kT) * kVol;
const auto benchmark = std::exp(-kRate * kT)
                     * Distribution::BlackOpt(fwd, volStd, kStrike, OptionType_::Value_::CALL);

// Black-Scholes PDE coefficients: drift (r-q)S, variance sigma^2 S^2
const Handle_<PDE::ScalarCoeff_> disc(PDE::NewConstCoeff(kRate));
const Handle_<PDE::VectorCoeff_> mu(PDE::NewVectorCoeff([](double s) { return (kRate - kDiv) * s; }));
const Handle_<PDE::MatrixCoeff_> var(PDE::NewMatrixCoeff([](double s) { return kVol * kVol * s * s; }));
PDE::ThetaScheme_ scheme(theta);
scheme.Prepare(dt, grids, *disc, *mu, *var);
```

Additional example programs that price off the Black/Bachelier kernels:

- [`dal-cpp/examples/digital/`](../../dal-cpp/examples/digital/) — a digital
  payoff priced analytically, by finite-difference bumps, and by pathwise AAD.
- [`dal-cpp/examples/uoc/`](../../dal-cpp/examples/uoc/) — an up-and-out call
  priced on a `Dupire_<T_>` local-volatility model fed by a flat vol surface.
- [`dal-cpp/examples/snowball/`](../../dal-cpp/examples/snowball/) — a snowball
  autocallable priced on a `BlackScholes_<T_>` model with scripted monitoring.

## See Also

- [Yield-curve Jacobian](yield_curve_jacobian.md) — how greeks feed calibration risk.
- [Dupire local volatility](dupire.md) — Black-Scholes calls are the input to the IVS
  inversion that recovers the local-volatility surface.
