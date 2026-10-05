# F02 prepared weighted C++ valuation

Verdict: Ready for draft publication. Binding, finite-difference, performance and
exact-head CI acceptance remain open; this does not complete F02.

`ValueByMonteCarloWithWeightedRisk` copies caller inputs before any history
callback, validates output/input choices and the full weighted numeric budget,
prepares once and rechecks complete axes. The passive result owns component
means, ordered outputs/weights, the objective mean/gradient, report factors,
complete inputs and actual execution provenance. Scalar projection is reused
privately without exposing its scalar-only legacy/output-ID projection.

The double and native drivers share compile-time objective policies. Existing
scalar entry signatures and arithmetic/aggregation order are preserved; their
policy has no weighted buffers. Weighted collection allocates selected output
storage once per batch. One root accumulates aliases through ordinary edges;
the existing recording/checkpoint and task-drain lifetimes stay authoritative.
Numeric component/objective sums divide once by paths; native gradients remain
their existing means. Profiling includes the additional component arrays.

## Evidence

Evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- `aad-weighted-execution-red-01.log`: the independent analytic public fixture
  fails compilation because the weighted public entry is missing.
- The first runtime fixture lacks a required PAYS instruction. The initial edge
  run also exposes an invalid ENDIF spelling and rounding in a recovery fixture's
  generated spot. Correct valid script syntax and use a literal recovery
  component; the analytic values, derivative tolerances and exact assertions
  stay unchanged. These failures remain in `green-02.log` and `edge-05.log`.
- `aad-weighted-execution-green-03.log`: the independent `9.5 / (5,3)` objective
  passes in tree and compiled modes.
- `aad-weighted-execution-green-06.log`: sixteen relevant OFF cases pass: eight
  new weighted cases and eight existing structured-risk cases. Coverage includes
  historical aliases/direct inputs/constants, consecutive signed/zero weights,
  one/four workers, output/input order, reported columns, native-empty versus
  price-only, exact budget, rejection before history/workers, zero-weight
  nonfinite components, callback mutation and partial submission recovery.
- `aad-weighted-execution-combined-08.log`: seventeen cases pass with lifetime
  diagnostics/profiling, ASan/UBSan, leak detection and halt-on-UB. Current
  tests/public valuation/weighted and scalar projection/native tape/recording/
  profiling sources are instrumented. Cached unchanged support and Google Test
  are reused; this is focused instrumentation, not a full instrumented-library
  claim. The first support archive lacks scalar-projection symbols; the retained
  `build-07.log` link failure is repaired by compiling that current source.
  After sharing the component-aggregation loop, only the affected valuation
  translation unit is rebuilt; `combined-final-09.log` passes the same seventeen
  cases. No full library or unrelated suite is rebuilt for that refactor.
- The added profiling fixture independently verifies 257 forward/payoff/suffix
  calls and one prefix reverse per batch in both evaluator modes, plus retained
  component workspace/result-array accounting.
- Common-path independent scalar component valuations agree with weighted
  means, spot/volatility gradients and a spot homogeneity derivative under the
  unchanged absolute `1e-10` checks. The fixed finite-difference fixture remains
  a separate open requirement.
- GCC 14's unchanged CI warning policy passes in `warning-01.log`; CCN-eight
  checks for planner/projection/public valuation pass in `ccn-01.log`. Formatting
  and `git diff --check` pass. Documentation checks pass for 140 Markdown files
  before this review is added; final publication rechecks the complete count.
- All four ASan/UBSan public filters include the weighted suite; both TSan
  configurations include parallel prefix and partial-submission recovery.
  `aad-weighted-execution-ci-filters-01.json` reconciles the actual executable
  test names and verifies that every prior public-filter pattern remains.

## Acceptance limits

The shared simulation source is changed, so earlier binary identity/performance
evidence is not reused as a new verdict. The affected scalar paired comparison
remains mandatory under the original two-round, ten alternating samples per
side, minimum-reduction and four-percent policy. New 1/4/16/64-output costs are
separate from that old-entry gate. No unrelated full local suites are repeated.

Python/Excel boundaries, generated registration, fixed finite differences,
final own-head CI/Codacy/review and cost acceptance remain open. F02 also retains
blocked Jacobians, recording-budget work and portfolio integration.
