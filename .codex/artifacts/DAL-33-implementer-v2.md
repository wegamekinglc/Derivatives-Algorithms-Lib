# DAL-33 implementer v2 handoff

## Scope and provenance

- Approved baseline: `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`
- Target ref: `refs/heads/fix/dal-33-bcg-scale-stability`
- Controlling documents: `DAL-33-spec-v2.md`, `DAL-33-api-v2.md`, and
  `DAL-33-critic-v2.md`
- Tester-blocked head repaired:
  `48aaf5b9af388cd0842a2918567c6570957cb61f`
- Tested repair GREEN:
  `869c08733e4621061351af14d3b746c9c02c192b`
- Fresh performance-evidence commit:
  `526e0af8422a52bb6cf6ff1f396a4234a9b2ead3`
- The delivery head is the tip containing this handoff; resolve it from the
  target ref. No PR was created and nothing was merged.

The production change remains private to `dal-cpp/dal/math/matrix/bcg.cpp`.
Tests changed only in `dal-cpp/tests/math/matrix/test_bcg.cpp`. No public
header, binding, generated API, release documentation, or benchmark source was
changed.

## Commit layering

### Original RED

- `9e6ad215c3150d406691530f6222eee271fc9a05`
  (`Add DAL-33 failing Krylov contract tests`)
- Added the approved v2 stability, convergence, callback, state-preservation,
  and signature coverage.
- Focused command:

  ```text
  ./build/Release-linux/dal-cpp/dal_cpp_tests \
    --gtest_filter='MatrixTest.*CGSolve*' --gtest_brief=1
  ```

- Result before production changes: exit 1; 26 tests ran, 14 passed, 12
  failed.

### Original GREEN

- `f3ea5695353d30ad5ae7675a133c70ab045a6508`
  (`Stabilize DAL-33 Krylov solver state`)
- Added the private stable-scaled arithmetic, candidate-state commit,
  confirmation, callback validation, and finite-check mechanisms.
- Focused result: exit 0; 26/26 passed.

### Tester repair RED

- `e1044baf2bd3ec69af65ceec19b4cfbc46fbf73b`
  (`Add DAL-33 callback-order regression assertions`)
- Added literal exact callback-count tables for every CG/BCG fault site,
  exact BCG nonzero-tolerance initial-return counts, and an OR-03 wide-exponent
  fixture with `{1.0, denorm_min}`.
- Focused result: exit 1; 26 tests ran, 24 passed, with exactly the two
  tester-reported defects reproduced:
  - a non-finite BCG `MultiplyLeft` update incorrectly reached
    `MultiplyRight`;
  - the prior oracle discarded the nonzero low exponent bin and reported
    convergence.

### Tester repair GREEN

- `869c08733e4621061351af14d3b746c9c02c192b`
  (`Validate DAL-33 callbacks before dependent work`)
- Every external callback result now immediately passes shape validation and
  then finite validation before any dependent reduction or later callback.
  The failed callback is counted, while all later callbacks remain at zero.
- Removed the deferred update-dot validation path; denominator reduction now
  consumes only an already validated callback result.
- Replaced the test oracle with an independent exact binary-bin oracle. It
  decomposes finite doubles into significand bits and exponents, performs
  exact integer-bin square/sum/shift/compare operations, and therefore retains
  nonzero bins across the full binary64 exponent range. It does not call the
  production scaled helpers, use an original-scale dot/norm, or reduce through
  a floating-point maximum-exponent accumulator.
- Focused result: exit 0; 26/26 passed.

### Evidence replacement

- `526e0af8422a52bb6cf6ff1f396a4234a9b2ead3`
  (`Replace contaminated DAL-33 performance evidence`)
- Replaced all 40 raw outputs, `results.json`, and `summary.md` under
  `.codex/artifacts/DAL-33-performance-v2/paired/` with the fresh exact-source
  run described below. The old instrumented evidence is not retained on the
  target ref.

## Behavioral audit

- Shape is checked before finiteness for every callback result.
- A shape or finite-value failure prevents every subsequent callback.
- Callback exceptions still propagate unchanged.
- The literal fault tables cover initial residual, left/right operator,
  left/right preconditioner, and direct-confirmation failures for both
  solvers, including unused callback sites as exact zeros.
- BCG's nonzero-tolerance initial return is exactly:
  `MultiplyLeft=1`, `MultiplyRight=0`, `PreconditionLeft=0`,
  `PreconditionRight=0`, with `x` unchanged.
- Candidate vectors remain separate from committed state; `x` changes only
  after candidate validation and required direct-residual confirmation.
- Common-exponent stable arithmetic, grouped FMA checks, and the symmetric BCG
  callback-count-preserving fast path remain as approved.

## Verification

- Focused native suite:

  ```text
  ./build/Release-linux/dal-cpp/dal_cpp_tests \
    --gtest_filter='MatrixTest.*CGSolve*' --gtest_brief=1
  ```

  Result: 26/26 passed.

