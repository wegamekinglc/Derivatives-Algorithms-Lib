# DAL Shared Rules

Use this reference for all DAL role skills. It is the single Codex-side home for shared
style, test, review, docs, and artifact conventions.

## Canonical Locations

- Codex skills: `.codex/skills/`.
- Codex role artifacts: `.codex/artifacts/`.
- Claude source material: `.claude/` and `CLAUDE.md`; read only unless the user explicitly asks to edit Claude files.
- Methodology docs: `docs/methodology/`.

## Build And Test

C++ build commands, workspace CMake options, and the preset matrix are canonical in
[CLAUDE.md](../../../../CLAUDE.md#build-commands) and
[CLAUDE.md](../../../../CLAUDE.md#running-tests).

Full Linux workflow:

```bash
bash ./build_linux.sh > test_output.txt 2>&1
```

Targeted test:

```bash
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter=<SuiteName>.<TestName>
```

Web tests:

```bash
(cd dal-web/backend && uv sync --locked --inexact && uv run --no-sync pytest)
(cd dal-web/frontend && npm run build)
./dal-web/scripts/setup-playwright.sh
(cd dal-web/frontend && npm run test:e2e)
```

## C++ Style

Canonical reference: [code-style.md](../../../../.claude/rules/code-style.md). The digest
below is what the Codex role skills apply inline.

- C++17. Use `.clang-format`: 4 spaces, attached braces, 150 columns, `T*` pointer binding.
- Classes/structs: PascalCase plus trailing `_`.
- Template params: short name plus trailing `_`.
- Functions/methods: PascalCase.
- Members: camelCase plus trailing `_`.
- Locals/params: camelCase.
- Files: lowercase, no separators; test files use `test_` prefix.
- Headers use `#pragma once`, the three-line file header, and `namespace Dal { ... }` with `} // namespace Dal`.
- Use `Handle_<T_>` for shared const ownership and `std::unique_ptr<T_>` for exclusive ownership.
- Use `REQUIRE` for runtime checks and `THROW` for error paths.
- Do not use `volatile`, `mutable` members, raw exceptions for normal DAL errors, or hand-written DAL enum classes.

## Machinist Enums

The markup format is specified in [code-style.md](../../../../.claude/rules/code-style.md#enums).
When enum markup changes, regenerate both core and Excel output. The `Machinist`
binary is a git-ignored build artifact, not checked in, so the reliable route is
the CMake target, which builds Machinist first and runs it with the right inputs:

```bash
cmake --preset=Release-linux -S . -B build/Release-linux
cmake --build build/Release-linux --target dal_generate
```

Commit generated `dal-cpp/dal/auto/MG_*` and `dal-excel/auto/MG_*` files with the markup source when committing is requested.

## Google Test

Canonical reference: [unit-test-style.md](../../../../.claude/rules/unit-test-style.md).

- Core tests: `dal-cpp/tests/<module>/test_<name>.cpp`.
- Include `<gtest/gtest.h>` first.
- Use `TEST(Suite, Name)`, never `TEST_F`.
- Use `ASSERT_*`, never `EXPECT_*`.
- Test names start with `Test`.
- Prefer `ASSERT_NEAR(actual, expected, 1e-10)` or `1e-8` for deterministic numerics.
- Use `ASSERT_THROW(stmt, Dal::Exception_)` for DAL exception paths.
- Each test owns its setup. Avoid shared mutable state and singleton pollution.
- Clear AAD tape state at the start and end of AAD tests.

## Review Rules

- Findings first, ordered by severity, with file and line when available.
- Read full changed files when behavior, API, build output, generated code, tests, docs, or guidance can change.
- Check correctness, style, tests, docs/changelog, generated files, security, and compatibility.
- For simplification reviews, cite files and symbols rather than source lines; line numbers go stale for design reports.
- Do not submit GitHub reviews or merge unless explicitly asked.

## Documentation Rules

Canonical reference: the Documentation section of
[code-style.md](../../../../.claude/rules/code-style.md#documentation).

- Docs describe the current/latest library only.
- Historical context belongs in `CHANGELOG.md`, not docs.
- Do not cite source line numbers in docs.
- Reuse published example code; route example design problems to `dal-api-designer`.
- Add changelog entries only for breaking public API changes, new methodology/numerical algorithms, significant capabilities, significant methodology shifts, or removal/deprecation of public surface.

## Artifact Paths

| Artifact       | Path                                        |
|----------------|---------------------------------------------|
| Specs          | `.codex/artifacts/specs/<slug>.md`          |
| API notes      | `.codex/artifacts/api-notes/<slug>.md`      |
| Critiques      | `.codex/artifacts/critiques/<slug>.md`      |
| Review reports | `.codex/artifacts/reviews/<slug>.md`        |
| Perf reports   | `.codex/artifacts/perf/<slug>.md`           |
| Simplification | `.codex/artifacts/simplifications/<slug>.md` |
