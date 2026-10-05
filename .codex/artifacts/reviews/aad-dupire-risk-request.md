# Automatic Dupire request review

Verdict: Comment Only pending the C++ increment's own publication-head CI.

## Findings

The complete new request header/implementation, two passive sharing headers,
additive scalar-axis/surface-layout/native quote-identity helpers and both test
files have been read against the spec/API/critique. No current correctness or
methodology finding remains. Full F01 bindings and PR/P01 acceptance are open.

The planner owns its sealed model/product/settings and rejects custom archive
types, including nested surfaces, before serialization. Native component sorting,
complete input layout and retained model/surface verification are shared with
accepted paths. Required surface/direct columns are independent of quote
selection. Explicit bindings verify both-side uniqueness and exact native values;
external direct inputs preserve quote-only identity and exclude bindings.

Result payload counts one scalar value, every required valuation derivative
and three full quote matrices. Getter copies, metadata/source and input/temporary
arrays remain explicitly outside that budget. The result owns valuation/common
risk and does not retain the execution plan's extra external seed storage.
Mean normalization, fixed calibration provenance and expired/mixed method
labels are preserved. Getter projections remain detached and passive.

## Open questions

None requiring user input. C++ local gates pass; strict Python/Excel
request/plan/result parity follows. Whole-PR fixes, production P01 acceptance,
master reconciliation and final-head review/checks precede the authorized merge.

## Tests

Evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.
Names below have prefix `aad-dupire-request-`.

- `red-01.log`, `binding-red-01.log`, `external-red-01.log`,
  `execution-red-01.log` and `arithmetic-red-01.log` record actual missing APIs
  or unimplemented behavior before the corresponding implementation.
- `green-01.log` is a wrong executable path and runs no test; `green-02.log`
  runs the actual initial planner test. Preserve both.
- `green-05.log` exposes roughly 3e-17 multi-worker reduction roundoff against
  the exact manual-path assertion. Unmodified assertions pass with one worker
  in `green-06.log`. Use the existing oracle's scoped single-worker fixture;
  restore its prior pool state and keep every numeric assertion/tolerance.
- `build-07.log` records noncopyable fixture misuse, corrected through native
  data constructors. `custom-red-01.log` then demonstrates the real nested
  archive callback bug; the added exact-type guard makes the test pass.
- `green-11.log` contains one invalid multi-asset LSM fixture, corrected by its
  required explicit default index; `green-12.log` passes all twelve then-current
  request tests. No production LSM behavior or tolerance changes.
- `off-related-02.log` passes all 69 related cases before formatting. The new
  17 request cases cover sealing/destruction, execution-time/global and explicit
  fixings, passive tape/getter preservation, combined budgets/overflow, early
  rejection, direct identities/normalization, empty smoothing, expired/mixed
  methods and failed report publication/recovery. Two automatic flat/Merton
  cases add 84 independent quote bump/recalibration/pricing rows using the
  unchanged original step/tolerance/adjacent-step protocol. Existing mathematical
  oracles remain unchanged.
- `ccn-01.log` catches a nine-branch axis checker. Two identity/value helpers
  preserve every comparison; `ccn-04.log` passes the CCN-eight scan.
- Preserve `combined-build-01.log` and `sanitized-build-01.log`: guessed build
  directories do not exist. The corrected `*-build-02.log` use inspected caches.
- Fresh regular CTest after formatting passes 2,457 OFF / 2,471 combined,
  excluding benchmark/slow labels, in `{off,combined}-full-01.log`. The fully
  instrumented combined Debug ASan/UBSan pass with leak detection and
  halt-on-error passes all 69 related cases in `sanitized-related-01.log`.
  Actual MSVC syntax passes all 12 production/test OFF/combined units without
  an additional NOMINMAX override in `msvc-syntax-01.json`. Own Windows runtime
  CI is still required. `functional-01/results.json` passes 128 isolated installed
  consumer processes with native/all-selected/contribution/report parity.
- The [performance report](../performance/aad-dupire-risk-request.md) records
  64/64 changed old installed-entry rows passing the unchanged paired gate and
  all 64 new informational rows: 3,840 processes and 1,578 unchanged inputs.
  Nine accepted gate binaries remain identical. Preserve the earlier identity
  inventory and its correction, and `format-failure-01.txt`: the first truncated
  formatting patch was rejected atomically; the complete new-file/changed-range
  patch is applied and all five new C++ files are format-idempotent.

## Summary

Local C++ request functionality, original-protocol oracles, configurations,
sanitizers, installed consumers, MSVC syntax and bounded old-entry performance
pass. Publication-head CI remains pending; full F01 bindings and whole-PR/P01
acceptance remain open.
