# Native recorded solve implementation review

Verdict: Request Changes before acceptance; the current increment may remain in
draft PR #491 while the following required work is completed.

Reviewed the local implementation against the controlling
[specification](../specs/aad-linear-solve-recording.md),
[API decisions](../api-notes/aad-linear-solve-recording.md) and
[critique](../critiques/aad-linear-solve-recording.md). Contract commit
`76f52d11` is the review base; this review concerns the subsequent local source
increment and does not represent a final-head publication audit.

## Findings

1. Required resource admission remains incomplete at
   `dal-cpp/dal/math/aad/linearsolve.cpp:105`, `RecordSolve`: a finite tape-capacity
   scope rejects before allocating solve caches. This prevents a silent bypass,
   but accepted finite-budget execution needs cache/descriptor/event-table
   admission and explicit retained/scratch capacity measurements. Preserve the
   original resource requirement; no acceptance or capability promotion yet.
2. Final platform and performance evidence remains open for
   `dal-cpp/dal/math/aad/tape.cpp:45`, `expr.hpp:612`, `native.hpp:59`
   and `aad.hpp:53`.
   Empty-event dispatch, reset and adjoint-read guards change shared paths.
   Earlier #490 executable identity evidence cannot establish this increment's
   performance. Compare fresh existing and recorded-solve workloads at the
   stable head, and inspect actual installed-consumer, Windows and sanitizer
   execution before merge.

The initial missing reset guard in `aad.hpp`, `ZeroAdjoints`, is repaired:
the internal callback RED erased gradients without throwing; GREEN now rejects
the operation and invalidates subsequent reads. No further correctness finding
is identified within the locally exercised prototype behavior.

## Open questions

No user decision is pending. Resource representation remains an implementation
task under the existing contract. Additional foreign-thread/input, admission
failure, scratch-budget and release measurement cases remain required.

## Tests

- Missing-interface analytic and passive/RHS-only RED logs are retained.
- Twenty initial new cases and affected old native/recording/capacity/numeric
  cases pass 71/71 in Release diagnostic OFF.
- Two subsequent serial/shared/direct-seed and forbidden mutation cases pass
  2/2. Final increment verification passes 24/24, adding insufficient-capacity
  rejection before output publication and a nonzero `1e-40` seed. Vector
  coverage includes 1/2/3/4/8 channels.
- Pivoted 3x3/two-RHS gradients match independent scalar AAD and central
  differences at `0.5e-5`, `1e-5` and `2e-5` for every A/B coordinate.
- Caller mutation/destruction, exact and multiple block boundaries, raw reverse
  windows and exact checkpoint/reset event releases pass.
- Fourteen fresh translation units pass combined lifetime-diagnostic/profiling
  syntax checks; actual ON runtime remains a final platform gate.
- Documentation integrity passes for 157 Markdown files including this review;
  patch integrity passes. This is a draft increment, not full acceptance.

## Summary

The prototype composes all three dense solve activity combinations with native
scalar/vector arithmetic without changing Number/TapNode layouts or ordinary
allocation/propagation loops. Caches survive repeated sweeps and prefix reuse;
suffix/reset release and failed-graph read rejection have targeted evidence.
Full resource, compatibility, performance and exact-head CI/review acceptance
remain open, as do the later structured/implicit and second-order plan stages.
