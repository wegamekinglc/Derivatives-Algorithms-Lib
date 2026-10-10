# Excel PDE scoped cost acceptance

Status: repaired scoped informational boundary costs accepted. Baseline is merged #534,
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

The initial publication reports Codacy complexity in the cost bridge/collector
and one combined diagnostic/execution test. Factor the bridge's shared row
copy and exception boundary, keep the committed driver as a single observation
process, and split diagnostic/execution assertions without dropping coverage.
Repeat only the two affected costs after rebuilding the bridge; preserve the
initial 80 observations as historical evidence rather than final-head acceptance.

Coverage gap found during final review: the new public validation facade was
not called by an installed consumer. Extend only that consumer's settings
check and its OFF/combined syntax probes. This adds no hot path or timing case;
the two selected financial costs and accepted numerical algorithm evidence
remain applicable.

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

Final timed head is `fd54fdca9db86d64e3f405cad44f5e541484d9aa` from isolated
detached source trees. GCC 15.2.0 Release/O3 runs on a shared WSL2 host,
CPU 0 pinned, one DAL thread, with no competing local C++ build/test.
The accepted public/core archives remain byte-identical. Both actual
production dependents of the added public inline facade are freshly rebuilt
with accepted flags and have identical objects, preserving previous numerical
and compatibility evidence without repeating old timing families.

Two cases retain 80 full observations in 8.167774303 measured seconds.
Every observed loop is at least 80.721953ms; each round contains ten
alternating interleaved pairs. Every call checks all solve errors and each
observation verifies accepted prices and all six adjoints before/after timing.
Complete process captures, raw samples, compiler commands, source/dependency
hashes and binary identities remain in
[the executable evidence](aad-excel-european-pde-risk-results.json).

| Nodes / ordinary intervals | Round | Public C++ minimum (us) | Typed Excel minimum (us) | Delta      |
|----------------------------|-------|-------------------------|--------------------------|------------|
| 9 / 8                      | 1     | 113.283483              | 112.073622               | -1.067994% |
| 9 / 8                      | 2     | 100.776471              | 105.370229               | +4.558364% |
| 61 / 120                   | 1     | 6765.861733             | 6816.950933              | +0.755103% |
| 61 / 120                   | 2     | 6736.979000             | 6809.084133              | +1.070289% |

These are unequal contracts: Excel additionally creates settings/request/result
handles, copied request/settings handles and all eight labeled cell spills.
The deltas measure that extra boundary work and are informational; the +4%
comparable-contract regression threshold is not applied. The small baseline's
round minima also vary by about 12%; this shared-host noise and unequal work
prevent a speedup claim from its negative first-round delta. No native algorithm
change or speedup is claimed. This measures portable typed function calls,
not the XLL exports, Excel host recalculation, COM or UI. Actual Windows export
correctness remains a separate required publication gate.

Base bridge SHA256: `21187e86078aa09c3fcff373499739e9b5f96bc02250eca8e24fc8ceb779e196`.
Head bridge SHA256: `a6954b6d636b6eb374bfdcf0b361c4003de32f090779b523b5d1c1d18f7b1fb0`.
The shared risk storable template, legacy risk headers, all native core sources
and old algorithms are unchanged. Their prior accepted timings are reused with
source/dependency/binary proof, not counted as new measured passes. No full
benchmark matrix, long-schedule repeat or additional unrelated case was run.
