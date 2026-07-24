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

| Header family                              | Purpose                                          |
|--------------------------------------------|--------------------------------------------------|
| `global.hpp`                               | Runtime initialization and evaluation date       |
| `script.hpp`, `models.hpp`, `value.hpp`    | Script product, model, and Monte Carlo workflow  |
| `random.hpp`                               | Pseudo-random and Sobol matrix fills             |
| `curveprotocol.hpp`, `curveinstrument.hpp` | Curve conventions and quoted instruments         |
| `curvedata.hpp`, `curvespec.hpp`           | Curve construction and single/staged calibration |
| `xccycalibration.hpp`                      | Cross-currency calibration                       |
| `interp.hpp`                               | Linear interpolation helper                      |
| `repository.hpp`                           | Host-environment repository operations           |

The [public API guide](../docs/public-api.md#c) lists the entry points and gives a
minimal valuation example.

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

## Bindings

`dal-python` and `dal-excel` build on this facade. They may use core types needed
to bind the exposed signatures, so a public-facade change should be checked across
C++, Python, and Excel surfaces together.

DAL is distributed under the repository [MIT license](../LICENSE).
