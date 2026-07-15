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
`CALIBRATE.JOINTXCCY`; result accessors return diagnostics, matrices, ranges, or
curve handles. The [public API guide](../docs/public-api.md#excel) lists the
primary worksheet families.

## Resettable and Joint XCCY Functions

`XCCYRESETCONVENTION.NEW` creates the business-day and timestamp convention for
FX resets. `CROSSCURRENCYSWAPCONFIG.NEW` combines it with the currency pair, leg
and index conventions, `FIXED` / `RESETTABLE` / `MARK_TO_MARKET`, and explicit
domestic and foreign rate-fixing identities. Pass that handle to
`CROSSCURRENCYSWAP.CONFIG.NEW` to create the quoted instrument.

`MARKETFIXINGSNAPSHOT.NEW` takes parallel index-name, fixing-time, and value
ranges and returns one immutable snapshot handle. The arrays must have equal
length, timestamps must be valid, and observations must be positive and finite.
The snapshot can contain both rate and canonical FX names such as
`FX[EUR/USD]`.

`CALIBRATE.JOINTXCCY` performs one domestic/foreign/basis solve. Its result
supports dedicated handle getters:

- `JOINTXCCYCALIBRATIONRESULT.GET.DOMESTICBLOCK`
- `JOINTXCCYCALIBRATIONRESULT.GET.FOREIGNBLOCK`
- `JOINTXCCYCALIBRATIONRESULT.GET.BASISCURVE`

`JOINTXCCYCALIBRATIONRESULT.GET` returns matrix views selected by
`fxForwards`, `marketRates`, `modelRates`, `residuals`, `jacobian`,
`parameterRanges`, or `residualRanges`. The generated HTML under `auto/` is the
exact argument and settings-key reference.

## Layout and Generated Registration

| Path | Contents |
|------|----------|
| `src/` | Handwritten Excel conversion, repository, and public-function implementations |
| `auto/` | Machinist-generated registration wrappers and HTML function help |
| `examples/` | Example workbooks |

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
