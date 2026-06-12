# Yield Curve Construction

This note describes the mathematics of the yield-curve framework: how discount
factors are represented, how a curve is parameterised, and how it is calibrated
to market instruments. The emphasis is on the financial mathematics and the
construction algorithm, not on the code that implements them.

## Mathematical Objects

A single-currency interest-rate curve is captured by the **discount factor**
$P(t_0, T)$ — today's value of one unit of currency paid at $T$. Everything else
is derived from it.

- **Instantaneous forward rate** $f(t)$ is the continuously-compounded rate for an
  infinitesimal period at $t$:

  $$
  P(t_0, T) = \exp\!\left( -\int_{t_0}^{T} f(t)\,dt \right).
  $$

- **Zero (spot) rate** $z(T)$ is the constant rate giving the same discount
  factor: $P(t_0,T) = e^{-z(T)\,(T-t_0)}$, so $z(T) = \tfrac{1}{T-t_0}\int f\,dt$.

- **Forward discount factor** between two future dates follows from the ratio:

  $$
  P(t_1, t_2) = \frac{P(t_0, t_2)}{P(t_0, t_1)} = \exp\!\left( -\int_{t_1}^{t_2} f(t)\,dt \right).
  $$

The library parameterises the curve by the **instantaneous forward rate $f(t)$**
and obtains discount factors by integration. Time is measured in days and
annualised by dividing the day-count integral by $365$, so

$$
P(t_1, t_2) = \exp\!\left( -\frac{1}{365}\int_{t_1}^{t_2} f(t)\,dt \right).
$$

Choosing $f$ as the state variable makes the integral — and therefore every
discount factor — a closed-form function of the curve parameters, which is what
makes both calibration and AAD sensitivities efficient.

## Forward-Rate Parameterisations

The forward curve $f(t)$ is described by a small number of parameters anchored at
**knot dates** $t_1 < t_2 < \dots < t_K$. Two parameterisations are used; both
precompute the cumulative integral $S(t) = \int_{t_0}^{t} f(u)\,du$ at each knot so
that a discount factor is a single subtraction $S(t_2) - S(t_1)$ followed by an
exponential.

### Piecewise-Constant Forwards

$f(t)$ is a step function: it holds the value $f_i$ on $[t_i, t_{i+1})$. This uses
$K$ parameters (one per knot). The cumulative integral is exact and trivial:

$$
S(t_i) = S(t_{i-1}) + (t_i - t_{i-1})\,f_{i-1},
$$

and for a date $t \in [t_{i-1}, t_i)$,

$$
S(t) = S(t_{i-1}) + (t - t_{i-1})\,f_{i-1}.
$$

Before the first knot the rate is held flat at $f_1$.

### Piecewise-Linear Forwards

$f(t)$ is continuous and linear between knots, with separate left- and
right-limit values at each knot to allow controlled kinks. This uses $2K$
parameters. Within $[t_i, t_{i+1}]$ the rate interpolates linearly,

$$
f(t) = f_i^{R} + \frac{t - t_i}{t_{i+1}-t_i}\left(f_{i+1}^{L} - f_i^{R}\right),
$$

and the integral over a knot interval is the trapezoid area,

$$
S(t_i) = S(t_{i-1}) + \tfrac{1}{2}\left(f_{i-1}^{R} + f_i^{L}\right)(t_i - t_{i-1}).
$$

Outside the knot range $f$ is held flat at the nearest endpoint value.

Piecewise-linear forwards give a smoother, more realistic curve (continuous
forwards) at the price of twice as many parameters; piecewise-constant forwards
are simpler and more robust. The calibration layer can use either.

## Deriving Market Rates from the Curve

Once $f(t)$ — hence $P(\cdot,\cdot)$ — is known, observable rates follow from
no-arbitrage relations.

**Forward LIBOR / IBOR.** A simply-compounded forward rate for the accrual period
$[T, T+\tau]$ is

$$
L(T, T+\tau) = \frac{1}{\tau}\left( \frac{1}{P(T, T+\tau)} - 1 \right),
$$

where $\tau$ is the year fraction under the relevant day-count convention. This is
the standard relation that a forward rate agreement has zero value at inception.

