---
name: dal-unit-test-write
description: |
  Write Google Test unit tests and fix failing test sets for the DAL C++ quantitative finance library. Use when the user asks to write tests, add test coverage,
  create unit tests, repair broken tests, fix failing test suites, or mentions testing for new or existing C++ code. Also trigger when implementing features
  that need test coverage, refactoring code without tests, or when the user mentions a function/class that should be tested.
user-invocable: true
---

# DAL Unit-Test Authoring

Follow the complete [shared workflow](../../../.codex/references/write-tests.md).
Its commands, source examples, verification, and reporting rules apply here.
The Claude skill name and user-invocable registration are platform-specific;
shared coding and test conventions remain in `.claude/rules/`, mirrored under
`.codex/references/`. Use the selected specialist contract for role boundaries
and keep verification proportional to the authorized task.
