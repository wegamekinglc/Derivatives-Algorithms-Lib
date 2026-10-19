# Constrained Market Implied-Volatility Surfaces

`FitMarketIvSurface` fits European call/put market quotes to a constrained SSVI
surface and returns an owning calibration result. Successful results provide a
snapshot for analytic price/variance derivatives, explicit Dupire grids and
full-refit quote bumps. The supported market has one asset, positive spot and
forwards, and deterministic proportional carry. Cash dividends, negative
forwards, normal volatility and stochastic-rate calibration are outside this path.

The implementation is in `dal-cpp/dal/model/surface/marketivsurface.hpp` and its
`marketiv*.cpp` files. The public header
`dal-public/src/marketivsurface.hpp` re-exports the core types/functions and
defines shared binding settings; there is one numerical implementation.
[Python](../python/README.md#market-implied-volatility-surfaces) and
[Excel](../excel/README.md#market-implied-volatility-surfaces) expose the same workflow.

## Quotes, dates and carry

`MarketIvMarket_` freezes an evaluation date, positive spot and strictly
increasing future terms `(expiry, forward, discount)`. Discount factors may
exceed one. Each quote expiry must match a term; extra carry terms need no
quotes and create no additional theta parameters. Times are actual calendar
days divided by 365, including leap days, without business-day adjustment or
global-date capture. Queries supply positive strike and years from that same date.

For general terms, `log(F/S)` and `log(D)` interpolate linearly from
`F(0)=S,D(0)=1` and extend with the last segment's slope. Instantaneous
`r=-d(log D)/dt` and `q=r-d(log F)/dt` follow from those curves. There is no
second independent r/q input. At a carry knot, time derivatives use the right
segment. `NewFlatMarketIvMarket` instead retains explicit constant r/q and the
`FLAT_CARRY` marker; numerically similar general terms do not acquire that marker.

Every `MarketIvQuote_` has a unique nonempty ID, expiry, positive strike,
`CALL`/`PUT`, `PRICE`/`DECIMAL_IV`, mid, optional paired bid/ask and positive
weight (default one). Quotes retain input order. Repeated economic quotes
with the same expiry/strike/option type reject; a call and put may coexist.
Bid and ask must both be present or both absent, with bid <= mid <= ask.

- `PRICE` is the discounted premium per unit of underlying, in spot/strike
  currency units. Call bounds are `[D*max(F-K,0),D*F]`; put bounds are
  `[D*max(K-F,0),D*K]`, including bid/ask.
- `DECIMAL_IV` is annualized Black volatility: `0.20` means 20%. Mid/ask must
  be positive; bid may be zero. Weights are objective weights, not path counts.

The fit uses `k=log(K/F)` and normalized call prices:

```text
call PRICE: c = C/(D*F)
put PRICE:  c = 1 + (P-D*K)/(D*F)
decimal IV: c = BlackCall(F,K,sigma*sqrt(t))/F
```

These are mathematical identities; the implementation selects stable scaled
expressions and certifies conversion error, including subnormal residual/DF
rounding. If the required precision cannot be certified, it returns
`NUMERIC_RANGE` without a snapshot. PRICE quotes do not undergo IV inversion.
There is no automatic quote repair, clipping or removal of low-weight rows.

## Surface and global certificate

The fitted family has one shared rho, eta and a monotone ATM total-variance
curve theta, with fixed gamma=1/2:

$$
w(k,t)=\frac{\theta(t)}{2}\left[1+\rho\phi(\theta)k+
\sqrt{(\phi(\theta)k+\rho)^2+1-\rho^2}\right],\qquad
\phi(\theta)=\frac{\eta}{\sqrt{\theta(1+\theta)}}.
$$

Each quoted expiry contributes a nonnegative theta increment; the first is
at least `theta_floor`. The origin is `(0,0)`. Theta uses monotone C1 PCHIP
in Hermite node/slope form and a nonnegative linear tail. It need not be C2.
Both strike wings use SSVI itself, without IV clamping or a fitted-strike cutoff.
The eta=0 closure is `w=theta`, with canonical rho=0.

The sufficient conditions are those of
[Gatheral–Jacquier, Theorems 4.1/4.2 and Corollary 4.1](https://arxiv.org/pdf/1204.0646).
DAL restricts the parameter box using `R=rho_limit` and `M=constraint_margin`:

```text
abs(rho) <= R < 1
0 <= eta <= 2*sqrt((1-M)/(1+R))
theta' >= 0
(theta*phi)'/phi = 1/(2*(1+theta))            [eta > 0]
theta*phi^2*(1+|rho|) <= 4*(1-M)
theta*phi*(1+|rho|) < 2*sqrt(2) < 4
```

This box and monotone theta imply the continuous butterfly and fixed-k
calendar conditions for all real k and all t>0 under deterministic proportional
carry, including the time extensions and wings. Calendar monotonicity concerns
normalized calls at fixed K/F; discounted prices at a fixed K need not increase
with expiry when carry changes.

`SSVI_GammaHalf_MonotoneTheta_v1` records inward parameter admission, outward
butterfly/wing bounds, increments, limiter branches and conservative per-segment
theta-derivative lower bounds for the stored Hermite representation. Rounding
that prevents certification rejects the candidate. A tolerance or sampled g
check cannot substitute for that certificate. The continuous guarantee does not
promise bitwise convexity of every double query, arbitrary-market fit quality,
dynamic arbitrage freedom or exact repricing after grid export and simulation.

## Fit admission, quality and failures

The fixed four starts are the flat-smile submodel followed by full-model rho
seeds -0.5, 0 and +0.5 with eta=0.5. Every seed is projected into the configured
box. Damped box Gauss–Newton uses analytic Jacobians and an original-coordinate
projected-gradient convergence test. All starts run under the work limit.

The objective is weighted normalized-call least squares, with each row scaled
by the larger of its half-bandwidth and `normalized_price_scale_floor`.
Acceptance independently requires convergence, a certificate and every row
inside its permitted band plus `feasibility_tolerance`. Supplied bid/ask define
the band. Without them, PRICE uses a normalized-call half-band
`normalized_price_tolerance`; IV uses a native-vol half-band
`decimal_iv_tolerance`, converted to normalized calls. Even a weight that
underflows during normalization cannot bypass its row's band check.

Among admitted candidates the smallest objective wins, with the earliest start
within `objective_tie_tolerance` selected deterministically. Step stagnation,
no descent or an iteration limit alone do not establish success. Local fitting
does not guarantee a global optimum, unique parameters or a fit to every market.
`UNDERIDENTIFIED` and near-tie diagnostics qualify successful sparse/low-rank
fits; they do not identify a reliable unquoted smile or a confidence interval.

`SUCCESS` and `SUCCESS_WITH_WARNINGS` permit `Snapshot()`. Numerical/work/quality
failures such as `NUMERIC_RANGE`, `WORK_LIMIT`, `NO_CONVERGENCE`,
`CONSTRAINT_FAILURE`, `INCONSISTENT_QUOTES` and `FIT_QUALITY_FAILURE` retain
diagnostics but no usable snapshot; `Snapshot()` throws `SnapshotUnavailable`.
A work limit cannot publish an earlier candidate as success. Failure to fit
this restricted family is not proof of market arbitrage. Structural/type errors
and planned byte-budget rejection throw `InvalidMarketIvRequest` or
`MarketIvBudgetExceeded` before numerical execution.

## Analytic derivatives and Dupire

Let `a=d(log F)/dt=r-q` and `u(K,t)=w(log(K/F(t)),t)`. Then

$$
u_t=w_t-a w_k,\qquad u_K=w_k/K,\qquad
u_{KK}=(w_{kk}-w_k)/K^2.
$$

The coordinate names deserve care:

| C++ getter                 | Python property               | Meaning                                               |
|----------------------------|-------------------------------|-------------------------------------------------------|
| `VarianceK`, `VarianceKK`  | `variance_k`, `variance_kk`   | First/second derivatives in log-moneyness k           |
| `VarianceT`                | `variance_t`                  | Time derivative at fixed log-moneyness k              |
| `FixedKVarianceT`          | `fixed_k_variance_t`          | Time derivative at fixed strike K                     |
| `CallK`, `CallKK`, `CallT` | `call_k`, `call_kk`, `call_t` | Discounted-call derivatives at fixed time or strike K |

`CallT` has premium/year units, `CallK` premium/strike and `CallKK`
premium/strike squared. At carry knots `carry_derivative` reports
`RIGHT_CARRY_DERIVATIVE`. Theta and carry can have different knot axes.

The new analytic Dupire calculation is

$$
g=\left(1-\frac{k w_k}{2w}\right)^2
 -\frac{w_k^2}{4}\left(\frac1w+\frac14\right)+\frac{w_{kk}}2,
\qquad \sigma_{\mathrm{loc}}^2=\frac{w_t|_k}{g}.
$$

It agrees with `2*(C_t+q*C+(r-q)*K*C_K)/(K^2*C_KK)` using the same carry
side. It does not substitute the fixed-K variance derivative in the numerator.
`value_status`, `derivative_status` and `dupire_status` separately gate usable
results. A valid price does not establish usable local volatility. Optional
unavailable values are empty/`None`; callers must inspect the relevant status.
No negative-variance floor, hidden finite difference or tail fill hides failure.
True zero w_t can produce zero local volatility.

`CalibrateMarketIvDupire` evaluates exactly the requested positive, strictly
increasing spot/time axes. It adds no fill points or ATM stability band. Nodes
retain grouped statuses; any failed node prevents `SurfaceData()` from returning
a completed grid. Successful `LocalVolSurfaceData_` has `[spot,time]` matrix
orientation. Its archive v1, log-spot/time interpolation and flat extrapolation
remain unchanged. That discrete grid does not inherit the continuous certificate.

## Complete quote refits

`RefitMarketIvSurface(snapshot,quotes)` replaces only original mid/bid/ask values
on the same quote axis. It freezes market, IDs, expiry/strike/types, weights and
settings, then repeats conversion, all four starts, interpolation and certification.
It does not silently warm-start from the old winner.

`EvaluateMarketIvRefitRisk` selects original IDs and any of `TOTAL_VARIANCE`,
`IMPLIED_VOL`, `CALL_PRICE`, `PUT_PRICE`, `LOCAL_VOL`. Results order quote axes
by original input row, with outputs in requested order. Each direction shifts
the selected mid and any bid/ask by the same original-unit h, independently
refits from the base and re-evaluates all outputs. Default h is `1e-4` decimal
IV or `1e-4*D*F` premium; explicit positive representable steps are supported.

`FullRefitSymmetricBump` reports f0/f+/f-, `(f+-f-)/(2h)` and both one-sided
secants in output units per original quote unit. Failed bumps/fits/required
outputs leave slopes unavailable, without single-sided fallback; other rows
remain readable but `Complete()` is false. `REGIME_CHANGED` compares discrete
active bounds, limiter/band branches, winning start/model, rank and near-tie
state. Valid finite slopes may carry that warning. An unchanged signature also
does not certify differentiability; inspect both sides and h/h2 behavior.

These are surface-output finite bumps, distinct from the existing fixed-base
decimal-vol-spread VJP. A trade/portfolio refit requires explicit grid export,
model reconstruction and full repricing per direction, with common random
numbers for Monte Carlo. This interface provides no generic trade callback or
analytic derivative through the constrained optimizer.

## Defaults and resource limits

Canonical snake-case names below are Python keywords and Excel settings keys;
C++ uses the corresponding trailing-underscore members.

| Fit setting                                                                           | Default                                    |
|---------------------------------------------------------------------------------------|--------------------------------------------|
| `rho_limit`, `constraint_margin`                                                      | `0.999`, `1e-8`                            |
| `theta_floor`, `max_variance_increment`, `initial_volatility`                         | `1e-10`, `100`, `0.20`                     |
| `projected_gradient_tolerance`, `parameter_step_tolerance`, `objective_tie_tolerance` | `1e-8`, `1e-10`, `1e-12`                   |
| `normalized_price_tolerance`, `decimal_iv_tolerance`                                  | `1e-4`, `5e-4`                             |
| `normalized_price_scale_floor`, `feasibility_tolerance`                               | `1e-4`, `1e-10`                            |
| `max_iterations_per_start`, `max_line_search_trials`                                  | `200`, `16`                                |
| `max_model_row_evaluations`                                                           | `5000000`                                  |
| `max_quotes`, `max_expiries`, `max_market_terms`                                      | `10000`, `64`, `256`                       |
| `numeric_payload_budget_bytes`, `workspace_budget_bytes`                              | `67108864` (64 MiB), `268435456` (256 MiB) |

Evaluation defaults are `density_factor_floor=1e-10`, `min_query_time=1e-8`
years and `min_total_variance=1e-12`; density cancellation/range checks also
apply. Dupire settings add `max_grid_points=100000`. Refit settings add
`max_selected_quotes=32`, `max_outputs=128`; omitted aggregate row budget is
`2*A*base.max_model_row_evaluations` for A selected quotes. Each directional fit
also enforces its own per-fit limit. Dupire/refit have separate 64/256 MiB caps
and share the evaluation defaults. Zero work/byte limits mean zero, not unlimited.

Preflight uses conservative full payload/workspace envelopes with checked
integer arithmetic. `Execution()` reports planned bytes, actual retained numeric
capacities, peak workspace, model-row work, quote conversions and output work.
Jacobian evaluations and rejected trials count toward model-row work. Workspace
is a capacity/fixed-frame ledger, not process RSS; string contents, allocator
overhead, language wrappers, worksheet cells and getter copies are excluded.
An admitted input count/memory envelope does not promise convergence within
the work budget. Budget increases do not improve the fit quality tolerances or
the mathematical certificate.

## C++ usage and ownership

This complete core example uses only core headers and can link `DAL::cpp`.
A facade consumer can replace the market-IV include with
`<dal-public/src/marketivsurface.hpp>` and link `DAL::public`; facade programs
remain separate from core examples.

```cpp
#include <dal/platform/platform.hpp>
#include <dal/model/surface/marketivsurface.hpp>
#include <iomanip>
#include <iostream>

int main() {
    const Dal::Date_ today(2026, 10, 19);
    const Dal::Vector_<Dal::Date_> expiries{
        Dal::Date_(2027, 4, 19), Dal::Date_(2027, 10, 19)};
    const auto market = Dal::NewFlatMarketIvMarket(today, 100., .03, .01, expiries);
    Dal::Vector_<Dal::MarketIvQuote_> quotes;
    int row = 0;
    for (const auto& expiry : expiries)
        for (const double strike : {90., 100., 110.}) {
            Dal::MarketIvQuote_ quote;
            quote.id_ = Dal::String_("q") + Dal::String_(std::to_string(row++));
            quote.expiry_ = expiry;
            quote.strike_ = strike;
            quote.optionType_ = Dal::OptionType_("CALL");
            quote.quoteType_ = Dal::MarketIvQuoteType_("DECIMAL_IV");
            quote.mid_ = .20;
            quotes.push_back(quote);
        }
    const auto fit = Dal::FitMarketIvSurface({market, quotes, {}});
    if (!fit.IsSuccess()) {
        std::cerr << fit.Status() << ": " << fit.Failure() << '\n';
        return 1;
    }
    const auto snapshot = fit.Snapshot();
    const auto point = Dal::EvaluateMarketIvSurface(snapshot, {100., 1.});
    const auto grid = Dal::CalibrateMarketIvDupire(snapshot, {{80., 100., 120.}, {.25, 1.}, {}});
    Dal::MarketIvRefitRiskRequest_ request;
    request.quoteIds_ = {"q1"};
    request.outputs_ = {{{100., 1.}, Dal::MarketIvQuantity_("CALL_PRICE")}};
    request.steps_ = {1e-6};
    const auto risk = Dal::EvaluateMarketIvRefitRisk(snapshot, request);
    if (point.ValueStatus() != "SUCCESS" || !grid.IsSuccess() || !risk.Complete())
        return 2;
    const auto& surface = grid.SurfaceData();
    std::cout << std::left << std::setw(16) << "Call price"
              << std::right << std::setw(16) << *point.CallPrice() << '\n';
    return surface.vols_.Rows() == 3 ? 0 : 3;
}
```

Results and snapshots share immutable owning data and retain their source
request. They survive caller input/result destruction. C++ references from
getters remain tied to the owning object's lifetime. Python containers/nested
getters and Excel spills are detached copies; native Python calls copy typed
inputs before releasing the GIL and use no Python pricing callback. Snapshot
archives are not provided; new Excel handles reject serialization.

## Legacy compatibility and model boundary

`MarketIvSurfaceAsLegacyIVS` is an explicit C++/Python owning adapter for
`FLAT_CARRY` snapshots only. Its implied volatility uses the new surface;
inherited `IVS_::Call/LocalVol` still use the original flat r/q and price
finite-difference method described in [Dupire calibration](dupire.md).
General forward/discount knots reject even if they approximate flat carry.
The old `DupireCalibration`, spread VJP/curvature and archive v1 keep their
existing contracts; their quote axis remains fixed-base spreads, not market refits.
Excel does not expose a market-snapshot legacy-IVS bridge factory; its existing
BS/Merton calibration dispatch does not automatically accept the new handle.

The explicit analytic grid can be exported for either carry mode. Existing
flat-rate Hybrid convenience constructors do not automatically reproduce a
nonflat F/D market. Supply consistent drift/rates externally and check grid/
simulation convergence; adding stochastic rates requires separate calibration
analysis. Exporting a grid neither stores the original quotes/SSVI certificate
in archive v1 nor establishes exact market repricing.
