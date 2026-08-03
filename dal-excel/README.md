# DAL Excel Add-in

`dal-excel` is the Windows Microsoft Excel XLL for DAL. It builds the
`DAL::excel` target on top of `DAL::public` and exposes worksheet functions for
products, models, Monte Carlo valuation, random generation, calendars,
interpolation, curve calibration, and repository management.

## Build

Use a Windows preset from a Visual Studio 2022 developer shell:

```powershell
cmake --preset Release-windows
cmake --build build/Release-windows --parallel
cmake --install build/Release-windows
```

The Windows preset enables `DAL_BUILD_EXCEL`. See the canonical
[installation guide](../docs/installation.md#windows-c-and-excel) for prerequisites
and test commands.

## Worksheet Model

Object constructors return repository handles. Pass those handles to later
worksheet calls:

```text
=PRODUCT.NEW("call", dates, events)
=BSMODELDATA.NEW("bs", 100, 0.20, 0.05, 0.02)
=MONTECARLO.VALUE(product_handle, model_handle, 65536, "sobol", FALSE, TRUE, 0.01)
```

Curve workflows use convention/instrument constructors followed by
`CALIBRATE.SINGLECURVE`, `CALIBRATE.XCCYMARKET`, or
`CALIBRATE.JOINTXCCY`; result accessors return diagnostics, supported matrix or
range views, and curve handles. Matrix visibility differs between staged and
joint XCCY as described below. The
[public API guide](../docs/public-api.md#excel) lists the primary worksheet
families.

## Resettable and Joint XCCY Functions

`XCCYRESETCONVENTION.NEW` creates the business-day and timestamp convention for
FX resets. `CROSSCURRENCYSWAPCONFIG.NEW` combines it with the currency pair, leg
and index conventions, `FIXED` / `RESETTABLE` / `MARK_TO_MARKET`, and explicit
domestic and foreign rate-fixing identities. Pass that handle to
`CROSSCURRENCYSWAP.CONFIG.NEW` to create the quoted instrument.

`MARKETFIXINGSNAPSHOT.NEW` takes parallel index-name, fixing-time, and value
ranges and returns one immutable snapshot handle. The arrays must have equal
length, timestamps must be valid, and observations must be finite. Canonical FX
observations must also be positive; rate fixings may be zero or negative.
Repeated `(index name, timestamp)` rows are rejected. The canonical name for a
domestic/foreign pair is `FX[foreign/domestic]`, for example `FX[EUR/USD]` for
USD/EUR. Lookup uses the requested direction when present and otherwise uses
the reciprocal of the reverse canonical observation. If both directions are
present at one timestamp, their product must differ from one by no more than
`1e-10`.

Staged `CALIBRATE.XCCYMARKET` returns a basis-curve handle plus fit diagnostics.
Its optional settings range accepts `jacobianMode` (`ANALYTIC` or `BUMPED`),
`computeForwardJacobian`, and `computeEffJacobianInverse`. Omitting these keys
selects `ANALYTIC`, `TRUE`, and `TRUE`.

`XCCYCALIBRATIONRESULT.GET` exposes `marketRates`, `modelRates`, `residuals`,
`maxAbsResidual`, `rmsResidual`, `instrumentNames`, `parameterKnotDates`,
`jacobian`, `effJacobianInverse`, `residualTolerance`, `jacobianScaling`,
`effJacobianInverseScaling`, `jacobianAvailability`, and
`effJacobianInverseAvailability`. The Jacobian rows follow instrument input
order; names are labels and may repeat. Its columns follow the basis-knot
right-forward parameter order. The effective inverse has the reversed axes.

The forward scaling label is `unscaled`; the effective-inverse scaling label is
`solver_scaled`. A raw decimal quote-bump vector therefore maps as
`dx = E * dq / residualTolerance`, where `E` is the retrieved effective
inverse. Availability is `available`, `not_requested`, or
`not_available_for_mode`; an unavailable matrix is returned empty.

`CALIBRATE.JOINTXCCY` performs one domestic/foreign/basis solve. Its result
supports dedicated handle getters:

- `JOINTXCCYCALIBRATIONRESULT.GET.DOMESTICBLOCK`
- `JOINTXCCYCALIBRATIONRESULT.GET.FOREIGNBLOCK`
- `JOINTXCCYCALIBRATIONRESULT.GET.BASISCURVE`

Joint settings can request both forward-Jacobian and effective-inverse
computation. `JOINTXCCYCALIBRATIONRESULT.GET` returns views selected by
`fxForwards`, `marketRates`, `modelRates`, `residuals`, `jacobian`,
`effJacobianInverse`, `parameterRanges`, or `residualRanges`; the range
selectors publish the matrices' named layout. The generated HTML under `auto/`
is the exact argument and settings-key reference.

## Layout and Generated Registration

| Path        | Contents                                                                      |
|-------------|-------------------------------------------------------------------------------|
| `src/`      | Handwritten Excel conversion, repository, and public-function implementations |
| `auto/`     | Machinist-generated registration wrappers and HTML function help              |
| `examples/` | Example workbooks                                                             |

Machinist generates both core storables and Excel wrappers from the shared
configuration. After markup changes, regenerate both trees:

```bash
cmake --build build/core-dev --target dal_generate
cmake --build build/core-dev --target dal_check_generated
```

Do not hand-edit `auto/*.inc` or `auto/*.htm`.

## Runtime State

Excel keeps storable objects in a host repository and passes them between cells
as handles. The evaluation date and legacy fixing store are process-wide DAL
state; `MARKETFIXINGSNAPSHOT.NEW` instead creates an immutable repository-held
snapshot. Worksheet failures are returned as error text annotated with the failing
argument.

DAL is distributed under the repository [MIT license](../LICENSE).
