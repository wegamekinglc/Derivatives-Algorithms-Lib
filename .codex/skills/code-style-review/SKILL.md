---
name: code-style-review
description: Review changed C++ code for compliance with this project's coding and unit test style. Use when the user asks to review code style, check style compliance, lint code, verify naming conventions, review changed files for project conventions, or says "review my changes", "check the style", "does this follow our conventions", or similar.
user-invocable: true
---

# Code Style Review

Review all changed files in the working tree against the project's coding conventions defined in `.codex/rules/code-style.md` and `.codex/rules/unit-test-style.md`.

## Steps

1. Read the style guides:
   - `.codex/rules/code-style.md`
   - `.codex/rules/unit-test-style.md`

2. Identify changed files by running:
   ```bash
   git diff --name-only HEAD
   ```
   If there are no uncommitted changes, check the latest commit instead:
   ```bash
   git diff --name-only HEAD~1
   ```

3. Read each changed `.hpp`, `.cpp` file and check for violations against the rules below.

4. Report findings grouped by file, with line numbers and specific violations. If no violations are found, say so.

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
- Comments: sparse, focus on "why" not "what", no docstrings
- All files must end with a newline
- Use `nullptr` instead of `NULL`
- Namespace closing braces must have a comment: `} // namespace Dal`
