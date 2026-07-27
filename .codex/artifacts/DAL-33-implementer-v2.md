# DAL-33 implementer v2 handoff

Date: 2026-07-28
Branch: `fix/dal-33-bcg-scale-stability`

## Current authoritative addendum: reviewer-v2 signed-dot repair

This section supersedes the revision, verification, performance, and
remaining-risk status in the historical tester-v4 handoff retained below.

### Revisions and scope

- Approved baseline:
  `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`
- Reviewer-v2 blocked head:
  `fea55b8faa117b37a0c227063f90bd5e0e8e45e9`
- Signed-dot regression RED:
  `d833a98746d648185f786dce926fa4e157b0e50a`
- Minimal signed-dot GREEN:
  `03d230da6489d37ef3ae3332569293982988436c`

The final evidence commit replaces this handoff and the paired benchmark
samples on top of the GREEN commit. Its exact remote SHA is reported in the
issue comment because a committed file cannot contain its own commit hash.

The RED commit changes only
`dal-cpp/tests/math/matrix/test_bcg.cpp`. The GREEN commit changes only the
private implementation in `dal-cpp/dal/math/matrix/bcg.cpp`. The evidence
commit changes only this handoff and
`.codex/artifacts/DAL-33-performance-v2/paired/**`.

There is no diff in the public header, `dal-public`, `dal-python`,
`dal-excel`, or `dal-cpp/dal/math/aad`. No PR was created and nothing was
merged.

### Reproduction and root cause

The public regression constructs:

```text
A = diag(2^-60, 1, -2^-60)
M_left = M_right = diag(2^60, 1, -2^60)
b = [1, 1, 1]
x0 = [0, 0, 0]
tolRel = EPSILON
tolAbs = 0
```

The ordered alpha-denominator dot contains the finite terms
`{2^60, 1, -2^60}`. A direct sequential binary64 sum is zero even though the
exact representable result is one. The previous private `SlowScaledDot`
stored the entire partial sum in one scaled binary64 mantissa, so it lost the
middle contribution in the same way and reported a false zero. Public
`Sparse::BCGSolve` consequently threw:

```text
BCGSolve: numerical breakdown: alpha denominator
```

### RED

The focused native and forced-scalar command was:

```text
build/Release-{native,scalar-fallback}-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter=MatrixTest.TestBCGSolvePreservesSignedDotCancellation \
  --gtest_brief=1
```

At RED commit `d833a98746d648185f786dce926fa4e157b0e50a`,
both variants failed the one test with the expected alpha-denominator
breakdown. The test also fixes the exact successful solution and public
callback counts:

```text
x = [2^60, 1, -2^60]
left/right = 3/1
preconditioner-left/preconditioner-right = 1/1
```

### Minimal GREEN

The existing ordinary finite, nonzero `InnerProduct` fast path is unchanged.
Only its existing zero/nonfinite fallback changed:

- accumulate positive and negative finite products separately in the
  already-present fixed-size exact limb representation;
- compare and subtract the two exact magnitudes;
- convert the nonzero exact difference to normalized `Scaled_` with
  round-to-nearest, ties-to-even.

The fallback is independent of input order for finite binary64 products and
does not add a public surface, callback, allocation, workspace, or loop
contract. At GREEN commit `03d230da6489d37ef3ae3332569293982988436c`,
the focused public regression passed on native and forced scalar.

### Final correctness and compatibility verification

Native focused:

```text
build/Release-native-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter='MatrixTest.TestCGSolve*:MatrixTest.TestBCGSolve*' \
  --gtest_brief=1
```

Result: 32/32 passed.

Forced scalar focused:

```text
build/Release-scalar-fallback-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter='MatrixTest.TestCGSolve*:MatrixTest.TestBCGSolve*' \
  --gtest_brief=1
```

Result: 31/31 passed. The scalar cache contains:

```text
CMAKE_CXX_FLAGS:STRING=-U__AVX2__ -U__FMA__ -U__SSE2__
```

Joint calibration:

```text
build/Release-native-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter='JointCalibrationTest.*' --gtest_brief=1
```

Result: 17/17 passed.

Fresh full native Release build, install, and CTest:

```text
DAL_BUILD_DIR=build/Release-reviewer-v2-full \
DAL_INSTALL_DIR=build/stage/Release-reviewer-v2-full \
NUM_CORES=8 \
ADDITIONAL_CMAKE_FLAGS='-DDAL_ENABLE_NATIVE_ARCH=ON' \
bash ./build_linux.sh
```

Result:

```text
100% tests passed, 0 tests failed out of 1171
Total Test time (real) = 20.54 sec
```

The tester-v3 public full-exponent probe was freshly rebuilt against both
final native and forced-scalar libraries. Both report:

