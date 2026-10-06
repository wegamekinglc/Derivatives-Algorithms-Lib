# Blocked-script scratch capacity increment

Verdict: Approve this allocation-boundary increment. Whole #484 remains a
draft; prepared replay, result/consumer integration and performance acceptance
are required before its merge.

## Findings and design decisions

No unresolved correctness finding in the exercised primitives. RED found that
only the parameter-pointer array was charged for a heap Black–Scholes model:
32 bytes instead of its 256-byte object-plus-array payload. Shared object
allocation helpers now admit the complete model/component object before its
constructor. A separate RED caught hidden nothrow allocation overloads;
aligned and nothrow construction, caller-owned placement and virtual deletion
are covered without adding object fields or changing the inheritance graph.

`Vector_` uses a stateless allocator for non-text/non-exception elements.
Accounting surrounds capacity allocation, never element access or AAD edges.
Reservation precedes physical allocation; commit failure and element-constructor
failure refund it. Physical freeing precedes release, so concurrent workers
cannot spend bytes which have not yet been freed. Growth admits old/new overlap.
Matrices, nested scenario/evaluator vectors and packed booleans use this same
boundary. Text arrays and the exception stack retain their original allocator.

One out-of-line accessor owns thread-local attachment, shared by library and
consumer allocation sites. The installed shared-library consumer creates and
deletes library-owned Sobol buffers under the caller's scope and rejects its
one-byte-short budget. This checks cross-module enforcement rather than relying
on linker coalescing of header-local state.

Container size remains unchanged in the tested ABI. The allocator changes the
underlying standard-library iterator type for tracked elements. Consumers must
rebuild with matching DAL headers and use `Vector_`'s iterator aliases or `auto`;
an explicit `std::vector<T>::iterator` assumption is not a compatibility promise.
This needs a release note at final delivery. Model/static allocation overloads
do not alter object fields or virtual function order.

## Coverage and producer obligations

`BufferCapacityBudget_` synchronizes all attached worker/coordinator reservations.
It charges allocation payloads including inline elements and nested-vector
descriptors; it does not report process RSS. Model/component objects include
their inline numeric members. Fixed evaluator payload, including inline stacks,
is admitted through the scope's fixed-byte argument before construction.
Dynamic text, exception/ledger metadata and external allocations are excluded.
DAL-owned RNG vectors are included when constructed in an attached scope.

Request buffers must be constructed and destroyed under a scope of their owning
budget. Moving a buffer between workers of the same budget preserves its charge.
Admission of buffers carried from a previous request is not implemented here;
the first producer must construct fresh scratch. Buffers destroyed outside an
attachment remain conservatively charged, so this is not a general allocator
ownership registry or the deferred P02 worker-cache implementation.

The driver must attach before collector construction, admit fixed evaluator
payload and retained result capacity, preallocate result slots under coordinator
ownership, and drain all tasks before destruction. The tape guard remains a
separate budget. Tape failure cleanup needs guaranteed replacement headroom
throughout execution, not just a minimum-byte check before starting.

## Validation

- Buffer interface/model-object/nothrow RED logs preserve their failures.
- `aad-blocked-buffer-build-06.json`: 94 focused primitive, native AAD,
  vector and stack cases pass in OFF, combined ASan/UBSan and combined TSan.
  Unchanged archive support is reused; this is not full-core sanitizer coverage.
- `aad-blocked-buffer-warnings-02.json`: canonical GCC 14 warning profile,
  five affected production/test/consumer units in OFF/combined, passes.
- `aad-blocked-buffer-complexity-01.log`: no new function exceeds complexity 8.
- A complete fresh OFF core/public shared build passes. Installed consumers
  pass 3/3, including caller-scope/library-buffer capacity enforcement.
- All six sanitizer CI selections retain existing entries and append the
  buffer-capacity suites. Full platform checks run on the published head.

## Remaining acceptance

Map and measure the changed cold allocation/freeing paths against all applicable
existing-entry workloads under the frozen two-round gate. No no-regression
verdict is inferred from correctness tests or unchanged object size. Prepared
multi-output Monte Carlo, budget-failure draining, provenance and C++/Python/
Excel results remain open.
