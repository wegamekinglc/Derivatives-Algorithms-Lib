# Phase 0: Build Targets and Directory Ownership

## Current Build Process

The build is driven by `build_linux.sh`. Steps:

1. Build Machinist code-gen tool from `externals/machinist/`
2. Run code generation: `Machinist -c config/dal.ifc -l config/dal.mgl -d ./dal` and `... -d ./public`
3. Configure with `cmake --preset Release-linux` (installs to repo root)
4. Build with `make -j<N>`
5. Install with `make install` (libs to `lib/`, bins to `bin/`, headers to `include/`)
6. Run `bin/test_suite` if SKIP_TESTS=false

Build type: Release, static libraries (BUILD_SHARED_LIBS=OFF on Linux via preset).

## CMake Targets

### dal_library (defined in dal/CMakeLists.txt)
- Type: static library (`libdal.a`)
- Sources: all `.h`, `.hpp`, `.cpp`, `.inc` under `dal/` EXCEPT `dal/auto/` and `dal/storage/_repository.*`
- Links: external AAD library (Adept by default)
- Install: headers to `include/dal/`, library to `lib/`, export set `DALTargets`

### dal_public (defined in public/src/CMakeLists.txt)
- Type: static library (`libdal_public.a`)
- **CRITICAL DUPLICATE COMPILATION**: When `BUILD_SHARED_LIBS=OFF` (current default), dal_public globs `dal/*.cpp` files directly:
  ```cmake
  file(GLOB_RECURSE PUBLIC_FILES "*.hpp" "*.cpp" "${PROJECT_SOURCE_DIR}/dal/*.h" 
       "${PROJECT_SOURCE_DIR}/dal/*.hpp" "${PROJECT_SOURCE_DIR}/dal/*.cpp")
  ```
  This means every dal/ source file is compiled TWICE: once in dal_library and once in dal_public.
  When `BUILD_SHARED_LIBS=ON`, dal_public links dal_library instead.
- Sources: `public/src/*.hpp`, `public/src/*.cpp`
- Links: AAD library directly when static (redundant with dal_library)
- Install: headers to `include/public/src/`, library to `lib/`

### dal_excel (defined in public/excel/CMakeLists.txt, conditional on DAL_HAS_EXCEL and Windows)
- Not built on Linux

### test_suite (defined in tests/CMakeLists.txt)
- Type: executable (`bin/test_suite`)
- Sources: all `.hpp` and `.cpp` under `tests/`
- Links: `dal_library`, `gtest`, `gtest_main`, `gmock`, `gmock_main`, plus AAD backend
- Install: to `bin/`

### Example programs (in examples/, not a single target)
- Each example subdirectory has its own CMakeLists.txt with `add_executable`
- All link `dal_library`

## Directory Ownership Mapping

| Current Directory    | Future Home    | Contents                                                                    |
|----------------------|----------------|-----------------------------------------------------------------------------|
| `dal/`               | `dal-cpp`      | Core library: math, model, curve, indice, risk, concurrency, storage, auto |
| `public/src/`        | `dal-public`   | Public C++ API wrapper layer                                                |
| `public/swig/`       | `dal-python`   | SWIG interface files for Python bindings                                    |
| `public/python/`     | `dal-python`   | Python packaging (setup.py, dal package)                                    |
| `public/excel/`      | `dal-excel`    | Excel add-in interface code                                                 |
| `tests/`             | `dal-cpp`      | All C++ unit tests (initially; later split)                                 |
| `examples/`          | `dal-cpp`      | Example programs                                                            |
| `config/`            | Top-level      | Codegen source of truth (dal.ifc, dal.mgl)                                  |
| `externals/`         | `dal-cpp`      | External dependencies (XAD, CoDiPack, Adept, gtest, rapidjson, machinist)  |

## Tech Debt to Resolve

1. **Duplicate compilation**: `dal-public` statically compiles all `dal/*.cpp` again. Fix: always link `DAL::cpp`.
2. **No CTest registration**: `test_suite` is built but not registered with CTest. Fix: add `gtest_discover_tests()`.
3. **No enable_testing()**: Missing at top-level CMakeLists.txt.
4. **AAD library linked in two places**: Both dal_library and dal_public link the external AAD library; should only be dal_library.
