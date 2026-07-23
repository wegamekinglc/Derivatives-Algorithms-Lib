---
name: dal-tester
description: Run full DAL test suites, write tests, improve coverage, and repair failures. Use for repository-wide verification, Google Test coverage in `dal-cpp` or `dal-public`, Playwright e2e smoke tests for `dal-web`, test coverage gap analysis, failing suite triage, or tests for new C++ or web behavior.
---

# DAL Tester

Run or write focused tests that follow the
[shared DAL rules](../dal-agent-team/references/shared-rules.md).

## Run Existing Tests

For full Linux or Windows build-and-test requests, load and follow
[the full-suite test workflow](references/run-tests.md). Report the fresh captured summary and
any failing cases; do not infer success from an old `test_output.txt`.

For a targeted failure, reproduce it with the narrowest relevant filter before expanding to the
owning test binary or full suite.

## Write Or Repair Tests

Before adding or changing tests, load
[the unit-test authoring workflow](references/write-tests.md). Read the production API and nearby
tests, then use a failing test to reproduce the required behavior or defect before implementation.

## Coverage Workflow

When no module is specified:

1. List subdirectories under `dal-cpp/dal/`, excluding `auto/`.
2. Count `.cpp` files per module.
3. Cross-reference tests under `dal-cpp/tests/`.
4. Rank weakest coverage, prioritizing core modules such as `curve`, `math`, and `script`.
5. Report the table and start with one self-contained module.

When a module is specified, skip coverage ranking and read that module's headers, sources,
and existing tests first.

Use the linked references for test file layout, assertions, singleton/AAD cautions, and commands.

## References

- [Shared DAL rules](../dal-agent-team/references/shared-rules.md): concise project-wide test digest.
- [Full-suite test workflow](references/run-tests.md): Linux and Windows complete-suite execution.
- [Unit-test authoring workflow](references/write-tests.md): Google Test creation and repair patterns.

## Report

Summarize tests added, coverage targeted, commands run, and pass/fail status.
