# Local Volatility in Hybrid Models

`LocalVolSurfaceData_` stores a reusable local volatility grid independently of
spot, dividends, and rates. A `HybridLocalVolEquityData_` attaches it to an
`EQ[...]` asset. The existing `HybridModelData_` combines that asset with one
domestic rate component and a named factor correlation matrix. Use a
deterministic rate for BS-style dynamics, `HybridGSRRateData_` for Gaussian
rates, or `HybridGSRSLVRateData_` for stochastic local volatility in rates.
There is one domestic numeraire for all components.

## Surface data

The surface takes positive, strictly increasing spot nodes; nonnegative,
strictly increasing ACT/365 time nodes; and a `[spot, time]` matrix of finite,
nonnegative instantaneous volatilities. It interpolates linearly in time and
log spot and extrapolates flat beyond either axis. A one-by-one grid defines a
constant local volatility. `NewLocalVolSurfaceData` constructs it directly;
`NewLocalVolSurfaceDataFromIVS` applies the existing `DupireCalib` inversion to
an `AAD::IVS_` and stores the calibrated grid. See [Dupire](dupire.md) for the
inversion's finite-difference and tail conventions.

The [market-IV surface](market-iv-surface.md#analytic-derivatives-and-dupire)
can instead export an exact requested analytic Dupire grid. Archive v1 retains
only that grid, not its quotes, carry or continuous certificate. Flat-rate
convenience model construction does not automatically represent nonflat F/D;
drift/rates must match the calibration market, with grid and simulation
convergence checked separately.

The surface and its enclosing hybrid model use DAL's storable archive format.
In Python, `dal._dal._StorableToJson(surface)` and
`dal._dal._StorableFromJson(payload)` provide a JSON round trip. The archive
contains the numeric grid; it does not retain a live link to the input IVS.

## Dynamics and composition

For an asset with dividend yield $q$ and domestic numeraire $N$, each internal
step uses the log-Euler update

$$
\Delta\log S = \Delta\log N - q\Delta t
  - \tfrac12\sigma_{\mathrm{loc}}(t,S)^2\Delta t
  + \sigma_{\mathrm{loc}}(t,S)\sqrt{\Delta t}\,Z_S.
$$

With a deterministic rate component, $\Delta\log N$ is the integrated curve
carry. With GSR, it is that path's realized change in log numeraire; the same
Hybrid path supplies GSR's rate observations and correlated asset and rate
factors. Correlation is supplied by the existing named-factor provider. This
update makes the zero-dividend discounted asset a martingale at each step.
For nonzero local volatility, Monte Carlo prices depend on the time-step size.
The existing Dupire inversion assumes deterministic rates. A surface
calibrated from an IVS under that assumption may need joint recalibration to
match the same option smile after adding stochastic GSR rates and correlation.

The local-vol component's `maxStep` defaults to `1/12` year. `Allocate` inserts
internal steps between product dates, then maps requested observations back to
their original event dates. GSR compositions use whole ACT/365 calendar days
for inserted dates. A requested GSR product date must itself be on that daily
axis, and `maxStep` must be at least one day for these compositions. A smaller
supported `maxStep` generally reduces discretization error at greater
simulation cost. `SimDim()` includes the internal steps times the number of
Brownian factors.

Initialization prepares a time-interpolated volatility column for each internal
step, together with the initial log spot and square roots of step lengths.
Path evolution interpolates these columns directly in log spot. AAD records the
columns on the initialization prefix, so sensitivities still reach every surface
parameter. Reinitialize after changing parameters or starting a new recording;
prepared columns are immutable while paths are generated and can be shared by
double-precision workers.

## Construction

```cpp
auto surface = Dal::NewLocalVolSurfaceData(
    "equity_vol", {80.0, 100.0, 120.0}, {0.0, 1.0},
    Dal::Matrix_<>(3, 2, 0.20));
auto equity = Dal::NewHybridLocalVolEquityData(
    "equity", "EQ[A]", "USD", "W_EQ", 100.0, 0.01, surface);
auto rate = Dal::NewHybridGSRRateData("rate", "W_RATE", curve, gsrVol);
Dal::HybridSettings_ settings;
settings.domesticCurrency_ = "USD";
settings.components_ = {equity, rate};
Dal::Matrix_<> rho(2, 2, 0.0);
rho(0, 0) = rho(1, 1) = 1.0;
rho(0, 1) = rho(1, 0) = 0.30;
settings.correlation_ = Dal::Handle_<Dal::HybridCorrelationData_>(
    new Dal::HybridConstantCorrelationData_("rho", {"W_EQ", "W_RATE"}, rho));
auto modelData = Dal::NewHybridModelData("gsr_local_vol", settings);
```

`curve` and `gsrVol` above are `Handle_<GSRCurveData_>` and
`Handle_<GSRVolData_>`; see [GSR](gaussian-short-rate.md). For a deterministic
BS-style model, `NewBSLocalVolModelData` accepts an existing `BSModelData_`
and replaces its scalar volatility with the local-vol surface while retaining
spot, dividend yield, and flat rate. The Python equivalents are
`LocalVolSurfaceData_New`, `HybridLocalVolEquityData_New`,
`HybridGSRRateData_New`, and `BSLocalVolModelData_New`. See the
[Python example](../../dal-python/examples/015.local_vol_hybrid.py).

The AAD parameter list includes asset spot and dividend yield, every local
volatility grid value (`lvol:EQ[A]:spot_row:time_column`), and the selected
rate component's parameters. Correlation entries and grid axes are fixed
inputs. The implementation supports one domestic currency with one- or
multi-factor GSR rates, including a GSR SLV rate component; foreign rates,
FX quanto adjustments, and stochastic dividend yields
are outside this model.

## Internal market-grid preparation

`dal/model/marketivlocalvolinternal.hpp` prepares an immutable source from a
successful `MarketIvDupireResult_`. It checks the requested axes and every
authoritative node, shares the immutable fit snapshot, and copies the surface,
grid request and nodes. Detached configuration, carry and warning records remain
valid after the caller destroys its inputs. Numeric capacity plans precede the
copies and retain the existing fit/grid envelopes; these ledgers exclude dynamic
strings and are not process-memory limits.

The source records JCS/SHA-256 identities for the full market/fit/grid content
and resolved model settings. Its carry uses endpoint differences of
`L=log(F/S0)` and `H=log(D)`: `R=H(a)-H(b)` and
`Q=R-(L(b)-L(a))`. Knot derivatives use the right segment; the tail keeps the
last log slope. An internal passive equity component prepares `R-Q` and reuses
the existing local-vol evolution with zero scalar dividend. Its timeline knots
include both carry terms and surface times. Domestic discounting uses the same
`H` with `HybridLogDfRateData_` and `LOG_LINEAR`.

## Owning market model in core C++

`BuildMarketIvLocalVolModelData(name, grid, settings)` in
`dal/model/marketivlocalvol.hpp` returns a dedicated owning `ModelData_` from
one successful market Dupire result. `MarketIvLocalVolModelSettings_` requires
`index_`, `currency_`, `factor_` and a finite positive `horizon_` in years.
These declare one plain `EQ[...]` asset, its domestic currency and one named
Brownian factor; the snapshot supplies spot and carry, but does not identify
the asset or currency. `maxStep_` defaults to 1/52 year and supports subday
steps on the ACT/365 axis without dated-GSR whole-day restrictions. Convert
the derived handle explicitly to `Handle_<ModelData_>` when calling the
existing core `Script::MCSimulation<double>` entry. Pass a valuation date
matching the snapshot, including for prepared requests; an omitted date follows
the existing date resolution and must still match. This core entry supports
passive compiled or tree valuation, domestic DF observations and delayed
payments, with the existing RNG and Brownian-bridge choices.

The wrapper keeps sample definitions alive, shares immutable source data and
prepares private delegate state when cloned. It merges carry, surface, product
and discount-maturity anchors and simulates only to the request's required
time. All requested event, observation, payment and DF maturity times must fit
within `horizon_`. `CreateModel<AAD::Number_>`, `enableAad_`, native/quote risk
and weighted/Jacobian replay are rejected before sampling or tape recording;
curvature and segmented risk are unavailable. EXERCISE is rejected before
training or sampling. The new model rejects archives; a standalone grid archive
cannot restore its source. Existing Hybrid native AAD and archives retain their
own contracts.

`Configuration()`, `Summary()`, `CarryTerms()`, `Warnings()` and
`ConstructionExecution()` return detached records. Warnings retain fit
underidentification and near ties, mark the first local-vol column's extension
to zero, and flag horizons beyond the last grid time or market carry term.
Local-vol tails remain flat; market carry retains the last log slope. These
warnings do not certify extrapolated prices. `MarketIvLocalVolError_` carries
`code_`, `stage_`, `field_`, `expected_` and `actual_`; for example,
`InvalidModelEvaluationDate`, `MarketIvLocalVolHorizonExceeded`,
`MarketIvLocalVolBudgetExceeded` and `DerivativeUnavailable` identify the
rejected request and field.

`Script::InspectMarketIvLocalVolExecution` in `dal/script/marketivlocalvol.hpp`
runs the same preparation without RNG or paths and returns a separate owning
report. Workspace admission uses checked conservative capacity envelopes, then
tracks retained and peak preparation allocations and worker normal/path buffers.
`maxInternalSteps_` and `maxSimulationDimension_` each default to 100000;
the selected RNG's smaller limits still apply. `numericPayloadBudgetBytes_`
defaults to 64 MiB and `workspaceBudgetBytes_` to 256 MiB. Zero limits deny
allocation or work. Horizon, step, dimension and workspace admission precede
large preparation allocations and sampling.
The workspace ledger includes vector metadata, mappings, prepared columns,
carry, numeraires and discounts; strings, payoff evaluators and RNG internals
are excluded. It is not an RSS limit. Preparation query counts describe the
fixed curve evaluations; path generation makes no market-curve queries.

These core checks establish construction and execution contracts. Independent
Black/smile/grid/MC accuracy qualification remains incomplete; fast checks do
not establish it. Facade, Python and Excel market-model factories, dedicated
runnable examples, formal performance acceptance and live Windows worksheet
validation are not yet delivered.

## Design references

The standalone surface and flat boundary behavior follow the same broad
separation used by QuantLib's
[fixed local-vol surface](https://github.com/lballabio/QuantLib/blob/master/ql/termstructures/volatility/equityfx/fixedlocalvolsurface.hpp).
The composition remains in DAL's existing Hybrid framework, in the spirit of
DiffFusion's
[composite model](https://github.com/frame-consulting/DiffFusion.jl/blob/main/src/models/hybrid/CompositeModel.jl).