**Swap par rate.** The fixed rate that makes a vanilla swap worth zero is the
ratio of the floating-leg PV to the fixed-leg annuity, both computed from the same
discount factors.

These relations turn the curve into a pricing engine for the calibration
instruments (deposits, futures/STIR, swaps).

## Multi-Curve Framework

Post-2008 markets discount and forecast on *different* curves. The framework
supports this through **base-curve layering**: a curve's discount factor is the
product of its own factor and that of an optional base curve,

$$
P(t_1,t_2) = P_{\text{spread}}(t_1,t_2)\,\cdot\,P_{\text{base}}(t_1,t_2).
$$

The typical construction uses an OIS curve as the base for discounting, with
tenor-specific spread curves layered on top to forecast each IBOR tenor (1M, 3M,
6M, ...). Discounting requests are routed to the appropriate collateral curve,
falling back to OIS when a specific collateral curve is absent; forecasting is
routed to the relevant tenor curve, falling back to the single discount curve in a
single-curve setup.

Because the spread is multiplicative in discount factors (additive in the
integrated forward rate), the dependency of derived curves on their base is exact
and is tracked so that a bump to the base curve flows consistently through every
curve that layers on it — the basis for bump-and-reprice risk.

## Calibration as a Root-Finding Problem

Calibration finds the forward-rate parameters $x$ (the $f_i$, or $f_i^L,f_i^R$)
that reprice a set of market instruments. For each instrument $j$ define a
**residual**

$$
r_j(x) = \text{model rate}_j(x) - \text{market rate}_j,
$$

and seek $x$ with $r_j(x) = 0$ for all $j$. Each residual is a smooth function of
$x$ through the discount-factor integrals, so the system can be solved by
Newton-type iteration, and the Jacobian $\partial r_j/\partial x_i$ is available
analytically via AAD.

### Underdetermination and Smoothness

A piecewise-linear curve carries $2K$ parameters but the market often quotes fewer
than $2K$ instruments, so the system is **underdetermined**: infinitely many
curves reprice the data. A unique, well-behaved solution is selected by preferring
the *smoothest* curve — the one that minimises a weighted norm of parameter
movements:

$$
\min_x \; \tfrac{1}{2}\,(x - x_0)^{\mathsf T} W\, (x - x_0)
\quad \text{subject to} \quad r(x) = 0,
$$

with $W$ a symmetric positive-definite weight matrix. Choosing $W$ to penalise
differences between neighbouring knots (a tridiagonal smoothing operator)
suppresses spurious oscillation between instrument maturities. The mechanics of
this constrained minimisation — and the approximate-fit variant for inconsistent
quotes — are covered in [Underdetermined search](underdetermined_search.md).

### Bootstrapping Order

Instruments are ordered by maturity so that the curve is built outwards in time:
short-dated instruments pin the near end of the curve, longer-dated instruments
extend it. Knots can be supplied by the caller, taken from the instrument
maturities, or formed from the union of both. After the solve, repricing residuals
(RMS and maximum absolute error) quantify the fit quality, and the effective
(weighted) Jacobian inverse is retained so that risk sensitivities can be obtained
without re-solving the system.

## Construction Pipeline

```text
market instruments (deposits, STIR, swaps)
        │  define residuals r_j(x) = model_j(x) − market_j
        ▼
underdetermined solver  (min ½‖x−x₀‖²_W  s.t. r(x)=0)
        │  Jacobian via AAD; smoothness via weight matrix W
        ▼
forward-rate parameters x   (piecewise constant: K, piecewise linear: 2K)
        │  cumulative integrals S(t) precomputed at knots
        ▼
discount factors  P(t₁,t₂) = exp(−∫f dt / 365) × P_base
        │
        ▼
multi-curve routing: OIS discounting + tenor forecasting
```

## See Also

- [Underdetermined search](underdetermined_search.md) — the optimisation method
  that performs the calibration solve.
- [Cross-currency calibration](xccy_calibration.md) — extends this framework to a
  cross-currency basis curve.
- [AAD methodology](aad.md) — supplies the analytic Jacobian used in calibration
  and the curve risk sensitivities.
