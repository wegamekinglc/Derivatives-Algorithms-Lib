---
name: dal-unit-test-write
description: |
  Write Google Test unit tests and fix failing test sets for the DAL C++ quantitative finance library. Use when the user asks to write tests, add test coverage,
  create unit tests, repair broken tests, fix failing test suites, or mentions testing for new or existing C++ code. Also trigger when implementing features
  that need test coverage, refactoring code without tests, or when the user mentions a function/class that should be tested.
user-invocable: true
---

# Unit Test Write

Write Google Test unit tests that follow this project's conventions. Start by reading `.claude/rules/unit-test-style.md` and
`.claude/rules/code-style.md` for the authoritative rules; this skill distills them into a workflow.

Tests are compiled into a single `test_suite` binary. The `tests/CMakeLists.txt` uses `file(GLOB_RECURSE)` — new `.cpp` files are picked up
automatically, no manual build-file edits needed.

## Workflow

### 1. Understand what to test

Read the header file for the module under test. Identify:
- Public functions, methods, and their signatures
- Class constructors and factory functions (e.g. `NewLinear`, `NewSobol`)
- Edge cases: empty inputs, boundary values, error conditions the implementation should reject
- Template parameters, if any

### 2. Find or create the test file

Check whether `tests/<module>/test_<name>.cpp` already exists. Reuse the module subdirectory pattern from existing tests (e.g.
`tests/math/interp/`, `tests/time/`).

**If adding to an existing file:** reuse its suite name — never introduce a second suite in the same file. Match the file's existing namespace style
(`using namespace Dal;` vs specific imports).

**If creating a new file,** use this template:

```cpp
//
// Created by <author> on <today's date>.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/<module>/<header>.hpp>

using namespace Dal;

TEST(<Suite>, <TestName>) {
}
```

- `<dal/platform/platform.hpp>` is the standard platform header (used in 96% of test files). Only use `<dal/platform/strict.hpp>` in
  `tests/platform/`.
- The file must end with a newline after the final `}`.

### 3. Write the tests

Every `TEST()` block is self-contained — set up its own data, exercise the API, assert results. No shared mutable state between tests.

**Naming:**
- Suite: `{ModuleName}Test` — PascalCase (e.g. `DateTest`, `PiecewiseLinearTest`, `MatrixTest`)
- Test: `Test{DescriptiveName}` — always `Test` prefix, PascalCase (e.g. `TestDateAddDays`, `TestCholeskySolve`)
- Group related tests near each other: construction → basic ops → edge cases → error handling

**Assertions by type:**

| Situation                        | Macro                                      | Example                                        |
|----------------------------------|--------------------------------------------|------------------------------------------------|
| Float (tolerant)                 | `ASSERT_NEAR(actual, expected, tol)`       | `ASSERT_NEAR(price, 8.5359, 1e-10)`            |
| Float (exact, e.g. knot points)  | `ASSERT_DOUBLE_EQ(expected, actual)`       | `ASSERT_DOUBLE_EQ(f[2], interp(x[2]))`         |
| Integer / size / object equality | `ASSERT_EQ(expected, actual)`              | `ASSERT_EQ(Year(dt), yyyy)`                    |
| Boolean condition                | `ASSERT_TRUE(expr)` / `ASSERT_FALSE(expr)` | `ASSERT_TRUE(dt.IsValid())`                    |
| Expected exception               | `ASSERT_THROW(stmt, Dal::Exception_)`      | `ASSERT_THROW(Mesher(0,0,5), Dal::Exception_)` |

- Never use `EXPECT_*` — always `ASSERT_*` for fail-fast.
- Tolerance for `ASSERT_NEAR`: `1e-10` is most common in this repo, followed by `1e-8` and `1e-6`. Use `1e-4` for Monte Carlo or iterative
  methods. Match the tolerance to the precision the algorithm actually guarantees.

**Data setup:**

```cpp
// Inline vectors via initializer lists
Vector_<> x = {1.0, 2.0, 3.0};
Vector_<Date_> knots = {Date_(2021, 3, 26), Date_(2022, 3, 26)};

// Polymorphic objects via factory functions and Handle_
Handle_<Interp1_> interp = Interp1::NewLinear("test", x, y);

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

For helper classes, define them at file scope before the tests. Use anonymous `namespace { }` only for larger helper blocks (this pattern appears in
~3 files in the repo).

### 4. Build and verify

```bash
mkdir -p build && cd build
cmake --preset=Release-linux .. && make -j$(nproc) test_suite && make install
cd ..
```

Then run the new tests:

```bash
bin/test_suite --gtest_filter=<SuiteName>.<TestName>
```

If the build fails, the most common causes are:
- Missing `#include` for a type used in the test
- Include order wrong (gtest must be first)
- Integer literal passed where a `double` is expected — write `1.0` not `1`

## Key Patterns

### AAD tests

When testing AAD-aware code, the test must manage the tape explicitly. Scoped blocks `{ }` within a TEST isolate tape operations. This is the only
case where scoped blocks are common in this repo's tests:

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

Many DAL types support Splat and JSON serialization. The pattern:

```cpp
TEST(SomeTest, TestSerialization) {
    auto original = /* create object */;
    Archive_::Store_ store("name");
    original->Splat(store);
    auto restored = Archive_::UnSplat(store, "name");
    // compare original and restored ...
}
```

Check existing serialization tests in `tests/storage/` for the exact API.

### Exception / error-handling tests

```cpp
TEST(MeshersTest, TestThrowsOnInvalidInput) {
    ASSERT_THROW(Uniform1DMesher_(5.0, 0.0, 10), Dal::Exception_);
    ASSERT_THROW(Uniform1DMesher_(0.0, 0.0, 5), Dal::Exception_);
}
```

## What Not to Do

- **No `TEST_F`** — this repo has zero uses of `TEST_F`. Always `TEST(Suite, Name)`.
- **No `EXPECT_*`** — all assertions are fatal `ASSERT_*`.
- **No `using namespace std`** — qualify `std::` types explicitly.
- **No comments** describing what the code does — the test name and assertions should be self-documenting. A short comment is OK for a non-obvious
  expected value or a subtle setup choice.
- **No shared mutable state** between tests — each test sets up its own data locally.
- **No empty test bodies** — every `TEST()` must exercise the API and assert something.
