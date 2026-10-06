# C++ Jacobian producer and PR repair review

Scope: the active #484 draft, including ordered planning, owning results,
native/passive C++ valuation and the four Copilot findings on `7c9cfa89`.

## Findings

- Delivery blocker: `dal-public/src/riskvalue.cpp`, `PreflightRiskValuation`,
  checks retained results, fixed evaluator/root/batch storage and initial tape
  blocks. It does not yet establish every model/path/evaluator minimum before
  history. Finish R10 with history-free model-aware admission and boundary tests.
- Delivery blocker: Python/Excel Jacobian consumers and their strict parsing,
  ownership/GIL and actual generated-export acceptance remain open. Existing
  weighted consumers do not satisfy R13 for this new result.
- Delivery blocker: run current-head platform CI and the frozen old-entry
  performance gates after the delivery implementation stabilizes. Earlier
  successful heads do not accept these allocator and public-producer changes.

## Repairs

1. `bufferallocation.hpp` retires an allocation address before calling its
   deallocator. A deallocation ticket retains the bytes until the callback
   returns. Forced recycling reproduces the original duplicate-address failure;
   the repaired test also verifies that physical-free overlap remains charged.
2. `adjointblockroot.hpp` materializes every root, clears every channel of every
   live root, then applies diagonal seeds. This preserves aliased rows while
   removing stale live-lane seeds. The new regression originally returned 69
   instead of 1; both two-row and aliased three-row cases now match analytically.
3. `blockedreplay.hpp` owns a copy of the RNG method string and binds captured
   batch settings to it. Caller mutation after submission originally produced an
   invalid-method failure; the sealed request now preserves its common paths.
4. Native/passive task groups suspend coordinator tracking while submitting,
   helping and draining the shared pool. Their future array is admitted through
   fixed capacity before suspension. Worker scopes attach their own request;
   reservations remain charged while the caller attachment is suspended. A
   single-worker queue forces an independent Jacobian request and a larger
   unbudgeted task to run inside both successful and exceptional draining.
   The original request nested-budget rejection is reproduced and repaired.

Suspension restores the caller attachment after task destruction, including
exceptional draining. It is used only by the new drivers. Profiling trace slots,
task shared-state allocator metadata and immutable compilation/axis text are
outside numeric scratch; the future-handle array remains admitted explicitly.

The macOS x86 wheel failure at `7c9cfa89` occurs in the existing Python
evaluation-date/GIL test. Its busy polling now yields to the pricing thread.
The path count, deadline and lock-inversion assertions remain unchanged. Local
focused Python acceptance uses the accepted weighted installed module; the
latest wheel job must still confirm the platform repair.

## Tests

- `aad-jacobian-current-core-green-02.log`: 46 focused core cases, including
  scalar/weighted result regressions and the four repair cases.
- `aad-jacobian-current-public-green-02.json`: 26 public cases, including ten new
  Jacobian tests and sixteen accepted scalar/weighted tests. Independent public
  scalar rows cover 1/4/16/64 outputs, tree/compiled execution and one/four workers;
  weighted `J^T w`, fixed central differences (step 0.1, tolerance `1e-10`),
  BS mutation during history and nested Hybrid/GSR archive snapshots also pass.
- `aad-blocked-review-sanitizers-01.json`: focused fresh native/buffer helper
  units linked with cached support; 19 combined ASan/UBSan cases pass. The
  initial TSan runner reports three future/thread-pool warnings. Its cached
  thread-pool object has zero TSan symbols, recorded in
  `aad-blocked-review-tsan-support-inventory-01.json`. Recompiling that exact
  support source with TSan and relinking passes all 19 cases without suppressions;
  see `aad-blocked-review-tsan-threadpool-green-02.json`. Retain the initial
  failure. Complete current-source platform sanitizer CI remains required.
- `aad-jacobian-public-warnings-02.json` and
  `aad-jacobian-plan-warnings-02.json`: canonical GCC 14 warning flags, OFF and
  combined diagnostics. Complexity remains within the configured limit of 8.

## Open questions

No change to width tuning or retry policy is authorized by these repairs.
Runtime capacity exhaustion still drains and fails without a partial result.
Resolve the complete known-minimum inventory in the controlling specification;
retain the default width of one and the accepted scalar/weighted arithmetic.

## Verdict

Request Changes for the remaining delivery blockers. The repaired findings must
be reconciled with the published commits and fresh remote review/CI evidence
before #484 can be marked ready or merged.