```text
binary64_power_exponents_tested=2098 range=[-1074,1023]
analytical_comparison_scenarios=20978 comparison_failures=0
atomic_scenarios=2 atomic_failures=0
failures=0
```

The tester-v4 fixed-seed non-power corpus was also freshly rebuilt against
both libraries. Both report:

```text
seed=0x6d5a56da3c9ef187
non_power_mantissa_anchors=18388
cases=165492 oracle_converged=91997 oracle_rejected=73495
solver_initial_returns=91997 solver_continued=73495
false_successes=0 false_rejections=0 failures=0
```

The 165,497-line native and scalar raw outputs are byte-identical:

```text
SHA-256 b7588a7b6a2f5c9a7fe46d9d70d0a469be58c11a912ce714802a37166f5fab62
```

Additional gates:

```text
python3 .github/scripts/check_docs.py
# Documentation checks passed for 39 Markdown files.

python3 -m unittest discover -s .github/scripts/tests \
  -p 'test_check_benchmark_regressions.py' -q
# Ran 19 tests; OK.

git diff --check 98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8 HEAD

git diff --quiet 98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8 HEAD -- \
  dal-cpp/dal/math/matrix/bcg.hpp dal-public dal-python dal-excel \
  dal-cpp/dal/math/aad
```

The two diff/surface commands exited zero.

### Fresh exact 10x2 paired performance gate

The tracked samples were completely replaced after a fresh exact run:

- baseline source:
  `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`;
- head source:
  `03d230da6489d37ef3ae3332569293982988436c`;
- both detached source worktrees were clean before and after the run;
- GNU `gcc-14`/`g++-14` 14.3.0, Release, AADET, native arch enabled;
- baseline benchmark binary SHA-256:
  `1e08b52b267352174dc634e2aa311fb109e3a38ba2e391616f40beaf195905f3`;
- head benchmark binary SHA-256:
  `a40af63c5e493c60e0714fde058b7d000becda8704c7e5c94625d55c24662498`.

Gate:

```text
DAL_NUM_THREADS=4 \
python3 .github/scripts/check_benchmark_regressions.py \
  --base-root /tmp/dal33-reviewer-v2-perf.odq9Hs/base-build \
  --head-root /tmp/dal33-reviewer-v2-perf.odq9Hs/head-final-build \
  --output-dir .codex/artifacts/DAL-33-performance-v2/paired \
  --benchmarks krylov_perf \
  --samples 10 \
  --confirmation-rounds 2 \
  --threshold-percent 4
```

Result: exit zero.

| Case | Base min | Head min | Combined | Round 1 | Round 2 | Gate |
|---|---:|---:|---:|---:|---:|:---:|
| `BCGSolve (500x500 tridiag)` | 19,112 ns | 18,478 ns | -3.32% | -2.80% | -4.08% | pass |
| `CGSolve (500x500 tridiag)` | 14,973 ns | 15,092 ns | +0.79% | +0.79% | +0.75% | pass |

Evidence audit: 40 raw outputs, 160 timing lines, 20 baseline and 20 head
samples per case, empty `failures`, and every comparison is both `passed`
and `gated`.

### Remaining risk and next gate

- To preserve the approved normal fast-path performance contract, ordinary
  finite nonzero dot results continue to use the legacy `InnerProduct`
  result. The new exact, order-independent path is entered when that result
  is zero or nonfinite, including the reviewer-v2 cancellation regression
  and its permutations. Independent review should decide whether a future
  specification requires ambiguity detection for heavily cancelled but
  still nonzero ordinary results.
- The exact fallback uses a fixed range sized for finite binary64 products;
  the focused regression, native/scalar suites, full-exponent scan, and
  fixed-seed corpus pass, but tester must independently reproduce the final
  remote evidence head.
- Tester retest must precede reviewer re-review. Doc-writer remains paused.
- No PR was created and nothing was merged.

## Historical tester-v4 handoff (superseded by the addendum above)

## Exact revisions and current scope

- Approved baseline:
  `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`
- Tester-v4 blocked head reproduced:
  `bae095a1d8111560fcd13c8f254025951000e340`
- Native subnormal-classification RED:
  `b88aa21089fecae6cb83d9f1fdd7e8eaa0074930`
- Minimal native direct-residual GREEN:
  `322aecad0d844bbe28f244329b98d63db69895b8`

The final evidence commit replaces this handoff and every paired benchmark
sample on top of the GREEN commit. Its exact remote SHA is reported in the
issue comment because a committed file cannot contain its own commit hash.

The v4 repair relative to the blocked head changes only:

- `dal-cpp/tests/math/matrix/test_bcg.cpp`
- `dal-cpp/dal/math/matrix/bcg.cpp`

The evidence commit additionally changes only:

- `.codex/artifacts/DAL-33-implementer-v2.md`
- `.codex/artifacts/DAL-33-performance-v2/paired/**`

