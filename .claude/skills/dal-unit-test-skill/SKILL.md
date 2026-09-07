---
name: dal-unit-test-skill
description: Run and inspect this repository's build and Google Test workflow. Use when the user asks to run all tests, verify the whole codebase, execute the Linux or Windows build/test scripts, inspect `test_output.txt`, or confirm whether the test suite passes.
user-invocable: true
---

# DAL Full-Suite Tests

Follow the complete [shared workflow](../../../.codex/references/run-tests.md).
Its commands, source examples, verification, and reporting rules apply here.
The Claude skill name and user-invocable registration are platform-specific;
shared coding and test conventions remain in `.claude/rules/`, mirrored under
`.codex/references/`. Use the selected specialist contract for role boundaries
and keep verification proportional to the authorized task.
