# DAL Style Review Reference

Review all changed C++ files and changed repository guidance in the working tree against
[the DAL C++ style conventions](../../dal-agent-team/references/code-style.md) and
[the DAL unit-test conventions](../../dal-agent-team/references/unit-test-style.md).

## Steps

1. Read the style guides:
   - [DAL C++ style conventions](../../dal-agent-team/references/code-style.md)
   - [DAL unit-test conventions](../../dal-agent-team/references/unit-test-style.md)

2. Identify changed files by running:
   ```bash
   git diff --name-only HEAD
   ```
   If there are no uncommitted changes, check the latest commit instead:
   ```bash
   git diff --name-only HEAD~1
   ```

   **Whole-codebase scan:** When the user's request includes "whole code base" or "entire
   codebase", do not rely on `git diff`. Instead scan all `.hpp` and `.cpp` files under the
   following directories (excluding `auto/` and `externals/` subdirectories):
   - `dal-cpp/dal/`
   - `dal-public/src/`
   - `dal-cpp/examples/`
   - `dal-cpp/benchmarks/`
   - `dal-cpp/tests/`

3. Read each changed `.hpp` and `.cpp` file and each changed repository-guidance `.md`
   file, then check for violations against the rules below.

4. If the code changes affect documented behavior, workflows, architecture notes, or
   methodology, also check whether the related Markdown files were updated consistently.

5. Report findings grouped by file, with line numbers and specific violations. If no violations are found, say so.

## What to Check

### Naming
- Classes/Structs: PascalCase with trailing `_` (e.g., `Date_`, `Vector_<>`)
- Template params: single letter or short name with trailing `_` (e.g., `T_`, `E_`)
- Functions/Methods: PascalCase (e.g., `AddDays()`, `GeneratePath()`)
- Member variables: camel case with trailing `_` (e.g., `serialNumber_`, `name_`)
- Local variables: camel case (e.g., `numPaths`, `batchSize`)
- Constants/Macros: UPPER_SNAKE_CASE
- Files: lowercase, no separators; test files use `test_` prefix

### Header Files
- Must use `#pragma once` (not include guards)
- Must have three-line file header: `//\n// Created by <author> on <date>.\n//`
- Include order: `<gtest/gtest.h>` first (test files) -> standard/system headers -> DAL/project headers

### Test Files
- Always `TEST(Suite, Name)` -- never `TEST_F`
- Suite names: PascalCase matching module (e.g., `InterpTest`)
- Test names: PascalCase with `Test` prefix (e.g., `TestNewCubic`)
- Prefer `ASSERT_*` over `EXPECT_*`
- Float comparison: `ASSERT_NEAR(actual, expected, tol)` with `1e-8` to `1e-10`
- Exception testing: `ASSERT_THROW(expr, Dal::Exception_)`

### General
- 4-space indentation, no tabs
- Brace style: Attach (opening `{` on same line)
- Pointer binds to type: `T*` not `T *`
- `using` over `typedef`
- `explicit` on single-argument constructors
- Error handling via `REQUIRE`/`ASSERT`/`THROW` macros, not raw exceptions
- Use `REQUIRE`, not `ASSERT`, for runtime state, precondition, or invariant checks that must also run in release builds
- Comments: sparse, focus on "why" not "what", no docstrings
- All files must end with a newline
- Use `nullptr` instead of `NULL`
- Namespace closing braces must have a comment: `} // namespace Dal`

### Markdown Guidance Files
- Review changed repository-guidance `.md` files as part of every code style review.
- Check Markdown tables against the [DAL C++ style conventions](../../dal-agent-team/references/code-style.md#markdown-tables):
  aligned pipe columns, compact separator rows, and project-relative C++ file paths in table cells.
- Check that code block diagrams (class hierarchies, pipelines) have trailing comments aligned to a consistent column.
- Check that guidance changes remain internally consistent with current source, build/test commands, APIs, architecture notes, and methodology.

### Documentation Sync
- If code changes alter documented behavior, APIs, build/test workflow, architecture notes,
  or methodology, check that the corresponding Markdown guidance is updated to stay consistent.
- Call out missing doc updates as review findings, and note both the code file and the stale markdown file when possible.
