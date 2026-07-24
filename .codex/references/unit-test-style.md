# Unit Test Style Guide

## Framework

- Google Test (gtest), with one binary per sub-project:
  - `dal_cpp_tests` — core library tests, built from `dal-cpp/tests/`
  - `dal_public_tests` — public-API tests, built from `dal-public/tests/`
  - `dal_excel_tests` — Excel binding tests, built from `dal-excel/tests/` (Windows-only)
- Tests are registered with CTest via `gtest_discover_tests` and run with `ctest --output-on-failure` from the build directory.
- The core test runner uses `Dal::RegisterAll_::Init()` before `RUN_ALL_TESTS()`.

## File Layout

- One test file per module under the owning sub-project's `tests/` directory:
  - core: `dal-cpp/tests/<module>/test_<name>.cpp`
  - public: `dal-public/tests/test_<name>.cpp`
  - excel: `dal-excel/tests/test_<name>.cpp`
- File header: `//`, `// Created by <author> on <date>.`, `//`
- Include order: `<gtest/gtest.h>` -> standard/system headers -> DAL/project headers -> local headers (if any)
- `using` declarations at file scope for frequently used types (e.g., `using Dal::Vector_;`)
- Helper classes/functions defined before tests, at file scope

## Test Structure

- Always `TEST(Suite, Name)` — never `TEST_F` (no fixtures)
- Suite names: PascalCase matching module (e.g., `InterpTest`, `AADTest`, `DateTest`, `VectorTest`)
- Test names: PascalCase with `Test` prefix (e.g., `TestNumberAdd`, `TestNewCubic`, `TestInterp1Linear`)
- Use scoped blocks `{ }` within a single `TEST` for sub-cases (see AAD tests)

## Assertions

- Prefer `ASSERT_*` over `EXPECT_*` (fail fast)
- Exact comparison: `ASSERT_EQ`, `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_DOUBLE_EQ`
- Float tolerance: `ASSERT_NEAR(actual, expected, tol)` with `1e-8` to `1e-10`
- Exception testing: `ASSERT_THROW(expr, Dal::Exception_)`

## Data Setup

- Inline test data using initializer lists: `Vector_<> x = {1., 2., 3.};`
- Use `Handle_<T_>` for polymorphic objects created via factory functions
- Construct objects directly when testing the class itself

## Patterns

- Test knot-point exactness for interpolators
- Test boundary/edge cases (unordered input, empty containers)
- Test factory functions (`NewLinear`, `NewCubic`) separately from direct construction
- For AAD tests: `Clear(*Tape())` → setup → `NewRecording` → compute → `PropagateToStart` → assert adjoints
