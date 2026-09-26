# Correlated Equity Black-Scholes Model

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

This stage exposes core model construction and direct path generation. The
script preparation and public valuation layers still reject multiple future
model indices; their integration is a later stage.
