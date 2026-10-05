# F01 common calibration request review

Verdict: Comment Only pending this increment's own publication-head CI.

## Findings

No remaining correctness, methodology or style findings in the local common
request increment. Read the full new public header/implementation and ten tests,
native common provider, request spec/API/critique and updated guide/changelog.
Review full F01 automatic Hybrid planning/direct bindings and Python/Excel
request parity separately before closing F01 or accepting PR #480 for merge.

Source-scoped quote IDs do not replace typed owner checks. Canonical row/column
and curve block/global ordinals remain inspectable; curves keep unavailable
quote values absent. A sealed passive plan copies request metadata and shares
immutable native source storage. It checks overflow and all three full raw
matrices' numeric payload before axis allocation. Subset and empty requests
cannot reduce this retained payload. The explicit budget excludes source/input
storage, metadata, getter copies and temporary tape/worker/VJP arrays.

The result retains the native immutable common risk and no cached selected
matrix. One projection helper implements all four detached row getters.
Publication checks all selected scaled contributions, so cancellation in total
cannot hide an overflowing reported calibration/direct term. Empty selection
still reaches native Dupire's independent-recording guard and retains full raw
contributions. Native mean/inverse scaling, fixed-input boundaries, parameter
identity and Dupire's accepted quote-only direct identity remain unchanged.
No PV value/currency is invented from external gradient seeds.

## Open questions

None requiring user input. Automatic Hybrid input planning/sealing, constant
direct bindings, combined numeric budget and all request bindings remain F01
work. P01 production performance and whole-PR fixes/final checks remain required
before the already authorized merge; subsequent phases require new PRs.

## Tests

Evidence root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.
All names below have prefix `aad-calibration-request-`.

- `red-01.log` confirms the missing public planner header before implementation;
  `green-01.log` passes its first actual test. Keep `projection-red-01.log`:
  missing result API and mistaken fixture calls both fail there. `build-02.log`
  retains those fixture calls after the result API is added. Correct fixtures
  against the actual RecordingScope/NativeOperations/node-storage APIs; do not
  describe this mixed failure as a pure result-interface RED.
- `green-02.log` contains a shell wildcard expansion failure and runs no test;
  quoted `green-03.log` passes all five initial actual cases. Keep `build-04.log`
  for the fixture's immutable matrix iterator misuse, corrected by cell mutation.
- `green-04.log` runs ten cases, eight pass/two fail. One incorrectly expects a
  nested-scope token instead of the native mode-selection guard. The other
  incorrectly rejects equal Dupire direct quotes under different fixed IVSs.
  Correct both expectations against previously accepted native contracts; retain
  complete parameter-owner rejection, changed-quote direct rejection, allowed
  equal-quote direct values, graph preservation/recovery and every numeric check.
  `green-05.log` passes ten actual cases. No production tolerance or step changes.
- Ten cases cover Dupire coordinates/values, all four curve providers in both
  ANALYTIC/BUMPED modes, ordered/subset/empty requests, exact/full payload budgets,
  overflow, NUL/duplicate/unknown IDs, malformed/nonfinite factors, detached owned
  getters, unrelated graph derivative recovery, direct identity and cancellation.
  Curve projection uses an independent scalar contraction of the retained inverse
  and its tolerance, with signed gradients/direct terms. Accepted common-provider
  independent recalibration/priced-portfolio oracles run unchanged in related tests.
- `off-related-01.log` and `combined-related-01.log` each pass 50 actual cases.
  Both complete public suites pass 229 in `{off,combined}-public-all-01.log`.
  Fresh regular CTest passes 2,438 OFF / 2,452 combined, excluding benchmark/slow
  labels, in `{off,combined}-full-01.log`. Existing Python CI/extra test acceptance
  is not replaced by a claim that these CTest counts contain every standalone case.
- Fully instrumented combined Debug ASan/UBSan with leak detection and halt-on-error
  passes all 50 related cases in `sanitized-related-01.log`.
- Actual MSVC production/test syntax passes 4/4 OFF/combined in
  `msvc-syntax-01.json`, without an extra NOMINMAX override. This supplements,
  rather than replaces, the pending own Windows runtime CI.
- Isolated installed public consumers pass 18 native cells plus six independent
  zero-calibration/direct/report oracle cells in both OFF/combined configurations.
  CCN-eight scan passes all 33 implementation/test functions. The three new C++
  files are clang-format idempotent; generated sources are untouched.
- The [performance report](../performance/aad-calibration-risk-request.md)
  records exact unchanged old executable/core/public-member identities and all
  six new-cost rows, two rounds/20 processes, 480 checked cells and 1,061 unchanged
  inputs. Keep the initial guessed nonexistent benchmark-target failure in
  `refactor-build-01.log`; `refactor-build-02.log` rebuilds the remaining actual
  inventory. No measurement overlaps builds, tests, edits or Git/PR activity.

## Summary

The local common request increment supplies quote-coordinate planning, honest
numeric-result budgets and report projections over the accepted native provider.
Own publication CI and remaining full F01 execution/binding acceptance are open.
