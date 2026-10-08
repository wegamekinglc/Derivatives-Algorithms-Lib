# P04 native structural Jacobian execution

Status: active after accepted numeric #507, merged as
`c11ebd8194fbbd340f99b43019fe1946c70ec618`.
Source: original plan `de5dd8b20089223e6938dfd8100d463aa6d4a169`,
sections 5.4, C.9 and D.4. This increment does not complete P04.

## Problem and boundaries

The owning numeric plan colors caller-proven conservative supports and recovers
complete matrices. Add opt-in complete native execution over a current fixed
recording. Financial support proof, canonical structure equality, dynamic
fallback and strategy/default promotion remain subsequent work.

Do not change existing production headers, source, scope/Number layouts, scalar
or vector kernels, calibration defaults, weighted VJPs or bindings. Reuse the
existing scope-identity and live-slot access bridge from a new cold source.

## Requirements

1. Bind the ordered independent input slots immediately after StartRecording.
   Require current RECORDING state, owning thread, supported fixed mode/width,
   finite input values and live distinct physical slots. Require every live node
   to have zero arguments and no reverse events; computed/event leaves cannot
   be mistaken for independent coordinates. Snapshot identities, not Numbers.
2. Return an immutable owning token. Copies own their input identities; it owns
   no tape/scope and never dereferences retained identities during destruction.
   Reassign moved-from tokens before use. Tokens cannot migrate execution across
   scopes or threads. Ordinary Number lifetime rules remain in force; diagnostics
   add generation checking without changing ordinary Number layout.
3. Execution requires the same current READY scope, recording identity, owner
   and unchanged mode/width. Check scope identity before retained identities;
   validate fresh live Number slots before reading their adjoints/properties.
   Match binding, plan and supplied input/output counts and input order exactly.
4. Validate every supplied input/output value and live slot before clearing seeds
   or allocating numeric results. Outputs include aliases, intermediate nodes,
   direct inputs, terminal nodes and materialized constants. Default Numbers with
   no slot reject, even for a declared empty support row.
5. Conditional mathematical supports remain the caller's responsibility. Binding
   establishes slot identity, not dependency truth. Never learn supports from
   sampled gradients or change the plan at the current numerical point.
6. Use the fixed scalar width one or current bounded vector width. Walk colors
   in consecutive blocks; a final partial block leaves unused lanes zero. Never
   resize a live graph or materialize new payoff roots during execution.
7. Clear all prior graph adjoints/seeds before each block. Add unit seeds for
   every output row of each live color lane; assignment or per-row clearing must
   not discard aliases. Run one complete reverse per block and harvest every
   ordered input coordinate into the color-by-input directions.
8. Reuse numeric recovery for the full owning ordered matrix and exact structural
   zeros. Validate every harvested gradient as finite before publication.
   Retain no Number handles in the result or token. Empty support plans use no
   reverse and preserve exact matrix shape after validation.
9. Reuse exact admitted result-plus-direction payload accounting from the numeric
   plan. Token metadata, tape capacity and allocator/process overhead are outside
   that budget. Do not allocate one dense Jacobian per block.
10. Validation failures leave graph/seeds and READY state usable. Backend reverse
    failures follow the existing FAILED/cleanup contract and publish no partial
    matrix. A non-finite harvest on a still-valid graph can be followed by a
    healthy cleared request; do not promise rollback of backend failures.

## Executable acceptance

11. RED first: missing native header/interface for D.4, then scalar/vector
    widths 1, 2 and 4 recover every analytic coordinate from two colors. Pollute
    old seeds and repeat on the same ready graph; unused lanes remain zero.
12. Frozen analytic nonlinear cases cover zero-to-nonzero and negative parameter
    points, aliases, direct/intermediate/terminal/constant outputs, empty axes and
    fresh bindings with numeric plan reuse. Dense seed references do not generate
    the supports being tested.
13. Reject duplicate inputs, count/order changes, foreign/closed/premature scopes,
    unsupported mode, invalid slots and non-finite values. Binding after ordinary
    expression or solve-event construction rejects. Validation error then success,
    detached result ownership and concurrent independent scopes pass.
14. Traverse a real recorded linear-solve event with independent analytic inverse
    derivatives and a final partial color block. Compare complete matrices.
15. Run only new/affected local tests, strict source/header checks, format/complexity,
    installed consumption and docs. Prove all 177 existing objects/inputs and
    seven fresh accepted caller identities. Freeze a small complete native cost
    set after correctness; no old timing repeats without changed-path evidence.
16. Inspect exact-head CI/Codacy and full reviews, actual new-case runtime logs,
    repeated final merge gates and tested/merged tree equality before delivery.

## Open questions

None for native execution. Financial proof/identity and measured selection remain
open under P04; weighted gradients retain their direct one-VJP path.
