# Phase 0: Dependency Inventory

## Summary

All three public-layer directories (`public/src/`, `public/excel/`, `public/swig/`) have significant direct dependencies on `dal/...` internal headers. This is the key challenge for Phase 3-5 extraction: public-layer code must be refactored to go through the stable `dal_public` API instead of reaching into `dal/` internals.

## public/src/ --> dal/ internal headers

| Public src file               | dal/ headers included                                  |
|-------------------------------|--------------------------------------------------------|
| `public/src/global.hpp`       | `<dal/time/date.hpp>`                                  |
| `public/src/globals.cpp`      | `<dal/platform/platform.hpp>`, `<dal/platform/strict.hpp>`, `<dal/storage/globals.hpp>` |
| `public/src/interp.cpp`       | `<dal/platform/platform.hpp>`, `<dal/platform/strict.hpp>`, `<dal/math/interp/interplinear.hpp>` |
| `public/src/interp.hpp`       | `<dal/math/vectors.hpp>`, `<dal/string/strings.hpp>`   |
| `public/src/models.cpp`       | `<dal/platform/platform.hpp>`, `<dal/platform/strict.hpp>` |
| `public/src/models.hpp`       | `<dal/model/blackscholes.hpp>`, `<dal/model/dupire.hpp>` |
| `public/src/random.cpp`       | `<dal/platform/platform.hpp>`, `<dal/platform/strict.hpp>` |
| `public/src/random.hpp`       | `<dal/string/strings.hpp>`, `<dal/math/random/sobol.hpp>`, `<dal/math/random/pseudorandom.hpp>` |
| `public/src/repository.cpp`   | `<dal/platform/platform.hpp>`, `<dal/platform/strict.hpp>`, `<dal/storage/_repository.hpp>` |
| `public/src/repository.hpp`   | `<dal/storage/storable.hpp>`, `<dal/utilities/exceptions.hpp>` |
| `public/src/script.cpp`       | `<dal/platform/platform.hpp>`, `<dal/platform/strict.hpp>` |
| `public/src/script.hpp`       | `<dal/script/event.hpp>`                               |
| `public/src/value.cpp`        | `<dal/platform/platform.hpp>`, `<dal/platform/strict.hpp>`, `<dal/model/factory.hpp>` |
| `public/src/value.hpp`        | `<dal/script/simulation.hpp>`                          |

### Observations
- Every .cpp file includes `<dal/platform/platform.hpp>` and `<dal/platform/strict.hpp>` -- this is a heavy internal dependency.
- Header files expose dal/ types directly: e.g., `interp.hpp` exposes `Vector_<>` and `String_` from dal/math/vectors.hpp.
- The `_repository.hpp` include (in repository.cpp) is from the `storage/` subdir that's explicitly excluded from `dal_library` build.

## public/excel/ --> dal/ internal headers

| Excel file                    | dal/ headers included                                  |
|-------------------------------|--------------------------------------------------------|
| `__platform.hpp`              | `<dal/platform/platform.hpp>`, `<dal/storage/_reader.hpp>`, `<dal/storage/_repository.hpp>`, `<dal/utilities/environment.hpp>`, `<dal/utilities/exceptions.hpp>` |
| `__interp.cpp`                | `<dal/math/interp/interplinear.hpp>`, `<dal/math/interp/interp2d.hpp>`, `<dal/math/interp/interpcubic.hpp>`, `<dal/math/smooth.hpp>` |
| `__random.cpp`                | `<dal/math/random/pseudorandom.hpp>`, `<dal/math/random/sobol.hpp>` |
| `__repository.cpp`            | `<dal/platform/platform.hpp>`                          |
| `__script.cpp`                | `<dal/script/event.hpp>`                               |
| `__value.cpp`                 | `<dal/math/matrix/matrixs.hpp>`, `<dal/platform/strict.hpp>` |
| `_excel.cpp`                  | `<dal/platform/platform.hpp>`, `<dal/platform/initall.hpp>`, `<dal/platform/strict.hpp>`, `<dal/math/cellutils.hpp>`, `<dal/storage/_repository.hpp>`, `<dal/utilities/exceptions.hpp>`, `<dal/utilities/numerics.hpp>`, `"dal/math/matrix/matrixutils.hpp"`*, `"dal/platform/optionals.hpp"`*, `"dal/string/strings.hpp"`*, `"dal/utilities/algorithms.hpp"`*, `"dal/utilities/dictionary.hpp"`* |
| `_excel.hpp`                  | `<dal/platform/platform.hpp>`, `<dal/math/matrix/matrixs.hpp>`, `<dal/platform/optionals.hpp>`, `<dal/storage/_reader.hpp>`, `<dal/storage/storable.hpp>`, `<dal/utilities/dictionary.hpp>`, `<dal/utilities/environment.hpp>` |

