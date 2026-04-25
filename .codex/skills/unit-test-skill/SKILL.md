---
name: unit-test-skill
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

- Review the output captured in the root-level `test_output.txt`. A representative passing tail looks like:

```bash
...
[----------] 2 tests from ModelTest
[ RUN      ] ModelTest.TestBlackScholesModelData
[       OK ] ModelTest.TestBlackScholesModelData (0 ms)
[ RUN      ] ModelTest.TestDupireModelData
[       OK ] ModelTest.TestDupireModelData (0 ms)
[----------] 2 tests from ModelTest (0 ms total)

[----------] Global test environment tear-down
[==========] 409 tests from 46 test suites ran. (9497 ms total)
[  PASSED  ] 409 tests.
```

### Linux

- In a Linux bash shell, run the following command to execute the full build-and-test workflow:

```bash
$ bash ./build_linux.sh > test_output.txt 2>&1
```

- Review the output captured in the root-level `test_output.txt`. A representative passing tail looks like:

```bash
...
[----------] 2 tests from ModelTest
[ RUN      ] ModelTest.TestBlackScholesModelData
[       OK ] ModelTest.TestBlackScholesModelData (0 ms)
[ RUN      ] ModelTest.TestDupireModelData
[       OK ] ModelTest.TestDupireModelData (0 ms)
[----------] 2 tests from ModelTest (0 ms total)

[----------] Global test environment tear-down
[==========] 409 tests from 46 test suites ran. (9497 ms total)
[  PASSED  ] 409 tests.
```

## Requirements

- Make sure the full test suite passes. A passing summary looks like this:

```bash
[----------] Global test environment tear-down
[==========] xxx tests from yy test suites ran. (xxxx ms total)
[  PASSED  ] xxx tests.
```

Here `xxx` is the total number of tests and `yy` is the total number of test suites. If any tests fail, the output will identify the failing cases and report the passed/failed counts.
