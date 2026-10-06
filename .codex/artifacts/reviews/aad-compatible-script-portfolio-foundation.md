# Compatible portfolio ownership, preparation and weighted batch review

Verdict: **Comment Only**. Implemented increments can be published in open #487;
the complete portfolio PR is not ready for acceptance or merge.

## Findings

No unresolved correctness or style finding in the implemented foundation after
the focused fixes. The no-payoff axis query originally accepted `x = 1`; its
regression test now requires rejection with the original trade ID. Generated
archive files were regenerated from Machinist markup, never manually edited.
Codacy reported `ReadTrades` complexity 9 against limit 8 on the first code
publication. Extracted archived-model validation preserves every null/distinct
owner guard. Repair head `0fffbf74` now passes all 35 exact-head checks, with zero
Codacy annotations and zero unresolved review threads.
All eleven snapshot/coordinate cases and both warning modes pass again with the
refactored archive code; no validation or assertion was removed.
The preparation publication at `8db78e83` reports `Prepare` complexity 10 against
limit 8. Extracted the live model/mesh planning and admission phase and removed
the unused generic export branch. Expired trades still skip model/history/
admission, legacy results retain return-value optimization, and every delayed
payment and LSM feature guard remains. Thirty-five affected core cases and six
public cases pass after this refactor; strict OFF/combined ON syntax checks pass.
Repair head `e9931d3a` passes Codacy with zero annotations and has no unresolved
review threads. Windows CI passes. After one failed-job retry, three Linux builds
and the Mac x86 wheel still fail during Eigen checkout before compilation;
aggregate gates consequently fail/skip. The retained exact-head audit distinguishes
these upstream download failures from source acceptance. Another failed-only retry
is running; no passing jobs or full local suite were repeated for this outage.

The remaining execution requirements are material acceptance gaps:

- `dal-cpp/dal/script/portfoliogroups.hpp` requires one sealed owner registry and
  absolute path range. Its owning producer now plans every trade using the
  original registry and meshes, freezes one date/history and constructs the
  passive groups. Six exact factory types admit full-contract grouping; their
  actual shared-path mathematical acceptance remains pending.
- Shared weighted batches now register model leaves once, retain private trade
  leaves/history/evaluators and measure actual scenario/evaluator/reverse calls.
  Whole-portfolio weighted/Jacobian execution, cross-group risk accumulation and
  selected-input validation remain pending.
- Whole-request date/history freezing and an admission callback are implemented.
  Actual aggregate startup budget policy, runtime capacity guards and worker
  recovery remain unimplemented for portfolios.
- Python/Excel construction and valuation, installed consumers and actual
  Windows generated portfolio exports remain pending.

The user moved #487 out of draft on 2026-10-06; preserve that state. These gaps
must be closed before merge or claiming F02 portfolio completion.
Passing metadata/group tests cannot substitute for the
independent finite-sample and derivative oracles in the active specification.

## Open Questions

No user decision is needed. Connect aggregate admission and group execution to
the new planning/completion boundary, retaining explicit/global provenance.
Keep original meshes and RNG dimensions; retain separate groups when sharing
proof is unavailable. Retain private evaluator/history state for every trade.

## Tests

- Public CI runs each discovered test with `gtest_main`, which does not register
  DAL index parsers. Its no-history axis case exposed missing test setup that
  the earlier combined core runner masked. Reproduced with the actual public
  entry and added explicit registration before installing the read/task spies;
  all original assertions and production paths remain unchanged. The corrected
  publication head passes every previously failing Linux/Windows job.
- Twenty-one new tests and two existing single-product risk contract tests pass
  in the static Release build. Historical scalar/vector cases reuse private
  states across `100,120,100` paths in both tree and compiled evaluation.
- All six model families preserve sealed values and passive coordinate identity.
- Three new implementation files pass the CI warning categories as errors in
  OFF and combined lifetime/profiling ON syntax checks. This is not a claim of
  linked/runtime ON-mode acceptance.
- The documented C++ example compiles, links and runs against both static
  libraries. Formatting and documentation checks pass.
- Fourteen new preparation cases pass. Whole planning precedes any historical
  reads or admission; one union snapshot retains original source metadata and
  isolated historical state. Tests mutate caller handles/settings and global
  fixing data during callbacks, inject late-trade/compilation failures, retain
  prior results and verify valid follow-up preparation.
- Refactoring the common completion epilogue preserves old return-value
  optimization, history-only behavior and LSM pruning/reinitialization. A
  targeted bundle passes 61 core cases, including the 14 new ones, 43 existing
  preparation/history cases and four affected LSM/exercise cases. Six public
  scalar/Jacobian preparation/admission cases pass using the public `gtest_main`.
- Both changed production units pass the CI warning categories as errors in
  OFF and combined lifetime/profiling ON syntax checks. Linked/runtime platform
  acceptance for this new preparation head remains pending.
- Broader local suites and the unchanged 160-case paired performance gate are
  reserved for the stable delivery head. Exact publication-head CI/Codacy and
  paginated review acceptance must be recaptured for each new publication head.
- Nine new weighted batch cases pass, covering both evaluators, Sobol/MRG32 and
  Brownian bridge OFF/ON with nonzero volatility and exact independent component
  sums on absolute paths. The ownership oracle verifies shared spot and private
  constants; aliases/private vectors and unselected poisonous trades retain
  independent state. Zero-weight nonfinite outputs, weighted overflow and private
  evaluation errors preserve context; valid follow-up native/legacy batches pass.
  Validation precedes recording entry and scalar execution restores an enclosing
  multi-adjoint mode. Two production error-context RED traces precede their fixes.
- The new batch unit passes strict OFF and combined lifetime/profiling ON syntax
  checks. Linked/runtime diagnostic and six-family execution acceptance remain
  pending; only BS execution is proved by this increment.

Evidence lives under the session evidence root: `aad-portfolio-focused-foundation-green-02.json`,
`aad-portfolio-warning-clean-01.json` and `aad-portfolio-doc-consumer-01.json`.
The focused Codacy repair adds `aad-portfolio-focused-codacy-green-01.json` and
`aad-portfolio-warning-clean-codacy-01.json`.
The corrected foundation audit is `aad-487-foundation-0fff-current-02/summary.json`.
Preparation evidence is `aad-portfolio-focused-preparation-regression-green-01.json`,
`aad-portfolio-focused-public-preparation-regression-green-01.json` and
`aad-portfolio-preparation-warning-clean-01.json`.
The preparation Codacy repair adds `aad-portfolio-focused-preparation-codacy-green-01.json`,
`aad-portfolio-focused-public-preparation-codacy-green-01.json` and
`aad-portfolio-preparation-warning-clean-codacy-01.json`; the original failure is
retained in `aad-487-preparation-8db-current-02/summary.json`.
Batch evidence is `aad-portfolio-focused-batch-behavior-green-04.json` and
`aad-portfolio-batch-warning-clean-01.json`. The expected error-context failures
are retained in `aad-portfolio-focused-batch-failure-context-red-01.json` and
`aad-portfolio-focused-batch-weighted-overflow-red-01.json`. The latest completed
preparation audit is `aad-487-batch-e993-current-01/summary.json`.

## Summary

Existing scalar/weighted/Jacobian drivers retain their default preparation
entry points and allocate no portfolio state. The
snapshot registry precedes cloning, the passive catalog preserves owner/private
constant identity, and full semantic grouping retains incompatible trades.
The active specification remains the complete acceptance boundary.
