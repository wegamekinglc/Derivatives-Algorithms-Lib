# F03 numeric solve implementation review

Verdict: Approve.

## Findings

No remaining local correctness or style finding in the independent numeric scope.
This approves the local implementation; final platform/publication gates remain.

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
  local Lizard initially reported maximum complexity 9. Codacy's actual limit
  is 8, and its two annotations at `b2235eaa` are retained in
  `aad-linear-solve-codacy-b2235-01.json`. Split the operation message and native
  oracle assertions; current production and reference-test complexity is at most 8.
- Codacy repair GREEN: `aad-linear-solve-codacy-green-01.log` (9/9, exit 0).
- Distinct seed addition: `aad-linear-solve-linearity-green-01.log` reruns only
  the changed ownership/linearity case; it passes at unchanged 1e-10 tolerance.
- The new translation unit without Eigen macros/includes produces the same
  object; all nine focused cases linked to it pass in `aad-linear-solve-eigen-off-01`.
- The [cost report](../performance/aad-linear-solve-pullback.md) retains all
  sixteen two-round comparisons, 640 processes and 81,920 checked owning results.
  Fourteen fresh legacy/portfolio links preserve accepted executable hashes.
- Documentation integrity: 157 Markdown files; new C++ follows `.clang-format`.

Residual risk: required final-head platform/sanitizer CI remains pending.
Complete cost/storage evidence is accepted within its stated reference/workload
limits. The new test suite runs in ordinary core CI through the existing
source/test globs and is added to all six focused sanitizer filters. Actual final
logs must contain all nine cases in each configuration. No CI skip, tolerance
increase or legacy hot-path change is used.
