# Compatible portfolio foundation review

Verdict: **Comment Only**. The foundation can be published in draft #487;
the complete portfolio PR is not ready for acceptance or merge.

## Findings

No unresolved correctness or style finding in the implemented foundation after
the focused fixes. The no-payoff axis query originally accepted `x = 1`; its
regression test now requires rejection with the original trade ID. Generated
archive files were regenerated from Machinist markup, never manually edited.
Codacy reported `ReadTrades` complexity 9 against limit 8 on the first code
publication. Extracted archived-model validation preserves every null/distinct
owner guard; exact-head Codacy acceptance must be recaptured after this repair.
All eleven snapshot/coordinate cases and both warning modes pass again with the
refactored archive code; no validation or assertion was removed.

The remaining execution requirements are material acceptance gaps:

- `dal-cpp/dal/script/portfoliogroups.hpp` requires one sealed owner registry and
  absolute path range. Its views are internal input contracts; the producer that
  establishes those contracts and model-family sharing proof does not exist yet.
- Portfolio weighted/Jacobian execution, shared model leaves, cross-group risk
  accumulation and real scenario/evaluator counters remain unimplemented.
- Whole-request date/history freezing, aggregate startup admission, runtime
  capacity guards and worker recovery remain unimplemented for portfolios.
- Python/Excel construction and valuation, installed consumers and actual
  Windows generated portfolio exports remain pending.

These gaps must be closed before changing the PR from draft or claiming F02
portfolio completion. Passing metadata/group tests cannot substitute for the
independent finite-sample and derivative oracles in the active specification.

## Open Questions

No user decision is needed. Split preparation admission from historical
resolution, retaining explicit/global provenance, before wiring group execution.
Keep original meshes and RNG dimensions; retain separate groups when sharing
proof is unavailable. Retain private evaluator/history state for every trade.

## Tests

- Public CI runs each discovered test with `gtest_main`, which does not register
  DAL index parsers. Its no-history axis case exposed missing test setup that
  the earlier combined core runner masked. Reproduced with the actual public
  entry and added explicit registration before installing the read/task spies;
  all original assertions and production paths remain unchanged. Recheck the
  publication head across the failed Linux/Windows jobs after this repair.
- Twenty-one new tests and two existing single-product risk contract tests pass
  in the static Release build. Historical scalar/vector cases reuse private
  states across `100,120,100` paths in both tree and compiled evaluation.
- All six model families preserve sealed values and passive coordinate identity.
- Three new implementation files pass the CI warning categories as errors in
  OFF and combined lifetime/profiling ON syntax checks. This is not a claim of
  linked/runtime ON-mode acceptance.
- The documented C++ example compiles, links and runs against both static
  libraries. Formatting and documentation checks pass.
- Broader local suites and the unchanged 160-case paired performance gate are
  reserved for the stable delivery head. Exact publication-head CI/Codacy and
  paginated review acceptance still need to be captured.

Evidence lives under the session evidence root: `aad-portfolio-focused-foundation-green-02.json`,
`aad-portfolio-warning-clean-01.json` and `aad-portfolio-doc-consumer-01.json`.
The focused Codacy repair adds `aad-portfolio-focused-codacy-green-01.json` and
`aad-portfolio-warning-clean-codacy-01.json`.

## Summary

Additive source preserves the existing scalar/weighted/Jacobian drivers. The
snapshot registry precedes cloning, the passive catalog preserves owner/private
constant identity, and full semantic grouping retains incompatible trades.
The active specification remains the complete acceptance boundary.
