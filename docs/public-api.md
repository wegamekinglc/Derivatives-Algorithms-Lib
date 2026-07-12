# DAL Public API Guide

DAL exposes the same main workflows through C++, Python, and Excel. This guide
identifies the supported entry points and their ownership contracts; it is not an
exhaustive reference for every core numerical type.

## API Layers

| Layer | Intended use | Compatibility contract |
|-------|--------------|------------------------|
| `DAL::cpp` | Direct access to quantitative algorithms and core types | Source-level core API; advanced consumers track core changes |
| `DAL::public` | Construction, calibration, scripted valuation, random generation, and repository helpers | Convenience facade; exposes core types and does not promise ABI isolation |
| Python `dal` | Python-friendly wrappers over the public facade | Supported names are those exported by `_dal` and `dal/api.py` |
| Excel XLL | Worksheet functions and repository handles | Supported worksheet names come from generated registrations |

Installed C++ consumers should link imported targets instead of copying library
paths. See the [installation guide](installation.md#installed-cmake-packages).

## C++

### CMake consumption

```cmake
find_package(dal-cpp 1.0 CONFIG REQUIRED)
find_package(dal-public 1.0 CONFIG REQUIRED)

add_executable(my_pricer main.cpp)
dal_cpp_apply_msvc_runtime(my_pricer)
target_link_libraries(my_pricer PRIVATE DAL::public)
```

`DAL::public` links `DAL::cpp` transitively. Link `DAL::cpp` directly when using
only core algorithms. The core package exports
`DAL_CPP_MSVC_RUNTIME_LIBRARY`; `dal_cpp_apply_msvc_runtime` applies that
configuration-aware ABI choice to a consumer target under MSVC and is a no-op
on other toolchains.

### Public facade headers

| Header | Main entry points |
|--------|-------------------|
| `<dal-public/src/global.hpp>` | `InitGlobalData`, `SetEvaluationDate`, `GetEvaluationDate` |
| `<dal-public/src/script.hpp>` | `NewScriptProduct`, `DebugScriptProduct` |
| `<dal-public/src/models.hpp>` | `NewBSModelData`, `NewDupireModelData` |
| `<dal-public/src/value.hpp>` | `ValueByMonteCarlo` |
| `<dal-public/src/random.hpp>` | Pseudo/Sobol constructors and uniform/normal matrix fills |
| `<dal-public/src/curveprotocol.hpp>` | Day-basis, tenor, collateral, rate-leg/index, and currency-pair builders |
| `<dal-public/src/curveinstrument.hpp>` | Deposit, FRA, future, swap, OIS, basis-swap, and cross-currency-swap builders |
| `<dal-public/src/curvedata.hpp>` | Piecewise-linear-forward, zero-rate, and curve-block builders |
| `<dal-public/src/curvespec.hpp>` | `CurveCalibrationSpecBuilder_`, `CalibrateSingleCurve`, `CalibrateMultiCurveBundle` |
| `<dal-public/src/xccycalibration.hpp>` | Cross-currency spec builder and `CalibrateXccyMarket` |
| `<dal-public/src/interp.hpp>` | Linear one-dimensional interpolation builder |
| `<dal-public/src/repository.hpp>` | Repository find, erase, and size helpers for a configured host environment |

The installed include path intentionally retains `dal-public/src/`. The facade
also uses core `Handle_`, `Date_`, curve, model, and diagnostics types directly.

### Sobol normal-draw policy

The public constructor is:

```cpp
Dal::NewSobolRSG(name, iPath, ndim = 1, precise = false, polish = false)
```

The two policy flags are independent and are forwarded unchanged. `polish`
controls whether the Acklam inverse-CDF result receives a Newton correction;
when polishing is enabled, `precise` selects the precise CDF instead of the fast
CDF for that correction. The default `false, false` path is Acklam-only, and the
precise-CDF correction requires `precise = true, polish = true`. Python
`dal.SobolRSG_New` and Excel `SOBOLRSG.NEW` use the same defaults and semantics.
See the [random methodology policy table](methodology/random.md#normal-draw-inverse-cdf-modes)
for all four combinations.

### Scripted Monte Carlo

This minimal pattern is exercised by the public API tests:

```cpp
#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

Dal::InitGlobalData();
Dal::SetEvaluationDate(Dal::Date_(2022, 9, 25));

const Dal::Vector_<Dal::Cell_> dates = {
    Dal::Cell_("STRIKE"),
    Dal::Cell_(Dal::Date_(2023, 9, 25)),
};
const Dal::Vector_<Dal::String_> events = {
    Dal::String_("100.0"),
    Dal::String_("call pays MAX(spot() - STRIKE, 0.0)"),
};

const auto product = Dal::NewScriptProduct(Dal::String_("call"), dates, events);
const auto model = Dal::NewBSModelData(Dal::String_("bs"), 100.0, 0.2, 0.05, 0.02);
const auto result = Dal::ValueByMonteCarlo(product, model, 1 << 16);
```

`ValueByMonteCarlo` requires `numPath > 0`. Its optional arguments select the
random generator, Brownian bridge, AAD risks, fuzzy smoothing, and compiled
script evaluator. The evaluation date is process-wide. Native Monte Carlo
valuation holds the valuation/mutation barrier for its full date-dependent
interval, so evaluation-date setters wait until valuation finishes while
getters remain available through the store lock. Monte Carlo valuations are
serialized within one process; callers that require independent concurrent
dates should use isolated processes.

### C++ curve calibration

The public zero-rate factory is:

```cpp
Dal::Handle_<Dal::DiscountCurve_> Dal::DiscountZeroRateNew(
    const Dal::String_& name,
    const Dal::String_& ccy,
    const Dal::Date_& anchorDate,
    const Dal::Vector_<Dal::Date_>& nodeDates,
    const Dal::Vector_<>& zeroRates,
    const Dal::DayBasis_& dayCount = Dal::DayBasis_("ACT_365F"),
    Dal::LogDfScheme_ scheme = Dal::LogDfScheme_::Value_::LOG_LINEAR,
    const Dal::Handle_<Dal::DiscountCurve_>& base = {});
```

`nodeDates` are strictly future dates and `zeroRates` are continuously compounded
decimal rates in matching order. The factory maps each node to
`logDF = -zeroRate * YearFrac(anchorDate,nodeDate)`, then applies the selected shared
log-DF interpolation and extrapolation scheme. The optional base is multiplied into the
curve, so the supplied rates describe a spread component. The result retains its
`DiscountZeroRate_` type and zero-rate bump coordinates when archived and restored.

The facade separates construction from solving:

1. Build conventions with `PeriodLength_New`, `DayBasis_New`,
   `RateLegConvention_New`, and `RateIndexConvention_New`.
2. Build quoted instruments with `DepositNew`, `FRANew`, `FutureNew`, `SwapNew`,
   `OISSwapNew`, or `BasisSwapNew`.
3. Fill `CurveCalibrationSpecBuilder_` and call `Build()`.
4. Call `CalibrateSingleCurve`, optionally selecting `CurveJacobianMode_`.
5. Read `CalibrationResult_::curve_` and its diagnostics.

For staged calibration, assemble `MultiCurveCalibrationSpec_` and call
`CalibrateMultiCurveBundle`. Cross-currency calibration has the analogous
`CrossCurrencyCalibrationSpecBuilder_` / `CalibrateXccyMarket` path. The full
methodology is in [yield-curve construction](methodology/yield_curve.md).

Set `parameterization_ = CurveParameterization_::Value_::ZERO_RATE` to calibrate future
zero-rate nodes. `initialGuess_` and `initialGuessPerNode_` are decimal continuously
compounded rates for this representation. Single, staged, and core joint calibration
support ZERO_RATE; only single and staged calibration are exposed by the public facade.

## Python

Import the installed package with:

```python
import dal
```

### Common workflows

| Workflow | Python entry points |
|----------|---------------------|
| Dates/global state | `Date_`, `Year`, `Month`, `Day`, `EvaluationDate_Set`, `EvaluationDate_Get` |
| Script products | `Product_New`, `Product_Debug` |
| Models | `BSModelData_New`, `DupireModelData_New` |
| Valuation | `MonteCarlo_Value` |
| Random generation | `PseudoRSG_New`, `SobolRSG_New`, `*_Get_Uniform`, `*_Get_Normal` |
| Calendar operations | `Holidays_`, `Is_BizDay`, `NextBizDay`, `PrevBizDay`, `Adjust` |
| Curves | `DiscountZeroRate_New`, convention/instrument builders, `CurveCalibrationSpecBuilder_`, `CalibrateSingleCurve`, `CalibrateMultiCurveBundle`, `CalibrateXccyMarket` |
| Convenience calibration | `calibrate_curve` from `dal/api.py` |

The basic valuation shape is:

```python
import dal

dal.EvaluationDate_Set(dal.Date_(2022, 9, 25))
product = dal.Product_New(
    ["STRIKE", dal.Date_(2023, 9, 25)],
    ["100.0", "call pays MAX(spot() - STRIKE, 0.0)"],
)
model = dal.BSModelData_New(100.0, 0.2, 0.05, 0.02)
result = dal.MonteCarlo_Value(product, model, 2**16, enable_aad=True)
```

`MonteCarlo_Value` requires `num_path > 0`. The binding releases the Python GIL
around native valuation, and `EvaluationDate_Get` / `EvaluationDate_Set` release
it before waiting on native synchronization. A setter waits for an in-progress
valuation; a getter can read the stable current date while valuation runs.

For precise-CDF-polished Sobol normal draws, pass both flags explicitly:

```python
rsg = dal.SobolRSG_New(i_path=0, ndim=3, precise=True, polish=True)
normals = dal.SobolRSG_Get_Normal(rsg, 1024)
```

### Matrix and Dupire surface input

`DoubleMatrix_` supports all of the following:

```python
surface = dal.DoubleMatrix_(3, 2, 0.20)
surface[1, 0] = 0.21

surface = dal.DoubleMatrix_([
    [0.24, 0.23],
    [0.21, 0.20],
    [0.22, 0.21],
])
```

Rows must be rectangular numeric sequences. `DupireModelData_New` expects a
spots-by-times matrix, so its shape must be
`len(spots) × len(times)`.

### Python curve calibration

`dal.calibrate_curve(...)` covers the common single discount-curve path. Use
`CurveCalibrationSpecBuilder_` directly for projection-curve inputs, staged
multi-curve calibration, or lower-level solver settings. Python enum names are:

- `CurveParameterization`: `PIECEWISE_LINEAR_FWD`,
  `PIECEWISE_CONSTANT_FWD`, `ZERO_RATE`, `LOG_DISCOUNT`;
- `CurveSolveMode`: `EXACT`, `APPROXIMATE`;
- `CurveJacobianMode`: `ANALYTIC`, `BUMPED`; and
- `LogDfScheme`: `LOG_LINEAR`, `LOG_CUBIC_NATURAL`, `MIXED`.

`ZERO_RATE` is supported by `CalibrateSingleCurve` and `dal.calibrate_curve`. Supply only
strictly-future knots; the anchor is internal and contributes no solver or Jacobian
column. The scalar `initialGuess_` is a decimal continuously compounded zero rate.
`dal.calibrate_curve(..., base_curve=...)` treats the calibrated zero rates as spreads
over that base.

Direct construction uses:

```python
curve = dal.DiscountZeroRate_New(
    "usd_zero", "USD", today, node_dates, zero_rates,
    day_count=dal.DayBasis_("ACT_365F"),
    log_df_scheme=dal.LogDfScheme.LOG_LINEAR,
    base=None,
)
```

The returned `DiscountZeroRate_` exposes read-only `anchor_date`, `node_dates`,
`zero_rates`, `day_count`, and `log_df_scheme` properties. Python exposes staged but not
core joint calibration.

See [dal-python/README.md](../dal-python/README.md) for package-focused examples.

## Excel

The Windows XLL exposes worksheet functions and stores constructed objects in an
Excel-side repository. Constructors return handles; pass those handles into later
functions rather than attempting to unpack native objects in cells.

### Script valuation

```text
=PRODUCT.NEW("call", dates, events)
=BSMODELDATA.NEW("bs", 100, 0.20, 0.05, 0.02)
=MONTECARLO.VALUE(product_handle, model_handle, 65536, "sobol", FALSE, TRUE, 0.01)
```

For a local-volatility model, use
`DUPIREMODELDATA.NEW(name, spot, rate, repo, spots, times, vols)`. The volatility
range must be a rectangular spots-by-times matrix. `MONTECARLO.VALUE` requires a
strictly positive path count.

`SOBOLRSG.NEW(name, i_path, n_dim, precise, polish)` uses the same independent
normal-draw flags as C++ and Python. Pass `TRUE, TRUE` for the precise-CDF Newton
correction; leaving both optional flags `FALSE` selects the Acklam-only default.

### Curve workflows

Primary worksheet families are:

| Purpose | Worksheet functions |
|---------|---------------------|
| Conventions | `PERIODLENGTH.NEW`, `DAYBASIS.NEW`, `RATELEGCONVENTION.NEW`, `RATEINDEXCONVENTION.NEW`, `COLLATERALTYPE.*` |
| Instruments | `DEPOSIT.NEW`, `FRA.NEW`, `FUTURE.NEW`, `SWAP.NEW`, `OISSWAP.NEW`, `BASISSWAP.NEW`, `CROSSCURRENCYSWAP.NEW` |
| Direct curves | `DISCOUNTPWLF.NEW`, `DISCOUNTZERORATE.NEW`, `CURVEBLOCK.NEW.SIMPLE` |
| Calibration | `CALIBRATE.SINGLECURVE`, `CALIBRATE.XCCYMARKET` |
| Results | `CALIBRATIONRESULT.GET`, `CALIBRATIONRESULT.GET.CURVE`, `XCCYCALIBRATIONRESULT.*` |
| Repository | `REPOSITORY.FIND`, `REPOSITORY.ERASE`, `REPOSITORY.SIZE` |

`DISCOUNTZERORATE.NEW` takes name, currency, anchor, future dates, and continuously
compounded decimal zero rates, with optional day count, log-DF scheme, and base handle.
`CALIBRATE.SINGLECURVE` accepts a two-column optional settings range. Supported keys
include curve name, target, solve mode, parameterization (`ZERO_RATE` included), log-DF
scheme, smoothing/tolerances, scalar initial guess, and evaluation budgets. Its optional
`baseCurve` input is the curve multiplied under the calibrated curve; it is distinct from
the `discountCurve` used to price a forward-curve calibration. Excel does not expose the
core joint-calibration API. Generated function help under `dal-excel/auto/*.htm` is the
argument-level catalog used by Excel registration.

See [dal-excel/README.md](../dal-excel/README.md) for build and add-in guidance.

## Error and State Conventions

- C++ failures use `Dal::Exception_` through `REQUIRE`/`THROW` paths.
- Python maps native exceptions to Python exceptions; invalid binding shapes and
  indices use `ValueError`/`IndexError` where appropriate.
- Excel returns worksheet error text annotated with the failing argument.
- Evaluation date and fixings are process-wide state. Evaluation-date mutation
  is serialized with native valuation; evaluation-date reads use the store lock
  and remain available during valuation. Fixings reads use the store mutex, but
  callers must externally serialize fixings mutation with other fixings access.
- AAD tapes are thread-local and must not be shared across recording frames.

For ownership details, see [architecture](architecture.md).
