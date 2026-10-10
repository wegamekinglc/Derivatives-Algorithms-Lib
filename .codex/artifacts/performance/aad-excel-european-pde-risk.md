# Excel PDE scoped cost acceptance

Status: scope selected before implementation/timing. Baseline is merged #534,
1ee4b6c19f74b99aeea9ac66d815b550edc3829d.

## Changed paths and selected callers

- New __europeanpderisk.* implements passive settings/request handling and
  typed result/spill projection. Select full requests at 9 nodes/8 intervals
  and 61 nodes/120 intervals. Compare the accepted public C++ request with
  the Excel typed request/result plus detached labeled spills, checking all
  prices, six risks and forward/transpose errors before/after each loop.
- The public header adds only an inline validation projection. Rebuild actual
  public/Python dependents and prove whether accepted binary identity permits
  reuse. Do not rebuild or time native core/solver families automatically.
- The shared StorableRiskValue_ body and existing risk headers/callers remain
  unchanged. Metadata is local to the new family. Confirm this source/dependency
  boundary before excluding scalar/weighted/blocked/portfolio timing.
- New worksheet raw guards/registration execute on Windows. Portable native
  typed-call timing does not claim Excel host recalculation, UI, COM or actual
  exported-wrapper performance. Test actual exports separately in Windows CI.

## Evidence contract

Correctness must pass first. Use isolated accepted/head paths, matching Release
compiler/native configuration, one DAL thread and fixed CPU. Preserve accepted
prefixes; record affected source/dependency/object/archive/bridge identities.
Use calibrated loops >=25ms and two rounds of ten alternating interleaved pairs.
Retain all raw observations and per-round minima.

The native/typed-Excel boundary contracts differ in ownership and spill work;
their deltas are informational. Apply a 4% regression gate only if an additional
truly identical caller comparison is justified and explicitly selected.
No unrelated nine-target benchmark gate, full mesh/parameter matrix or new
long-schedule sample is required: accepted public report detachment already has
linear traversal and 4096-interval evidence, and this adapter adds linear copies.

Repair only affected cases; record a concrete gap before expansion. Do not
claim omitted cases passed. Required exact-head CI, actual new Windows runtime,
complete reviews/Codacy and guarded publication remain independent gates.

## Outcome

Pending implementation and correctness; no timing result is claimed.
