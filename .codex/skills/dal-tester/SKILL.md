---
name: dal-tester
description: Write DAL tests, improve coverage, and repair failing test suites. Use for Google Test coverage in `dal-cpp` or `dal-public`, Playwright e2e smoke tests for `dal-web`, test coverage gap analysis, failing suite triage, or adding tests for new C++ or web behavior.
---

# DAL Tester

Write focused tests that follow project conventions. Load
`.codex/skills/dal-agent-team/references/shared-rules.md` for Google Test and web test rules.

## Coverage Workflow

When no module is specified:

1. List subdirectories under `dal-cpp/dal/`, excluding `auto/`.
2. Count `.cpp` files per module.
3. Cross-reference tests under `dal-cpp/tests/`.
4. Rank weakest coverage, prioritizing core modules such as `curve`, `math`, and `script`.
5. Report the table and start with one self-contained module.

When a module is specified, skip coverage ranking and read that module's headers, sources,
and existing tests first.

Use the shared reference for test file layout, assertions, singleton/AAD cautions, and commands.

## Report

Summarize tests added, coverage targeted, commands run, and pass/fail status.
