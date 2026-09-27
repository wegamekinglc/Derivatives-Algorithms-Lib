# Hybrid Monte Carlo Model

`HybridModel_<T>` composes model components under one domestic numeraire. The
current components are ordinary Black-Scholes equities and a deterministic
domestic rate. Each equity contributes one log-spot state, one Brownian factor,
and one named `EQ[...]` observation. The rate contributes no state or Brownian
factor. Stochastic rates, FX, credit, foreign currencies, quanto drift, and
cross-currency discounting are not implemented.

## C++ construction

```cpp
#include <dal-public/src/models.hpp>
#include <dal/model/factory.hpp>

Dal::HybridSettings_ settings;
settings.domesticCurrency_ = "USD";
settings.components_ = {
    Dal::Handle_<Dal::HybridComponentData_>(
        new Dal::HybridDeterministicRateData_("usd_rate", "USD", 0.05)),
    Dal::Handle_<Dal::HybridComponentData_>(
        new Dal::HybridBSEquityData_("aaa", "EQ[AAA]", "USD", "W_AAA", 100.0, 0.20, 0.01)),
    Dal::Handle_<Dal::HybridComponentData_>(
        new Dal::HybridBSEquityData_("bbb", "EQ[BBB]", "USD", "W_BBB", 120.0, 0.30, 0.02)),
};
Dal::Matrix_<> correlations(2, 2, 0.35);
correlations(0, 0) = correlations(1, 1) = 1.0;
settings.correlation_ = Dal::Handle_<Dal::HybridCorrelationData_>(
    new Dal::HybridConstantCorrelationData_("corr", {"W_AAA", "W_BBB"}, correlations));

auto data = Dal::NewHybridModelData("hybrid", settings);
auto model = Dal::CreateModel<double>(data);
```

The model data and each typed component are serializable. `CreateModel` also
accepts the restored archive. Components are ordered by their stable names
during setup, independent of declaration order. Factor labels are sorted
case-insensitively; the correlation matrix is keyed by its own explicit
factor-name list and reordered to that registry. A missing or duplicate name,
unsupported currency, absent or multiple numeraire providers, or invalid
correlation matrix fails before path generation. The constant correlation
provider precomputes its Cholesky factor once. `FactorCorrelation_` exposes a
per-step lower factor for a future time-bucketed provider; no interpolation is
performed by the current provider.

`Allocate` resolves requested observation names into integer component and
output slots. `Init` precomputes each equity's drift and volatility for every
time step and the deterministic numeraire. The per-path loop does no name
parsing or matrix factorization. `SimDim()` is the number of positive time
steps multiplied by the number of registered factors. Independent Gaussian
inputs are time-major, with factors in sorted label order within each step.
The output `Sample_::observations_` follows the requested name order;
`Sample_::spot_` remains the first equity's compatibility field.

## Brownian bridge and risks

For multiple factors, `Script::CreateRNG(method, model, true)` applies a
Brownian bridge independently to each factor's time series. Sobol coordinate
`factor * nSteps + bridgeCoordinate` maps to the output at
`step * nFactors + factor`. The model then applies the per-step correlation
factor to those independent normals. A one-factor request still uses the
original bridge and preserves its ordering. The factor-aware wrapper forwards
`SkipTo` and `Clone` to the underlying generator; Sobol retains its seek and
clone sequence guarantees.

`CreateModel<AAD::Number_>` exposes `spot:EQ[...]`, `vol:EQ[...]`, and
`div:EQ[...]` for each equity, plus `rate:USD` for the domestic rate. These
parameters are owned by their components; cloned workers receive independent
AAD parameter addresses. Correlations are passive inputs and have no AAD
risk labels. The `NumeraireIsDeterministic()` capability distinguishes the
current model from future stochastic-numeraire compositions.

This stage supports direct core path generation. The script preparation and
public valuation layers still reject multiple future model indices; their
multi-index and LSM integration belongs to the next stage.
