# Installation Guide

This is the canonical setup guide for the DAL workspace. Component READMEs link
here instead of maintaining separate build recipes.

## Requirements

| Surface  | Requirements                                                                                 |
|----------|----------------------------------------------------------------------------------------------|
| Core C++ | Git with submodules, CMake 3.21+, a C++17 compiler, and a build tool                         |
| Linux    | GCC 13+ or Clang 18+; Make or Ninja                                                          |
| Windows  | Visual Studio 2022 toolchain; Ninja for the supplied presets                                 |
| Python   | CPython 3.10-3.13 with development headers; `uv` recommended                                |
| Web      | Python 3.13+, `uv`, npm, Node.js `^20.19.0` or `>=22.12.0`, and a built native `dal` package |
| Excel    | Windows and Microsoft Excel; build the XLL with `DAL_BUILD_EXCEL=ON`                         |

Clone all submodules:

```bash
git clone --recursive git@github.com:wegamekinglc/Derivatives-Algorithms-Lib.git
cd Derivatives-Algorithms-Lib
```

For an existing non-recursive clone:

```bash
git submodule update --init --recursive
```

## Linux Workspace Build

The supported automated workflow is:

```bash
bash ./build_linux.sh
```

The default is a core development build: core and public C++ libraries, tests,
and examples, with Python and benchmarks disabled. It configures
`build/Release-linux`, installs to `build/stage/Release-linux`, and runs CTest.
It does not regenerate tracked Machinist output unless requested.

### Script options

| Option         | Effect                                                               |
|----------------|----------------------------------------------------------------------|
| `--full`       | Enable Python bindings and benchmarks                                |
| `--benchmarks` | Enable native benchmark targets                                      |
| `--generate`   | Run the `dal_generate` target before the normal build                |
| `--coverage`   | Enable coverage and produce a report with an available coverage tool |

Examples:

```bash
bash ./build_linux.sh --full
bash ./build_linux.sh --benchmarks
bash ./build_linux.sh --generate
BUILD_TYPE=Debug bash ./build_linux.sh
```

When Python is requested, the script creates or reuses
`dal-python/.venv`, installs `pytest` and `numpy` if needed, and configures CMake
with that interpreter. Useful environment overrides are:

| Variable                 | Meaning                                                          |
|--------------------------|------------------------------------------------------------------|
| `BUILD_TYPE`             | `Release` by default; `Debug` selects the matching legacy preset |
| `DAL_BUILD_DIR`          | Override the build-tree path                                     |
| `DAL_INSTALL_DIR`        | Override the staging prefix                                      |
| `NUM_CORES`              | Override parallel build jobs                                     |
| `ADDITIONAL_CMAKE_FLAGS` | Append simple `-D...` cache overrides                            |
| `VERBOSE=1`              | Run CTest verbosely                                              |

For example, select the XAD backend explicitly:

```bash
ADDITIONAL_CMAKE_FLAGS="-DDAL_USE_XAD_AAD=ON" bash ./build_linux.sh
```

Use only one external AAD backend at a time. With XAD, CoDiPack, and Adept all
disabled, DAL uses its native AAD implementation.

## CMake Profiles

The named development profiles make build intent explicit:

| Preset         | Contents                                                                           |
|----------------|------------------------------------------------------------------------------------|
| `core-dev`     | Core/public C++, tests, and examples; no Python or benchmarks                      |
| `full-dev`     | Core development plus Python and benchmarks                                        |
| `distribution` | Portable release libraries and install packages; no tests, examples, or benchmarks |

Configure, build, test, and install the core profile with:

```bash
cmake --preset core-dev
cmake --build build/core-dev --parallel
ctest --test-dir build/core-dev --output-on-failure
cmake --install build/core-dev
```

The install prefix is `build/stage/<preset>`. The legacy `Release-linux`,
`Debug-linux`, `Release-windows`, and `Debug-windows` presets remain available.

Release optimization is target-scoped. Builds are portable by default;
machine-specific CPU instructions are opt-in:

```bash
cmake --preset core-dev -DDAL_ENABLE_NATIVE_ARCH=ON
```

Do not enable `DAL_ENABLE_NATIVE_ARCH` for artifacts that will run on machines
with an unknown CPU baseline.

### Common CMake options

