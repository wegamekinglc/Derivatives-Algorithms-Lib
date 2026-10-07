# F03 numeric solve implementation review

Verdict: Comment Only.

## Findings

No remaining local correctness or style finding in the independent numeric scope.
This is not a publication approval: full performance and platform acceptance remain.

Read scope: the complete new header, source and nine-test file, the controlling
[specification](../specs/aad-linear-solve-pullback.md),
[critique](../critiques/aad-linear-solve-pullback.md), matrix methodology, changelog
and updated implementation ledger.

The first analytic runtime check exposed reversed triangular-substitution order;
the production correction is tested and the failed raw result is retained.
Forward solves now apply row permutations before L/U substitution. Transpose
solves use U-transpose/L-transpose, then undo permutations in reverse order.
Both reuse the normalized factors. Gradient outer products sum over RHS columns
and reject non-finite accumulation before returning owning results.

## Open questions

No user decision is required. An explicit pivot cutoff cannot establish a general
condition-number guarantee; current methodology names that limit. Dense results
need separate later mappings for symmetric/shared coordinates. Recording events,
native node overhead and checkpoint cache ownership are outside this PR.

## Tests

- Missing-interface RED: `aad-linear-solve-analytic-red-01.log` (build exit 2).
- Initial analytic failure: `aad-linear-solve-analytic-green-01.log` (test exit 1).
  The name records the attempted step, not a passing result.
- Analytic GREEN: `aad-linear-solve-analytic-green-02.log` (one case, exit 0).
- Expanded independent oracles: `aad-linear-solve-oracles-01.log` (9/9, exit 0).
- Refactored helpers: `aad-linear-solve-refactor-01.log` (9/9, exit 0).
- Final split helpers: `aad-linear-solve-final-local-01.log` (9/9, exit 0);
  local Lizard reports maximum complexity 9 in production and reference tests.
- Documentation integrity: 156 Markdown files; new C++ follows `.clang-format`.

Residual risk: required platform/sanitizer CI and complete cost/storage evidence
are pending. The new test suite will run in ordinary core CI through the existing
source/test globs; no CI skip, tolerance increase or legacy hot-path change is used.
