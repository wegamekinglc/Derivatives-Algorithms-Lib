# Fixed-Grid European PDE Prices and Adjoints

The runnable [European AAD example](../../../dal-cpp/examples/european_aad_fd/european_aad_fd.cpp)
records a complete Black-Scholes call/put rollback through
[native sampled theta steps](../aad.md#recorded-sampled-pde-theta-steps).
Two price layers share rate, volatility and strike inputs. Two reverse channels
return each price's rho, vega and strike derivative, including the terminal
payoff and discounted boundary contributions.

## Discrete Financial Program

The fixed physical grid spans $S\in[0,400]$, with evaluation spot $S_0=100$,
expiry $T=1$ and dividend yield $q=0.02$. Registered parameters are
$r=0.05$, $\sigma=0.20$ and $K=110$. The interior operator is

$$
L V=\tfrac12\sigma^2S^2V_{SS}+(r-q)SV_S-rV.
$$

Ordinary native expressions build sampled rate $r$, drift $(r-q)S$ and
variance $\sigma^2S^2$. The variance binding therefore carries the physical
volatility chain factor $2\sigma$. Coefficient vectors have one entry per
interior node; the state matrix has one row per grid node and two option layers.

Terminal states are $\max(S-K,0)$ and $\max(K-S,0)$. At new time-to-expiry
$\tau$, externally supplied call boundaries are $0$ and
$400e^{-q\tau}-Ke^{-r\tau}$; put boundaries are $Ke^{-r\tau}$ and $0$.
The strike participates in both terminal and boundary expressions, and rate
participates in both interior and boundary expressions. Every active state or
boundary cell has a current recording slot, including shared recorded zeros.

With ordinary step size $\Delta t=1/120$, four fully implicit half steps cover
the first two ordinary intervals. The remaining 118 intervals use
Crank-Nicolson, producing 122 recorded steps over exactly one year. Each active
solution becomes the next step's old-state binding. Each step owns its values
and decomposition for subsequent transpose solves; the complete chain retains
these caches until the recording closes.

The physical grid, evaluation location, time schedule, expiry and dividend
yield are passive. Payoff branches are selected at the parameter point; this
fixture keeps strike away from grid nodes. The example reports first-order
derivatives of that fixed discrete program. Its finite upper boundary is a
model approximation, and its recorded chain retains all time-step caches.

## Run the Example

With examples enabled in the normal build:

```bash
cmake --build build/Release-linux --target european_aad_fd
./build/Release-linux/dal-cpp/examples/european_aad_fd/european_aad_fd
```

The default 61-node/120-interval result is:

| Option | Price     | dPrice/dr  | dPrice/dSigma | dPrice/dK |
|--------|-----------|------------|---------------|-----------|
| Call   | 5.190003  | 35.050189  | 38.015123     | -0.315791 |
| Put    | 11.805380 | -69.584698 | 38.015123     | 0.635439  |

Rate and volatility derivatives use absolute decimal parameter units; multiply
rho by $10^{-4}$ for a one-basis-point sensitivity and vega by $0.01$ for one
volatility point. The example also prints the largest actual physical forward
and transpose backward errors under the declared $10^{-12}$ limits.

The [shared caller](../../../dal-cpp/test-support/europeanaadfd.hpp) supplies
the financial expressions while the example owns input registration, channel
selection, reverse seeds and report extraction. It copies doubles before
closing the scope. This support header is repository example/test code; reusable
library interfaces remain the native sampled-step headers linked above.

## Derivative and Model Acceptance

The [financial tests](../../../dal-cpp/tests/math/aad/test_sampledthetastep_financial.cpp)
separate derivative correctness from model discretization. A complete small-mesh
dense reference builds its own operator and solves every step without using DAL
stencils, caches or solvers. Three centered bump sizes rebuild coefficients,
terminal states and boundaries. Independent direct-generator banded solves with
complex-step derivatives supply frozen price/Greek references without adding a
Python dependency to the C++ tests. Weighted objectives, direct parameter terms,
scalar/vector channels, repeated sweeps and a zero lane exercise composition.

Three refinements use 21/61/181 space nodes and 40/120/360 ordinary intervals.
Refinement by three keeps strike at each payoff-cell midpoint and spot at a
node. Each grid executes once; its outputs receive frozen-reference, continuum
convergence and price/risk parity checks. Against independent erfc Black-Scholes
closed forms, the largest absolute error across call and put is approximately:

| Nodes / intervals | Price    | Rho      | Vega     | Strike derivative |
|-------------------|----------|----------|----------|-------------------|
| 21 / 40           | 0.020071 | 0.636913 | 1.665017 | 0.036446          |
| 61 / 120          | 0.001429 | 0.013092 | 0.098394 | 0.002731          |
| 181 / 360         | 0.000160 | 0.001013 | 0.010539 | 0.000289          |

These fixture errors describe spatial/time discretization and finite-domain
approximation. Tight discrete-reference tolerances, centered-difference
truncation allowances, continuum error ceilings and solve residual limits are
separate checks. Residual acceptance alone does not bound a financial Greek's
model error. The tests also prove nonzero terminal/boundary contributions and
the minimum four-half-step schedule.
