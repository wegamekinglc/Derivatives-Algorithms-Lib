# Copilot Instructions for DAL (Derivatives Algorithms Library)

C++17 quantitative finance library with built-in Automatic Adjoint Differentiation (AAD),
organized as a multi-project CMake workspace.

[CLAUDE.md](../CLAUDE.md) is the agent-facing source of truth for build/test commands,
workspace CMake options, and the architecture map; [docs/installation.md](../docs/installation.md)
is the published setup guide. The essentials:

## Build, Test, Lint

```bash
# Default Linux workflow (configures core + public C++ + examples via the
# Release-linux preset, installs into build/stage/Release-linux, runs CTest)
bash ./build_linux.sh

# Manual preset flow (Release-linux/Debug-linux declare no binaryDir, so pass -S/-B)
cmake --preset=Release-linux -S . -B build/Release-linux
cmake --build build/Release-linux --parallel
cmake --install build/Release-linux
```

CMake installs per preset into `build/stage/<preset>/`: binaries in `bin/`, libs in `lib/`,
headers in `include/`.

Tests run through CTest or directly from the build tree:

```bash
ctest --test-dir build/Release-linux --output-on-failure
./build/Release-linux/dal-cpp/dal_cpp_tests
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter=<SuiteName>.*
```

Formatting is enforced by `.clang-format` (LLVM base, 4-space indent, 150 column limit,
`T*` not `T *`, attach braces).

CMake cache variables (AAD backends, sub-project/test/example/benchmark toggles, sanitizers)
are listed in [CLAUDE.md](../CLAUDE.md#build-commands) and the
[installation guide options table](../docs/installation.md#common-cmake-options). CI runs
the full matrix: native/xad/codipack/adept × gcc-13, gcc-14, gcc-15, clang-18,
clang-19, clang-20, plus MSVC.

## Architecture

Dependency graph: `dal-cpp ← dal-public ← {dal-python, dal-excel}`; `dal-web` consumes
the Python public API. The published map is [docs/architecture.md](../docs/architecture.md).

| Sub-project   | Target / purpose                                        |
|---------------|---------------------------------------------------------|
| `dal-cpp/`    | `DAL::cpp` — core: math, curves, models, scripting, AAD |
| `dal-public/` | `DAL::public` — stable public API wrapping `DAL::cpp`   |
| `dal-python/` | pybind11 Python bindings (`dal` package)                |
| `dal-excel/`  | `.xll` add-in, Windows-only                             |
| `dal-web/`    | FastAPI backend + React/Vite frontend                   |

Core modules: `math/` (interp, opt, PDE, RNG, matrix, `aad/`), `script/` (lexer → parser →
AST → simulation), `model/`, `curve/`, `indice/`, `risk/`, `concurrency/`, `storage/`,
`auto/` (Machinist-generated).

## Codegen (Machinist)

Enums are declared in Machinist markup `/*IF---- ... -IF----*/` blocks before `namespace Dal {`.
Machinist generates `dal-cpp/dal/auto/MG_*_enum.{hpp,inc}`; the markup format and regeneration
contract live in [.claude/rules/code-style.md](../.claude/rules/code-style.md).

**Regenerate only when markup changes.** The `Machinist` binary is a git-ignored build
artifact, not checked in, so use the CMake target, which builds Machinist first and runs
it with the right inputs:

```bash
cmake --preset=Release-linux -S . -B build/Release-linux
cmake --build build/Release-linux --target dal_generate
```

## Conventions

Naming, type idioms, error handling, and enum markup follow
[.claude/rules/code-style.md](../.claude/rules/code-style.md); tests follow
[.claude/rules/unit-test-style.md](../.claude/rules/unit-test-style.md). Highlights:

- Classes: PascalCase + trailing `_` (`Date_`); members `camelCase_`; methods PascalCase.
- `Handle_<T_>` wraps `shared_ptr<const T_>`; `Vector_<E_>` privately inherits `std::vector`.
- Errors: `REQUIRE(cond, msg)` (release), `THROW(msg)`, `NOTICE` for context.
- Tests: `TEST(Suite, Name)` — never `TEST_F`. Prefer `ASSERT_*`; AAD tests use `Clear(*Tape())`.

## Tooling

- Python: **uv** is required for `dal-web/` and recommended for `dal-python/`;
  standalone `dal-python` package builds also support pip.
- Web: `./dal-web/scripts/start.sh`; frontend http://localhost:5173.
- DAL agent rules: `.codex/skills/*/SKILL.md` and `.codex/skills/dal-agent-team/references/shared-rules.md`.