| Option                     | Base default | Description                                                                           |
|----------------------------|--------------|---------------------------------------------------------------------------------------|
| `DAL_BUILD_PUBLIC`         | `ON`         | Build the public convenience facade                                                   |
| `DAL_BUILD_PYTHON`         | `OFF`        | Build the pybind11 module                                                             |
| `DAL_BUILD_EXCEL`          | `OFF`        | Build the Windows Excel add-in                                                        |
| `DAL_CPP_BUILD_TESTS`      | `ON`         | Build core tests                                                                      |
| `DAL_PUBLIC_BUILD_TESTS`   | `ON`         | Build public-facade tests                                                             |
| `DAL_CPP_BUILD_EXAMPLES`   | `ON`         | Build C++ examples                                                                    |
| `DAL_CPP_BUILD_BENCHMARKS` | `OFF`        | Build benchmarks                                                                      |
| `DAL_ENABLE_NATIVE_ARCH`   | `OFF`        | Tune Release code for the build machine                                               |
| `DAL_ENABLE_SANITIZERS`    | `""`         | Semicolon-separated sanitizer list for all targets (GCC/Clang only)                   |
| `DAL_USE_XAD_AAD`          | `OFF`        | Use XAD                                                                               |
| `DAL_USE_CODIPACK_AAD`     | `OFF`        | Use CoDiPack                                                                          |
| `DAL_USE_ADEPT_AAD`        | `OFF`        | Use Adept                                                                             |
| `MSVC_RUNTIME`             | `dynamic`    | MSVC-only C++ runtime: `static` for `/MT` (`/MTd` in Debug), otherwise `/MD` (`/MDd`) |

### Selecting an AAD backend

The native AADET backend is selected when all three external-backend options
are `OFF`. XAD, CoDiPack, and Adept are mutually exclusive; configuration
fails if more than one is enabled. For example, configure, build, and test a
separate CoDiPack tree with:

```bash
cmake --preset=Release-linux -S . -B build/Release-codipack \
  -DDAL_USE_XAD_AAD=OFF \
  -DDAL_USE_CODIPACK_AAD=ON \
  -DDAL_USE_ADEPT_AAD=OFF
cmake --build build/Release-codipack --parallel
ctest --test-dir build/Release-codipack --output-on-failure
```

CoDiPack recording is isolated by native thread-local storage: each operating
system thread owns its underlying CoDiPack tape and DAL wrapper, and that
storage is destroyed when the thread exits. This lifecycle does not depend on
the Python GIL. A `Number_`, `Tape_`, or tape position remains thread-affine;
create, record, propagate, and clear it on the same thread instead of moving it
to another thread.

## Windows C++ and Excel

From a Visual Studio 2022 developer shell with Ninja available:

```powershell
cmake --preset Release-windows
cmake --build build/Release-windows --parallel
ctest --test-dir build/Release-windows --output-on-failure
cmake --install build/Release-windows
```

`Release-windows` enables the Excel add-in. Use `Debug-windows` for a Debug
configuration. The staged prefix is `build/stage/Release-windows` or
`build/stage/Debug-windows`.

