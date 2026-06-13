# Copilot Instructions for DAL (Derivatives Algorithms Library)

C++17 quantitative finance library with built-in Automatic Adjoint Differentiation (AAD),
organized as a multi-project CMake workspace.

## Build, Test, Lint

```bash
# Full build: Machinist codegen → configure → build all sub-projects → install → ctest
bash ./build_linux.sh          # add --coverage for a coverage report

# Manual build from the workspace root
mkdir build && cd build
cmake --preset=Release-linux -DDAL_BUILD_PUBLIC=ON .. && make -j32 && make install
```

CMake installs into the repo root: binaries in `bin/`, libs in `lib/`, headers in `include/`.

Tests run through CTest. Each sub-project registers its own GoogleTest binary
(`dal_cpp_tests`, `dal_public_tests`) via `gtest_discover_tests`.

```bash
(cd build && ctest --output-on-failure)          # all registered tests
bin/dal_cpp_tests                                # one binary directly
bin/dal_cpp_tests --gtest_filter=<Suite>.*       # one suite
bin/dal_cpp_tests --gtest_filter=<Suite>.<Test>  # one test
```

Formatting is enforced by `.clang-format` (LLVM base, 4-space indent, 150 column limit,
`T*` not `T *`, attach braces).

### CMake presets and options

`CMakePresets.json` base preset turns off all AAD backends and the Excel sub-project
(Windows-only). Examples, benchmarks, public, and Python are enabled by default.
Tests are enabled by the source default (`DAL_CPP_BUILD_TESTS=ON`). With AAD backends
off, the native ("aadet") backend is used. Presets: `Release-linux`, `Debug-linux`,
`Release-windows`, `Debug-windows`.
Override via cache vars when configuring manually:

- `DAL_BUILD_PUBLIC`, `DAL_BUILD_PYTHON`, `DAL_BUILD_EXCEL` (Excel is Windows-only)
- `DAL_CPP_BUILD_TESTS`, `DAL_CPP_BUILD_EXAMPLES`, `DAL_CPP_BUILD_BENCHMARKS`
- `DAL_USE_XAD_AAD` / `DAL_USE_ADEPT_AAD` / `DAL_USE_CODIPACK_AAD` — pick an AAD backend.
  CI runs the full matrix across native/xad/codipack/adept and gcc-13/14, clang-18/19.

## Architecture

Dependency graph: `dal-cpp ← dal-public ← {dal-python, dal-excel}`; `dal-web` consumes
the Python public API. Each sub-project owns its own `CMakeLists.txt` and stands alone.

| Sub-project   | Target / purpose                                                  |
|---------------|------------------------------------------------------------------|
| `dal-cpp/`    | `DAL::cpp` — core: math, curves, models, scripting, AAD          |
| `dal-public/` | `DAL::public` — stable public API wrapping `DAL::cpp`            |
| `dal-python/` | pybind11 Python bindings (`dal` package), depends on `DAL::public` |
| `dal-excel/`  | `.xll` add-in, Windows-only, depends on `DAL::public`           |
| `dal-web/`    | FastAPI backend + React/Vite frontend portfolio app             |

Core modules under `dal-cpp/dal/`: `math/` (interpolation, optimization, PDE, RNG, matrix,
`math/aad/`), `script/` (visitor-pattern expression engine: lexer → preprocessor → parser
→ AST → simulation), `model/`, `curve/` (yield curves, calibration), `indice/`, `risk/`,
`concurrency/` (thread pool), `storage/`, and `auto/` (Machinist-generated, glob-included).

The public API flattens everything into the `Dal::` namespace via `using` aliases.

## Codegen (Machinist) — critical, non-obvious

Enums are **never hand-written** as `enum class`. They are declared with Machinist markup
in `/*IF---- ... -IF----*/` blocks placed before `namespace Dal {` in the header. Machinist
reads `dal-cpp/config/dal.ifc` + `dal.mgl` and generates `dal-cpp/dal/auto/MG_*_enum.{hpp,inc}`
(and `dal-excel/auto/MG_*_public.inc`). `build_linux.sh` runs Machinist twice (`-d ./dal-cpp/dal`
and `-d ./dal-excel`). After changing markup, regenerate and **commit the generated files**:

