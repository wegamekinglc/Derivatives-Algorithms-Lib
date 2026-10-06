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
  Internal weighted replay now accumulates cross-group model leaves, projects
  selected inputs and owns task cleanup. Public weighted requests/results,
  passive execution and portfolio Jacobians remain pending.
- Whole-request date/history freezing and an admission callback are implemented.
  Actual aggregate startup budget policy and runtime capacity guards remain
  unimplemented for portfolios. Internal worker/submission failure recovery is
  locally verified through the existing task-group ownership.
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
- Seven internal replay cases pass with the actual public `gtest_main`; a final
  focused bundle includes all seven plus six existing portfolio axis cases.
  Native tree/compiled execution matches every independent existing scalar-call
  risk for all six models with one/four workers. BS mixed-mesh/shared-owner and
  distinct-owner cases also cover both RNGs and requested coordinate order.
  Required derivative overflow names its global input/group; empty selected
  inputs preserve finite native pricing. Unselected groups generate no scenarios.
  Malformed output/input selections submit no tasks, and submission/worker
  failures preserve prior results and allow a bitwise-equal follow-up request.
- The replay unit passes strict OFF/combined ON syntax checks. Final linked
  diagnostic acceptance is pending. A new analytic spot assertion initially
  required bitwise equality to mathematical one; the actual BS derivative differs
  by one ULP because of its existing exp/log arithmetic. It now uses the same
  `ASSERT_DOUBLE_EQ` rule as the batch oracle, retaining all analytic coordinates;
  independent nonzero-volatility comparisons keep their unchanged `1e-10` bound.
- CI dependency recovery retains the Eigen Gitlink and official primary URL,
  fetching the same commit from a GitHub mirror only after primary failure.
  Seven helper cases, 20 workflow-classification and 33 release regressions pass.
  A real forced-primary-failure checkout fetches the unchanged commit from the
  mirror, verifies its exact tree and registered submodule status. Windows,
  Linux, wheel, release and benchmark workflows use this shared helper; no gate,
  compiler, sanitizer, dependency version or workload was removed.
- Publication head `070bf9d1` clears pinned-dependency initialization on all
  wheel platforms and every Linux/Windows build. Strict warnings and three
  extended diagnostic/profiling configurations pass. Codacy identifies only
  complexity 10/limit 8 in the mixed-mesh test. Extracted its independent scalar
  reference assertions into a file-scope helper; the same eight execution settings,
  every coordinate assertion, work count and `1e-10` bound remain. This repair
  needs its own exact publication-head checks; earlier passing jobs are not merge
  acceptance for it.
- The internal weighted request plan validates namespaced IDs, weights, factors
  and numeric payload before history/tasks, and owns its portfolio/selections.
  Existing scalar `ValidateCompleteAxis` deliberately rejects namespaced IDs;
  the first valid-plan RED proves this boundary. A new internal request-validation
  delegate reuses the existing factor/shape/budget constraints; no old scalar
  function or ordinal validation is changed. Five plan cases and ten existing
  scalar result cases pass. Strict warnings pass in OFF/combined ON syntax modes.
  Public valuation integration and actual recording/scratch capacity policy
  remain pending.

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
Replay evidence is `aad-portfolio-focused-replay-regression-green-01.json`,
`aad-portfolio-focused-replay-families-green-01.json` and
`aad-portfolio-replay-warning-clean-01.json`. The missing-interface RED and
analytic fixture failure remain in the corresponding `replay-ownership` files.
CI fallback evidence is `aad-ci-checkout-dependencies-{red,green}-01.log`,
`aad-ci-checkout-workflow-regression-02.log`,
`aad-ci-checkout-release-regression-01.log` and
`aad-ci-eigen-mirror-integration-01.json`.
The retained Codacy finding is `aad-487-replay-070b-current-03/summary.json`;
focused repair evidence is `aad-portfolio-focused-replay-codacy-green-01.json`.
Weighted preflight evidence is `aad-portfolio-focused-weighted-plan-green-02.json`,
`aad-portfolio-focused-weighted-plan-regression-green-01.json` and
`aad-portfolio-weighted-plan-warning-clean-01.json`. The interface and legacy-ID
schema failures remain in `weighted-plan-red-01` and `weighted-plan-green-01`.

## Summary

Existing scalar/weighted/Jacobian drivers retain their default preparation
entry points and allocate no portfolio state. The
snapshot registry precedes cloning, the passive catalog preserves owner/private
constant identity, and full semantic grouping retains incompatible trades.
The active specification remains the complete acceptance boundary.
