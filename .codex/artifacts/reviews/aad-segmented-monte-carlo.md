# Segmented Monte Carlo local review

Verdict: Approve the local implementation — correctness, installed use and scoped
cost acceptance pass. Current-head CI/review/publication gates remain open.

## Findings

The final remote-thread audit exposed one remaining zero-driver boundary finding:
the sole path at SIZE_MAX was incorrectly rejected by exclusive-end admission.
The focused time-zero/history case reproduces the rejection before the fix. The
repair admits the inclusive final index only without drivers; nonzero-driver
RNG admission is unchanged. Three related cases, four affected strict probes and
installed use pass. One short-request canary passes two best-of-ten rounds at
+2.95%/-0.87%, retaining all prior matrix and allocation evidence unchanged.
Final remote review/thread closure remains required on the repaired head.
Copilot's two findings are repaired in `288a0b07`: zero-driver requests retain
only size_t range admission, and the benchmark catches std::exception. The large
offset time-zero/historical case fails before the repair and passes for all three
generators afterward; two related RNG/admission cases also pass. An injected
bad_alloc aborts before the benchmark repair and returns status 2 afterward.
All seven affected strict probes pass, with the other four retained. Installed
use passes again. One short-request canary passes two best-of-ten rounds at
+2.32%/-0.57%; unchanged replay/resource evidence is reused. Current-head remote
review/thread closure still requires acceptance.
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
measured latency and aggregate heap tradeoffs are documented. IRN seeking cost depends
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

Four selected ordinary-caller gates pass; long requests save about 9.5–21.5% warm
payload at about 7.3–8.1x latency. Cold short worker scheduling can increase memory.
Retain the explicit choice and shared-host measurement caveat. No whole suite or
unrelated benchmark matrix is required for this additive request.
