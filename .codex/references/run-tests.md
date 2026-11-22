# DAL Full-Suite Test Reference

Run these commands from the repository root. The initial redirection creates a
fresh `test_output.txt` for the selected platform.

## Windows

In Command Prompt, `--full-tests` includes the examples and slow qualification
cases. CTest excludes the `benchmark` label.

```bat
build_windows.bat --full-tests > test_output.txt 2>&1
```

## Linux

The build script runs CTest with `-LE "benchmark|examples"`. For full
non-benchmark coverage, run the enabled example tests from the same build
tree after the script succeeds. The `examples` label expression matches both
`examples` and `examples_slow`.

```bash
build_dir=${DAL_BUILD_DIR:-build/${BUILD_TYPE:-Release}-linux}
bash ./build_linux.sh > test_output.txt 2>&1 &&
    ctest --test-dir "$build_dir" -L examples -LE benchmark --output-on-failure >> test_output.txt 2>&1
```

## Requirements

Check the command exit status and report the fresh CTest summary for each
pass from `test_output.txt`. A passing summary has the form

```text
100% tests passed, 0 tests failed out of <N>
```

Here `<N>` is the number of CTest entries selected for the active configuration,
including discovered Google Test cases, configuration checks, and enabled example
smoke tests. Do not pin a historical count: selected entries vary with build options,
labels, and platform. If an entry fails, CTest names it and
`--output-on-failure` prints its diagnostics.
