---
name: unit-test-skill
description: do unit tests for the whole codebase
user-invocable: true
---

# Unit Test Skill

## Instructions

- Before starts make sure you have delete any old files named `test_output.txt`. in the root directory of the codebase to avoid confusion with the new output.

### Windows

- In the windows terminal, run the following command to execute all unit tests for the codebase:

```bash
$ powershell -Command "Set-Location 'D:\dev\github\dal.cpp'; Start-Process -FilePath '.\build_windows.bat' -Wait -NoNewWindow -RedirectStandardOutput 'test_output.txt'" 2>&1
```

- Get the output of the unit tests from the `test_output.txt` file generated in the root directory of the codebase. The output should look like:

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

- In the Linux bash shell terminal, run the following command to execute all unit tests for the codebase:

```bash
$ bash ./build_linux.sh > test_output.txt 2>&1
```

- Get the output of the unit tests from the `test_output.txt` file generated in the root directory of the codebase. Output should be like this:

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

- Make sure all the unit tests for the codebase are passed.