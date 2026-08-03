# Contributing to DAL

DAL is a C++17 quantitative-finance workspace with AAD, Python, Excel, and web
surfaces. Keep changes small, test the contract being changed, and preserve the
dependency direction:

```text
dal-cpp <- dal-public <- {dal-python, dal-excel}
                           ^
                           |
                        dal-web
```

## Set Up

Clone submodules and follow the canonical [installation guide](docs/installation.md).
For the standard Linux core workflow:

```bash
git submodule update --init --recursive
bash ./build_linux.sh
```

This builds core/public C++, stages the install under
`build/stage/Release-linux`, and runs CTest. Use `--full` when Python and
benchmarks are in scope, `--benchmarks` for benchmark targets, and `--generate`
only when Machinist output must be refreshed.

Before editing, inspect `git status` and leave unrelated work intact. Do not mix
cleanup with a behavior change unless the cleanup is needed to make that change
safe.

## Development Loop

For C++ behavior, prefer red-green-refactor:

1. Add or tighten a focused Google Test that fails for the missing behavior.
2. Make the smallest production change that satisfies the contract.
3. Run the focused test, then the relevant suite.
4. Refactor only while the tests stay green.

Configure a reusable core tree with:

```bash
cmake --preset core-dev
cmake --build build/core-dev --parallel
ctest --test-dir build/core-dev --output-on-failure
```

Focused examples:

```bash
build/core-dev/dal-cpp/dal_cpp_tests --gtest_filter=CalibrationTest.*
build/core-dev/dal-public/dal_public_tests --gtest_filter=PublicApiTest.*
```

Google Tests use `TEST(Suite, TestName)`, `ASSERT_*`, deterministic tolerances,
and locally owned setup. Clear AAD tape state at the beginning and end of tests
that manipulate it directly.

## Surface-Specific Checks

Run checks in proportion to the files changed.

Python bindings:

```bash
bash ./build_linux.sh --full
```

After installing the editable package against a staged DAL prefix, focused
Python tests can be run with:

```bash
(cd dal-python && python -m pytest tests -v)
```

Web backend/frontend:

```bash
(cd dal-web/backend && uv run --no-sync pytest)
(cd dal-web/backend && uv run --no-sync ruff check .)
(cd dal-web/frontend && npm run build)
./dal-web/scripts/setup-playwright.sh
(cd dal-web/frontend && npm run test:e2e)
```

Installed-package changes should also configure and run the consumer under
`tests/installed-consumer/` against a fresh staging prefix.

### Benchmark regressions

Build benchmark targets locally with:

```bash
bash ./build_linux.sh --benchmarks
```

Linux pull requests compare base and head builds on the same runner with GCC 14,
Release mode, native CPU tuning, the native AAD backend, and
`DAL_NUM_THREADS=4`. The gate runs two independent rounds of ten interleaved
process-level samples per side. A comparable case fails only when its head/base
median exceeds `+4%` in both rounds, which requires a repeated regression rather
than a single noisy measurement. Base-only cases fail as removals or renames.
Head-only cases are reported as new informational coverage; the explicit Sobol
precise-policy migration remains validated separately, and the head Sobol
precise/fast ratio has a `10x` ceiling. The Windows benchmark job remains
informational.

To reproduce the comparator after building separate base and head trees:

```bash
python3 .github/scripts/check_benchmark_regressions.py \
  --base-root <base-build> \
  --head-root <head-build> \
  --output-dir <results-dir> \
  --samples 10 \
  --confirmation-rounds 2 \
  --threshold-percent 4 \
  --precise-slowdown-limit 10
```

The roots are the CMake build directories containing
`dal-cpp/benchmarks/<benchmark>/<benchmark>` executables. Preserve the CI build
configuration when investigating a CI-only regression.

## C++ Style

Use the root `.clang-format` configuration. The main conventions are:

- four-space indentation, attached braces, and C++17;
- PascalCase classes/functions, trailing underscore on class names and members;
- lowercase filenames without separators; `test_` prefix for test files;
- `Handle_<T_>` for shared const ownership and `std::unique_ptr<T_>` for exclusive ownership;
- `REQUIRE` for contract validation and `THROW` for DAL error paths; and
- `#pragma once` plus the existing DAL file/namespace conventions in headers.

Format only touched C++ files, for example:

```bash
clang-format -i path/to/changed.cpp path/to/changed.hpp
```

Avoid broad mechanical rewrites in a feature patch.

## Generated Code

Machinist markup drives core and Excel output. When markup changes, regenerate
both trees:

```bash
cmake --build build/core-dev --target dal_generate
cmake --build build/core-dev --target dal_check_generated
```

The outputs are `dal-cpp/dal/auto/` and `dal-excel/auto/`. Do not hand-edit
generated files; commit them with the markup source. `dal_check_generated`
fails on both tracked drift and newly generated untracked files.

## Public Surfaces

A change to `dal-public` should be considered across all consumers:

- installed CMake targets and headers;
- Python binding names, defaults, conversions, and exceptions;
- Excel worksheet registration/generated output; and
- published examples and the [public API guide](docs/public-api.md).

`DAL::public` is a convenience facade over core types, not an ABI-isolated
boundary. Even an apparently internal type change can affect downstream source
compatibility.

## Documentation Ownership

Documentation describes the current library only.

- `docs/installation.md` owns setup commands and prerequisites.
- Component READMEs explain component purpose and link to the canonical setup.
- `docs/architecture.md` owns boundaries, state, and execution flows.
- `docs/public-api.md` owns supported C++/Python/Excel entry points.
- `docs/methodology/` owns mathematical definitions and numerical invariants.
- `docs/README.md` indexes every public document.
- `CHANGELOG.md` alone owns historical context.

Do not cite source line numbers in durable docs. Add a changelog entry only for a
breaking public API change, new numerical methodology, significant capability or
methodology shift, or removal/deprecation of public surface.

## Review Checklist

Before handing work off for review, check:

- the behavior and error contract are explicit;
- numerical expectations come from an independent oracle where feasible;
- AAD and non-AAD paths agree when both exist;
- invalid inputs fail before unsafe conversions or host-process hazards;
- focused and relevant broad tests pass;
- public/binding/generated surfaces remain synchronized;
- docs describe the resulting current state;
- relative links, Markdown tables, trailing whitespace, and final newlines are clean; and
- performance-sensitive changes have representative benchmark coverage or a stated reason they do not need it.

Contributions are distributed under the repository [MIT license](LICENSE).