- Full native Release/AADET/AVX2 build and suite:

  ```text
  bash ./build_linux.sh
  ```

  Result: exit 0; 1165/1165 passed in 23.80 s.

- Explicit full suite:

  ```text
  ctest --test-dir build/Release-linux --output-on-failure --quiet
  ```

  Result: exit 0.

- Scalar fallback:

  ```text
  cmake --preset=Release-linux -S . -B build/Release-scalar-fallback \
    -DDAL_ENABLE_NATIVE_ARCH=OFF \
    -DCMAKE_CXX_FLAGS='-U__AVX2__ -U__FMA__ -U__SSE2__' \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  cmake --build build/Release-scalar-fallback --target dal_cpp_tests -j32
  ./build/Release-scalar-fallback/dal-cpp/dal_cpp_tests \
    --gtest_filter='MatrixTest.*CGSolve*' --gtest_brief=1
  ```

  Result: the compile database contains all three explicit `-U` flags and the
  focused suite passed 26/26.

- Large finite RHS repeated three times:

  ```text
  ./build/Release-linux/dal-cpp/dal_cpp_tests \
    --gtest_filter=MatrixTest.TestCGSolveAndBCGSolveLargeFiniteRhs \
    --gtest_repeat=3 --gtest_brief=1
  ```

  Result: 3/3 passed.

- Documentation:

  ```text
  python3 .github/scripts/check_docs.py
  ```

  Result: documentation checks passed for 39 Markdown files.

- Benchmark-script unit tests:

  ```text
  python3 -m unittest discover -s .github/scripts/tests \
    -p 'test_check_benchmark_regressions.py' -q
  ```

  Result: 19/19 passed.

- Patch and public-surface gates:

  ```text
  git diff --check \
    98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8 HEAD
  git diff --exit-code --name-only \
    98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8 HEAD -- \
    dal-cpp/dal/math/matrix/bcg.hpp dal-public dal-python dal-excel
  ```

  Result: both exit 0; the public-surface diff is empty.

## Fresh native paired performance gate

Configuration:

- machine: 13th Gen Intel Core i9-13900HX
- compiler: GNU `gcc-14`/`g++-14`; Unix Makefiles; Release static build
- AADET native backend with `DAL_ENABLE_NATIVE_ARCH=ON`
- public, tests, examples, Python, and alternate AAD backends disabled;
  benchmarks enabled
- `DAL_NUM_THREADS=4`
- separate detached, clean sources and out-of-tree builds:
  - baseline `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`
  - repair GREEN `869c08733e4621061351af14d3b746c9c02c192b`
- both sources used the same locked recursive submodules
- the baseline and head `krylov_perf.cpp` blob is identical
  (`4ba845ea1615728b04c1d2bf14b03ceb66e3ec84`) and contains no
  `calls/solve` instrumentation

Command:

```text
env DAL_NUM_THREADS=4 \
  python3 .github/scripts/check_benchmark_regressions.py \
    --base-root <fresh-root>/baseline-build \
    --head-root <fresh-root>/head-build \
    --output-dir .codex/artifacts/DAL-33-performance-v2/paired \
    --benchmarks krylov_perf \
    --samples 10 \
    --confirmation-rounds 2 \
    --threshold-percent 4
```

Result: exit 0.

| Case | Base min | Head min | Combined | Round 1 | Round 2 | Gate |
|---|---:|---:|---:|---:|---:|---|
| `BCGSolve (500x500 tridiag)` | 19,813 ns | 19,060 ns | -3.80% | -4.02% | -3.78% | pass |
| `CGSolve (500x500 tridiag)` | 15,291 ns | 15,617 ns | +2.13% | +6.29% | +2.13% | pass |

The approved gate fails only when every independent confirmation round exceeds
+4%; neither case did. The machine-readable `failures` array is empty.

Evidence audit:

- 40 raw process outputs: 20 base and 20 head;
- each raw file has exactly four lines and both benchmark cases;
- no raw file contains `calls/solve`;
- source worktrees remained clean and at their exact SHAs after the run;
- distributions:
  - base CG: n=20, min=15,291 ns, median=16,015 ns, max=18,297 ns;
  - head CG: n=20, min=15,617 ns, median=16,128.5 ns, max=19,540 ns;
  - base BCG: n=20, min=19,813 ns, median=20,650 ns, max=25,347 ns;
  - head BCG: n=20, min=19,060 ns, median=19,871 ns, max=21,134 ns.

## Remaining risk

- CG's first confirmation round measured +6.29% while its second measured
  +2.13%; this passes the approved two-round rule but records noisy-host timing
  variance for tester/CI comparison.
- The explicit macro-undefined scalar fallback ran on this x86 host. CI remains
  authoritative for native non-x86 ABIs and toolchains.
- The symmetric BCG fast path relies on the existing
  `Sparse::Square_::IsSymmetric()` contract. Non-symmetric and preconditioned
  BCG continue through the independent shadow-direction path.
- No design deviation from the approved v2 spec/API/critique was taken.