*Note: `_excel.cpp` uses BOTH angle-bracket and quoted includes for dal/ headers. Quoted `"dal/..."` includes (lines 84-88) are particularly unusual in a project that otherwise standardizes on `<dal/...>`.

### Observations
- `public/excel/` is the most deeply entangled layer. It includes dal/platform, dal/math, dal/script, dal/storage, and dal/utilities headers.
- Excel `__*.cpp` files each include `<public/excel/__platform.hpp>` which pulls in even more dal/ dependencies transitively.
- Excel files also include `public/src/*.hpp` (the public wrapper layer) -- creating a chain: excel -> public/src -> dal/
- Excel also includes `public/auto/MG_*_public.inc` generated files for Excel function stubs.

## public/swig/ --> dal/ internal headers

| SWIG file                     | dal/ headers included (in `%{ %}` blocks)              |
|-------------------------------|--------------------------------------------------------|
| `cell.i`                      | `<dal/math/cell.hpp>`                                  |
| `dal.i`                       | `<dal/platform/platform.hpp>`                          |
| `date.i`                      | `<dal/time/date.hpp>`                                  |
| `handle.i`                    | `<dal/platform/platform.hpp>`                          |
| `init.i`                      | `<dal/platform/initall.hpp>`                           |
| `matrix.i`                    | `<dal/math/matrix/matrixs.hpp>`                        |
| `strings.i`                   | `<dal/string/strings.hpp>`                             |

### Observations
- SWIG files directly include dal/ headers in `%{ ... %}` (raw C++ code) blocks.
- `dal.i` is the main entry point; it `%include`s init.i, handle.i, date.i, cell.i, global.i, strings.i, matrix.i, script.i, models.i, value.i, random.i.
- The SWIG dependency chain is: swig -> dal/ internal headers, not swig -> public/src/. This means Python bindings currently bypass the public API layer entirely.

## public/python/ --> dal/ headers

**None found.** `public/python/` contains only Python packaging files (setup.py, setup.cfg, MANIFEST.in, requirements.txt, README.md) with no C++ includes.

## Public-layer self-references (within public/)

All `public/src/*.cpp` files reference their corresponding `.hpp` via `<public/src/...>`:
- `globals.cpp` -> `<public/src/global.hpp>`
- `interp.cpp` -> `<public/src/interp.hpp>`
- `models.cpp` -> `<public/src/models.hpp>`
- `repository.cpp` -> `<public/src/repository.hpp>`
- `script.cpp` -> `<public/src/script.hpp>`
- `value.cpp` -> `<public/src/value.hpp>`

All `public/excel/__*.cpp` files reference `<public/excel/__platform.hpp>` and `<public/src/...>`:
- `__global.cpp` -> `<public/excel/__platform.hpp>`, `<public/src/global.hpp>`
- `__interp.cpp` -> `<public/excel/__platform.hpp>`, `<public/src/interp.hpp>`
- etc.

## Risk Assessment for Refactoring

1. **HIGH**: `public/excel/` has 20+ distinct dal/ header dependencies -- the most entangled layer.
2. **MEDIUM**: `public/src/` exposes dal/ types in its public headers (Vector_<>, String_, etc.), breaking the abstraction wall.
3. **MEDIUM**: SWIG bypasses public API entirely, directly including dal/ headers.
4. **LOW**: `public/python/` has no C++ includes -- already clean.