```bash
export MACHINIST_TEMPLATE_DIR=$PWD/dal-cpp/externals/machinist/template/
./dal-cpp/externals/machinist/bin/Machinist -c dal-cpp/config/dal.ifc -l dal-cpp/config/dal.mgl -d ./dal-cpp/dal
./dal-cpp/externals/machinist/bin/Machinist -c dal-cpp/config/dal.ifc -l dal-cpp/config/dal.mgl -d ./dal-excel
```

Use values as `EnumName_::Value_::NAME`; `switch (obj.Switch())`; `obj == EnumName_::Value_::X`
when the markup is `switchable`. Include `MG_*_enum.hpp` inside `namespace Dal { }` in the
header and `MG_*_enum.inc` inside `namespace Dal { }` in the `.cpp`.

Files matching `_repository.*` under `dal-cpp/dal/storage/` are excluded from the build.

## Conventions

Naming (see `.claude/rules/code-style.md` for the full table):

- Classes/structs: PascalCase with trailing `_` (`Date_`, `ThreadPool_`); template params
  single-letter + `_` (`T_`, `RHS_`); methods PascalCase; member vars camelCase + `_`
  (`spot_`); locals camelCase; constants/macros UPPER_SNAKE_CASE; files lowercase with no
  separators (`blackscholes.hpp`); test files `test_<name>.cpp`.
- Headers: `#pragma once` (no guards); three-line `//\n// Created by <author> on <date>.\n//`
  file header; include order system → DAL → local; most `.cpp` include `<dal/platform/platform.hpp>`.
- Types: `using` over `typedef`; `Handle_<T_>` wraps `shared_ptr<const T_>`; `Vector_<E_>`
  privately inherits `std::vector`; `String_` is case-insensitive; `explicit` single-arg ctors;
  `[[nodiscard]] const` getters; `nullptr` not `NULL`; all files end with a newline.
- Errors: custom `Exception_`; use `REQUIRE(cond, msg)` (runs in release) not debug-only
  `ASSERT`; `THROW(msg)`; stack context via `NOTICE`/`NOTE`.
- Patterns: CRTP (AAD expression templates), Visitor (script AST), Factory (`New*`, `Clone`),
  RAII tape/stack scopes, thread-local `Tape()`.

Tests (Google Test, `.claude/rules/unit-test-style.md`):

- Always `TEST(Suite, Name)` — never `TEST_F`/fixtures. Suite PascalCase matching the module
  (`AADTest`); test names PascalCase with `Test` prefix (`TestNumberAdd`).
- Prefer `ASSERT_*` over `EXPECT_*`; floats via `ASSERT_NEAR(actual, expected, 1e-8..1e-10)`;
  exceptions via `ASSERT_THROW(expr, Dal::Exception_)`; use scoped `{ }` blocks for sub-cases.
- The core runner calls `Dal::RegisterAll_::Init()` before `RUN_ALL_TESTS()` (`tests/test_main.cpp`).
- For AAD: `Clear(*Tape())` → setup → `NewRecording` → compute → `PropagateToStart` → assert adjoints.

## Tooling notes

- Python dependencies are managed with **uv** (not pip/venv directly) for both the web backend
  and the Python public-API/pybind11 build.
- DAL agent guidance (rules, skills, agents) lives under `.claude/`. Copilot guidance is in
  `.github/copilot-instructions.md` (this file). Keep markdown tables column-aligned with compact
  separator rows. Use SSH (`git@github.com:`) URLs, not HTTPS.
- Web UI: `./dal-web/scripts/start.sh` / `stop.sh`; frontend at http://localhost:5173,
  API docs at http://127.0.0.1:8001/docs.
- Web UI e2e: run `./dal-web/scripts/setup-playwright.sh` once, then
  `cd dal-web/frontend && npm run test:e2e`.
