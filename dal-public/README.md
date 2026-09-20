# DAL Public C++ Facade

`dal-public` builds `DAL::public`, a convenience facade over `DAL::cpp` for
scripted products, model construction, Monte Carlo valuation, random generation,
curve construction/calibration, and host repository helpers.

## Compatibility Contract

The facade is developer-facing but not ABI-isolated. Its headers expose core DAL
handles, curves, models, diagnostics, and value types, and installed includes
retain the current `<dal-public/src/...>` spelling. Consumers should build against
matching `DAL::cpp` and `DAL::public` packages.

The installed packages support normal CMake consumption:

```cmake
find_package(dal-public 1.0 CONFIG REQUIRED)
add_executable(my_pricer main.cpp)
dal_cpp_apply_msvc_runtime(my_pricer)
target_link_libraries(my_pricer PRIVATE DAL::public)
```

`dal-publicConfig.cmake` resolves `DAL::cpp` as a dependency. See the
[installation guide](../docs/installation.md#installed-cmake-packages) for staging
and out-of-tree consumer commands. On MSVC,
`dal_cpp_apply_msvc_runtime` applies the configuration-aware runtime ABI stored
in `DAL_CPP_MSVC_RUNTIME_LIBRARY`; it is a no-op on other toolchains.

## Surface

| Header family                                                                        | Purpose                                                      |
|--------------------------------------------------------------------------------------|--------------------------------------------------------------|
| `dal-public/src/global.hpp`                                                          | Runtime initialization and evaluation date                   |
| `dal-public/src/script.hpp`, `dal-public/src/models.hpp`, `dal-public/src/value.hpp` | Script product, model, and Monte Carlo workflow              |
| `dal-public/src/random.hpp`                                                          | Pseudo-random and Sobol matrix fills                         |
| `dal-public/src/curveprotocol.hpp`, `dal-public/src/curveinstrument.hpp`             | Curve conventions and quoted instruments                     |
| `dal-public/src/curvedata.hpp`, `dal-public/src/curvespec.hpp`                       | Curve construction and single/staged calibration             |
| `dal-public/src/curvepricing.hpp`                                                    | Typed rate-cashflow pricing, node risk, and quote-space DV01 |
| `dal-public/src/xccycalibration.hpp`                                                 | Cross-currency calibration                                   |
| `dal-public/src/interp.hpp`                                                          | Linear interpolation helper                                  |
| `dal-public/src/repository.hpp`                                                      | Host-environment repository operations                       |

The [public API guide](../docs/public-api.md#c) lists the entry points and gives a
minimal valuation example.

Script product and valuation settings expose named `FIX(index[,date])`
pricing in tree/compiled and price/AAD modes. `NewScriptProduct` keeps its
three-argument form and accepts a typed contract as a fourth argument;
`ValueByMonteCarlo` keeps its old three-to-eight-argument calls and adds a
required typed valuation argument with optional simulation settings.
Explicit dates avoid global date access, and non-null fixing snapshots are
authoritative even when empty. Product defaults identify legacy `SPOT()`;
model-sourced named fixings bind the model's `spot` output to the script's
own future FIX index by name, and several distinct future indices fail with
`MultipleModelIndices`. See the
[field/default contract](../docs/methodology/script_engine.md#public-c-settings)
and executable [settings example](examples/script_settings.cpp).

`DescribeScriptProduct` provides contract JSON /2 without historical, model,
or global-date access. `ExplainScriptValuation` provides valuation JSON /1
using default price preparation with history access but no workers; every
call prepares independently. `ExplainScriptSimulation` provides simulation
JSON /1 from a full double valuation with per-exercise-event statistics.
Product archive v2 preserves the original
contract/default index and retains a v1 reader. Legacy debug JSON /1 rejects
FIX/nonempty defaults with a Describe /2 migration hint. See
[archives and diagnostics](../docs/methodology/script_engine.md#product-archive-and-diagnostics).

`dal-public/src/curvespec.hpp` exposes generic joint multi-curve calibration
through `CalibrateJointMultiCurveBundle`.

Quote-space risk freezes single-curve, generic joint multi-curve, joint-XCCY, or staged-XCCY-basis
calibration provenance, verifies bound component fingerprints, and aggregates
portfolio price-per-decimal quote sensitivities plus DV01 without recalibration.
See the [quote-space DV01 contract](../docs/public-api.md#c-quote-space-dv01).

## Build and Test

The standard core profile builds and tests this component:

```bash
cmake --preset core-dev
cmake --build build/core-dev --parallel
ctest --test-dir build/core-dev --output-on-failure
```

For a focused public-facade check:

```bash
build/core-dev/dal-public/dal_public_tests --gtest_filter=PublicApiTest.*
```

The repository also carries an installed-package consumer under
`tests/installed-consumer/`.

The existing `ScriptApiConsumer` and `ScriptSettingsExample` CTests exercise
legacy/typed calls and the settings example. Standalone public tests against
installed core require Google Test, RapidJSON, and matching core curve-fixture
headers; these dependencies are private to tests and omitted when tests are
disabled. See [standalone setup](../docs/installation.md#standalone-public-facade-and-test-dependencies).

## Bindings

`dal-python` and `dal-excel` build on this facade. They may use core types needed
to bind the exposed signatures, so a public-facade change should be checked across
C++, Python, and Excel surfaces together.

Both bindings retain their legacy script signatures, and their old valuation
wrappers use default preparation through the public facade. Each binding also
projects the script settings and Describe/Explain diagnostics in its own form:
Python exposes keyword-only settings classes, `MonteCarlo_ValueWithSettings`,
`Product_Describe`, `ScriptValuation_Explain`, and `ScriptSimulation_Explain`;
Excel exposes immutable settings handles, `PRODUCT.NEWWITHSETTINGS`,
`MONTECARLO.VALUEWITHSETTINGS`, `PRODUCT.DESCRIBE`, `SCRIPTVALUATION.EXPLAIN`,
and `SCRIPTSIMULATION.EXPLAIN`.

DAL is distributed under the repository [MIT license](../LICENSE).
