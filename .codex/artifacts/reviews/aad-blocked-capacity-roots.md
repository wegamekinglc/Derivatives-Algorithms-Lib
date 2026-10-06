# Native tape capacity and block roots

Verdict: Comment Only. The native building blocks pass focused acceptance;
prepared replay, complete scratch enforcement, bindings and delivery gates
remain required for #484.

## Findings

No unresolved correctness finding in the implemented native increment.
The tail regression originally propagates an obsolete lane seed into a
gradient of 68 instead of zero. `SeedAdjointBlock` now clears unused lanes
explicitly, including when `PayoffRoot` reuses a terminal node.
Codacy flags complexity 11 in seeding and 10 in its analytic test at `dc399208`.
Extracted preflight and fatal-propagating value assertions bring every new
capacity/root function within the unchanged complexity limit of eight.

## Capacity ownership

`dal-cpp/dal/math/aad/tapecapacity.hpp` owns one request-wide synchronized
ledger. Its scopes admit the calling thread's existing default tape before
recording. Every allocated node, derivative, pointer and vector-adjoint block
is charged, including inactive vector storage and skipped tails. Detached
worker caches stay charged until budget destruction or readmission updates
their actual retained capacities. The budget must outlive all attached scopes.

The hook in `dal-cpp/dal/math/aad/blocklist.hpp` reserves before allocating a
new block. An allocation ticket refunds a failed allocation. Only the four
attached tape lists participate; unrelated lists and unbudgeted requests do
not create ledger entries. No budget operation is added per node or edge;
the existing storage-growth branch calls the cold boundary. Tape/node/number
member layouts are unchanged.

Cold scope admission installs the allocation guard before calling `Tape()`.
The initial four list addresses and admitted payload are collected while the
tape constructs, then transferred into its permanent ledger entry. Constructor
failure unwinds the partial tape and refunds the temporary payload. Cached
tapes still undergo complete admission before recording. The block batch
attaches this guard before selecting vector mode, which also calls `Tape()`.

`BlockList_::Clear` allocates its replacement before discarding old storage,
so a rejected replacement preserves the list. Admission includes this
temporary overlap; old payload is released after destruction. Peak reports
admitted payload reservations, including transient or subsequently refunded
reservations. Allocator/list/map metadata and process RSS are excluded.

The optional cleanup reservation protects the largest single replacement block
per worker throughout execution. Ordinary growth excludes all attached workers'
headroom; replacement uses its owner's headroom and still checks the aggregate
physical payload limit. Concurrent replacements fit their combined reservations.
Unused headroom is capacity held for cleanup, not allocated payload in the
current/peak counters. A newly attached scope can conservatively reject while
another worker has a replacement in flight. The producer preflights initial
resident payload plus headroom before scheduling. Existing one-argument scopes
retain their explicit overlap-only behavior.

## Native roots

`dal-cpp/dal/math/aad/adjointblockroot.hpp` uses the checked native operations
and existing path-local root materialization. Root vector capacity must be
reserved before recording. Bounds, vector width, active zero and selected
finite values are validated before materializing roots. Each row has a
separate lane; aliases retain attribution, and padded lanes are zero.
Scalar-width-one execution is not used by this helper: width one is vector
mode. The legacy scalar and weighted collectors do not call it.

## Verification

Evidence lives in `dal-aad-evidence-20261004-8886c083/evidence`.

- `aad-blocked-capacity-red-01.{json,log}` captures the absent-interface RED.
  `aad-blocked-capacity-build-02.json` and both GREEN logs pass 43 relevant
  native/storage/recording cases with newly compiled affected native units.
- `aad-blocked-root-red-01.{json,log}` captures the root interface RED.
  `aad-blocked-native-build-tail-red-07.json` and its log reproduce the
  obsolete padded seed. Subsequent GREEN keeps the zero-gradient assertion.
- `aad-blocked-native-build-09.json` and OFF/combined logs pass 50 focused
  cases per configuration. Combined enables lifetime diagnostics, profiling
  and ASan/UBSan. Fresh affected native/test units link cached unchanged DAL
  and Google Test support; this is focused instrumentation.
- `aad-blocked-native-build-tsan-01.json` and its log pass all nine new
  capacity/root cases with TSan and combined diagnostics. Fresh affected
  native units are instrumented; unchanged supporting archive code is cached.
- `aad-blocked-capacity-codacy-annotations-01.json` retains both complexity
  findings. `aad-blocked-capacity-complexity-02.json` verifies maximum eight.
  `aad-blocked-codacy-fix-build-01.json` and its three logs pass the same 50/50/9
  cases after extraction. Only the affected root-test unit is rebuilt in each
  mode; unchanged native units retain the prior acceptance and instrumentation.
- Native oracles cover widths 1/2/4/16, 17 ordered rows and tails, three
  suffixes before prefix reverse, aliases, constants, direct and prefix roots,
  finite values and exact inactive-lane zeros. Capacity cases cover all four
  lists, cache rejection/readmission, transient replacement, rollback/overflow,
  owner/nesting errors and aggregate concurrent/retained-worker reservations.
- CI sanitizer selections append these cases while retaining every previous
  selection. Ordinary platform suites discover them through the existing glob.
  All six filter extensions preserve prior selections; YAML parses successfully.
  GCC 14's original strict OFF/combined warning profiles pass. Formatting,
  patch checks and documentation integrity pass for 149 Markdown files.

## Cold admission and cleanup increment

- `aad-blocked-buffer-build-cold-red-01.json` reproduces missing cold admission:
  the failed new-thread initialization reports peak zero instead of admitting
  its first three initial blocks before rejecting the fourth. The same thread
  then successfully records and reverses with an exact resident budget.
- `aad-blocked-buffer-build-cleanup-red-02.json` reproduces growth consuming the
  space required for cleanup. `aad-blocked-buffer-build-cleanup-overlap-red-01.json`
  exposes subtraction wraparound when a replacement is already admitted.
  Both unchanged rejection assertions pass after protected headroom and the
  overflow-safe capacity guard.
- `aad-blocked-buffer-build-cold-cleanup-green-02.json` passes 23 capacity and
  recording cases in OFF, combined ASan/UBSan and combined TSan. This remains
  focused affected-unit instrumentation over unchanged support archives.
  Two simultaneous replacement tickets reach the exact aggregate limit;
  refund, actual clear and detached accounting then recover correctly.
- All 11 storage cases also pass OFF/combined in the corresponding
  `aad-blocked-cold-cleanup-storage-*-01.log` files. Six canonical GCC 14
  OFF/combined syntax checks pass; changed functions remain within complexity
  eight. Fresh matching OFF core/public libraries provide integration coverage.
  Initial planner check-head `294e0d09` passes all 35 CI/Codacy checks and has
  no review threads. These are separate from this increment's acceptance.

## Open questions and remaining gates

The producer must preflight cleanup overlap as well as resident blocks:
a poisoned recording clears all four lists, so an exact resident-only limit
can reject its replacement. Include conservative per-worker replacement
capacity in known bounds and exercise failure/retry recovery under that bound.
Do not convert cleanup into an unbudgeted allocation during request draining.

Inventory and admit numeric model/path/evaluator/root/batch capacities before
their allocations; the passive planner remains only a named lower bound.
Implement sealed common-path replay, complete task draining and matrix
consumers. Map the changed cold allocation/clear paths to tape, Jacobian,
rate-risk and Monte Carlo performance coverage; prior #483 performance cannot
prove this increment's costs. Current-head CI, Codacy and reviews remain open
until the complete delivery is stable and audited.
