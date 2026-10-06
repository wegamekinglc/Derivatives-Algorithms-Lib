# Compatible portfolio ownership, preparation and weighted execution review

Verdict: **Comment Only**. Implemented increments can be published in open #487;
the complete portfolio PR is not ready for acceptance or merge. Native/passive
weighted execution, owning C++ results and startup/runtime budgets are locally
verified. Native/passive attribution and budgets are locally verified; complete
width-narrowing/failure acceptance and bindings remain open.

## Findings

Publication `2338fc7d` reports one Codacy issue: weighted replay complexity
9 against limit 8. Extracted preparation-mode validation without changing its
guards, order, tape/passive choice or normalized reductions. Twenty affected
public result/replay/admission cases and strict OFF/combined ON warnings pass.
The failed-head audit is retained; fresh repair-head checks are required.

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
  actual shared-path native weighted risks pass independent scalar-call oracles.
- Shared weighted batches now register model leaves once, retain private trade
  leaves/history/evaluators and measure actual scenario/evaluator/reverse calls.
  Internal weighted replay now accumulates cross-group model leaves, projects
  selected inputs and owns task cleanup. Public owning C++ weighted requests/results
  and passive shared execution are implemented. Portfolio Jacobians remain pending.
- Whole-request date/history freezing and an admission callback are implemented.
  Native weighted startup admission reserves known coordinator/result/task and
  every selected private evaluator/vector capacity before history. Aggregate
  runtime recording/scratch guards admit actual growth and cleanup replacement.
  Passive startup capacity is now admitted before history; passive execution
  ignores recording limits and retains a zero-column gradient. Blocked capacity
  policy remains pending. Accepted tasks drain on
  submission, worker and capacity failure; prior results and follow-up requests
  remain usable.
- Python/Excel construction and valuation, installed consumers and actual
  Windows generated portfolio exports remain pending.

The user moved #487 out of draft on 2026-10-06; preserve that state. These gaps
must be closed before merge or claiming F02 portfolio completion.
Passing metadata/group tests cannot substitute for the
independent finite-sample and derivative oracles in the active specification.

## Open Questions

No user decision is needed. Complete blocked attribution and binding integration
while retaining native/passive weighted admission and explicit/global provenance.
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
  Public valuation integration remains pending. Native weighted capacity policy
  now connects this owning request plan to preparation and replay.

- Seven new cases cover prospective/completed group agreement, finite aggregate
  limits/peaks, zero-budget rejection, before-history private vector admission,
  capacity failure/recovery and sparse native vector derivatives. A finite quota
  fits one selected private vector and rejects both before historical reads;
  unselected evaluator state is omitted. Final targeted runs pass 17 public
  request/admission/replay cases, 21 core preparation/grouping/vector cases and
  nine batch cases. Eight existing vector parity cases also pass.
- Initial runtime slot replacement freed old buffers outside the worker budget
  attachment, causing stale ownership on address reuse. Assignment now remains
  attached, including inline task execution. All independent risk and recovery
  assertions pass. Finite tape tests explicitly allow retained startup/replacement
  headroom rather than assuming a prior scheduling-dependent peak is a future quota.
- Startup integration exposed sparse vector zero holes without tape nodes. An
  isolated RED rejects adjoint access before unsafe reverse; a retained ASan stack
  identifies the original crash. Native sparse writes bind only new zero holes;
  arithmetic scalar writes retain their original compiled branch. Scalar/block
  reverse and historical portfolio tests pass. Three admission tests also pass
  with ASan-instrumented new batch/admission units and tests; the remaining library
  is Release, so this does not replace complete CI sanitizer acceptance.
- Five changed production units pass strict OFF/combined ON warnings as errors.
  Latest `e90fd8f` audit has all 35 checks complete, zero Codacy annotations and
  zero unresolved threads. This accepts the capacity/sparse-vector publication;
  the public weighted/passive increment requires its own complete audit.
