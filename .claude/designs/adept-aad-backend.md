# Adept AAD Backend — Technical Design

> **Historical document.** Written before the multi-project refactor. Path references in the file table (e.g. `dal/CMakeLists.txt`, `public/src/CMakeLists.txt`, `tests/CMakeLists.txt`) reflect the old layout; the equivalents now live under `dal-cpp/`, `dal-public/`, and `dal-cpp/tests/`. The Adept backend itself works unchanged.


## Summary
Add Adept as a third external AAD backend behind the existing `Dal::AAD` free-function API. The rest of DAL should continue to use `AAD::Number_`, `Tape()`, `PutOnTape`, `Adjoint`, `Value`, and propagation helpers without backend-specific code.

## Affected Files
| File                        | Action | Purpose                                     |
|-----------------------------|--------|---------------------------------------------|
| `CMakeLists.txt`            | Modify | Add `DAL_USE_ADEPT_AAD` selection           |
| `dal/CMakeLists.txt`        | Modify | Link `dal_library` to `adept` when selected |
| `public/src/CMakeLists.txt` | Modify | Link public/static fold build to `adept`    |
| `tests/CMakeLists.txt`      | Modify | Link `test_suite` to `adept`                |
| `examples/*/CMakeLists.txt` | Modify | Link examples to `adept`                    |
| `dal/math/aad/tape.hpp`     | Modify | Add Adept tape wrapper                      |
| `dal/math/aad/tape.cpp`     | Modify | Add Adept clear/mark/rewind/propagation     |
| `dal/math/aad/expr.hpp`     | Modify | Add Adept `Number_` alias and math API      |
| `dal/math/aad/aad.hpp`      | Modify | Treat Adept as external backend             |
| `dal/platform/initall.cpp`  | Modify | Report Adept backend                        |
| `examples/aad/aad.cpp`      | Modify | Report Adept in example output              |

## Design Decisions
- **Decision:** Use `using Number_ = adept::adouble` directly.
  Adept active values require an active stack before construction, so DAL AAD entry points that construct active model objects explicitly activate the thread-local tape first.
- **Decision:** Derive a DAL tape helper from `adept::Stack`.
  Adept exposes full reverse propagation but not partial propagation to a saved DAL mark. A small derived helper can record and restore protected statement/operation positions and run the same reverse loop over a selected range.
- **Decision:** Preserve existing DAL checkpoint semantics.
  `PropagateToMark` accumulates adjoints at the mark, `PropagateMarkToStart` propagates those accumulated mark adjoints to inputs, and rewind resets only recorded statements after the saved position.

## API Design
No public DAL API change. New configuration flag:

```cmake
-DDAL_USE_ADEPT_AAD=on -DDAL_USE_XAD_AAD=off -DDAL_USE_CODIPACK_AAD=off
```

## Implementation Notes
- `Tape_` stores an Adept stack plus saved start/mark statement and operation positions.
- Adept gradient storage is used through `Stack::set_gradients` / `get_gradients`; `Adjoint(Number_&)` returns a proxy assignable from `double` and convertible to `double`.
- `NPDF` and `NCDF` materialize active results as `Number_` values to avoid returning Adept expressions with references to helper-local temporaries.
- `max`/`min` use Adept `max`/`min` to preserve branch derivative behavior.

## Test Plan
- Build with Adept selected.
- Run `bin/test_suite --gtest_filter=AADTest.*:AnalyticsTest.TestBlackScholesAAD:AnalyticsTest.TestBachelierAAD`.
- Run full `bin/test_suite` if the focused build passes.