Public headers, `dal-public`, `dal-python`, `dal-excel`, bindings, and the AAD
surface remain unchanged. No PR was created and nothing was merged.

## Tester-v4 reproduction and root cause

The tester-v4 sources were rebuilt against exact blocked head
`bae095a1d8111560fcd13c8f254025951000e340`, once against the native AVX2/FMA
library and once against the forced scalar library.

The minimal public `Sparse::BCGSolve` probe uses:

```text
A=I2
b=[0x1.02cc22b489eadp-537,0]
x0=[0,-0x1.02cc22b489eadp-557]
tolRel=1
tolAbs=0
```

Blocked native result:

```text
x=[0,-0x1.02cc22b489eadp-557] counts=1/0 correct=0
```

Scalar result:

```text
x=[0x1.02cc22b489eadp-537,0] counts=3/1 correct=1
```

The fixed-seed independent corpus uses seed `0x6d5a56da3c9ef187`,
18,388 unique non-power-of-two mantissa anchors, and nine relative, absolute,
mixed, equality, near-boundary, and zero-threshold scenarios per anchor.
Before the repair:

```text
cases=165492 oracle_converged=91997 oracle_rejected=73495
solver_initial_returns=91860 solver_continued=73632
false_successes=207 false_rejections=344
false_success_scenarios=absolute-above:69,mixed-above:69,relative-above:69
false_reject_scenarios=absolute-equality:86,mixed-near-below:86,relative-equality:86,relative-near-below:86
failures=758
```

The same corpus on scalar reported zero false successes, zero false
rejections, and zero failures.

`ValidatedDirectResidual` accumulated native residual squares with AVX/FMA
but trusted every finite nonzero sum. Near exponent `-537`, individual
squares and their sum are subnormal; binary64 rounding can therefore move
the direct norm across the conservative inner or outer convergence boundary.
The generic/scalar `ScaledNorm` path already rejects such a direct sum unless
it is at least `std::numeric_limits<double>::min()`.

## RED commit

Commit `b88aa21089fecae6cb83d9f1fdd7e8eaa0074930` adds:

- the exact native-only public minimal probe;
- a repository fixed-seed scan of 128 non-power mantissas at the vulnerable
  normal exponent, with all nine tester-v4 threshold scenarios;
- the existing independent common-exponent oracle as the expected
  classifier; and
- exact public success/rejection state plus `MultiplyLeft` /
  `MultiplyRight` callback-count assertions.

The repository sweep covers 1,152 deterministic solver classifications.
The production mutation it catches is accepting a finite subnormal direct
square sum without conservative or exact validation.

RED commands:

```text
cmake --build build/Release-native-v3 --target dal_cpp_tests --parallel 4
cmake --build build/Release-scalar-fallback-v3 \
  --target dal_cpp_tests --parallel 4

build/Release-native-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter='MatrixTest.TestBCGSolveNativeDirectResidualPreservesNonPowerSubnormalNorm:MatrixTest.TestBCGSolveFixedSeedNonPowerSubnormalClassification'

build/Release-scalar-fallback-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter='MatrixTest.TestBCGSolveNativeDirectResidualPreservesNonPowerSubnormalNorm:MatrixTest.TestBCGSolveFixedSeedNonPowerSubnormalClassification'
```

Results:

```text
native: exit 1; 0/2 passed; both tests failed on the wrong initial-return classification
scalar: exit 0; 1/1 passed; the native-only minimal test was not compiled
```

Production remained at the blocked head until the failing tests were
recorded and committed.

## Minimal GREEN

Commit `322aecad0d844bbe28f244329b98d63db69895b8` changes one private
acceptance predicate:

```text
finite && squareSum != 0
```

becomes:

```text
finite && squareSum >= min_normal
```

Normal direct sums keep the existing AVX/FMA square-root fast path. Zero,
subnormal, overflowed, or otherwise non-trustworthy sums reuse the existing
allocation-free `SlowScaledNorm` path and then the unchanged conservative /
exact convergence gates. No callback, commit, public API, workspace,
allocation, AAD, or loop structure changed.

Focused GREEN results:

```text
native: 2/2 passed
scalar: 1/1 passed
```

The rebuilt tester-v4 minimal probe reports the same successful result on
both paths:

```text
x=[0x1.02cc22b489eadp-537,0] counts=3/1 correct=1
```

The rebuilt full tester-v4 corpus reports identically on native and scalar:

```text
seed=0x6d5a56da3c9ef187
non_power_mantissa_anchors=18388
cases=165492 oracle_converged=91997 oracle_rejected=73495
solver_initial_returns=91997 solver_continued=73495
false_successes=0 false_rejections=0
strict_inner_available=145736 collapsed_inner=19756
conservative_fast_candidates=18217
conservative_fast_candidate_oracle_failures=0
failures=0
```