- Seven new public-result cases cover owning values/axes/report matrices, frozen
  settings/history, native-empty versus passive-sharp prices, numeric/startup
  rejection, zero-weight nonfinite output context and follow-up recovery.
  One additional passive admission case checks private historical vector shapes
  before reads, zero tape limits and independent scalar-call prices. Existing
  six-family and mixed-owner/mesh oracles now include both native/passive modes,
  preserving every tree/compiled, RNG/bridge and one/four-worker setting.
- Read-first result review found inherited native engine labels in passive
  per-trade snapshots and missing trade/group context on report overflow.
  Two RED tests retain both defects; the additive result capture/projection fixes
  preserve old single-script provenance and execution bodies.
  A third RED exposes lost output/trade context on aggregate weighted overflow;
  the batch failure boundary now retains the original group members and requested
  output IDs in both modes. This adds work only to exception construction.
- Six production units pass strict OFF/combined ON syntax warnings. Three new
  headers compile independently. Unchanged exact-source warning evidence is
  reused; only the changed public result unit was rechecked after context fixes.
  Final weighted batch error-context/formatting changes have their own two-mode
  warning evidence. The 16-case public-result/batch repair regression passes.
  Across the increment, 58 distinct affected public/core cases pass, with no
  repeated full local suite. The actual combined documented C++ construction and
  weighted example compiles, links and runs, observing 4096 scenarios/8192 trade
  evaluations and a one-by-three gradient. All 157 Markdown files and changed
  source formatting pass.

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
Public result/passive evidence includes `aad-portfolio-focused-public-result-boundary-green-02.json`,
`aad-portfolio-focused-passive-public-oracles-green-02.json`,
`aad-portfolio-focused-public-provenance-context-red-01.json`,
`aad-portfolio-focused-public-weighted-error-context-red-01.json`,
`aad-portfolio-public-passive-warning-02.json` and
`aad-portfolio-focused-public-passive-core-green-01.json`.
The 48-case public/legacy increment regression is
`aad-portfolio-focused-public-passive-final-regression-green-03.json`.
Final repair evidence is `aad-portfolio-focused-public-weighted-context-final-green-01.json`,
`aad-portfolio-public-weighted-context-warning-02.json` and
`aad-portfolio-public-doc-consumer-01.json`.
Capacity publication acceptance is `aad-487-aad-487-capacity-e90-current-06/summary.json`.
The public increment's retained Codacy finding is
`aad-487-aad-487-public-2338-current-02/summary.json`. Repair evidence is
`aad-portfolio-focused-public-codacy-regression-green-01.json` and
`aad-portfolio-public-codacy-warning-01.json`.

Native blocked group batches reuse the existing private evaluator/recording
lifecycle and return independent matrix rows. Three new cases and nine existing
weighted cases pass, including every model column against independent scalar
batches across tree/compiled, both RNGs and bridge settings. Historical prefix
and direct aliases, tail zeros, invalid widths, finite capacity recovery and
outer recording-mode restoration are covered. The production unit passes strict
OFF/combined ON warnings. The first mixed core/public harness used a public main
without core parser registration; its two historical core cases failed before
valuation. The corrected core-main regression passes all 32 affected core/public
cases; neither a production behavior nor a numerical oracle was changed.
Evidence: `aad-portfolio-focused-blocked-batch-roots-red-01.json`,
`aad-portfolio-focused-blocked-batch-boundary-green-04.json` and
`aad-portfolio-blocked-batch-warning-01.json` and
`aad-portfolio-focused-blocked-batch-shared-driver-regression-green-02.json`.
Public attribution/admission and
final linked/platform/performance acceptance remain required.

