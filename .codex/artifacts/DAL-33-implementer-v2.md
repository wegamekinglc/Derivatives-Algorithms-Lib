# DAL-33 implementer v2 handoff

Date: 2026-07-28
Branch: `fix/dal-33-bcg-scale-stability`

## Current authoritative addendum: PR #262 Codacy complexity repair

This section supersedes the revision, verification, performance, and
remaining-risk status in every historical addendum retained below.

### Revisions and permitted scope

- reviewed pre-refactor head:
  `b0de03b6817c68d461b385e0aeea677c9be20ce8`;
- behavior-preserving complexity refactor:
  `74e6468ee80aa98c26cd89785cf96250062a3638`.

The final evidence commit is on top of the refactor and changes only this
handoff plus `.codex/artifacts/DAL-33-performance-v2/paired/**`. Its exact SHA
is reported in the issue comment because a committed file cannot contain its
own commit hash.

The refactor is limited to private helper decomposition in
`dal-cpp/dal/math/matrix/bcg.cpp`, equivalent helper decomposition in
`dal-cpp/tests/math/matrix/test_bcg.cpp`, and the exact pre-fix Codacy evidence
in `.codex/artifacts/DAL-33-codacy-complexity-before.json`. It does not change
public headers, bindings, AAD, callback order/counts, atomic publication,
numeric predicates, arithmetic order, or loop allocation contracts.

### Codacy reproduction and minimal repair

Codacy check run `90156634450` on the reviewed head concluded
`action_required` with exactly ten new complexity annotations. The attachment
records their exact paths, lines, messages, check title, details URL, and
reviewed head before any refactor.

The production annotations were `AddScaled` (10), `ScaledFromExact` (9),
`FastScaledDot` (15), `HasOnlyFiniteValues` (9), `ScaledRatio` (9),
`ValidatedDirectResidual` (10), and `PrepareDirection` (12). The test
annotations were `CommonExponentConverged` (10), `AssertCallbackFault` (31),
and the finite-nonzero permutation `TEST` (9), all against a limit of 8.

The repair extracts single-purpose private helpers while retaining the
original branch predicates and operation order. No rule suppression,
complexity-threshold change, file exclusion, or test weakening is present.
The same Lizard 1.23.0 metric now reports the ten annotated methods at,
respectively:

```text
production: 5, 3, 2, 3, 7, 2, 1
tests:      1, 1, 2
```

The only local complexity warnings remaining are the unchanged baseline
methods `ValidateKrylovParams` and `KrylovSolve`; they are outside the ten new
PR annotations and were not modified.

### Current correctness and compatibility evidence

```text
native focused MatrixTest.*CGSolve*: 33/33 passed
forced-scalar MatrixTest.*CGSolve*:   32/32 passed
complete native CTest:                1172/1172 passed
documentation:                        39 Markdown files passed
benchmark-script unit tests:          19/19 passed
git diff --check:                     passed
public/header/binding/AAD diff:        empty
```

### Fresh exact 10x2 paired 4% performance gate

The baseline executable was retained from the independently built exact
baseline source `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`. The head
executable was rebuilt from a detached clone of exact production
refactor `74e6468ee80aa98c26cd89785cf96250062a3638`. Both use GNU
`gcc-14`/`g++-14` 14.3.0, Release shared-library AADET native builds,
benchmarks enabled, and tests/examples/public/Python/Excel disabled.

Runtime provenance:

```text
DAL_NUM_THREADS=4
host: 13th Gen Intel(R) Core(TM) i9-13900HX, 32 logical CPUs
kernel: Linux 5.15.167.4-microsoft-standard-WSL2 x86_64
baseline krylov_perf sha256:
  81d46f1b204a00cd0d44c9a9a5a8a520afc5a07a5d0a2e7dfa344fbfac657e74
head krylov_perf sha256:
  72241957bbf0b0a75fbceb2eac0b239367d9b93c0da4c800383d27bf2b25e284
head libdal_cpp.so.1.0.0 sha256:
  c8a556bca20038e5bcd6103f3c17e506450ec38d62782b434d0894e0396a1cc2
runtime binding:
  libdal_cpp.so.1.0.0 => exact detached head-build library
```

Gate:

```text
DAL_NUM_THREADS=4 \
python3 .github/scripts/check_benchmark_regressions.py \
  --base-root <exact-baseline-build> \
  --head-root <fresh-refactor-build> \
  --output-dir .codex/artifacts/DAL-33-performance-v2/paired \
  --benchmarks krylov_perf \
  --samples 10 \
  --confirmation-rounds 2 \
  --threshold-percent 4
```

Result: exit zero.

