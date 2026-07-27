# DAL-33 implementer v2 handoff

Date: 2026-07-28
Branch: `fix/dal-33-bcg-scale-stability`

## Exact revisions and scope

- Approved baseline: `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`
- Tester-v3 blocked head reproduced: `0b279713ee383075834ca991f80cad7d35ccc7ec`
- Subnormal-boundary RED:
  `6f725abdda41a18366b04a0293d08d80f2e208ae`
- Minimal subnormal-guard GREEN:
  `1872301400d7dcc847850309bec0f22ad454d140`

The final evidence commit replaces the performance samples and this handoff on
top of the GREEN commit. Its exact remote SHA is reported in the issue comment
because a committed file cannot contain its own commit hash.

Code changes relative to the blocked head are limited to:

- `dal-cpp/dal/math/matrix/bcg.cpp`
- `dal-cpp/tests/math/matrix/test_bcg.cpp`

The production delta is three inserted lines and one guarded condition. Public
headers, `dal-public`, `dal-python`, `dal-excel`, bindings, and AAD surface have
an empty diff.

## Tester-v3 reproduction and RED

The tester-v3 public diagnostic was rebuilt against exact blocked head
`0b279713ee383075834ca991f80cad7d35ccc7ec`. It scans all 2,098 finite
binary64 power-of-two exponents from `-1074` through `1023` through the public
CG/BCG API.

Before the repair:

```text
bcg_boundary exponent=-1030 x=[0,-denorm_min] counts=1/0
bcg_boundary exponent=-1029 x=[0,-denorm_min] counts=1/0
bcg_boundary exponent=-1028 x=[2^-1028,0] counts=3/1
binary64_power_exponents_tested=2098
comparison_failures=274
above_threshold_failures=92
absolute_comparison_failures=92
mixed_threshold_failures=90
failures=274
```

Commit `6f725abdda41a18366b04a0293d08d80f2e208ae` adds a public
`Sparse::BCGSolve` regression for:

- the minimum subnormal, a representative failing-band exponent `-1030`, the
  last failing exponent `-1029`, and the first passing exponent `-1028`;
- relative, absolute, and mixed tolerance forms at the boundary;
- an independent exact-oracle assertion that every initial residual is above
  the inclusive threshold;
- exact successful callback counts (`MultiplyLeft=3`,
  `MultiplyRight=1`) and exact final solution.

Focused RED:

```text
cmake --build build/Release-native-v3 --target dal_cpp_tests -j8
build/Release-native-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter=MatrixTest.TestBCGSolveDoesNotRoundSubnormalThresholdIntoConvergence
```

Result: exit 1 as required. The failing cases retained the initial solution
and reported only `MultiplyLeft=1`, `MultiplyRight=0`; production was unchanged
until this failure was recorded and committed.

## Minimal GREEN

For subnormal thresholds,

```text
certainThreshold = threshold * (1 - uncertainty) / (1 + uncertainty)
```

can round back to `threshold`. Such a value is not a strict conservative inner
bound and therefore cannot prove convergence.

Commit `1872301400d7dcc847850309bec0f22ad454d140` precomputes one private
boolean while constructing the existing convergence object:

- if `certainThreshold < threshold` is representably true, the existing
  early-success fast path remains available;
- otherwise early success is disabled and the existing fixed-width exact
  comparison decides the inclusive contract;
- the conservative outer rejection, exact-zero behavior, callback order,
  direct confirmation, and atomic commit are unchanged.

The boolean is computed before the iteration loop. No public state, heap
allocation, O(n) workspace, callback, or loop allocation was added.

Focused GREEN:

```text
build/Release-native-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter='MatrixTest.TestBCGSolveDoesNotRoundSubnormalThresholdIntoConvergence:MatrixTest.TestBCGSolvePreservesWideExponentResidualContribution:MatrixTest.TestCGSolveAndBCGSolveCommonExponentBoundary:MatrixTest.TestCGSolveAndBCGSolveCommonExponentOracleHandlesZeroThreshold'
```

Result: 4/4 passed.

## Full exponent and build verification

The tester-v3 public scan was rebuilt separately against the native and forced
scalar libraries. Both runs report:

```text
reviewer_probe_x=[1,0] counts=3/1
bcg_boundary exponent=-1030 x=[2^-1030,0] counts=3/1
bcg_boundary exponent=-1029 x=[2^-1029,0] counts=3/1
bcg_boundary exponent=-1028 x=[2^-1028,0] counts=3/1
bcg_boundary exponent=-1027 x=[2^-1027,0] counts=3/1
binary64_power_exponents_tested=2098 range=[-1074,1023]
analytical_comparison_scenarios=20978 comparison_failures=0
reviewer_probe_failures=0 construction_failures=0
above_threshold_failures=0 relative_equality_failures=0
zero_threshold_failures=0 absolute_comparison_failures=0
mixed_threshold_failures=0 atomic_failures=0
failures=0
```

