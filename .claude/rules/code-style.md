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
- **Do not use `volatile`** — it is banned in this project. Use other mechanisms (atomics, synchronization primitives, or algorithm restructuring) if you need to prevent compiler reordering or FP contraction.
- **Do not use `mutable`** class members — const evaluators must be pure const. If a caching/hint optimization requires hidden state, redesign the API to make the caller manage the state explicitly (e.g., pass a hint by reference), or skip the optimization.

| Element           | Convention                 | Examples                                          |
|-------------------|----------------------------|---------------------------------------------------|
| Classes/Structs   | PascalCase + trailing `_`  | `Date_`, `Vector_<>`, `ThreadPool_`, `Model_<T_>` |
| Template params   | Single letter + `_`        | `T_`, `E_`, `LHS_`, `RHS_`, `OP_`                 |
| Functions/Methods | PascalCase                 | `FromExcel()`, `AddDays()`, `GeneratePath()`      |
| Member variables  | camelCase + trailing `_`   | `serialNumber_`, `spot_`, `vol_`, `name_`         |
| Local variables   | camelCase                  | `numPaths`, `batchSize`, `nThreads`               |
| Function params   | camelCase                  | `fwdJacobianAtSolution`, `fitTol`, `nInst`        |
| Constants/Macros  | UPPER_SNAKE_CASE           | `BATCH_SIZE`, `EPSILON`, `FORCE_INLINE`           |
| Files             | lowercase, no separators   | `threadpool.cpp`, `blackscholes.hpp`              |
| Test files        | `test_` prefix, snake_case | `test_vectors.cpp`, `test_date.cpp`               |
| Namespaces        | PascalCase or lowercase    | `Dal`, `Dal::AAD`, `namespace exception`          |

## Functional Style and Parameter Mutation

- Prefer a **functional style wherever it is proper to do so**: pure functions, return-new-state, no side effects. This continues the const-correctness theme above — if a result can be computed without mutating shared state, do it that way.
- **Avoid mutating the state of an input parameter** unless performance genuinely matters. Return new state instead.
- When in-place mutation is genuinely needed, **pass by pointer, not by reference**, so the mutation is visible at the call site:
  - Good: `Foo(&x);` — the `&` signals that `x` may be modified.
  - Bad: `Foo(x);` where the signature is `void Foo(T_& x)` — the call site hides the mutation and is indistinguishable from a by-value call.
- **Exception — calibration:** mutating inputs / out-parameters is acceptable inside calibration routines (large, performance-sensitive Jacobian and curve assembly, e.g. the joint and cross-currency calibration code under `dal-cpp/dal/curve/`). Keep this carve-out confined to calibration code and leave a one-line comment at the function noting why mutation is used.

## No Duplication

- Don't repeat logic — if two code paths do the same thing, extract a shared function/template/helper.
- **Includes duplication hidden in control flow.** `if`/`else if`/`else` and `switch` branches that copy-paste the same logic with only a type or value differing must be unified — via a template parameter, a lambda, a lookup table, or a shared helper — not spelled out per branch. Before adding a new `case` or `else` block, check whether an existing branch already does the same thing in a different guise.
- Apply the same standard to near-duplicate classes/structs (e.g. a templated and a non-templated version of the same concept): collapse to one definition plus a `using` alias or specialization, unless they are genuinely interface-divergent (and if they are, leave a one-line comment saying why they are not unified).

## Markdown Tables

- Align pipe-table columns by padding cells with spaces. Each column is exactly wide enough for its longest cell content plus one leading and one trailing space — no extra padding.
- Keep separator rows compact: each column's dash count equals the column width (content length + 2). Do not add spaces around dashes between pipes.
- When table cells reference specific C++ files, use project-relative paths such as `dal-cpp/dal/curve/yc.hpp`, not short names like `yc.hpp` or shorthand like `yc.hpp/cpp`.
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
- Exception: a private opcode enum may be hand-written when it must be used as
  non-type template parameters or as stable integer bytecode operands; document
  the constraint next to the enum and do not add unused Machinist markup.