| Case | Base min | Head min | Combined | Round 1 | Round 2 | Gate |
|---|---:|---:|---:|---:|---:|:---:|
| `BCGSolve (500x500 tridiag)` | 18,975 ns | 15,874 ns | -16.34% | -14.93% | -17.29% | pass |
| `CGSolve (500x500 tridiag)` | 14,870 ns | 12,630 ns | -15.06% | -15.02% | -16.49% | pass |

Evidence audit: 40 raw outputs, 20 baseline and 20 head samples per case,
empty `failures`, and every comparison both `passed` and `gated`.

### Remaining risk and required next gate

- Local complexity output confirms the requested decomposition, but the
  authoritative Codacy rerun is asynchronous and must be read from PR #262.
- Helper extraction is intended to preserve branch predicates and arithmetic
  order and passed native/scalar/full/performance gates; tester must still
  independently rerun its exact numerical and callback corpora before reviewer
  and doc-writer re-entry.
- PR #262 remains unmerged. No merge is authorized in this round.

## Current authoritative addendum: tester-v6 finite-nonzero signed-dot repair

This section supersedes the revision, verification, performance, and
remaining-risk status in every historical addendum retained below.

### Revisions and permitted scope

- approved baseline:
  `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`;
- tester-v6 blocked head:
  `72b4af7177de5f2134a1ce2addf1aa1dd1600d89`;
- finite-nonzero permutation RED:
  `6c3cf6c1630c5d8021af969e4843c756057ac0e0`;
- minimal signed-dot GREEN:
  `0d03402a404d21e2c3fd758480f59bc96b023f1c`.

The final evidence commit is on top of the GREEN commit and changes only this
handoff plus `.codex/artifacts/DAL-33-performance-v2/paired/**`. Its exact SHA
is reported in the issue comment because a committed file cannot contain its
own commit hash.

The RED commit changes only
`dal-cpp/tests/math/matrix/test_bcg.cpp`. The GREEN commit changes only the
private implementation in `dal-cpp/dal/math/matrix/bcg.cpp`. Public headers,
bindings, AAD, callback order/counts, atomic publication, and loop allocation
contracts are unchanged. No PR was created and nothing was merged.

### Tester-v6 reproduction and root cause

The new public BCG regression exhausts all 24 permutations of:

```text
L = 2^60
b = permutation({L, 100, -L, 2})
A = diag(3*b[i])
M_left(input)[i] = M_right(input)[i] = input[i] / b[i]
x0 = [0, 0, 0, 0]
tolRel = EPSILON
tolAbs = 0
maxIterations = 1
```

The independent exact result is:

```text
beta = L + 100 - L + 2 = 102
alpha denominator = 3L + 300 - 3L + 6 = 306
alpha = 1/3
x = [1/3, 1/3, 1/3, 1/3]
callbacks left/right/pre-left/pre-right = 3/1/1/1
```

Eighteen permutations produce ordinary sequential beta and denominator
results that are both finite and nonzero. The blocked implementation trusted
all such values and therefore never entered its exact signed accumulator.
Tester-v6 observed 10 failures among those 18 fast-path permutations and 16
failures among all 24 permutations.

### RED

Commit `6c3cf6c1630c5d8021af969e4843c756057ac0e0` fixes the exact solution,
callback counts, 24-permutation coverage, and 18 finite-nonzero fast cases in
one public solver test:

```text
MatrixTest.TestBCGSolvePreservesFiniteNonzeroSignedDotCancellationPermutations
```

Focused command:

```text
build/Release-{native,scalar-fallback}-v3/dal-cpp/dal_cpp_tests \
  --gtest_filter=MatrixTest.TestBCGSolvePreservesFiniteNonzeroSignedDotCancellationPermutations \
  --gtest_brief=1
```

At RED, native and forced scalar each reported `0/1`; the expected failure was
`Exhausted iterations in BCGSolve`. Production remained at the blocked head
until this test was committed.

### Minimal GREEN

Commit `0d03402a404d21e2c3fd758480f59bc96b023f1c` replaces the previous
“finite and nonzero means safe” decision with a private conservative filter:

- native AVX2 and scalar paths accumulate the signed product sum and the
  absolute-product sum;
- a nonzero input product that underflows, is subnormal, or is non-finite
  rejects the fast path;
- a non-finite/zero sum, non-finite absolute sum, or length-derived roundoff
  interval that reaches the signed result rejects the fast path;
- every rejected or ambiguous dot uses the existing positive/negative exact
  limb accumulator, whose finite binary64 products have unbounded intermediate
  range, order-independent cancellation, and one final ties-to-even binary64
  rounding.

