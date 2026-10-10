# Excel segmented Monte Carlo local review

Verdict: Comment Only — local numerical/binding checks pass; cost and remote
Windows/publication acceptance remain pending.

## Findings

No open implementation finding after reading the complete new adapter, typed
tests, new raw/registration cases and controlling spec/API/critique.
Review caught and corrected a raw fixture's missing payoff assignment before
publication. This was test preparation, not a native numerical behavior change.

## Open questions

None. Native public mean/curvature entries control numerical behavior. This
increment does not change the simulator, tape, Python or existing financial APIs.

## Tests and evidence

- The missing worksheet interface failed compilation first. Independent
  time-zero value/mixed products passed after implementation.
- Eleven scoped typed tests now pass, including deterministic future values,
  partial batches, exact uint64 text, settings/admission, sealed history,
  detached queries, separate caps/recovery, zero directions and caller tape.
- Strict OFF/combined probes pass for the changed production/tests and owning
  bridge. Raw Windows branches still require actual Windows execution.
- The real CMake public binding-boundary check passes.
- Official Machinist regeneration passes after tracking its 27 wrapper/help
  pairs; existing generated files have no drift.
- Changed/covered functions have CCN at most seven. No full local correctness
  or performance matrix is justified by the adapter's caller analysis.
- Two selected full-request costs remain pending, including independent
  finite-path values, all gradients and actual-step secants before/after batches.

## Remaining merge gates

Inspect actual applicable exact-head CI logs, all Codacy annotations, complete
review bodies/comments/threads and required branch policy. Verify new typed
cases in every applicable runtime profile and new raw/registration tests on
Windows; do not substitute syntax or registration for execution. Audit twice,
merge by captured SHA, and compare the accepted and merged trees.
