---
name: dal-unit-test-skill
description: Run and inspect this repository's build and Google Test workflow. Use when the user asks to run all tests, verify the whole codebase, execute the Linux or Windows build/test scripts, inspect `test_output.txt`, or confirm whether the test suite passes.
user-invocable: true
---

# Unit Test Skill

## Instructions

- Before starting, delete any existing `test_output.txt` in the repository root so it cannot be confused with fresh output.

### Windows

- In a Windows terminal, run the following command to execute the full build-and-test workflow:

```bash
$ ./build_windows.bat > 'test_output.txt' 2>&1
```

- Review the output captured in the root-level `test_output.txt`. The build
  scripts run CTest, so a representative passing tail looks like:

```bash
100% tests passed, 0 tests failed out of <N>

Label Time Summary:
...

Total Test time (real) = <seconds> sec
```

### Linux

- In a Linux bash shell, run the following command to execute the full build-and-test workflow:

```bash
$ bash ./build_linux.sh > test_output.txt 2>&1
```

- Review the output captured in the root-level `test_output.txt`. The build
  script excludes the `benchmark` CTest label from its normal test pass, so a
  representative passing tail looks like:

```bash
100% tests passed, 0 tests failed out of <N>

Label Time Summary:
...

Total Test time (real) = <seconds> sec
```

## Requirements

- Make sure the full test suite passes. The authoritative acceptance line is:

```bash
100% tests passed, 0 tests failed out of <N>
```

Here `<N>` is the number of Google Test cases discovered by CTest for the active
configuration. Do not pin a historical test count: the discovered count changes
as tests and platform-specific targets change. If any test fails, CTest names
the failing case and `--output-on-failure` prints its Google Test diagnostics.