The Python oracle uses exact `Fraction.from_float` dyadic arithmetic and does
not call production or repository-test helpers.

## Native, scalar, exponent, and full verification

Native focused command:

```text
build/Release-native-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter='MatrixTest.TestCGSolve*:MatrixTest.TestBCGSolve*' \
  --gtest_brief=1
```

Result: 31/31 passed.

Forced scalar command:

```text
build/Release-scalar-fallback-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter='MatrixTest.TestCGSolve*:MatrixTest.TestBCGSolve*' \
  --gtest_brief=1
```

Result: 30/30 passed. Its cache contains:

```text
CMAKE_CXX_FLAGS:STRING=-U__AVX2__ -U__FMA__ -U__SSE2__
```

The tester-v3 full public binary64 power-exponent scan was rebuilt and linked
separately to both libraries. Both report:

```text
reviewer_probe_x=[0x1p+0,0x0p+0] counts=3/1
binary64_power_exponents_tested=2098 range=[-1074,1023]
analytical_comparison_scenarios=20978 comparison_failures=0
reviewer_probe_failures=0 construction_failures=0
above_threshold_failures=0 relative_equality_failures=0
zero_threshold_failures=0 absolute_comparison_failures=0
mixed_threshold_failures=0
atomic_scenarios=2 atomic_failures=0
failures=0
```

Full native Release build, install, and CTest:

```text
DAL_BUILD_DIR=build/Release-full-native-v3 \
DAL_INSTALL_DIR=build/stage/Release-full-native-v3 \
NUM_CORES=8 \
ADDITIONAL_CMAKE_FLAGS='-DDAL_ENABLE_NATIVE_ARCH=ON' \
bash ./build_linux.sh
```

Result:

```text
100% tests passed, 0 tests failed out of 1170
Total Test time (real) = 20.56 sec
```

Documentation, benchmark-script, diff, and surface commands:

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

Results:

```text
documentation: 39 Markdown files passed
benchmark-script tests: 19/19 passed
diff --check: passed
public/binding/AAD surface diff: empty
```

## Fresh exact 10x2 paired performance gate

All prior samples were replaced only after a new gate passed. The new run
used freshly created detached sources and new out-of-tree build roots:

- baseline source: exact
  `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`;
- head source: exact
  `322aecad0d844bbe28f244329b98d63db69895b8`;
- both source worktrees were clean before and after the run;
- recursive submodule SHAs were identical;
- `krylov_perf.cpp` blob was identical:
  `4ba845ea1615728b04c1d2bf14b03ceb66e3ec84`;
- GNU `gcc-14`/`g++-14` 14.3.0;
- Release shared-library AADET native builds with benchmarks enabled and
  tests, examples, public library, Python, and Excel disabled;
- baseline binary SHA-256:
  `73851a7f7c7567b6ce2a2dc14706077830c365e0b3e7c7ba09d5b57433dddb49`;
- head binary SHA-256:
  `b18858ad4b051787b1d2ef0fb1242f2772d47e21dec6e737ebaea2b99e6e68fa`.

The shared-library description above corrects the previous handoff's
inaccurate `static` label; the configuration is the repository/CI Unix
default and matches both fresh roots.

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

| Case                              | Base min  | Head min  | Combined | Round 1 | Round 2 | Gate |
|-----------------------------------|-----------|-----------|----------|---------|---------|------|
| `BCGSolve (500x500 tridiag)`      | 19,025 ns | 18,890 ns | -0.71%   | -0.71%  | -0.13%  | pass |
| `CGSolve (500x500 tridiag)`       | 14,978 ns | 15,231 ns | +1.69%   | +1.69%  | +1.88%  | pass |

Evidence audit:

- 40 raw outputs: 20 baseline and 20 head;
- both cases are present in every raw output, 160 timing lines total;
- every case has 20 base and 20 head samples in `results.json`;
- `failures` is empty;
- every comparison is both `passed` and `gated`;
- no raw output contains the forbidden `calls/solve` marker; and
- the tracked directory contains only the replacement exact-baseline /
  exact-GREEN samples.

## Remaining risk and required next gate

- The independent tester-v4 corpus now agrees exactly between native and
  scalar across 165,492 cases, including success/rejection and callback
  classification. Tester must still independently reproduce this against the
  final remote evidence head.
- The new branch is exercised on x86 AVX2/FMA with GNU 14.3. CI remains
  authoritative for other processors, compilers, and ABIs.
- The normal direct-square-sum control path and operations are unchanged
  above `min_normal`, and the exact paired performance gate passes, but later
  broad performance CI remains authoritative.
- Tester retest must precede reviewer re-review. Doc-writer remains paused.
- No PR was created and nothing was merged.
