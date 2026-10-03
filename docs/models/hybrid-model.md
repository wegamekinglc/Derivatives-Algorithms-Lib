# Hybrid Monte Carlo Model

See the [models index](README.md) for the other supported simulation models.

`HybridModel_<T>` composes model components under one domestic numeraire. The
supported equity components are ordinary Black-Scholes and
[local-volatility equities](local-volatility.md). The domestic rate can be
constant, a deterministic log-discount-factor term structure, or a stochastic
[GSR](gaussian-short-rate.md) component with any number of factors; the GSR
stochastic-local-volatility model plugs in as a single component through the
same interface. Each equity contributes one log-spot state, one Brownian
factor, and one named `EQ[...]` observation. A stochastic rate component
contributes its Brownian drivers and supplies `IR[...]` observations. FX,
credit, foreign currencies, quanto drift, and cross-currency discounting are
not implemented.

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

To use a deterministic term structure, replace the constant-rate component
with `NewHybridLogDfRateData`:

```cpp
settings.components_[0] = Dal::NewHybridLogDfRateData(
    "usd_curve", "USD", {0.0, 0.5, 1.0, 2.0},
    {0.0, -0.018, -0.041, -0.089});
auto termData = Dal::NewHybridModelData("hybrid_curve", settings);
```

The times are ACT/365 years from the script valuation date, matching the model
simulation clock. Supply at least two nodes, beginning with `t=0, logDF=0`,
then strictly increasing finite times and finite log discount factors. All
components must use the domestic currency. The rate component remains
deterministic and has no factors. It uses the existing `LogDfInterpolation_`
schemes: `LOG_LINEAR` (default), `LOG_CUBIC_NATURAL`, or `MIXED`.
Natural cubic needs at least three nodes and mixed needs at least four. Values
beyond the last node use the final two-node secant. A negative logDF is usual
for positive rates, but negative rates are allowed.

For a calibrated dated `DiscountCurve_`, use
`NewHybridLogDfRateDataFromCurve(name, *curve, evaluationDate, nodeDates)`.
The first snapshot date must equal the valuation date. The helper samples
`curve(evaluationDate, nodeDate)`, so the snapshot is rebased to that date,
uses the curve's currency, and converts dates to the model ACT/365 axis even
when the source curve uses another day count. The resulting component is a
snapshot; recalibration of the source curve does not change it. Set the
script valuation date to the same `evaluationDate`. The same raw-node and
curve-snapshot factories are available in Python as
`HybridLogDfRateData_New` and `HybridLogDfRateDataFromCurve_New`, and in Excel
as `HybridLogDfRateData_New` and `HybridLogDfRateDataFromCurve_New`.

### Stochastic rate components

For stochastic rates, replace the deterministic component with a GSR rate
component. The one-factor form pairs a `GSRVolData_` handle with one hybrid
factor name; the multi-factor form pairs a `MultiFactorGSRVolData_` handle with
one hybrid factor name per Gaussian factor:

```cpp
settings.components_[0] = Dal::Handle_<Dal::HybridComponentData_>(
    new Dal::HybridGSRRateData_("usd_rate", {"W_LEVEL", "W_SLOPE"}, curve, multiVol));
```

Plain GSR also consumes one independent internal Gaussian per step for the
bank-account integration bridge. This driver is outside the named correlation
matrix and cannot have cross-component correlation links. Gaussian rate and
asset endpoint increments use the supplied named correlations.

The [GSR SLV model](gaussian-short-rate.md) enters the same slot as one
component: `HybridGSRSLVRateData_(name, volFactor, bridgeFactor, slvModelData)`
contributes one named factor per Gaussian rate driver, a variance driver, and an
independent bridge driver that carries the bank-account integration noise. Both
components are the sole numeraire provider of their model, expose the kernel's
AAD parameters (`logdf`, `g`, `H`, and for SLV `kappa`, `volOfVol`,
`leverage:i:j`), and keep their standalone pricing and calibration APIs
unchanged; the hybrid adapter only adds the co-evolution path.

Each component declares its own intra-block factor correlations through
`HybridComponentData_::FactorCorrelations()` (the GSR volatility correlations, and
the SLV driver correlation extended with an independent bridge row), and
`AssembleHybridCorrelation(name, components, links)` builds the global constant
correlation from those blocks plus explicit cross-component links (unlisted
cross entries are independent). This removes the mismatch-by-handshake error
class; a directly supplied correlation block is still validated against the
kernel at `Init`.
Both rate components insert their rate breakpoints into the shared timeline;
SLV also inserts leverage breakpoints. Breakpoints must fall on whole ACT/365
days. Plain GSR integrates the bank account exactly on each constant-parameter
interval, including its independent bridge noise. SLV subdivides by its
`maxStep` because its Euler scheme evaluates piecewise kernels at step starts.

The model data, local-vol surface, and each typed component are serializable. `CreateModel` also
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
output slots. A local-vol component inserts internal time steps up to its
configured `maxStep`; rate components with an evaluation date use whole ACT/365
days on this internal grid and reject `maxStep` below one day. `Init`
samples initial logDF once per simulation time and precomputes each step's
initial integrated carry. Deterministic-rate paths use
`N(t)=exp(-logDF(t))`; stochastic rate paths use their realized numeraire and
conditional rate observations. The path loop does no name parsing or matrix factorization.
`SimDim()` is the number of internal positive time
steps multiplied by `NumFactors()`, which includes internal bridge drivers.
Independent Gaussian inputs are time-major, with registered factors in sorted
label order, followed by internal drivers within each step. `FactorNames()`
lists only the registered factors used by the correlation matrix.
The output `Sample_::observations_` follows the requested name order;
`Sample_::spot_` remains the first equity's compatibility field.
The `script_mc_perf --correlated-bs` benchmark compares the constant-rate and
logDF hybrid path loops at 100,000 paths, 12 steps, and two assets; model
initialization is outside its timed section.

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
`div:EQ[...]` for each equity, plus `rate:USD` for a constant domestic rate.
For a term structure, only nonzero-time logDF nodes are active AAD parameters;
their labels are `logdf:USD:1`, `logdf:USD:2`, and so on in input order.
The anchor node is fixed at zero. These parameters are owned by their
components; cloned workers receive independent
AAD parameter addresses. Local-vol equity exposes `lvol:EQ[...]:i:j` grid
risks and stochastic rate components contribute their curve, volatility and SLV
risks. Correlations are passive inputs and have no AAD risk labels. `NumeraireIsDeterministic()`
reports the selected rate component's behavior.

`ValueByMonteCarlo` and `ExplainScriptValuation` accept `HybridModelData_`.
Each future `FIX(EQ[...])` binds independently to its named component; requests
on one date may use either order. Historical fixings retain their own index and
never fall back to simulated values. A multi-equity `SPOT()` requires
`ScriptProductSettings_::defaultIndex_`. Stochastic rate components supply
future `IR[...]` observations; FX remains unsupported. See [`dal-cpp/examples/hybrid_script/`](../../dal-cpp/examples/hybrid_script/)
for the runnable two-equity C++ example
and [Python example](../../dal-python/examples/hybrid_script.py).

The deterministic-rate hybrid supports LSM early exercise with either
one `defaultIndex_` regressor or up to three explicit `regressionFeatures_`
(`EQ[...]` model outputs or `VAR[...]` scalar script states). The selected
coordinates feed training, frozen hard pricing, and fuzzy AAD replay;
`RetrainedBump` uses each component's parameter constraints. The linked C++
and Python examples above include a two-state Bermudan exchange claim.
