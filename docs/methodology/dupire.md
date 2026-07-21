# Dupire Local Volatility

This note describes the **Dupire local volatility** surface: what it is, how the
library inverts an implied-volatility surface (IVS) to obtain it, and the
grid-construction conventions used when calibrating a discrete local-volatility
matrix. The IVS inversion and the calibration grid are the load-bearing
numerical content of `dal-cpp/dal/model/ivs.hpp` (`IVS_::LocalVol`) and
`dal-cpp/dal/model/dupire.hpp` (`DupireCalibMaturity`, `DupireCalib`).

## From Implied Volatility to Local Volatility

Given a continuum of discounted European spot-call prices $C(K, T)$ implied by an IVS, the
**Dupire formula** recovers the instantaneous local variance
$\sigma_{\text{loc}}^2(K, T)$ of the risk-neutral spot dynamics
$dS_t = (r - q) S_t \, dt + \sigma_{\text{loc}}(S_t, t) S_t \, dW_t$ by
differentiating the call price in maturity and strike:

$$
\sigma_{\text{loc}}^2(K, T) =
\frac{2 \left( \dfrac{\partial C}{\partial T} + (r - q)\,K\,\dfrac{\partial C}{\partial K} + q\,C \right)}
     {K^2 \,\dfrac{\partial^2 C}{\partial K^2}} .
$$

Under the usual smoothness and no-arbitrage assumptions, this is the local-volatility
surface whose continuum model reproduces the input call surface. A discrete
calibration and simulation additionally carry finite-difference, interpolation, and
time-stepping error.

## IVS Inversion by Central Differences

The library stores spot $S_0$, continuously compounded rates $r$ and $q$, and a
Black-Scholes implied-volatility surface (`IVS_`). `IVS_::Call` has a precise
contract: it returns the discounted spot-call price

$$
\begin{aligned}
F(T) &= S_0 e^{(r-q)T}, \\
C(K,T) &= e^{-rT}\,\mathrm{Black}\!\left(
    F(T), K, \sigma_{\mathrm{imp}}(K,T)\sqrt{T}
\right).
\end{aligned}
$$

`IVS_::LocalVol` therefore evaluates the Dupire numerator and denominator by
central finite differences of these discounted calls re-priced at bumped $T$
and $K$. With the bump sizes $\Delta_T = 10^{-4}\,T$ and
$\Delta_K = 10^{-4}\,K$,

$$
\begin{aligned}
c_T      &= \frac{C(K, T+\Delta_T) - C(K, T-\Delta_T)}{2\,\Delta_T}, \\[2pt]
c_K      &= \frac{C(K+\Delta_K, T) - C(K-\Delta_K, T)}{2\,\Delta_K}, \\[2pt]
c_{KK}   &= \frac{C(K+\Delta_K, T) - 2 C(K, T) + C(K-\Delta_K, T)}{\Delta_K^{\,2}},
\end{aligned}
$$

the local volatility is assembled as

$$
\sigma_{\text{loc}}(K, T) =
\frac{1}{K}\sqrt{\,2\,\frac{c_T + (r-q)Kc_K + qC}{c_{KK}}\,}.
$$

The strike multiplier in $(r-q)Kc_K$ is required by the discounted spot-call
contract above. In particular, a flat implied-volatility surface with nonzero
$r$ and $q$ inverts back to the same flat local volatility.

The strike bump is scaled by $K$ and the maturity bump by $T$ (rather than being
absolute constants) so the relative perturbation is uniform across the grid:
$\Delta_K / K = \Delta_T / T = 10^{-4}$. That keeps the central-difference
truncation error ($O(\Delta^2)$) and the round-off floor ($\varepsilon/\Delta$)
roughly constant across strikes and maturities, instead of degrading on long
maturities or deep OTM strikes.

The method is generic over the scalar type `T_`, so the same inversion feeds a
bumped-IV Jacobian (via a `RiskView_<T_>` volatility-bump grid) as well as the
plain `double` calibration. A `RiskView_` that is empty (`IsEmpty() == true`)
returns a zero spread everywhere and the inversion reduces to the unbumped
local-vol surface.

## Calibration Grid Construction

`DupireCalib` turns a (typically sparse) set of user-supplied "inclusion"
strikes and maturities into a dense calibration grid, then fills it slice by
slice with `DupireCalibMaturity`. Two conventions matter for the user.

### Strike cutoff: 2.5 standard deviations around spot

For a fixed maturity $T$, `DupireCalibMaturity` evaluates the local volatility
only at strikes within a band around the spot:

$$
K \in \bigl[\,S_0 - 2.5\,\Sigma,\; S_0 + 2.5\,\Sigma\,\bigr],
\qquad
\Sigma \;=\; C_{S_0}(T) \cdot \sqrt{2\pi}.
$$

Here $C_{S_0}(T)$ is the discounted call price returned by
`ivs.Call(ivs.Spot(), T)`. It is the ATM call when carry is zero; with nonzero
carry its strike is still fixed at spot rather than at the forward. The literal
$\sqrt{2\pi} \approx 2.506628274631$ in the source is exactly this factor.

**Why $\sqrt{2\pi}$.** In the zero-rate, zero-dividend Black-Scholes limit, the
spot-strike call has the small-volatility expansion

$$
C_{S_0} = S_0\,\phi(0)\,\sigma\sqrt{T}
          + O\!\left(S_0(\sigma\sqrt{T})^3\right),
$$

where $\phi(0) = 1/\sqrt{2\pi}$. Therefore, to leading order,

