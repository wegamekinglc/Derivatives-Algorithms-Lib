# C++ Jacobian producer and PR repair review

Scope: active #484, including ordered planning, owning C++/Python/Excel results,
native/passive replay, history-free capacity admission and the four Copilot
findings on `7c9cfa89`.

## Findings

- Delivery blocker: the first frozen nine-target comparison at `77193fd8`
  fails on tape clear/rewind (about 4–5%) and PDE rollback (about 5–18%).
  `aad-jacobian-final-nine-paired-01/` retains every sample. Capacity tickets
  now use cold helpers; ordinary tape clear retains its original allocation
  order. The stable TLS-slot accessor permits address reuse without caching
  the changing budget value. Focused correctness passes; matched static
  performance and new-head CI remain required.
- Delivery blocker: actual Windows generated-export acceptance remains open.
  Implemented typed portable bindings and local parsing tests do not substitute
  for executing the new raw XLL exports on Windows in all diagnostic modes.
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

## Startup admission and bindings

The Windows raw-export failure at `77193fd8` is a fixture error: the shared
`WriteExcelCell_` marshals `std::monostate` as an empty string. The test now
requires a one-cell `xltypeStr` with zero length, followed by exact `(1, 0)`
shape checks. No marshalling behavior or numerical oracle changes. Failure
evidence is `aad-jacobian-ci-failure-112174264433-01.log`; actual Windows
rerun acceptance remains required.

The initial long-timeline test accidentally rejected an unbound SPOT before
history; its fixture now supplies the required default index and asserts the
budget error identity. The corrected RED (`aad-jacobian-minimum-red-02.json`)
reaches the rejecting historical observer. Guarded startup admission now rejects
the known model/path capacity before any history read or submission.

The exact-width-one scratch budget test initially failed at replay width nine
(`aad-jacobian-minimum-green-02.json`). Pre-history `VarValues()` is empty, so
the first probe omitted current scalar/fuzzy storage. Admission now uses parsed
variable counts and known nesting metadata. The unchanged test succeeds at
width one and retains the caller's width-sixteen request. Actual allocation
guards enforce each probe's finite per-worker quota before capacity growth;
probes do not evaluate historical statements or generate a Monte Carlo path.

Historical-vector RED (`aad-jacobian-vector-red-01.json`) confirms missing seed
capacity could reach history. Admission now conservatively fills known vector
capacity bounds when past events exist and includes replacement overlap. Native
startup rewinds before registration and admits per-path model scratch after the
temporary historical evaluator is destroyed, matching the execution phases.

`aad-jacobian-admission-final-01.json` captures twelve public Jacobian, 46 core
and sixteen affected legacy public cases. Finite-budget four-worker oracles
include BS, correlated BS, Hybrid, GSR, multifactor GSR and GSR-SLV, each in tree
and compiled modes. The old preparation policy calls an inline empty admission
overload; it constructs no callback or Jacobian probe state.

`aad-jacobian-python-red-01.log` reproduces the missing Python surface.
`aad-jacobian-python-green-01.json` records 97 targeted new/scalar/weighted cases
against a fresh coherent installed core and standalone Python module. New
requests reject implicit integer conversions and expose only read-only fields;
container/matrix/result copies remain detached. The native entry uses the
accepted shared typed-copy/GIL-release helper.

`aad-jacobian-excel-red-01.json` records the missing typed worksheet surface.
`aad-jacobian-excel-green-01.log` passes nineteen new/scalar/weighted typed cases.
Shared output-coordinate formatting retains the existing weighted table.
Twelve generated markup exports accompany two new Windows raw-export cases.
Those tests still need actual Windows execution, not a portable compile claim.

## Open questions

No change to width tuning or retry policy is authorized by these repairs.
Runtime capacity exhaustion still drains and fails without a partial result.
Retain the default width of one and the accepted scalar/weighted arithmetic.

## Verdict

Request Changes for remaining platform/performance acceptance. The four
published findings are resolved at `a62b0f10`; every subsequent head requires
fresh paginated review, exact-head CI and Codacy evidence before merge.