Both Windows presets set `MSVC_RUNTIME=static`, so every workspace target
links the static C++ runtime (`/MT`, or `/MTd` in Debug). `MSVC_RUNTIME` is a
workspace-level cache variable in the top-level `CMakeLists.txt`; it applies
only under MSVC, defaults to `dynamic` (`/MD`, or `/MDd` in Debug), and any
value other than `static` selects the dynamic runtime. The installed package
republishes the linked runtime as `DAL_CPP_MSVC_RUNTIME_LIBRARY` — see
[Installed CMake Packages](#installed-cmake-packages).

## Installed CMake Packages

A staged install contains headers, libraries, and relocatable package metadata
for both targets:

```text
include/
lib/
lib/cmake/dal-cpp/
lib/cmake/dal-public/
bin/                         # installed runtime/example targets when enabled
```

An out-of-tree consumer can use:

```cmake
find_package(dal-cpp 1.0 CONFIG REQUIRED)
find_package(dal-public 1.0 CONFIG REQUIRED)

add_executable(my_pricer main.cpp)
dal_cpp_apply_msvc_runtime(my_pricer)
target_link_libraries(my_pricer PRIVATE DAL::cpp DAL::public)
```

The core package publishes its runtime ABI as
`DAL_CPP_MSVC_RUNTIME_LIBRARY`. Call `dal_cpp_apply_msvc_runtime` for each
consumer target that links the installed static libraries. The helper applies
the matching configuration-aware `/MT`/`/MTd` or `/MD`/`/MDd` selection under
MSVC and is a no-op on other toolchains.

Point CMake at the staged prefix:

```bash
cmake -S /path/to/consumer -B /path/to/consumer/build \
  "-DCMAKE_PREFIX_PATH=/path/to/Derivatives-Algorithms-Lib/build/stage/<platform-preset>"
cmake --build /path/to/consumer/build
```

Replace `<platform-preset>` with the preset that produced the install, such as
`Release-linux` or `Release-windows`.

The repository's consumer smoke test is under `tests/installed-consumer/`. Run it
against an installed prefix with a separate build directory:

```bash
cmake \
  -DDAL_INSTALL_PREFIX="$PWD/build/stage/core-dev" \
  -DDAL_CONSUMER_BINARY_DIR="$PWD/build/installed-consumer-smoke" \
  -DDAL_BUILD_CONFIG=Release \
  -P tests/installed-consumer/run.cmake
```

For a generator that must be selected explicitly, add
`-DDAL_GENERATOR=Ninja` (or the required local generator).

## Python Bindings

To build and test Python as part of the workspace:

```bash
bash ./build_linux.sh --full
```

For an editable package in a chosen environment, first build the C++ staging
prefix, then install `dal-python` against that prefix:

```bash
bash ./build_linux.sh
cd dal-python
uv venv
source .venv/bin/activate
uv pip install -e ".[test]" "--config-settings=cmake.define.DAL_INSTALL_PREFIX=/absolute/path/to/Derivatives-Algorithms-Lib/build/stage/<platform-preset>"
python -m pytest tests -v
```

Replace `<platform-preset>` with `Release-linux` for the Linux workflow above
or the matching Windows preset when building under MSVC. On Windows, activate
with `.venv\Scripts\activate`. See the
[Python component guide](../dal-python/README.md)
for the exposed API and package layout.

## Web UI

The web application is not part of the CMake workspace. It is native-only: the
backend requires the compiled `dal` Python package and has no runtime stub
fallback. Backend unit tests inject a fake module only to isolate FastAPI wiring.

### Install the native package into the backend environment

Build a staged C++ install, then from `dal-web/backend`:

```bash
uv sync --inexact
uv pip install ../../dal-python \
  --config-settings=cmake.define.DAL_INSTALL_PREFIX=/absolute/path/to/Derivatives-Algorithms-Lib/build/stage/Release-linux
uv run --no-sync python -m app.native_runtime
cd ../..
```

`uv sync --inexact` preserves the manually installed local DAL package. The
preflight command checks the import and required binding symbols.

PowerShell uses the Windows stage:

```powershell
Set-Location dal-web/backend
uv sync --inexact
$stage = Resolve-Path ../../build/stage/Release-windows
uv pip install ../../dal-python --config-settings "cmake.define.DAL_INSTALL_PREFIX=$stage"
uv run --no-sync python -m app.native_runtime
Set-Location ../..
```

### Start both services

Run the launchers from the repository root.

Linux/macOS:

```bash
./dal-web/scripts/start.sh
./dal-web/scripts/stop.sh
```

The bash launcher requires `curl`, `grep`, `nohup`, and either `ss` or `lsof`;
the stopper requires `grep` and `lsof`. These commands are available through
standard Linux packages and macOS developer tooling/Homebrew.

Windows (PowerShell 7+):

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/start.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/stop.ps1
```

Both launchers run `uv sync --inexact`, perform the native-DAL preflight, and
then launch the backend with `uv run --no-sync` so dependency synchronization
cannot remove the local binding. They also install frontend packages, check
ports, wait for readiness, and smoke-test the Vite proxy.

- Frontend: <http://localhost:5173>
- Backend API docs: <http://127.0.0.1:8001/docs>

See [dal-web/README.md](../dal-web/README.md) for persistence, API, and service
details.

## Verification

The automated Linux script runs the configured CTest suite. For a manual build:

```bash
ctest --test-dir build/core-dev --output-on-failure
```

Run a focused Google Test from the build tree:

```bash
build/core-dev/dal-cpp/dal_cpp_tests --gtest_filter=CalibrationTest.*
build/core-dev/dal-public/dal_public_tests --gtest_filter=PublicApiTest.*
```

Python and web checks:

```bash
(cd dal-python && python -m pytest tests -v)
(cd dal-web/backend && uv run --no-sync pytest)
(cd dal-web/frontend && npm run build)
(cd dal-web/frontend && npm test)
./dal-web/scripts/setup-playwright.sh
(cd dal-web/frontend && npm run test:e2e)
```

## Code Generation

Machinist markup changes require both core and Excel generated output:

```bash
bash ./build_linux.sh --generate
```

For a configured build tree, the equivalent targets are:

```bash
cmake --build build/core-dev --target dal_generate
cmake --build build/core-dev --target dal_check_generated
```

`dal_generate` updates `dal-cpp/dal/auto/` and `dal-excel/auto/` from
`dal-cpp/config/dal.ifc` and `dal-cpp/config/dal.mgl`. Commit generated changes
with the markup that produced them. `dal_check_generated` reruns generation and
fails for either tracked diffs or new untracked files in those generated trees.

## Troubleshooting

### Missing submodule content

```bash
git submodule update --init --recursive
```

### Python cannot find DAL

Confirm that the editable install used an absolute staged
`DAL_INSTALL_PREFIX`, then check:

```bash
python -c "import dal; print(dal.__version__)"
```

For the web environment, run the actionable preflight:

```bash
cd dal-web/backend
uv run --no-sync python -m app.native_runtime
```

### Web ports are occupied

Use the matching stop script before restarting. The launchers report whether
ports 8001 or 5173 are already bound.

### A clean Debug build appears optimized

Use a Debug preset or `BUILD_TYPE=Debug`. Release-only optimization is selected
with configuration expressions; `DAL_ENABLE_NATIVE_ARCH` controls only optional
CPU-specific tuning.

## Next Steps

- Read the [architecture guide](architecture.md).
- Choose an entry point in the [public API guide](public-api.md).
- Follow [CONTRIBUTING.md](../CONTRIBUTING.md) for development workflow.
