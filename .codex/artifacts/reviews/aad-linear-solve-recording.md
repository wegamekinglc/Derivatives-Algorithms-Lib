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

1. Final platform and change-scoped performance evidence remains open for
   `dal-cpp/dal/math/aad/tape.cpp:45`, `expr.hpp:612`, `native.hpp:59`
   and `aad.hpp:53`.
   Empty-event dispatch, reset and adjoint-read guards change shared paths.
   Earlier #490 executable identity evidence cannot establish this increment's
   performance. Compare fresh existing and recorded-solve workloads at the
   stable head, and inspect actual installed-consumer, Windows and sanitizer
   execution before merge.

The measured resource-head regressions are repaired locally: the final build
passes 128 affected cases and matches both accepted 17-case tape/Jacobian binary
hashes. Passive recording is +0.97%/+0.98%, and all six original failing cases
pass. Failure/mode checks remain active; caller-owned tape reset cannot expose
adjoints from a failed default graph. See the controlling
[performance evidence](../performance/aad-linear-solve-recording.md).
Affected request boundaries and exact-head platform evidence remain the open gate.
The unstarted 81-case portfolio matrix is removed under the user's project-wide
2026-10-07 performance-scope instruction.

The GSR regression at `64cdbfa6` is repaired locally by a stable-address getter
and cold validation helper, with the original GSR implementation restored.
Final affected functionality passes 132/132. Same-binary noise controls pass;
30-pair confirmation passes all six GSR rows, with the 48-node AAD case at
+3.13%/+3.95%. This is close to the unchanged 4% boundary. Preserve the earlier
borderline failure and verify final binary identity before reusing this evidence.
All eleven affected translation units pass combined lifetime/profiling ON syntax
checks for this final repair. Required actual platform execution remains open.

The resource finding is resolved locally: finite tape scopes now admit exact
descriptor/owner/table/cache and transient storage before allocation. Failed
allocations refund, replacement capacity overlaps, suffix/close release matches
actual capacity, and reverse scratch obeys parent buffer ceilings. Forty-five
new tests pass; the production resource batch additionally passes 113 affected
tests. Parent-budget, event-table growth and ownership increments pass 4/4 and
3/3. Final platform acceptance must use this source increment, not `c79bcc58`.

The initial missing reset guard in `aad.hpp`, `ZeroAdjoints`, is repaired:
the internal callback RED erased gradients without throwing; GREEN now rejects
the operation and invalidates subsequent reads. No further correctness finding
is identified within the locally exercised prototype behavior.

## Open questions

No user decision is pending. Resource representation remains an implementation
task under the existing contract. Foreign-thread/live-input, admission failure,
scratch-budget and release cases now have focused local coverage.

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
- The final repair passes all 11 affected combined diagnostic/profiling ON
  translation units in `aad-recorded-solve-repaired-combined-syntax-01.log`.
  Final OFF functionality passes 128/128; binary identity retains the accepted
  17 tape/Jacobian performance cases.
- Documentation integrity passes for 157 Markdown files including this review;
  patch integrity passes. This is a draft increment, not full acceptance.

## Summary

The prototype composes all three dense solve activity combinations with native
scalar/vector arithmetic without changing Number/TapNode layouts or ordinary
allocation/propagation loops. Caches survive repeated sweeps and prefix reuse;
suffix/reset release and failed-graph read rejection have targeted evidence.
Full resource, compatibility, performance and exact-head CI/review acceptance
remain open, as do the later structured/implicit and second-order plan stages.
