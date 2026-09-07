# DAL Unit-Test Authoring Reference

Write Google Test unit tests that follow this project's
[unit-test conventions](unit-test-style.md) and
[C++ conventions](code-style.md).

## Contents

- [Workflow](#workflow)
- [Build and verification](#4-build-and-verify)
- [AAD tests](#aad-tests)
- [Serialization tests](#serialization-round-trip-tests)
- [Error-handling tests](#exception--error-handling-tests)
- [Prohibited patterns](#what-not-to-do)

Core tests are compiled into `dal_cpp_tests`; public and Excel tests use their
own component targets. `dal-cpp/CMakeLists.txt` uses
`file(GLOB_RECURSE ... CONFIGURE_DEPENDS)`, so new core test files are picked
up through CMake regeneration without manual build-file edits.

## Workflow

### 1. Understand what to test

Read the header file for the module under test. Identify:
- Public functions, methods, and their signatures
- Class constructors and factory functions (e.g. `NewLinear`, `NewSobol`)
- Edge cases: empty inputs, boundary values, error conditions the implementation should reject
- Template parameters, if any

### 2. Find or create the test file

Check whether `dal-cpp/tests/<module>/test_<name>.cpp` already exists. Reuse the module subdirectory pattern from existing tests (e.g.
`dal-cpp/tests/math/interp/`, `dal-cpp/tests/time/`).

**If adding to an existing file:** reuse its suite name — never introduce a second suite in the same file. Preserve its namespace style, whether it
uses `using namespace Dal;` or specific `using Dal::...;` declarations.

**If creating a new file,** add specific `using Dal::...;` declarations between the includes and tests as needed for frequently used names. Keep
one-off names qualified. The generic template leaves these declarations out because the required names depend on the module:

```cpp
//
// Created by <author> on <today's date>.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/<module>/<header>.hpp>

TEST(<Suite>, <TestName>) {
}
```

- `<dal/platform/platform.hpp>` is the standard platform header. Only use `<dal/platform/strict.hpp>` in
  `dal-cpp/tests/platform/`.
- The file must end with a newline after the final `}`.

### 3. Write the tests

Every `TEST()` block is self-contained — set up its own data, exercise the API, assert results. No shared mutable state between tests.

**Naming:**
- Suite: `{ModuleName}Test` — PascalCase (e.g. `DateTest`, `PiecewiseLinearTest`, `MatrixTest`)
- Test: `Test{DescriptiveName}` — always `Test` prefix, PascalCase (e.g. `TestDateAddDays`, `TestCholeskySolve`)
- Group related tests near each other: construction → basic ops → edge cases → error handling

**Assertions by type:**

| Situation                        | Macro                                      | Example                                                                 |
|----------------------------------|--------------------------------------------|-------------------------------------------------------------------------|
| Float (tolerant)                 | `ASSERT_NEAR(actual, expected, tol)`       | `ASSERT_NEAR(price, 8.5359, 1e-10)`                                     |
| Float (exact, e.g. knot points)  | `ASSERT_DOUBLE_EQ(expected, actual)`       | `ASSERT_DOUBLE_EQ(f[2], interp(x[2]))`                                  |
| Integer / size / object equality | `ASSERT_EQ(expected, actual)`              | `ASSERT_EQ(Year(dt), yyyy)`                                             |
| Boolean condition                | `ASSERT_TRUE(expr)` / `ASSERT_FALSE(expr)` | `ASSERT_TRUE(dt.IsValid())`                                             |
| Expected exception               | `ASSERT_THROW(stmt, Dal::Exception_)`      | `ASSERT_THROW(Dal::PDE::MakeUniformGrid(1.0, 0.0, 5), Dal::Exception_)` |

- Prefer `ASSERT_*` for fail-fast, as required by the shared unit-test conventions.
- Tolerance for `ASSERT_NEAR`: `1e-10` is most common in this repo, followed by `1e-8` and `1e-6`. Use `1e-4` for Monte Carlo or iterative
  methods. Match the tolerance to the precision the algorithm actually guarantees.

**Data setup:**

```cpp
// Inline vectors via initializer lists
Vector_<> x = {1.0, 2.0, 3.0};
Vector_<Date_> knots = {Date_(2021, 3, 26), Date_(2022, 3, 26)};

// Polymorphic objects via factory functions and Handle_
Handle_<Interp1_> interp(Interp::NewLinear("test", x, y));

// Direct construction for the class under test
Date_ dt(2024, 1, 15);
```

**Element-by-element comparison** for vectors and matrices:

```cpp
for (int i = 0; i < n; ++i)
    ASSERT_NEAR(result[i], expected[i], 1e-10);
```

**Helper types** at file scope (before tests) when they improve readability:

```cpp
using vector_t = Dal::Vector_<>;
```

For helper classes, define them at file scope before the tests. Use an anonymous
namespace for file-local helpers when appropriate.

### 4. Build and verify

```bash
cmake --preset=Release-linux -S . -B build/Release-linux
cmake --build build/Release-linux --target dal_cpp_tests -j$(nproc)
```

Then run the new tests:

```bash
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter=<SuiteName>.<TestName>
```

If the build fails, the most common causes are:
- Missing `#include` for a type used in the test
- Include order wrong (gtest must be first)
- Integer literal passed where a `double` is expected — write `1.0` not `1`

## Key Patterns

### AAD tests

When testing AAD-aware code, the test must manage the tape explicitly. Scoped
blocks within a TEST can isolate tape operations and other sub-cases:

```cpp
TEST(AADTest, TestGradient) {
    auto* tape = AAD::Tape();
    Clear(*tape);

    Number_ x0 = 1.0, x1 = 2.0;
    PutOnTape(x0);
    PutOnTape(x1);
    Mark(*tape);

    Number_ y = x0 * x1 + log(x0);
    Adjoint(y) = 1.0;
    PropagateToMark(*tape);

    ASSERT_NEAR(Adjoint(x0), x1 + 1.0 / x0, 1e-10);
    ASSERT_NEAR(Adjoint(x1), x0, 1e-10);

    Clear(*tape);
}
```

Key points:
- `Clear(*tape)` at start and end
- `PutOnTape(...)` each input, then `Mark(*tape)`
- `Adjoint(y) = 1.0` seeds the output, then `PropagateToMark(*tape)`
- Assert adjoints against known analytical derivatives

### Serialization round-trip tests

Use concrete archive helpers rather than constructing the abstract
`Archive::Store_`. This JSON round trip is from
`StorageTest.TestJSONStore` in `dal-cpp/tests/storage/test_json.cpp`:

```cpp
TEST(StorageTest, TestJSONStore) {
    Vector_<> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Handle_<Interp1_> src(Interp::NewLinear("interp", x, f));

    auto dst = JSON::WriteString(*src);
    Handle_<Storable_> rtn = JSON::ReadString(dst, true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));
    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_DOUBLE_EQ((*src)(2.5), (*val)(2.5));
}
```

Use `dal/storage/json.hpp` and `dal/math/interp/interplinear.hpp` for this
example. `Splat(*src)` and `UnSplat(blob, true)` provide the matrix archive
equivalent; see `dal-cpp/tests/storage/test_splat.cpp`.

### Exception / error-handling tests

```cpp
TEST(PdeGridTest, TestThrowsOnInvalidInput) {
    ASSERT_THROW(Dal::PDE::MakeUniformGrid(5.0, 0.0, 10), Dal::Exception_);
    ASSERT_THROW(Dal::PDE::MakeUniformGrid(0.0, 0.0, 5), Dal::Exception_);
}
```

## What Not to Do

- **No `TEST_F`** — use `TEST(Suite, Name)` per the repository convention.
- **Prefer `ASSERT_*`** — fail fast when a failed prerequisite invalidates later checks.
- **No `using namespace std`** — qualify `std::` types explicitly.
- **No comments** describing what the code does — the test name and assertions should be self-documenting. A short comment is OK for a non-obvious
  expected value or a subtle setup choice.
- **No shared mutable state** between tests — each test sets up its own data locally.
- **No empty test bodies** — every `TEST()` must exercise the API and assert something.
