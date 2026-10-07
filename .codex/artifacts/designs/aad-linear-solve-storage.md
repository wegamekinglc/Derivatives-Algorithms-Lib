# Recorded solve storage admission

Status: implemented locally inside draft PR #491. The sufficient-budget
recording RED and an owned-vector allocation RED have GREEN evidence. Final
platform and shared-path performance acceptance remain open.

Use a cold `Detail::OwnedBufferScope_` with a non-owning account pointer during
owned payload construction, reverse scratch and destruction. Existing vector
storage functions already have a cold allocation context behind a global active
scope guard. Reuse that guard: ordinary unscoped buffer allocation retains its
existing path, and Number/TapNode allocation/propagation remain unchanged.

The account reserves exact allocator-supplied bytes before allocation and
releases after deallocation. Its scope cannot contain escaped allocations or
destruction of foreign buffers. Keep caller matrices outside it. A solve event
owns an optional payload: create/reset that payload while the account is active,
and preallocate output bindings before creating output nodes. The returned X
container is allocated in the caller's context. Numeric reverse temporaries are
created and destroyed inside the account; a separate high-water metric identifies
their peak above retained storage.

Suspend existing request-buffer accounting during retained payload construction
and destruction, so cache ownership cannot escape an unrelated buffer scope.
During reverse, temporary buffers may obey both the attached request scratch
ceiling and tape-owned storage ceiling. These independent ceilings overlap for
reverse scratch; do not add their peaks as disjoint actual storage.

Add an event-storage domain to tape capacity admission. Maintain its current
bytes only at cold owned allocations; read it explicitly from MeasureTape and
worker readmission. Owned event handles, the lazy owner and the event-table
allocator reserve their complete descriptors/capacities before allocation and
release after destruction. Prepare table capacity before output publication.
Vector growth admits new and old capacity concurrently. Reset destroys event
storage before scalar rewind; detached tape readmission replaces the previous
aggregate charge using current measured capacity, matching existing semantics.

## Local critique

Verdict: Proceed with caveats.

No user decision is pending. Required caveats:

- Owned allocation contexts must not let caller buffers escape or release a
  foreign vector. Numeric snapshots and cache bindings belong to the payload;
  returned output-container storage belongs to its caller.
- Destructor code must explicitly reset the payload before its allocation scope
  ends. Member destruction after a destructor body would otherwise miss releases.
- Event descriptors and table replacement overlap need tickets, not estimated
  postallocation charges. Destruction releases after actual deallocation.
- Failed construction and reverse must refund scratch/cache storage and preserve
  graph invalidation. Peak reservation metrics include admitted attempts; retained
  capacity measures successfully live owned storage outside allocation boundaries.
- Preserve generic buffer-budget rejection/refund behavior and aligned/sized
  deallocation. Add proportional generic-buffer tests plus native resource tests.
- Ordinary no-event timing, installed consumers and actual OFF/ON CI remain
  required; the design alone proves neither compatibility nor performance.
