# Code Style Guide for DAL C++

## Formatting (primarily enforced by `.clang-format`)

- Base style: LLVM
- Indent: 4 spaces (no tabs)
- Column limit: 150
- Brace style: Attach (opening `{` on same line)
- Pointer binding: to type (`T*` not `T *`)
- Short `if`/loops: not on single line
- Short functions: allowed on single line
- Namespace indent: all
- Project language target: C++17
- Note: `.clang-format` still uses `Standard: Cpp11` for formatting behavior; do not treat that as the language target

## General

- All files must end with a newline
- Use `nullptr` instead of `NULL`

| Element           | Convention                 | Examples                                          |
|-------------------|----------------------------|---------------------------------------------------|
| Classes/Structs   | PascalCase + trailing `_`  | `Date_`, `Vector_<>`, `ThreadPool_`, `Model_<T_>` |
| Template params   | Single letter + `_`        | `T_`, `E_`, `LHS_`, `RHS_`, `OP_`                 |
| Functions/Methods | PascalCase                 | `FromExcel()`, `AddDays()`, `GeneratePath()`      |
| Member variables  | camelCase + trailing `_`   | `serialNumber_`, `spot_`, `vol_`, `name_`         |
| Local variables   | camelCase                  | `numPaths`, `batchSize`, `nThreads`               |
| Constants/Macros  | UPPER_SNAKE_CASE           | `BATCH_SIZE`, `EPSILON`, `FORCE_INLINE`           |
| Files             | lowercase, no separators   | `threadpool.cpp`, `blackscholes.hpp`              |
| Test files        | `test_` prefix, snake_case | `test_vectors.cpp`, `test_date.cpp`               |
| Namespaces        | PascalCase or lowercase    | `Dal`, `Dal::AAD`, `namespace exception`          |

## Markdown Tables

- Align pipe-table columns by padding cells with spaces. Each column is exactly wide enough for its longest cell content plus one leading and one trailing space — no extra padding.
- Keep separator rows compact: each column's dash count equals the column width (content length + 2). Do not add spaces around dashes between pipes.
- When table cells reference specific C++ files, use project-relative paths such as `dal/curve/yc.hpp`, not short names like `yc.hpp` or shorthand like `yc.hpp/cpp`.
- For convention-only filename examples, use filenames without project-relative paths, such as `threadpool.cpp` or `test_date.cpp`.
- Keep related markdown tables consistent across `.claude` guidance files.

## Header Files

- Always `#pragma once` (no include guards)
- File header comment uses three-line format: `//\n// Created by <author> on <date>.\n//`
- Include order: standard/system headers -> DAL/project headers -> local headers
- Platform header: most `.cpp` files include `<dal/platform/platform.hpp>`

## Namespace Patterns

- Top-level: `namespace Dal { ... }`
- Nested for modules: `Dal::AAD`, `Dal::Script`, `Dal::Date`, `Dal::String`
- Anonymous `namespace { }` for file-local helpers
- Public API flattens everything into `Dal::` via `using` aliases
- Always add closing comment on namespace braces: `} // namespace Dal`

## Type Idioms

- `using` over `typedef`: `using base_t = std::vector<E_>;`
- `Handle_<T_>`: wraps `std::shared_ptr<const T_>` for const-correct shared ownership
- `std::unique_ptr<T_>`: for exclusive ownership and `Clone()` return types
- `Vector_<E_>`: private inheritance from `std::vector<E_>` with custom API
- `String_`: case-insensitive string via custom `ci_traits`
- `explicit` constructors on all single-argument constructors
- `[[nodiscard]]` + `const` on all pure getters

## Enums

- All enumeration types must use **Machinist markup** — never hand-write `enum class` definitions.
- The Machinist code-generation tool reads `/*IF----------...` blocks and produces auto-generated `.hpp` (class definition) and `.inc` (implementation) files under `dal/auto/`.
- Generated enum types are classes with a nested `enum class Value_ : char`, a `String()` method, construction from `String_`, comparison operators, and a `ListAll()` vector.
- Use `switchable` in the markup when the enum needs `.Switch()` and `operator==` against `Value_`.

### Enum markup format

Place the markup block before the `namespace Dal {` line in the header file:

```
/*IF--------------------------------------------------------------------------
enumeration EnumName
    One-line description
switchable                              ← include if comparison/dispatch needed
alternative VALUE_NAME optional_alias
-IF-------------------------------------------------------------------------*/
```

Include the generated header inside `namespace Dal { ... }` alongside your declarations:

```cpp
namespace Dal {
#include <dal/auto/MG_EnumName_enum.hpp>

    // your code that uses EnumName_
} // namespace Dal
```

Include the generated `.inc` implementation inside `namespace Dal { ... }` in the corresponding `.cpp` file:

```cpp
namespace Dal {
#include <dal/auto/MG_EnumName_enum.inc>
    // your implementation code
} // namespace Dal
```

### Using generated enums

- Refer to enum values with `EnumName_::Value_::VALUE_NAME` (e.g. `CurveSolveMode_::Value_::EXACT`).
- In switch statements, call `.Switch()` on the object: `switch (obj.Switch()) { case EnumName_::Value_::X: ... }`.
- Comparison with values uses `operator==` directly when `switchable`: `if (obj == EnumName_::Value_::X)`.

### Building after adding or changing enum markup

Run Machinist from the repo root to regenerate auto files before compiling:

```bash
export MACHINIST_TEMPLATE_DIR=$PWD/externals/machinist/template/
./externals/machinist/bin/Machinist -c config/dal.ifc -l config/dal.mgl -d ./dal
```

Then build normally. The auto-generated files (`dal/auto/MG_*_enum.hpp`, `dal/auto/MG_*_enum.inc`) must be committed to the repository alongside the markup source.

## Error Handling

- Custom `Exception_` (from `std::runtime_error`) capturing file/line/function
- Macro-based: `THROW(msg)`, `ASSERT(cond, msg)` (debug-only), `REQUIRE(cond, msg)` (configurable)
- Use `REQUIRE`, not `ASSERT`, for runtime state, precondition, or invariant checks that must also run in release builds
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
