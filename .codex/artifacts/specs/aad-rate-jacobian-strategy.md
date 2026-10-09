# Native rate Jacobian strategy acceptance

Status: active. This increment follows accepted execution PR #510 at
`d18ebe4e70e7c2cb3a42cbcdc32341ac1b18e270`.

## Problem and scope

The accepted three-output workload is faster with dense reverse execution than
with compressed execution, even when the structural plan is reused. A decision
based only on the direction count would choose the slower implementation.
The dense reference also constructs a general conflict coloring for a known
fully connected matrix; this unnecessary work must not manufacture an apparent
compression advantage at larger output counts.

This increment supplies a direct dense numeric planner, measures a small set of
complete financial requests, and establishes the supported strategy decision.
The existing dense default remains the starting strategy. A native forward
adapter is a separate capability: it must be verified before being selectable.

## Requirements

1. `AAD::PlanDenseJacobian(inputs, outputs, settings)` returns exactly the same
   row supports, row colors, color groups, byte extents and recovery semantics
   as a general fully connected structural plan. Each nonempty output has its
   own direction; an empty input axis has no directions.
2. The dense planner checks dimension limits, overflow and the combined result
   and direction payload budget before allocating support or coloring metadata.
   Construction performs O(inputs * outputs + outputs) metadata work without
   a conflict graph. Zero axes preserve their declared matrix shape.
3. Complete rate dense execution uses that planner. Existing financial formulas,
   input coordinates, output order, currencies, numeric budget meaning, recording
   lifecycle and scalar/vector adjoint behavior retain their accepted contracts.
4. Compare complete dense, compressed cold and compressed reused requests on
   the same numeric point. Include validation, curve preparation, recording,
   reverse, recovery, owning result creation and destruction. Reused-plan
   preparation remains outside the timed request and is reported separately.
5. Select three financial shapes: the accepted small layered 3-by-5 case;
   64 deposits over eight independent flat curves; and 64 deposits on two shared
   curves. Measure scalar execution and one width-four representative for the
   larger independent shape. Do not run a parameter Cartesian product.
6. Validate every price and matrix entry against the frozen layered reference
   or an independent deposit formula before measuring. For deposits, explicitly
   derive the curve-relative date fractions and payment factor used in pricing.
   Compare scalar and vector results and direction/sweep counts.
7. Strategy advice uses full cost, cold versus reuse and observed benefit, never
   numerical zeros or axis lengths alone. Cache identity must still be checked
   before compressed execution; unverifiable metadata cannot seed a reverse.
   No global automatic cutoff is accepted from these three observations.
8. Inspect the native active type and relevant pricing operators for executable
   forward support. If no valid adapter exists, report reverse-only capability;
   neither an external backend nor a theoretical forward count is executable.
9. An explicit cached-plan overload of `RateTradeParameterJacobian` accepts the
   current requested input axis and a stored plan. It captures current structure
   once, before seeding. If the proof is available and matches, reuse numeric
   coloring on a freshly recorded graph; otherwise execute the current request
   densely. Never reuse the stored output/input mapping after a mismatch. Price,
   input or numeric-budget errors still fail the request rather than publishing
   a partial result. The strict compressed entry point retains stale rejection.
10. Dense overflow and budget failures identify `PlanDenseJacobian`; structural
    planning retains `PlanStructuralJacobian`. A rate dense request additionally
    preserves `RateJacobian` context, including a cached-plan dense fallback.
    Only planner DAL exceptions are wrapped at that financial boundary; input,
    pricing and reverse failures retain their existing behavior.

## Acceptance

- A focused RED test references the missing dense planner. GREEN compares its
  full metadata and recovered matrices with generic planning at empty, scalar
  and rectangular boundaries, exact budgets and excessive extents.
- Rerun affected numeric planning and complete rate execution tests, strict
  changed-unit compilation and an installed-only consumer. Reuse unchanged
  provider/native execution evidence with source and archive identity proof.
- Freeze accepted #510 and finished branch archives. Run two alternating rounds
  of ten samples per side, keep raw observations and apply the existing sustained
  +4% gate to affected comparable requests. Measure only the declared shapes.
- Inspect complete Codacy/review bodies and all threads; require final exact-head
  CI and actual execution logs before a guarded merge.

## Selection decision

The declared lightweight workloads all lose with compression, even after plan
reuse. Keep the ordinary dense default and avoid a timer or universal cutoff.
Native `Number_` records scalar values and reverse derivatives; no executable
forward adapter exists for the closed financial path. Forward is unavailable.

An explicit cached-plan overload is useful for a different reason: safe dense
fallback closes the required structure-invalidation behavior. It is a caller's
opt-in strategy, not a claim of automatic speedup. Measure its matching and stale
paths on the accepted layered case after correctness; preserve the first study.
