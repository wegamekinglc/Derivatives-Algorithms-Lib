# WP-4c local review

## Findings

No findings. Read the full changed C++ files, complete LSM guide and controlling
spec/API/critique/performance artifacts against baseline 226b4b3. This is a local
working-content review; reconcile the committed and published blobs before merge.

Checked generic-plan source compatibility, legacy 8192-path boundaries and ordered
floating-point folds, absolute Sobol offsets/RQMC keys, historical active constants,
prefix/input-adjoint reuse, callback captures and cancellation/draining lifetimes.
AAD state is initialized, used and destroyed on its executing thread. Curvature
mode and capacity scopes retain destruction order; independent worker capacity
slots are folded after recording close and task draining. Result-ring slot reuse
does not own active numbers. No dependency or public API/default change.

## Open questions

None for the local implementation. Exact-head CI, sanitizer/profiling evidence,
Codacy and remote review audits remain publication gates.

## Tests

Actual baseline RED has two normal task-count failures and three with profiling;
the independent fresh-segment oracle passes on the baseline. The original formatted
RED source is retained byte-for-byte as the prefix of the final test file.
Final tests pass: 9 normal / 11 profiling contracts, 100 focused profiling,
686 related normal, 7 CLI and full native/public/portable Excel CTest 3457/3457.
Contracts cover exact tree/compiled multi-batch/RQMC/historical risks, retrained
value passes, 257-batch numeric bounds, one-slot order/cancel/recover, concurrent
pool requests, and curvature products/budget/failure recovery. Changed-range
formatting, complexity, whitespace and documentation checks pass.

The caller-selected 14-case/560-output paired measurement passes the unchanged
two-round gate. PV, all risks, weighted components and curvature coordinates
match exactly. Dense LV and RetrainedBump improve; other cases are roughly
neutral within the gate. GSR has mixed +8.45%/-2.22% rounds, retained explicitly
without claiming a stable directional gain. See the scoped performance report.

## Summary

Approve the local implementation. Residual risk is platform/sanitizer-specific
behavior; remote exact-head validation is still required before merge. Published
documentation describes current worker ownership and bounded numeric storage.
The separate documentation decision omits a routine-refactor changelog entry.