Native focused:

```text
cmake --build build/Release-native-v3 --target dal_cpp_tests -j8
build/Release-native-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter='MatrixTest.TestCGSolve*:MatrixTest.TestBCGSolve*' \
  --gtest_brief=1
```

Result: 29/29 passed.

Forced scalar fallback:

```text
cmake --preset=Release-linux -S . -B build/Release-scalar-fallback-v3 \
  -DDAL_ENABLE_NATIVE_ARCH=OFF \
  -DCMAKE_CXX_FLAGS='-U__AVX2__ -U__FMA__ -U__SSE2__' \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/Release-scalar-fallback-v3 \
  --target dal_cpp_tests -j8
build/Release-scalar-fallback-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter='MatrixTest.TestCGSolve*:MatrixTest.TestBCGSolve*' \
  --gtest_brief=1
```

Result: the cache contains all three macro undefinitions; 29/29 passed.

Full native Release build, install, and CTest:

```text
DAL_BUILD_DIR=build/Release-full-native-v3 \
DAL_INSTALL_DIR=build/stage/Release-full-native-v3 \
NUM_CORES=8 \
ADDITIONAL_CMAKE_FLAGS='-DDAL_ENABLE_NATIVE_ARCH=ON' \
bash ./build_linux.sh
```

Result: 1168/1168 passed.

Documentation, benchmark-script, hygiene, and surface gates:

```text
python3 .github/scripts/check_docs.py
python3 -m unittest discover -s .github/scripts/tests \
  -p 'test_check_benchmark_regressions.py' -q
git diff --check \
  98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8 HEAD
git diff --exit-code --name-only \
  98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8 HEAD -- \
  dal-cpp/dal/math/matrix/bcg.hpp dal-public dal-python dal-excel
```

Results: 39 Markdown files passed; 19/19 script tests passed; diff check
passed; public/binding/AAD surface diff is empty.

## Fresh exact 10x2 paired performance gate

The previous head's samples were replaced only after a new gate passed. The
run used newly created detached sources and new out-of-tree builds:

- baseline source: exact
  `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`;
- head source: exact
  `1872301400d7dcc847850309bec0f22ad454d140`;
- both source worktrees were clean before and after the run;
- recursive submodule SHAs were identical;
- `krylov_perf.cpp` blob was identical:
  `4ba845ea1615728b04c1d2bf14b03ceb66e3ec84`;
- GNU `gcc-14`/`g++-14` 14.3.0, Release static AADET native build;
- benchmarks enabled; tests, examples, public library, and Python disabled;
- base binary SHA-256:
  `e2d4c1694cd78c8c6de35d630f2e3dc64c5c06939fb4eba057c4d59cfab9d6cb`;
- head binary SHA-256:
  `50aa64b31228dfa36c02ca5783c1e1fc890ee229bb9c656b1baf09863771182f`.

Gate command:

```text
DAL_NUM_THREADS=4 \
python3 .github/scripts/check_benchmark_regressions.py \
  --base-root <fresh-root>/base-build \
  --head-root <fresh-root>/head-build \
  --output-dir .codex/artifacts/DAL-33-performance-v2/paired \
  --benchmarks krylov_perf \
  --samples 10 \
  --confirmation-rounds 2 \
  --threshold-percent 4
```

Result: exit 0.

| Case | Base min | Head min | Combined | Round 1 | Round 2 | Gate |
|---|---:|---:|---:|---:|---:|:---:|
| `BCGSolve (500x500 tridiag)` | 19,317 ns | 18,709 ns | -3.15% | -2.94% | -3.15% | pass |
| `CGSolve (500x500 tridiag)` | 14,979 ns | 15,353 ns | +2.50% | +2.50% | +3.51% | pass |

Evidence audit:

- 40 raw outputs: 20 baseline and 20 head;
- both cases are present in every raw output, 160 lines total;
- `failures` is empty in `results.json`;
- no raw output contains the forbidden `calls/solve` marker;
- the tracked directory contains only the replacement exact-baseline /
  exact-GREEN run.

## Remaining risk and next gate

- Subnormal threshold cases now intentionally use the exact fallback when a
  strict inner bound is not representable. The full exponent scan covers all
  power-of-two anchors and the repository regression covers boundary and
  tolerance-mode representatives; independent tester fuzzing of non-power
  mantissas remains useful.
- Native and forced scalar paths passed on this x86 host. CI remains
  authoritative for other ABIs and toolchains.
- No PR was created and nothing was merged.
- Required next step: tester independently retests the exact remote head and
  replacement evidence. Reviewer re-review follows only after tester passes.
  Doc-writer remains paused.
