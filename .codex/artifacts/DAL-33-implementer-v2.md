# DAL-33 implementer v2 handoff

Date: 2026-07-28
Branch: `fix/dal-33-bcg-scale-stability`

## Exact revisions

- Approved baseline: `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`
- Reviewer-blocked head reproduced: `235167a1ed2e44b46b155ab3360f839a9df272bc`
- RED: `8cb313e6eafc743f59a3e3022237ff4737ecbb45`
- GREEN: `9307ccfc2cca46fe56bb668c36a86c8c403b367e`
- Green performance refactor: `f195dda78f6d24ad3961b76683fc2f6685a625d9`

The final delivery commit adds only this handoff and the replacement paired
benchmark evidence on top of the code head. Its exact remote SHA is reported in
the issue handoff because a committed file cannot contain its own commit hash.

## Reviewer blocker reproduction and RED

The reviewer-supplied public-API probe was rebuilt against exact head
`235167a1ed2e44b46b155ab3360f839a9df272bc`:

```text
g++ -std=c++17 -O2 -I dal-cpp \
  ../.multica-control/dal-33-reviewer2/dal33_reviewer_wide_residual_probe.cpp \
  build/Release-reviewer/dal-cpp/libdal_cpp.a -pthread \
  -o /tmp/dal33_reviewer_wide_residual_probe
/tmp/dal33_reviewer_wide_residual_probe
```

Result before the repair:

```text
x=[0x0p+0,-0x0.0000000000001p-1022]
exit 1
```

Commit `8cb313e6eafc743f59a3e3022237ff4737ecbb45` adds:

- a public `Sparse::BCGSolve` regression with `A=I₂`, `b=[1,0]`,
  `x0=[0,-denorm_min]`, `tolRel=1`, and `tolAbs=0`;
- exact successful callback counts (`MultiplyLeft=3`, `MultiplyRight=1`,
  both preconditioners zero);
- an independent-oracle assertion that the initial residual is not converged;
- a zero-threshold test that exercises an empty right-hand bit map.

Focused RED:

```text
./build/Release-linux/dal-cpp/dal_cpp_tests \
  --gtest_filter='MatrixTest.TestBCGSolvePreservesWideExponentResidualContribution:MatrixTest.TestCGSolveAndBCGSolveCommonExponentOracleHandlesZeroThreshold'
```

Result: the public solver regression failed as required: `x[0]` was `0`,
expected `1`. The zero-threshold path exposed the separately reviewed empty-map
precondition; the production fix was not started before the solver RED was
recorded and committed.

## Minimal GREEN and scope

Changed code files relative to the blocked head:

- `dal-cpp/dal/math/matrix/bcg.cpp`
- `dal-cpp/tests/math/matrix/test_bcg.cpp`

Commit `9307ccfc2cca46fe56bb668c36a86c8c403b367e`:

- retains the fast scaled-norm comparison away from the convergence boundary;
- uses a private fixed-width exact nonnegative accumulator only in the
  conservative uncertainty interval;
- compares the inclusive contract without dropping any nonzero binary64
  squared contribution, including contributions separated by the full binary64
  exponent range;
- fixes the test oracle's zero-threshold empty right-hand bit map before any
  iterator dereference;
- removes the added `mutable`; `StableBatch_::Finish` is non-const;
- leaves public headers, bindings, callback ordering/counting, atomic commit,
  and all O(n) solver workspaces unchanged. The exact fallback uses fixed-size
  automatic storage and performs no heap or O(n) allocation in the loop.

Commit `f195dda78f6d24ad3961b76683fc2f6685a625d9` is a behavior-preserving GREEN
refactor. It algebraically precomputes the same conservative inner and outer
thresholds before the iteration loop. The common not-yet-converged path again
needs one scaled comparison, while the uncertainty interval still routes to
the exact comparator.

The repaired public probe now reports:

```text
x=[0x1p+0,0x0p+0]
exit 0
```

## Functional verification

Focused native (`DAL_ENABLE_NATIVE_ARCH=ON`, AADET/AVX2):

```text
./build/Release-native-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter='MatrixTest.TestCGSolve*:MatrixTest.TestBCGSolve*' \
  --gtest_brief=1
```

Result: 28/28 passed.

The new public solver regression was also repeated ten times without failure.

Explicit scalar fallback:

