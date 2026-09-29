# Local Volatility in Hybrid Models

`LocalVolSurfaceData_` stores a reusable local volatility grid independently of
spot, dividends, and rates. A `HybridLocalVolEquityData_` attaches it to an
`EQ[...]` asset. The existing `HybridModelData_` combines that asset with one
domestic rate component and a named factor correlation matrix. Use a
deterministic rate for BS-style dynamics or `HybridGSRRateData_` for stochastic
GSR rates. There is one domestic numeraire for all components.

## Surface data

The surface takes positive, strictly increasing spot nodes; nonnegative,
strictly increasing ACT/365 time nodes; and a `[spot, time]` matrix of finite,
nonnegative instantaneous volatilities. It interpolates linearly in time and
log spot and extrapolates flat beyond either axis. A one-by-one grid defines a
constant local volatility. `NewLocalVolSurfaceData` constructs it directly;
`NewLocalVolSurfaceDataFromIVS` applies the existing `DupireCalib` inversion to
an `AAD::IVS_` and stores the calibrated grid. See [Dupire](dupire.md) for the
inversion's finite-difference and tail conventions.

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
axis. A smaller `maxStep` generally reduces discretization error at greater
simulation cost. `SimDim()` includes the internal steps times the number of
Brownian factors.

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
inputs. The implementation supports one domestic currency and one GSR rate
factor; foreign rates, FX quanto adjustments, and stochastic dividend yields
are outside this model.

## Design references

The standalone surface and flat boundary behavior follow the same broad
separation used by QuantLib's
[fixed local-vol surface](https://github.com/lballabio/QuantLib/blob/master/ql/termstructures/volatility/equityfx/fixedlocalvolsurface.hpp).
The composition remains in DAL's existing Hybrid framework, in the spirit of
DiffFusion's
[composite model](https://github.com/frame-consulting/DiffFusion.jl/blob/main/src/models/hybrid/CompositeModel.jl).
