# DAL Full-Suite Test Reference

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

Here `<N>` is the number of CTest entries selected for the active configuration,
including discovered Google Test cases, configuration checks, and enabled example
smoke tests. Do not pin a historical count: selected entries vary with build options,
labels, and platform. If an entry fails, CTest names it and
`--output-on-failure` prints its diagnostics.
