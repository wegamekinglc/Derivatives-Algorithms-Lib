# DAL-33 implementer v2 handoff

## Scope and admission

- Approved baseline: `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`
- Target ref: `refs/heads/fix/dal-33-bcg-scale-stability`
- Controlling documents: `DAL-33-spec-v2.md`, `DAL-33-api-v2.md`, and `DAL-33-critic-v2.md`
- Pre-RED admission:
  - local peeled target: `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`
  - remote peeled target: `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`
  - merge-base with the approved baseline: `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`
  - `git diff --exit-code 98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8...refs/heads/fix/dal-33-bcg-scale-stability`: exit 0, empty
- No public header, binding, generated API, or release-documentation change was made. No PR was created and nothing was merged.

## Commits and changed files

### RED

- Commit: `9e6ad215c3150d406691530f6222eee271fc9a05` (`Add DAL-33 failing Krylov contract tests`)
- File: `dal-cpp/tests/math/matrix/test_bcg.cpp`
- Added 17 contract tests for large finite scaling, both FMA cancellation sites, BCG shadow direction, direct confirmation, callback counts and terminal inputs, callback shape/finiteness/exception handling, state preservation, common-exponent boundary behavior, retained large-initial-guess behavior, generic validation messages, denominator breakdown, scale metamorphism, and exact public signatures.
- Command:

  ```text
  ./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='MatrixTest.*CGSolve*' --gtest_brief=1
  ```

- Result: exit 1; 26 tests ran, 14 passed, 12 failed. The failures reproduced the missing stability, validation, confirmation, and commit-protection contracts before production changes.

### GREEN

- Commit: `f3ea5695353d30ad5ae7675a133c70ab045a6508` (`Stabilize DAL-33 Krylov solver state`)
- File: `dal-cpp/dal/math/matrix/bcg.cpp`
- Focused command: same as RED.
- Result: exit 0; 26/26 passed.

The production implementation remains private to `bcg.cpp`:

- common-exponent scaled dot products, norms, products, sums, comparisons, and ratios avoid representational overflow in convergence and recurrence scalars;
- `std::fma` computes the six recurrence/direct-residual formulas:
  `p = z + beta*p`, `pp = zz + beta*pp`, `x' = x + alpha*p`,
  `r' = r - alpha*A*p`, `rr' = rr - alpha*A^T*pp`, and `b - A*x'`;
- candidate vectors are separate from committed state, and `x` is swapped only after all candidate validation and any required direct-residual confirmation succeed;
- callback size and finite-value validation retains solver/callback/category identity and the lowest invalid index; callback exceptions propagate unchanged;
- all solver-owned O(n) work vectors are allocated before the iteration loop and owned by one invocation, with no thread-local or global mutable solver state;
- callback finite validation is fused with already-required reductions where possible; grouped FMA checks use MXCSR only on x86/SSE and restore the caller's prior status flags;
- symmetric, unpreconditioned BCG reuses its mathematically identical left/right direction while still invoking and validating `MultiplyRight` once per update, preserving the specified callback counts.

Callback/count audit:

- both solvers call `MultiplyLeft` once for the initial direct residual and once for every attempted update;
- a recursively converged update receives exactly one additional `MultiplyLeft` for direct confirmation;
- BCG calls `MultiplyRight` exactly once per attempted update;
- a present left preconditioner is called once per attempted update, and BCG's right preconditioner is called once per attempted update;
- exhaustion adds no direct-confirmation callback, and non-finite inputs are rejected before any callback.

## Verification

- Focused contract suite:

  ```text
  ./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='MatrixTest.*CGSolve*' --gtest_brief=1
  ```

  Result: 26/26 passed.

- Full build and suite:

  ```text
  bash ./build_linux.sh
  ```

  Result: exit 0; Release/AADET/AVX2 build completed; 1165/1165 tests passed in 34.72 s.

- Explicit full suite:

  ```text
  ctest --test-dir build/Release-linux --output-on-failure --quiet
  ```

  Result: exit 0.

- Documentation:

  ```text
  python3 .github/scripts/check_docs.py
  ```

  Result: documentation checks passed for 39 Markdown files.

- Benchmark gate unit tests:

  ```text
  python3 -m unittest discover -s .github/scripts/tests -p 'test_check_benchmark_regressions.py' -v
  ```

  Result: 19 tests passed.

- Patch hygiene:

  ```text
  git diff --check
  ```

  Result: exit 0.

## Native paired performance gate

Configuration:

- machine: 13th Gen Intel Core i9-13900HX
- compiler: GNU `g++-14`
- generator/build: Unix Makefiles, Release, static library
- AAD backend: AADET
- public API, tests, examples, and Python disabled; benchmarks enabled
- `DAL_ENABLE_NATIVE_ARCH=ON`
- `DAL_NUM_THREADS=4`
- baseline source: exact `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`
- head source: exact GREEN `f3ea5695353d30ad5ae7675a133c70ab045a6508`
- both source worktrees were clean before the formal run

Command:

```text
env DAL_NUM_THREADS=4 python3 .github/scripts/check_benchmark_regressions.py \
  --base-root /tmp/dal33-perf.ExVvT4/baseline-build \
  --head-root /tmp/dal33-perf.ExVvT4/head-build \
  --output-dir .codex/artifacts/DAL-33-performance-v2/paired \
  --benchmarks krylov_perf \
  --samples 10 \
  --confirmation-rounds 2 \
  --threshold-percent 4
```

Result: exit 0; 40 raw process outputs retained (10 interleaved base/head samples in each of 2 rounds for one benchmark).

| Case | Round 1 | Round 2 | Gate |
|---|---:|---:|---|
| `BCGSolve (500x500 tridiag)` | -3.64% | -1.85% | pass |
| `CGSolve (500x500 tridiag)` | +1.52% | +6.20% | pass |

The gate fails only when every independent confirmation round exceeds +4%; neither case met that failure condition. Machine-readable results contain an empty `failures` array. Evidence is retained under `.codex/artifacts/DAL-33-performance-v2/paired/`.

## Remaining risk

- The generic non-x86 finite-check fallback compiled by conditional design but was not exercised on this x86/AVX2 host; CI remains authoritative for other toolchains.
- The symmetric BCG fast path relies on the existing `Sparse::Square_::IsSymmetric()` contract being truthful. Non-symmetric and preconditioned BCG continue through the independent shadow-direction path.
- The CG second performance round measured +6.20%, while the independent first round measured +1.52%; this passes the approved two-round rule but indicates normal host timing variance worth comparing with CI evidence.
- No design deviation from the approved v2 spec/API/critique was taken.
