# Phase 0: CTest Registration

## Status: COMPLETED

CTest was not previously registered. Two files were modified:

### Edit 1: Top-level `CMakeLists.txt` (line before `add_subdirectory(dal)`)

Added:
```cmake
enable_testing()
```

### Edit 2: `tests/CMakeLists.txt` (top and bottom)

Added `include(GoogleTest)` at the top (before `file(GLOB_RECURSE...)`):
```cmake
include(GoogleTest)
```

Added `gtest_discover_tests(test_suite)` at the bottom (after `install(...)`):
```cmake
gtest_discover_tests(test_suite)
```

## Verification

```bash
cd build && cmake .. && make -j4 && ctest --output-on-failure
```

Results:
- **Tests discovered**: 524
- **Passed**: 524
- **Failed**: 0
- **Time**: 11.89 seconds (real)

Previously, `ctest` would have shown "No tests were found." Now all 524 gtest tests are registered as individual CTest tests, e.g.:
- `AADTest.TestNumberAdd` (test 1)
- `InterpTest.TestInterp1Linear` 
- ... up to `NumericsTest.TestCorrelation` (test 524)

## Notes

- `gtest_discover_tests()` requires GoogleTest to be `include()`ed before the `add_executable` call. This is satisfied since gtest is built as a subdirectory early in the CMake configuration.
- Each test case is now individually addressable via `ctest -R <pattern>`, enabling CI to run subsets.
- The `test_main.cpp` entry point (`Dal::RegisterAll_::Init()` + `RUN_ALL_TESTS()`) continues to work unchanged -- CTest simply runs the same binary with GoogletTest's `--gtest_list_tests` and `--gtest_filter` flags.