- The Machinist code-generation tool reads `/*IF----------...` blocks and produces auto-generated `.hpp` (class definition) and `.inc` (implementation) files under `dal-cpp/dal/auto/` (and `dal-excel/auto/` for Excel public-function stubs).
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
export MACHINIST_TEMPLATE_DIR=$PWD/dal-cpp/externals/machinist/template/
./dal-cpp/externals/machinist/bin/Machinist -c dal-cpp/config/dal.ifc -l dal-cpp/config/dal.mgl -d ./dal-cpp/dal
./dal-cpp/externals/machinist/bin/Machinist -c dal-cpp/config/dal.ifc -l dal-cpp/config/dal.mgl -d ./dal-excel
```

Then build normally. The auto-generated files (`dal-cpp/dal/auto/MG_*_enum.hpp`, `dal-cpp/dal/auto/MG_*_enum.inc`, plus `dal-excel/auto/MG_*_public.inc` for Excel stubs) must be committed to the repository alongside the markup source.

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

## Public Function Naming (`dal-excel` and `dal-python`)

These conventions apply to both:

- **`dal-excel`**: Machinist `public` function names in `dal-excel/src/` markup blocks. The generated Excel name turns underscores into dots and uppercases; names containing a dot keep that form, while names without a dot get a `DA.` prefix (e.g., `Deposit_New` → `DEPOSIT.NEW`, `Is_BizDay` → `IS.BIZDAY`, `Calibrate_SingleCurve` → `CALIBRATE.SINGLECURVE`).
- **`dal-python`**: `m.def()` names in `dal-python/src/bindings/` files. The Python-visible name keeps the underscores as-is (e.g., `PseudoRSG_New` stays `PseudoRSG_New`).

Reference files:
- `dal-excel/src/__random.cpp` — `PseudoRSG_New`, `SobolRSG_New`, `PseudoRSG_Get_Uniform`, `SobolRSG_Get_Normal`
- `dal-python/src/bindings/random.cpp` — same names, mirrored in pybind11 `m.def()` calls

- **Instance creation (factory) functions**: end with `_New`, preceded by the type name in PascalCase.
  - `Deposit_New`, `Swap_New`, `CrossCurrencySwap_New`, `DiscountPWLF_New`, `CurveBlock_New`
  - `PseudoRSG_New`, `SobolRSG_New`
  - The function returns a handle to a newly-created object.
- **Enum value constructors**: follow the pattern `<EnumType>_<EnumValue>`, where `EnumType` is the enum class name (without trailing `_`) and `EnumValue` is the value name.
  - `CollateralType_OIS`, `CollateralType_Libor` — create a `CollateralType_` with a specific enum value.
  - These are named constants, not factories — they do NOT use `_New`.
- **Result / getter functions** (operating on an existing handle): follow the pattern `<Type>_Get_<Result>`, where `Type` is the handle's class name and `Result` describes what is retrieved.
  - `PseudoRSG_Get_Uniform`, `PseudoRSG_Get_Normal`, `SobolRSG_Get_Uniform`, `SobolRSG_Get_Normal`
  - For global state accessors (no handle input), the form is `<Name>_Get`: `EvaluationDate_Get`
  - Use this pattern only when the function extracts or computes a result from an existing handle input or global state; standalone utility functions (e.g., `PrevBizDay`, `NextBizDay`) and action functions (e.g., `CalibrateSingleCurve`) do not need this prefix.
- **Setter / mutator functions**: mirror the getter pattern with `_Set` in place of `_Get`.
  - `<Type>_Set_<Result>` for handle-based setters, or `<Name>_Set` for global state.
  - `EvaluationDate_Set` — sets the global evaluation date.
  - A `_Set` function should always have a corresponding `_Get` function; do not create a standalone `_Set` without its getter counterpart.
- **Status / check functions**: start with `Is_` followed by the condition being tested in PascalCase.
  - `Is_BizDay` — returns a boolean indicating whether a date is a business day.
  - Use this prefix for any public function whose primary purpose is to answer a yes/no question about its inputs.

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
- **No large explanatory comments.** Multi-line comments that explain design, methodology, or
  algorithm derivations belong in `docs/methodology/`, not in source. Move the prose to the doc
  (via the `dal-doc-writer` agent) and leave at most a one-line `// why` pointer — or nothing. A
  comment block that reads like a paragraph of documentation is a signal to migrate it.

## Documentation

- Docs under `docs/` describe the **current state only** — no historical narrative, design
  alternatives, implementation-phase plans, or "how this design was reached" sections.
  Historical context belongs in `CHANGELOG.md` and only there.
- **No source line numbers in docs.** Line citations (`calibration.hpp:70-95,142`) go stale
  as soon as the file is edited. Reference the struct, function, or file name instead
  (e.g., "the `CurveCalibrationSpec_` struct in `dal-cpp/dal/curve/calibration.hpp`").
- Fundamental changes (breaking API, new methodology, significant capability, removal of a
  public surface) must be recorded in `CHANGELOG.md`. Routine refactors, test work,
  formatting, and build changes are deliberately omitted from the changelog.
- **`docs/` is the home for methodology prose migrated out of source comments.** When a code
  comment grows into design/methodology/algorithm explanation, move the prose into the matching
  `docs/methodology/` note and reduce the source comment to a one-line pointer or delete it.
  See the Comment Style section and the `dal-doc-writer` agent.
- **Math notation uses only macros GitHub renders.** DAL math is LaTeX inside `$...$` /
  `$$...$$`. Restrict yourself to macros GitHub's math renderer displays; in particular do **not**
  use `\operatorname{}`, which GitHub fails to render — use `\mathrm{}` (or `\mathbb{}` for number
  sets), e.g. `\operatorname{sgn}` → `\mathrm{sgn}`, `\operatorname{YearFrac}` → `\mathrm{YearFrac}`.
  `.github/scripts/check_docs.py` enforces a forbidden-macro list; add any newly-found unrenderable
  macro there rather than reintroducing it.