```text
cmake --preset=Release-linux -S . -B build/Release-scalar-fallback-v3 \
  -DDAL_ENABLE_NATIVE_ARCH=OFF \
  -DCMAKE_CXX_FLAGS='-U__AVX2__ -U__FMA__ -U__SSE2__' \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/Release-scalar-fallback-v3 --target dal_cpp_tests -j8
./build/Release-scalar-fallback-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter='MatrixTest.TestCGSolve*:MatrixTest.TestBCGSolve*' \
  --gtest_brief=1
```

Result: the compile database contains all three explicit macro undefinitions;
28/28 passed.

Full native Release build, install, and CTest:

```text
DAL_BUILD_DIR=build/Release-full-native-v3 \
DAL_INSTALL_DIR=build/stage/Release-full-native-v3 \
NUM_CORES=8 \
ADDITIONAL_CMAKE_FLAGS='-DDAL_ENABLE_NATIVE_ARCH=ON' \
bash ./build_linux.sh
```

Result: 1167/1167 passed.

Large finite RHS repeated three times:

```text
./build/Release-full-native-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter=MatrixTest.TestCGSolveAndBCGSolveLargeFiniteRhs \
  --gtest_repeat=3 --gtest_brief=1
```

Result: 3/3 passed.

Documentation and benchmark-script checks:

```text
python3 .github/scripts/check_docs.py
python3 -m unittest discover -s .github/scripts/tests \
  -p 'test_check_benchmark_regressions.py' -q
```

Results: 39 Markdown files passed; 19/19 script tests passed.

Patch and public-surface gates:

```text
git diff --check \
  98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8 HEAD
git diff --exit-code --name-only \
  98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8 HEAD -- \
  dal-cpp/dal/math/matrix/bcg.hpp dal-public dal-python dal-excel
```

Results: both exit 0; the public-surface diff is empty.

## Fresh exact 10x2 paired performance gate

The prior paired directory was removed from the deliverable and replaced. The
final run used newly initialized detached sources and new out-of-tree builds:

- baseline source: exact
  `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`;
- head source: exact
  `f195dda78f6d24ad3961b76683fc2f6685a625d9`;
- both worktrees were clean before and after the run;
- recursive submodule SHAs were identical;
- `krylov_perf.cpp` blob was identical:
  `4ba845ea1615728b04c1d2bf14b03ceb66e3ec84`;
- GNU `gcc-14`/`g++-14`, Release static AADET native build;
- tests, examples, public library, Python, XAD, CoDiPack, and Adept disabled;
  benchmarks enabled;
- no benchmark source instrumentation and no raw `calls/solve` marker.

Command:

```text
DAL_NUM_THREADS=4 \
python3 .github/scripts/check_benchmark_regressions.py \
  --base-root <fresh-root>/baseline-build \
  --head-root <fresh-root>/head2-build \
  --output-dir .codex/artifacts/DAL-33-performance-v2/paired \
  --benchmarks krylov_perf \
  --samples 10 \
  --confirmation-rounds 2 \
  --threshold-percent 4
```

Result: exit 0.

| Case | Base min | Head min | Combined | Round 1 | Round 2 | Gate |
|---|---:|---:|---:|---:|---:|---|
| `BCGSolve (500x500 tridiag)` | 19,277 ns | 18,633 ns | -3.34% | -3.38% | -3.21% | pass |
| `CGSolve (500x500 tridiag)` | 14,918 ns | 15,321 ns | +2.70% | +2.23% | +2.70% | pass |

Evidence audit:

- 40 raw outputs: 20 baseline and 20 head;
- both cases are present in every raw output;
- `failures` is empty in `results.json`;
- the tracked evidence directory contains only this passing exact-baseline /
  exact-head run.

For transparency, the first fresh run against GREEN
`9307ccfc2cca46fe56bb668c36a86c8c403b367e` failed the CG gate in both rounds
(+4.56%, +4.51%). It was not retained as evidence. The equivalent threshold
precomputation in `f195dda78f6d24ad3961b76683fc2f6685a625d9` removed the repeated
loop overhead, after which the entire paired gate was rebuilt and rerun from
exact revisions.

## Remaining risk and next gate

- The exact fallback is private and fixed-width for the complete finite
  binary64 square/product range. Focused tests cover the reported widest
  exponent separation and the existing boundary/scale-metamorphic cases, but
  independent tester fuzzing remains valuable.
- The explicit scalar fallback ran on this x86 host; CI remains authoritative
  for non-x86 ABIs and other toolchains.
- No PR was created and nothing was merged.
- Required next step: tester independently checks the new remote head and
  evidence. Reviewer re-review follows only after tester passes. Doc-writer
  remains paused.
