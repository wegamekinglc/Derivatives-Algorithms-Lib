# Python rate-curvature local review

Verdict: Approve locally; exact-head publication gates remain required.

## Findings

The integration documentation audit found a stale statement that the native
rate-trade entry had no Python projection. The methodology now links the new
Python entry while accurately keeping Excel unavailable. The implementation
ledger also reconciles accepted native estimator validation and subdivides the
remaining interface effort. These documentation corrections reuse unchanged
native/binding/test/cost identities.

No blocking correctness or design findings in the complete binding, registration,
build integration, tests and active controls. Native numerical implementations,
public headers, shared bump conversion and capability flags are unchanged.

The four typed calibration overloads copy their specification before releasing
the GIL. Financial execution similarly owns trade values, snapshot, bump request
and settings. No Python callback or borrowed list survives into native work.
Snapshots share only const native captured data. Results and container getters
are detached; immutable fixing handles retain their native owner.

Strict conversion excludes bool/enums, arbitrary coercion and nonfinite values,
with function/field context. Native code retains exact-square calibration,
currency/component/fixing admission, explicit zero caps, early numeric admission
and failure recovery. Empty matrices preserve the quote width. Work evidence and
finite-step labeling do not imply general native higher-order support.

## Tests

- RED: merged installed #529 lacks `RateCalibration_New`; the first public test
  fails for that missing interface. The fresh extension passes it.
- Final installed run: 55 new cases pass with `DAL_NUM_THREADS=4`. Independent
  off-knot deposit PV/gradient and actual-step analytic-gradient secants pass;
  a separate two-step check converges toward the smooth analytic Hessian.
- All four calibration alternatives preserve axes through changed-quote replay
  and return to original parameters. Tests cover weighted/duplicate/zero rows,
  history ownership and saved-fixing conflicts, zero/exact/short budgets,
  strict types/numbers, empty geometry, copies/GC, recovery and concurrency/GIL.
- Nine affected existing installed-package cases pass: shared bump ownership/
  zero budgets, rate pricing/preparation, single/joint/staged quote provenance
  and joint-XCCY layout. No full native or trade/curve parameter matrix repeats.
- Four strict OFF/combined binding/module syntax checks and formatting pass.
  Fresh standalone Release extension links accepted native archives. Eighteen
  old binding objects retain bytes; the registration object is rebuilt.
- Two scoped new boundaries retain 80 calibrated interleaved process samples
  in 3.203935253 measured seconds. Different admission/ownership contracts make
  their ratios informational; base-only ownership overhead is disclosed.

## Open questions and residual risk

None requiring user input. Native seven-trade/four-curve mathematical and fixing
semantics reuse accepted native evidence; this projection does not duplicate
that full matrix. Linux extended and MSVC Python execution, Codacy and complete
remote review bodies still require inspection for the published exact head.
All current-head checks and two final audits must pass before guarded merge.
