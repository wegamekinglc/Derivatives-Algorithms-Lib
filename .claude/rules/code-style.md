# Code Style Guide for dal.cpp

## Formatting (enforced by `.clang-format`)

- Base style: LLVM
- Indent: 4 spaces (no tabs)
- Column limit: 150
- Brace style: Attach (opening `{` on same line)
- Pointer binding: to type (`T*` not `T *`)
- Short `if`/loops: not on single line
- Short functions: allowed on single line
- Namespace indent: all
- Standard: C++17

## Naming Conventions

| Element              | Convention                    | Examples                                          |
|----------------------|-------------------------------|---------------------------------------------------|
| Classes/Structs      | PascalCase + trailing `_`     | `Date_`, `Vector_<>`, `ThreadPool_`, `Model_<T_>` |
| Template params      | Single letter + `_`           | `T_`, `E_`, `LHS_`, `RHS_`, `OP_`                |
| Functions/Methods    | PascalCase                    | `FromExcel()`, `AddDays()`, `GeneratePath()`      |
| Member variables     | Trailing `_`                  | `serial_`, `spot_`, `vol_`, `name_`               |
| Local variables      | snake_case                    | `num_paths`, `batch_size`, `n_threads`            |
| Constants/Macros     | UPPER_SNAKE_CASE              | `BATCH_SIZE`, `EPSILON`, `FORCE_INLINE`           |
| Files                | lowercase, no separators      | `threadpool.cpp`, `blackscholes.hpp`              |
| Test files           | `test_` prefix, snake_case    | `test_vectors.cpp`, `test_date.cpp`               |
| Namespaces           | PascalCase or lowercase       | `Dal`, `Dal::AAD`, `namespace exception`          |

## Header Files

- Always `#pragma once` (no include guards)
- File header comment: `// Created by <author> on <date>.`
- Include order: standard library -> dal headers -> local headers
- Platform header: most `.cpp` files include `<dal/platform/platform.hpp>`

## Namespace Patterns

- Top-level: `namespace Dal { ... }`
- Nested for modules: `Dal::AAD`, `Dal::Script`, `Dal::Date`, `Dal::String`
- Anonymous `namespace { }` for file-local helpers
- Public API flattens everything into `Dal::` via `using` aliases

## Type Idioms

- `using` over `typedef`: `using base_t = std::vector<E_>;`
- `Handle_<T_>`: wraps `std::shared_ptr<const T_>` for const-correct shared ownership
- `std::unique_ptr<T_>`: for exclusive ownership and `Clone()` return types
- `Vector_<E_>`: private inheritance from `std::vector<E_>` with custom API
- `String_`: case-insensitive string via custom `ci_traits`
- `explicit` constructors on all single-argument constructors
- `[[nodiscard]]` + `const` on all pure getters

## Error Handling

- Custom `Exception_` (from `std::runtime_error`) capturing file/line/function
- Macro-based: `THROW(msg)`, `ASSERT(cond, msg)` (debug-only), `REQUIRE(cond, msg)` (configurable)
- Stack context via `NOTICE(x)` / `NOTE(msg)` macros
- Safe pointer ops: `ASSIGN(p, v)`, `DEREFERENCE(p, v)`

## Common Patterns

- **CRTP**: AAD expression templates
- **Visitor**: Script AST traversal (`Visitor_<V_>`, `Visitable_<...>`)
- **Factory**: `NewLinear()`, `NewSobol()`, `Clone()`
- **Singleton**: `ThreadPool_::instance_`, thread-local `Tape()`
- **RAII**: Smart pointers, scope-based tape/stack cleanup
- **Private inheritance**: `Vector_<E_> : private std::vector<E_>`

## Key Macros

- `FORCE_INLINE`: platform-specific forced inline
- `BASE_EXPORT`: DLL export for shared builds
- `RUN_AT_LOAD(code)`: execute at program startup
- `BAREWORD(w)`: generate `static const String_`
- `RETURN_STATIC(...)`: return static local variable
- `DYN_PTR(n, t, s)`: `dynamic_cast` shorthand

## Test Conventions (Google Test)

- Always simple `TEST(Suite, Name)` — no fixtures (`TEST_F`)
- Strongly prefer `ASSERT_*` over `EXPECT_*`
- Float comparison: `ASSERT_NEAR(actual, expected, tol)` with `1e-8` to `1e-10`
- Scoped blocks `{ }` within a single `TEST` for sub-cases
- Test suites: PascalCase (`AADTest`, `VectorTest`)
- Test names: PascalCase with `Test` prefix (`TestNumberAdd`)

## Comment Style

- Sparse — code is self-documenting
- Focus on "why" not "what"
- Single-line `//` for inline notes
- No docstrings or doxygen-style comments
- File headers are the only mandatory comments
