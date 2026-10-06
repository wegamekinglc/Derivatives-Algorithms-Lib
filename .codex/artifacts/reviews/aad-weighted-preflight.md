# F02 native output and budget preflight

Verdict: Ready for draft publication; weighted execution remains open.

The additive `ScriptRiskOutputAxis` lists indexed scalar slots with canonical
receiver ID `payoff` and other IDs `output:<ordinal>`. `WeightedRiskRequest_`
composes scalar selections and weights, avoiding a conversion that discards
weights. `WeightedRiskPlan_` owns read-only choices/axes/date/method and an exact
numeric payload. This is metadata preflight, not a sealed product/model execution
plan or a complete weighted valuation API.

Planning rejects empty/repeated/unknown selections, invalid weights/input/report
choices, missing scalar receivers, invalid dates, exercise and fully expired
calendars. Scalar input validation is reused with scalar-only output/budget
controls cleared; the larger weighted payload is then enforced before allocating
normalized weight storage. Prepared-axis checks preserve full slot/input identity,
values, units, report metadata and valuation date.

## Evidence

Evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- RED: `aad-weighted-plan-red-01.log` rejects the missing planner header.
  The initial standalone compile exposes an include-order dependency; the new
  header includes the platform contract before the existing script header.
- GREEN: `aad-weighted-plan-green-02.log` passes ordered selection, copied
  weights, canonical identity and the independent seven-double budget fixture.
- `aad-weighted-plan-edge-03.log` passes all ten plan tests, including exact and
  one-byte-short budgets, allocation-free maximum-extent arithmetic, signed/zero
  weights, native-empty versus passive mode, vector/scalar distinction and
  prepared slot/value/unit/date mismatches.
- `aad-weighted-plan-off-final-04.log` passes all twenty new-plan/existing-result
  cases with current planner and scalar projection sources compiled. Existing
  mean normalization, reporting, shape, errors and detached ownership stay valid.
- `aad-weighted-plan-combined-sanitized-05.log` passes the same twenty with
  lifetime diagnostics/profiling, ASan/UBSan, leak detection and halt-on-UB.
  Current test/planner/scalar-result/AAD sources are instrumented; unchanged
  cached support and Google Test are reused. This is focused instrumentation,
  not a complete instrumented library acceptance claim.
- An overly broad local warning probe hits existing unused-parameter debt.
  `aad-weighted-plan-warning-02.json/.log` then uses the unchanged CI policy
  and GCC 14 and succeeds. No CI warning category is relaxed.
- `aad-weighted-plan-sanitizer-filter-01.json` reconciles executable test names
  with all four ASan/UBSan matrix filters. All eighteen weighted/plan/legacy-root
  cases are selected; original filters remain present. TSan selections are
  unchanged because this metadata increment adds no concurrent execution.
- Formatting and CCN-eight checks pass; maximum new-function CCN is four.

The scalar request/result, simulation, AAD root/tape and public valuation sources
are unchanged. This adds no weighted metadata or traversal to their runtime path.
Prepared integration will still require independent common-path oracles,
failure/task-drain checks, actual three-language boundaries, affected scalar cost
acceptance and own-head CI/review. Full F02 also retains blocks and portfolios.
