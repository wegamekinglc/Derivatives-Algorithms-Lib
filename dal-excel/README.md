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
`CALIBRATE.SINGLECURVE` or `CALIBRATE.XCCYMARKET`; result accessors return either
diagnostics or a curve handle. The [public API guide](../docs/public-api.md#excel)
lists the primary worksheet families.

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
as handles. Evaluation date and fixings are process-wide DAL state. Worksheet
failures are returned as error text annotated with the failing argument.

DAL is distributed under the repository [MIT license](../LICENSE).
