# Copilot Instructions for DAL (Derivatives Algorithms Library)

C++17 quantitative finance library with built-in Automatic Adjoint Differentiation (AAD),
organized as a multi-project CMake workspace.

## Build, Test, Lint

```bash
# Full build from workspace root
mkdir build && cd build
cmake .. && cmake --build . --parallel

# With presets (see CMakePresets.json)
cmake --preset=full-dev && cmake --build build/full-dev --parallel
```

CMake installs per preset into `build/stage/<preset>/`: binaries in `bin/`, libs in `lib/`, headers in `include/`.

Tests run through CTest or directly:

```bash
ctest --test-dir build/full-dev --output-on-failure          # all registered tests
build/full-dev/dal-cpp/dal_cpp_tests                         # one binary directly
build/full-dev/dal-cpp/dal_cpp_tests --gtest_filter=<Suite>.*   # one suite
```

Formatting is enforced by `.clang-format` (LLVM base, 4-space indent, 150 column limit,
`T*` not `T *`, attach braces).

### CMake options

Key cache variables (all AAD backends default OFF; pick one or use native "aadet"):

- `DAL_USE_XAD_AAD` / `DAL_USE_ADEPT_AAD` / `DAL_USE_CODIPACK_AAD`
- `DAL_BUILD_PUBLIC`, `DAL_BUILD_PYTHON`, `DAL_BUILD_EXCEL` (Windows-only)
- `DAL_CPP_BUILD_TESTS`, `DAL_CPP_BUILD_EXAMPLES`, `DAL_CPP_BUILD_BENCHMARKS`

CI runs the full matrix: native/xad/codipack/adept × gcc-13/14, clang-18/19, plus MSVC.

## Architecture

Dependency graph: `dal-cpp ← dal-public ← {dal-python, dal-excel}`; `dal-web` consumes
the Python public API.

| Sub-project   | Target / purpose                                                   |
|---------------|--------------------------------------------------------------------|
| `dal-cpp/`    | `DAL::cpp` — core: math, curves, models, scripting, AAD            |
| `dal-public/` | `DAL::public` — stable public API wrapping `DAL::cpp`              |
| `dal-python/` | pybind11 Python bindings (`dal` package)                           |
| `dal-excel/`  | `.xll` add-in, Windows-only                                        |
| `dal-web/`    | FastAPI backend + React/Vite frontend                              |

Core modules: `math/` (interp, opt, PDE, RNG, matrix, `aad/`), `script/` (lexer → parser →
AST → simulation), `model/`, `curve/`, `indice/`, `risk/`, `concurrency/`, `storage/`,
`auto/` (Machinist-generated).

## Codegen (Machinist)

Enums are declared in Machinist markup `/*IF---- ... -IF----*/` blocks before `namespace Dal {`.
Machinist generates `dal-cpp/dal/auto/MG_*_enum.{hpp,inc}`.

**Regenerate only when markup changes:**
```bash
export MACHINIST_TEMPLATE_DIR=$PWD/dal-cpp/externals/machinist/template/
./dal-cpp/externals/machinist/bin/Machinist -c dal-cpp/config/dal.ifc -l dal-cpp/config/dal.mgl -d ./dal-cpp/dal
./dal-cpp/externals/machinist/bin/Machinist -c dal-cpp/config/dal.ifc -l dal-cpp/config/dal.mgl -d ./dal-excel
```

## Conventions

- Classes: PascalCase + trailing `_` (`Date_`); members `camelCase_`; methods PascalCase.
- `Handle_<T_>` wraps `shared_ptr<const T_>`; `Vector_<E_>` privately inherits `std::vector`.
- Errors: `REQUIRE(cond, msg)` (release), `THROW(msg)`, `NOTICE` for context.
- Tests: `TEST(Suite, Name)` — never `TEST_F`. Prefer `ASSERT_*`; AAD tests use `Clear(*Tape())`.

## Tooling

- Python: **uv** (not pip) for `dal-web/` and `dal-python/`.
- Web: `./dal-web/scripts/start.sh`; frontend http://localhost:5173.
- DAL agent rules: `.codex/skills/*/SKILL.md` and `.codex/skills/dal-agent-team/references/shared-rules.md`.
