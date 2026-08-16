# DAL Core C++ Library

`dal-cpp` is the quantitative engine of DAL. It builds the `DAL::cpp` target and
contains math, AAD, curves, models, scripting, Monte Carlo, PDEs, random
generation, storage, and concurrency.

## Layout

| Path          | Contents                                                     |
|---------------|--------------------------------------------------------------|
| `dal/`          | Core headers and implementations, organized by domain        |
| `tests/`        | Google Test coverage for core behavior and numerical methods |
| `test-support/` | Test-only helpers shared across suites                       |
| `examples/`   | Runnable C++ examples                                        |
| `benchmarks/` | Opt-in native performance executables                        |
| `config/`     | Machinist interface and generation configuration             |
| `cmake/`      | Platform options and installed package configuration         |
| `externals/`  | Git-submodule dependencies and AAD backends                  |

Direct core consumers get the widest API surface and therefore track core source
changes. Applications that want construction and valuation helpers should also
use the [`DAL::public` facade](../dal-public/README.md).

## Build and Test

Use the repository [installation guide](../docs/installation.md). The core
development profile is:

```bash
cmake --preset core-dev
cmake --build build/core-dev --parallel
ctest --test-dir build/core-dev --output-on-failure
```

Run a focused core test from the build tree:

```bash
build/core-dev/dal-cpp/dal_cpp_tests --gtest_filter=CalibrationTest.*
```

Benchmarks are opt-in with `--benchmarks` on `build_linux.sh` or
`DAL_CPP_BUILD_BENCHMARKS=ON`. Machine-specific CPU tuning is separately opt-in
with `DAL_ENABLE_NATIVE_ARCH=ON`; portable builds leave it off.

## AAD and Concurrency

With all external backend options disabled, DAL uses its native AAD
implementation. XAD, CoDiPack, and Adept are selectable CMake alternatives.
Live AAD tape state is thread-local.

The process-wide thread pool starts lazily. Set a positive `DAL_NUM_THREADS`
before loading DAL to cap its logical capacity; the value is limited by detected
hardware concurrency. See [architecture](../docs/architecture.md#runtime-ownership)
for ownership rules.

## Generated Code

Machinist markup changes must update both core and Excel output:

```bash
cmake --build build/core-dev --target dal_generate
cmake --build build/core-dev --target dal_check_generated
```

Do not hand-edit files under `dal/auto/`. Commit generated output with the markup
that produced it.

## Documentation

- [Architecture](../docs/architecture.md)
- [Public API guide](../docs/public-api.md)
- [Methodology index](../docs/README.md)
- [Contributing](../CONTRIBUTING.md)

DAL is distributed under the repository [MIT license](../LICENSE).