$$
\sigma\sqrt{T} \;\approx\; \frac{C_{S_0}}{S_0}\,\sqrt{2\pi}.
$$

The library multiplies the spot-strike call *price* by $\sqrt{2\pi}$ without dividing by
$S_0$, so $\Sigma$ is a price-unit proxy for $S_0 \cdot \sigma\sqrt{T}$ — the
standard deviation of the terminal spot distribution expressed in price units.
That is exactly the right quantity to compare against a strike increment, and
the band $S_0 \pm 2.5\,\Sigma$ covers roughly $\pm 2.5\,\sigma\sqrt{T}$ of the
terminal distribution. The slightly loose "standard deviation" name in the
source comment is shorthand for this price-unit proxy.

**Why cut the grid at all.** Outside this band the call surface becomes
near-linear in $K$ (deep ITM/OTM), the denominator $c_{KK}$ collapses toward
zero, and the Dupire ratio blows up into numerical noise. Cutting at
$\pm 2.5\,\Sigma$ keeps the inversion inside the region where $c_{KK}$ is
well-conditioned.

**Flat extrapolation outside the band.** Strikes below (above) the band do not
get an inverted local volatility at all; they are filled by flat extrapolation
from the nearest in-band value. This makes the calibrated surface constant in
the tails, which is the standard pragmatic choice for a Monte Carlo local-vol
pricer: it avoids both the noise of an ill-conditioned inversion and an
unbounded terminal distribution.

### Maturity and strike fill density

The user supplies "inclusion" abscissae (`inclSpots`, `inclTimes`) and maximum
step sizes (`maxDs`, `maxDt`). `FillData` (in `dal-cpp/dal/model/utilities.hpp`)
densifies each axis: it walks the sorted union of the inclusion points and any
explicit additions, and whenever two consecutive points are farther apart than
the maximum step it inserts uniformly spaced fill points so no gap exceeds the
step. The maturity axis is densified with a minimum step floor of `ONE_HOUR_YF`
(the literal `0.000114469`, i.e. a $1/8760$ year fraction), and the strike axis
with a minimum step floor of `0.01`. The `Dupire_` simulation timeline itself is
densified against `HALF_DAY_YF` (`0.00136986301369863`, i.e. $1/730$ year) via
the same helper.

The dense grid is then evaluated maturity-by-maturity: each maturity slice is
filled by one call to `DupireCalibMaturity`, which applies the 2.5-$\Sigma$
cutoff and flat-extrapolation policy described above. The result is returned as
a struct of (`spots_`, `times_`, `lVols_`) with `lVols_` indexed
`[strike_index, time_index]` — the matrix is built time-major during the slice
loop and transposed once at the end so that downstream consumers (e.g. the
`Dupire_<T_>` model, which interpolates on log-spot against the strike axis per
simulation step) read the strike axis as a contiguous row.

## Examples

The local-volatility model is driven by `DupireModelData_`, which stores the
spot, rates, and a dense strike-by-maturity local-volatility matrix. The
up-and-out call program builds a flat 31-by-61 surface by hand and prices the
barrier option on a `Dupire_<T_>` model via the script Monte Carlo driver; a
calibrated run would fill the same matrix from `DupireCalib`. See
[`dal-cpp/examples/uoc/`](../../dal-cpp/examples/uoc/) for a runnable version;
its model construction and pricing call are:

```cpp
// from dal-cpp/examples/uoc/uoc.cpp
#include <dal/platform/platform.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/model/dupire.hpp>
#include <dal/script/event.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

using namespace Dal;
using namespace Dal::Script;
using Dal::AAD::Dupire_;

RegisterAll_::Init();
Global::Dates_::SetEvaluationDate(Date_(2022, 9, 25));

const double spot = 100.0;
const double rate = 0.0;
const double div = 0.0;
const double vol = 0.15;

// dense strike/time grid; DupireCalib would produce this from an IVS_
auto times = Vector::XRange(0.0, 5.0, 61);
auto spots = Vector::XRange(50.0, 200.0, 31);

Handle_<ModelData_> modelData(new DupireModelData_("dupiremodel",
                                                   spot,
                                                   rate,
                                                   div,
                                                   spots,
                                                   times,
                                                   Matrix_<double>(spots.size(), times.size(), vol)));
ScriptProduct_ product(eventDates, events);
product.PreProcess(false, false);
SimResults_ results = MCSimulation<double>(product, modelData, numPath, String_("sobol"), false, false);
```

The same program also runs the pricing with finite-difference bumps and with
pathwise AAD (`MCSimulation<Number_>`), so the local-volatility vega in the AAD
output is a dense grid adjoint rather than a single scalar.

Related example programs used as flat-volatility baselines against the Dupire
pricer:

- [`dal-cpp/examples/vanilla/`](../../dal-cpp/examples/vanilla/) — European call
  priced analytically and by Monte Carlo on a `BlackScholes_<T_>` model.
- [`dal-cpp/examples/european_mc/`](../../dal-cpp/examples/european_mc/) —
  convergence of the quasi-Monte Carlo pricer to the Black-Scholes closed form.
- [`dal-cpp/examples/european_fd/`](../../dal-cpp/examples/european_fd/) —
  finite-difference European pricer that recovers the Black-Scholes closed form
  from the PDE.

## See Also

- [AAD methodology](aad.md) — the reverse-mode machinery that makes a
  local-volatility calibration (and the resulting Monte Carlo pricer)
  differentiable end-to-end.
- [Interpolation](interpolation.md) — the bilinear-on-log-spot interpolation
  the `Dupire_<T_>` model uses to read the calibrated surface during simulation.
