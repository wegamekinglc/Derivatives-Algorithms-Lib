# Correlated Equity Black-Scholes Model

See the [models index](README.md) for the other supported simulation models.

`CorrelatedBSModelData_` is the core model-data type for jointly simulating
several ordinary `EQ[...]` indices. Each asset has its own spot $S_i(0)$,
constant volatility $\sigma_i$, and dividend yield $q_i$. All assets use one
deterministic domestic rate $r$. The input correlation matrix $R$ is symmetric,
has unit diagonal, and must be positive definite. Zero volatility is allowed;
zero or negative spots are not.

For each time step of length $\Delta t$, the model consumes independent
standard normals $z_k$, applies the lower Cholesky factor $L$ satisfying
$LL^\mathsf{T}=R$, and evolves

$$
\log S_i(t_{k+1}) = \log S_i(t_k)
  + (r-q_i-\tfrac12\sigma_i^2)\Delta t
  + \sigma_i\sqrt{\Delta t}\,(Lz_k)_i.
$$

The Gaussian vector is **time-major, factor-major**: step 0 takes factors
`0..nAssets-1`, then step 1 takes the same factor order, and so on. `SimDim()`
is `nSteps * nAssets`. A sample at time zero consumes no Gaussian values. The
numeraire at sample time $t$ is $e^{rt}$.

## C++ construction and path generation

```cpp
#include <dal-public/src/models.hpp>
#include <dal/model/factory.hpp>

Dal::CorrelatedBSSettings_ settings;
settings.assets_ = {{"EQ[AAA]", 100.0, 0.20, 0.01},
                    {"EQ[BBB]", 120.0, 0.30, 0.02}};
settings.rate_ = 0.05;
settings.correlations_ = Dal::Matrix_<>(2, 2, 0.0);
settings.correlations_(0, 0) = settings.correlations_(1, 1) = 1.0;
settings.correlations_(0, 1) = settings.correlations_(1, 0) = 0.35;

auto data = Dal::NewCorrelatedBSModelData("basket", settings);
auto model = Dal::CreateModel<double>(data);
Dal::Vector_<> times{0.0, 1.0};
Dal::Vector_<Dal::AAD::SampleDef_> definitions(2);
definitions[1].indexNames_ = {"EQ[BBB]", "EQ[AAA]"};
model->Allocate(times, definitions);
model->Init(times, definitions);
Dal::AAD::Scenario_<> path;
Dal::AAD::AllocatePath(definitions, path);
Dal::AAD::InitializePath(path);
model->GeneratePath({0.4, -0.7}, &path);
// path[1].observations_[0] is BBB; [1] is AAA.
```

Each requested output slot is resolved to an asset during `Allocate`.
Requests may repeat an asset or use a different order from the model's asset
list. The single `Sample_::spot_` compatibility field is the first configured
asset; multi-asset consumers should use the named `observations_` slots. The
model validates unknown or duplicate asset names, malformed correlation
matrices, invalid parameters, and unsupported output indices before path
generation. Configured ordinary equity names are canonicalized when model data
is constructed, so its parameter labels match the runtime model's risk labels.

`CreateModel<AAD::Number_>` generates the same paths on an AAD tape. Risk
parameters are ordered `spot:EQ[...]`, `vol:EQ[...]`, and `div:EQ[...]` for each
configured asset, followed by `rate`. Correlations are passive inputs and do
not appear in the AAD risk vector. `Clone()` owns independent parameter
pointers. The core RNG helper now supports `useBb=true` with multiple factors:
it bridges each independent factor along the time axis before the model
applies its correlation transform. See the [hybrid model](hybrid-model.md).

`ValueByMonteCarlo` accepts this model data. Named `FIX(EQ[...])` expressions
read their respective path outputs in tree, compiled, double, and AAD modes;
`SPOT()` needs an explicit product default when the model has multiple assets.
For early exercise, that default selects the legacy single LSM state;
`regressionFeatures_` can instead select two or three equity and scalar script
states for a multivariate continuation fit. The model supports a deterministic
numeraire, so its paths can enter the existing LSM discounting contract.
Correlation remains passive for AAD.

The runnable [basket call example](../../dal-cpp/examples/basket_mc/) prices
`max(0.5 S_A(T) + 0.5 S_B(T) - 110, 0)` with two correlated equity paths and
named `FIX` observations. It averages discounted path payoffs over $2^{18}$
Sobol paths. The program compares compiled double and AAD valuation, double
valuation with central-difference Delta, and a reference obtained by
conditioning on the first asset and integrating the second asset's Black call
value with 48-point normal Gauss–Hermite quadrature. The example uses spots of
100 and 120, volatilities of 20% and 30%, zero dividends, a 5% rate, and a
one-year expiry. It repeats the same Sobol sequence at correlations of -0.60,
0, 0.40, and 0.80 so the prices can be compared directly:

| Correlation | Reference PV | MC double PV | MC AAD PV | AAD dPV/dS_A | AAD dPV/dS_B |
|-------------|--------------|--------------|-----------|--------------|--------------|
| -0.60       | 8.672015     | 8.671933     | 8.671933  | 0.305711     | 0.345761     |
| 0.00        | 10.958546    | 10.957995    | 10.957995 | 0.301603     | 0.330575     |
| 0.40        | 12.167945    | 12.167312    | 12.167312 | 0.301353     | 0.325646     |
| 0.80        | 13.242248    | 13.241683    | 13.241683 | 0.301793     | 0.322374     |

For the 0.40 correlation case, the program also compares AAD Delta with a
central-difference estimate from non-AAD Monte Carlo:

| Method                 | PV        | dPV/dS_A | dPV/dS_B |
|------------------------|-----------|----------|----------|
| Conditional GH + Black | 12.167945 | 0.301358 | 0.325651 |
| MC double              | 12.167312 | —        | —        |
| MC double + FD         | 12.167312 | 0.301349 | 0.325645 |
| MC AAD                 | 12.167312 | 0.301353 | 0.325646 |

The finite differences bump each starting spot by 0.01 and reuse the Sobol
sequence. MC values are numerical estimates; the executable also reports
their differences from the reference and elapsed times.