Thus zero or non-finite is no longer the sole fallback condition. Ordinary
normal, low-cancellation recurrence dots retain the vector fast path; all
wide-exponent, underflowed-product, overflowed-product, zero, and
cancellation-ambiguous cases fail safe to exact accumulation.

### Current correctness and compatibility evidence

Focused repository suites:

```text
native AVX2/FMA: 33/33 passed
forced scalar:   32/32 passed
joint calibration: 17/17 passed
```

The scalar cache still contains:

```text
CMAKE_CXX_FLAGS:STRING=-U__AVX2__ -U__FMA__ -U__SSE2__
DAL_ENABLE_NATIVE_ARCH:BOOL=OFF
```

Tester-v3 full public exponent scan, rebuilt separately against both final
libraries:

```text
binary64_power_exponents_tested=2098 range=[-1074,1023]
analytical_comparison_scenarios=20978 comparison_failures=0
atomic_scenarios=2 atomic_failures=0
failures=0
```

Tester-v4 fixed-seed non-power corpus, rebuilt separately against both final
libraries and checked by the unchanged independent Python oracle:

```text
seed=0x6d5a56da3c9ef187
non_power_mantissa_anchors=18388
deterministic_boundary_anchors=16
random_candidates=10000
cases=165492
false_successes=0
false_rejections=0
conservative_fast_candidate_oracle_failures=0
failures=0
```

The native and scalar v4 raw TSVs are byte-identical:

```text
b7588a7b6a2f5c9a7fe46d9d70d0a469be58c11a912ce714802a37166f5fab62
```

The unchanged tester-v6 external public probe and `Fraction.from_float`
oracle now report identically on native and scalar:

```text
reviewer_permutations=6
finite_nonzero_permutations=24
finite_nonzero_fast_cases=18
finite_nonzero_fast_failures=0
failures=0
```

The two v6 raw TSVs are byte-identical:

```text
01dfe8f0dfb6b192f7da247887faffd04b8c7b57cdf9a8b7231877a376ef5e8a
```

Fresh native Release build/install/CTest:

```text
DAL_BUILD_DIR=build/Release-tester-v6-green-full \
DAL_INSTALL_DIR=build/stage/Release-tester-v6-green-full \
NUM_CORES=8 \
ADDITIONAL_CMAKE_FLAGS='-DDAL_ENABLE_NATIVE_ARCH=ON' \
bash ./build_linux.sh

100% tests passed, 0 tests failed out of 1172
Total Test time (real) = 21.04 sec
```

Additional gates:

```text
documentation: 39 Markdown files passed
benchmark-script unit tests: 19/19 passed
git diff --check: passed
public/header/binding/AAD diff: empty
```

### Fresh exact 10x2 paired 4% performance gate

All tracked samples were replaced after a fresh run from detached exact
sources and separate out-of-tree build roots:

- baseline source:
  `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`;
- GREEN source:
  `0d03402a404d21e2c3fd758480f59bc96b023f1c`;
- both sources clean before and after the run;
- GNU `gcc-14`/`g++-14` 14.3.0;
- Release shared-library AADET native builds, benchmarks enabled, tests,
  examples, public library, Python, and Excel disabled;
- baseline `krylov_perf` SHA-256:
  `81d46f1b204a00cd0d44c9a9a5a8a520afc5a07a5d0a2e7dfa344fbfac657e74`;
- GREEN `krylov_perf` SHA-256:
  `bbf2cd9dde3985d5229df5dc8f75b8a43177c04df7898b93a9414862c03ede1c`.

Gate:

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

Result: exit zero.

| Case | Base min | GREEN min | Combined | Round 1 | Round 2 | Gate |
|---|---:|---:|---:|---:|---:|:---:|
| `BCGSolve (500x500 tridiag)` | 18,967 ns | 15,800 ns | -16.70% | -17.04% | -16.39% | pass |
| `CGSolve (500x500 tridiag)` | 14,924 ns | 12,419 ns | -16.79% | -16.76% | -16.87% | pass |

Evidence audit: 40 raw outputs, 160 raw lines, 20 baseline and 20 GREEN
samples per case, no `calls/solve` marker, empty `failures`, and every
comparison both `passed` and `gated`.

### Remaining risk and required next gate

- The fast-path proof deliberately detects cancellation ambiguity rather than
  attempting to recover ambiguous low bits; exact accumulation is the
  authority whenever its conservative interval reaches the signed result.
  Tester should independently review this filter and rerun its unchanged v6
  permutation oracle.
- The exact fallback is intentionally private and fixed-range for finite
  binary64 products. Native/scalar wide-exponent and non-power scans passed,
  but the tester remains the next gate before reviewer re-entry.
- Reviewer and doc-writer remain paused. No PR or merge is authorized.

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
