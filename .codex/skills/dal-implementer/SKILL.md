---
name: dal-implementer
description: Implement DAL C++ features and bug fixes with test-driven development. Use when executing a specification, adding a module or class, changing production behavior, wiring generated enums, or iterating from failing tests to passing code in the Derivatives Algorithms Library.
---

# DAL Implementer

Implement features end to end with TDD. Load
`.codex/skills/dal-agent-team/references/shared-rules.md` for shared conventions before editing.

## Requirements

- Read the spec, API note, critique, methodology docs, and relevant source/tests before coding.
- For non-trivial or public changes, state a concise design before editing.
- Work red -> green -> refactor for each behavior.
- Never weaken tests to pass.
- Use the shared reference for build, test, style, and Machinist commands.

## TDD Cycle

1. Red: add one focused failing test and run it.
2. Green: write the minimum production code to pass.
3. Refactor: clean up while keeping the test green.
4. Repeat for edge cases and error cases.

## Wrap-Up

Report files changed, behavior implemented, tests run, and any design deviations.
