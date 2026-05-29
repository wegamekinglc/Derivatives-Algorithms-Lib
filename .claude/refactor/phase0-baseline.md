# Phase 0: Build and Test Baseline

## Build Result: PASSED

- **Date**: 2026-05-30
- **Command**: `bash ./build_linux.sh`
- **Build type**: Release, static (BUILD_SHARED_LIBS=OFF)
- **AAD backend**: Native (AADET) -- no external backend enabled in current preset
- **Platform**: Linux (WSL2), 32 cores
- **Warnings**: None
- **Errors**: None
- **Targets built**: dal_library, dal_public (not built, SKIP_TESTS=false means tests subdirectory was added), test_suite, all examples (aad, concurrency, curve_calibration, digital, european_mc, european_fd, matrix_perf, script, snowball, sobol, underdetermined, uoc, uoc_compiled, vanilla)

## Test Result: PASSED

- **Binary**: `bin/test_suite`
- **Framework**: Google Test (gtest)
- **Total tests**: 524
- **Test suites**: 56
- **Passed**: 524
- **Failed**: 0
- **Skipped**: 0
- **Duration**: 10019 ms (~10 seconds)

## CTest Status

- **Current state**: NOT registered. There is no `enable_testing()`, `add_test()`, or `gtest_discover_tests()` anywhere in the CMake project. Running `ctest` would produce "No tests were found."
- **Action needed**: Add `enable_testing()` to top-level CMakeLists.txt, `include(GoogleTest)` and `gtest_discover_tests(test_suite)` to tests/CMakeLists.txt.

## Key Observations

1. The build produces a complete, passing codebase -- ideal baseline for refactoring.
2. dal_public was NOT built in this run because SKIP_TESTS=false only controls the tests subdirectory. However, DAL_BUILD_PUBLIC is ON by default, so dal_public IS built as part of the `make` step.
3. Excel example targets (from public/excel) were not built -- Excel detection failed on Linux (expected).
4. Code generation ran successfully: `dal/auto/` has 86 generated files, `public/auto/` was also generated.
