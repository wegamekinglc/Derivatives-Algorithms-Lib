# Native solve accuracy reporting

Status: active; builds on accepted #497, without reopening numeric acceptance.
Source: the approved AAD plan sections 4.3.3/4.3.5 and the native-only scope.

## Problem and boundary

Forward diagnostics cannot describe the seeds accumulated during a native
reverse. This increment checks the actual transpose solve and returns an owning
report for each checked event executed in a successful reverse invocation.
Implicit-root derivatives, PDE and efficient checked coordinate APIs remain
separate F03 work. This dense delivery must not be labeled complete F03.

## Requirements

1. Support both-active, active-A/passive-B and passive-A/active-B dense inputs.
   Require an explicit immutable accuracy policy; preserve numeric pivot defaults.
2. Capture one owning CheckedLinearSolve_ and active bindings. Reuse its one LU
   and actual returned transpose adjoints. Do not recompute a different seed or
   add a second factorization. Passive-A reverse requests only RHS contributions.
3. Each checked event enforces its policy even through ordinary scope.Reverse().
   Optional full/prefix/suffix wrappers collect only events actually executed.
4. Publish collections only after the entire requested reverse succeeds. A
   failure destroys partial reports and preserves existing failed-graph behavior.
   Do not promise rollback of previously scattered native adjoints.
5. Event identity is opaque and never reused. Each owning report retains event,
   recording, invocation, scalar/vector mode and an m-by-channel error matrix.
   Each detached entry retains provenance independently of its container.
6. Repeated reverse uses a new invocation identity. Missing-event lookup rejects;
   it must not invent a zero or reuse a report from another window. Restored
   slots, replaced checkpoints and discarded suffix events cannot reuse identity.
7. Preserve zero channels and their zero residuals. Reject malformed/nonfinite
   input, policy, output seed, requested contributions and adjoint accumulation.
   Preserve representable ill-conditioned risk without clipping.
8. Reports/metadata are caller-budget-owned. Numeric cache/bindings and scratch
   are tape-budget-owned. Preallocate report storage before entering scratch
   ownership; never nest owned accounts or move tape-owned vectors into reports.
   Check widths/extents before narrowing. Refund every failure allocation.
9. Enforce recording owner, state, mode and checkpoint checks. TLS collection
   exists only in the optional implementation and resets on every exit. Reject
   nested collection; independent threads collect independently.
10. Keep Number/node/tape layouts and ordinary payload member order unchanged.
    Reuse private snapshot/publication/scatter/buffer helpers. No collector
    branch is added to ordinary node loops or ordinary payload reverse.

## Acceptance

- First missing-header RED and an independently derived asymmetric composed
  objective GREEN; activity combinations and true aliases.
- Rational delta=2^-27 threshold oracle, inclusivity and below-limit rejection;
  scalar/vector widths 1/4/8, multiple RHS and dependent checked events.
- Repeated/windowed reverse, restore/close/source destruction, owning detached
  entries, foreign-thread/mode/state misuse and failed-graph read rejection.
- Exact retained/scratch/report budgets, one-byte-short failures, refunds and
  passive-A unused-entry overflow omission.
- Strict OFF/combined diagnostics/profiling checks and installed consumption.
- Freeze affected caller/performance scope before timing. Reuse byte-identical
  accepted callers; measure changed callers only, using the two-round 4% rule.
  Report new optional checking costs separately.
- Exact-head CI, Codacy, review and actual new-case execution in the fourteen
  established sanitizer/extended/MSVC configurations before merge.

No user decision remains open. Identity access and helper extraction are local
design checks that must pass before publishing this increment.