Owning native C++ attribution now uses the common sealed selection plan with
the exact independent matrix payload and shared result metadata. Original group
blocks reuse task draining, deterministic batch reduction and owner/private
scatter; each result records requested/actual widths and actual work. Startup
capacity admission covers each block's selected private states, matrices and root
lanes before history, with equivalent-capacity narrowing and shared runtime guards.
The admission RED retained one historical read before zero-budget rejection; the
callback now rejects without reads/tasks. Local review also retained a RED for
report overflow naming the wrong related trade; context now comes from the actual
failing output row. No numerical assertion or tolerance was relaxed.
Forty-one affected planning/result/replay/admission/batch cases pass. After the
metadata refactor and row-context repair, all 24 affected public cases pass,
including the new private historical-vector oracle and independent risk columns
for every accepted model family, original mixed meshes/owners, tree/compiled,
one/four workers and widths one/two/three. This is 43 distinct affected cases.
Four changed production units pass strict OFF/combined ON warnings; the final
result unit and three independent headers have fresh two-mode evidence.
Evidence: `aad-portfolio-focused-jacobian-plan-red-01.json`,
`aad-portfolio-focused-jacobian-plan-green-01.json`,
`aad-portfolio-focused-jacobian-public-red-01.json`,
`aad-portfolio-focused-jacobian-public-capacity-red-01.json`,
`aad-portfolio-focused-jacobian-report-context-red-01.json`,
`aad-portfolio-focused-jacobian-native-capacity-regression-green-01.json`,
`aad-portfolio-focused-jacobian-native-families-context-green-01.json`,
`aad-portfolio-jacobian-native-warning-01.json` and
`aad-portfolio-jacobian-metadata-warning-01.json`.
The actual documented construction/weighted/native-attribution consumer compiles,
links and runs with a two-by-three matrix and 4096 scenarios/8192 evaluations for
the attribution group (`aad-portfolio-jacobian-doc-consumer-02.json`). All 157
Markdown checks and changed-source formatting pass. The first generated consumer
placed the attribution snippet outside main; its failure is retained separately
and the generator boundary is corrected without changing documented code.

Passive attribution reuses private double batches with zero internal objective
weights, retaining separate row sums and no native state/widths/reversals. A RED
first rejects the missing passive entry. Two new cases then verify finite rows
whose unused aggregate overflows, exact zero-column payload/shape, zero tape
limits and known private historical-vector rejection before reads followed by
valid recovery. Existing independent six-family/mixed-mesh/owner oracles now
cover attribution in both modes at several requested widths; the sharp/fuzzy test
also compares native-empty/passive Jacobian row prices. All 26 affected public
cases pass, with strict OFF/combined ON warnings for the changed replay unit.
Evidence: `aad-portfolio-focused-jacobian-passive-red-01.json`,
`aad-portfolio-focused-jacobian-passive-public-green-01.json`,
`aad-portfolio-focused-jacobian-passive-oracles-green-02.json` and
`aad-portfolio-jacobian-passive-warning-01.json`.

Exact head `4194be4b` has one real extended OFF/OFF CI failure in the finite
weighted capacity fixture. The following invocation needed 5505024 tape bytes
against its 5242880-byte limit. A prior scheduling-dependent peak plus cleanup
allowance is not a guaranteed future concurrency/replacement quota. The fixture
now declares finite per-worker scratch/tape allowances, retaining every exact
numeric comparison, nonzero peak assertion and final peak-within-limit check.
Runtime quota guards and independent zero-capacity/exhaustion rejection tests are
unchanged. The exact-head audit, annotations and full job log are retained in
`aad-487-aad-487-native-jacobian-4194-current-02`,
`aad-487-4194-extended-off-annotations-01.json` and
`aad-487-4194-extended-off-job-01.log`. All nine affected replay cases pass in
`aad-portfolio-focused-jacobian-passive-ci-budget-green-01.json`.
Fresh repair-head CI acceptance is required.

## Summary

Existing scalar/weighted/Jacobian drivers retain their default preparation
entry points and allocate no portfolio state. The
snapshot registry precedes cloning, the passive catalog preserves owner/private
constant identity, and full semantic grouping retains incompatible trades.
The active specification remains the complete acceptance boundary. Complete
narrowing/failure tests, bindings, installed consumers, final performance/platform
checks and a fresh publication-head review are required before merge.
