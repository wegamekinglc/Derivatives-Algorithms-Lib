# Segmented Monte Carlo local review

Verdict: Comment Only — local implementation checks pass; complete MC cost and
current-head publication gates remain open.

## Findings

No open actionable correctness or style findings in the reviewed implementation.
Read the new API, coordinator, tests, benchmark, changed fixed-path admission and
command dispatch, active spec/API/critique, methodology, changelog and workflow
filter changes. All six sanitizer filters include the new suite; four extended
and four MSVC profiles discover it normally. Remote execution is still required.

The request copies inputs and settings before submission. Each stable lane is
exclusive until its wave drains; no ThreadNum() scratch aliasing. Persistent RNGs
seek forward through later waves, and the prototype moves into lane zero. Fixed
32-path batch sums fold in ordinal order across worker counts. Task groups outlive
their accepted futures and are destroyed before captured owners on all exits.
Overflow checks precede task/result allocation; finite sums are checked locally
and at coordinator reduction. Detached results contain no tape-backed references.

The benchmark runner now initializes DAL registration, matching standalone
financial drivers. Its full and segmented consumers both copy normalized risks
into the same owning payload. Preparation and thread startup are outside latency
timing but must be included in separate whole-request memory accounting.

## Open questions and residual risk

No design question requires user input. Native segmentation stays explicit;
latency and aggregate heap tradeoffs require measurement. IRN seeking cost depends
on lane count. Bitwise numerical consistency assumes the same executable/platform
and floating-point environment; resource maxima depend on retained tape capacity.
Sanitizer/MSVC runtime, complete bot review and Codacy results remain unverified.

## Tests

Eleven focused tests cover native and independent analytic references, all six
RNG/bridge pairs, tail/offset, history/vectors/payments, fuzzy/time-zero semantics,
fresh script/model points and digital shift, one/four threads, lane reuse,
admission/budget/worker/submission/aggregate failures, recovery, input snapshots,
concurrent callers and detached results. Eleven strict syntax probes pass OFF and
combined diagnostics, including the baseline-only benchmark macro. The added
registration call passes all three refreshed benchmark probes before timing.
An installed-only CMake consumer passes mean value and all five risk columns.

## Summary

Core correctness and ownership look ready for scoped cost acceptance. No whole
suite or unrelated benchmark matrix is required for this additive request.
