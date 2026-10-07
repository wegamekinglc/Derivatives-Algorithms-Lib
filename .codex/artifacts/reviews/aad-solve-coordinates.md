# Solve-coordinate implementation review

Verdict: Approve (local code and scoped acceptance; remote publication gates open).

## Findings

No unresolved code findings in the current numeric implementation. Review covers
both complete new production files, tests, controlling spec/API/critique,
methodology, changelog and ledger. Existing runtime files and build/workflow
configuration are unchanged. Completed #493/#494 artifacts are retired with an
immutable specification link and accepted delivery evidence retained in the ledger.

Wide metadata uses size_t and validates the allocator's double-buffer range,
without an incorrect INT_MAX element restriction. Row intervals and offsets
are bounded before expansion. Reverse computes only actual parameter entries,
sums symmetric pairs, retains one LU and publishes owning results. Const failure
does not mutate the cache. Allocation admission/refund follows existing vectors.

## Open questions

None for the numeric API. Native recording and diagnostic composition remain
separate delivery requirements; F03 is incomplete.

## Tests

- Missing-header RED, then analytic symmetric GREEN.
- 16 new coordinate cases and 11 affected ordinary numeric cases pass 27/27.
- Exhaustive small layouts and allocation-free counts above INT_MAX; independent
  elimination, three-step differences and directional identity; indefinite A;
  snapshots, repeated seed scaling/zero, const concurrency and numerical failure.
- Exact construction and parameter-sized reverse budgets; one-byte-short refunds,
  RHS-only recovery and paired overflow recovery.
- Installed CMake consumer passes 1/1. All changed C++ complexity is at most 8;
  formatting, patch and documentation checks pass.
- All 167 existing library members and a fresh ordinary boundary executable
  match accepted immutable baseline bytes. Six new-API cost rows are observed
  in 40 alternating processes. The full-band helper-call overhead is reduced
  after inspecting assembly; all 16 affected cases and a fresh installed
  consumer pass after that repair. Small/full-band costs remain explicit in the
  [performance report](../performance/aad-solve-coordinates.md).

## Summary

The API is additive and explicit about packing, passive ownership and dense LU.
Residual risk is pending exact-head platform, sanitizer, Codacy and review
acceptance. Inspect actual new-case execution before
merge; local checks do not establish remote acceptance.
