# P04 owning structural Jacobian plan

Status: active first P04 implementation after accepted #506, merged as
`bef4d87a752da7d0459eeece44f8c8799480539e`.
Source: original detailed plan at `de5dd8b20089223e6938dfd8100d463aa6d4a169`,
sections 5.4, C.9 and D.4. This increment does not complete P04.

## Problem, goals and exclusions

Existing `HarvestCurveJacobian` performs a scalar reverse per residual;
`rowWidths` masks extraction and does not establish structural dependency.
Introduce a separate owning numeric plan for conservative row supports,
deterministic coloring and exact full-matrix recovery. Establish mathematical
and resource acceptance before adding native seeds or a financial provider.

The plan validates the descriptor, not its mathematical truth. Supports must
contain every potentially nonzero dependency; an unknown support is conservatively
full, not empty. Current numerical zeros and nominal maturity are not proof.
This increment does not change tape kernels/layouts, dense calibration defaults,
weighted VJPs, bindings, cache identity, mode selection or higher-order capability.

## Functional requirements

1. Accept the complete input extent and one conservative support vector per
   output row. Snapshot and normalize each support by sorting and deduplicating
   indices. Original row and column order remains the result axis order.
2. Both extents must fit nonnegative Matrix int dimensions. Reject invalid
   columns and checked numeric byte overflow before publishing a plan. Test
   maximum/one-over-limit inputs without attempting massive allocation.
3. Empty input/output axes are valid. Empty supports have no color, consume no
   direction and produce exact zero rows. Preserve result shapes such as 3-by-0
   and 0-by-4. A 0-by-INT_MAX plan with zero numeric budget remains lightweight.
4. Color nonempty rows in declared order using the smallest available color.
   Rows sharing any support column conflict; same-color rows have disjoint
   supports. Do not claim optimal coloring or allocate a rows-squared conflict
   matrix. Store incidence only for used columns, not the full declared axis.
5. Expose immutable input/output counts, normalized row supports, optional row
   colors, color members and color count. Reject invalid getter indices with an
   operation/constraint-specific DAL exception. Caller descriptors may change or
   disappear without changing the plan; concurrent const use is supported.
6. Accept a color-by-input matrix of complete direction gradients. Validate
   both dimensions and every supplied value's finiteness before reconstruction.
   Non-finite values outside a row support are rejected, not hidden by zero masks.
7. Recover each supported coordinate from its row color; set all other result
   coordinates to exact zero. Return a complete owning output-by-input matrix,
   without mutating plan or gradients. It survives destruction/mutation of source
   descriptors and direction data. Failure leaves later requests usable.

## Resource and compatibility requirements

8. Expose result bytes `8 * outputs * inputs`, direction bytes
   `8 * colors * inputs` and their checked sum. The optional numeric payload
   budget covers both simultaneously resident numeric matrices. It excludes plan
   metadata, allocator overhead, tape state and unrelated caller memory.
9. Check the result lower-bound budget before copying/constructing a potentially
   large plan; enforce the final sum once color count is known. Zero is an actual
   budget. Test exact equality and one-byte-short rejection, including empty
   payloads. Do not name this estimate process or total peak memory.
10. Preserve all existing production callers and AAD behavior. New code stays
    in its own cold object/header. Reuse prior performance evidence only after
    proving production inputs, existing archive members and old caller identity.
    Two constructor-plus-recovery cost rows are informational new coverage.

## Frozen independent acceptance

11. Original D.4 supports `{0,1}`, `{2}`, `{1,3}` yield row colors `0,0,1`.
    Recover all entries of rows `{2,3,0,0}`, `{0,0,4,0}`, `{0,5,0,6}` from
    directions `{2,3,4,0}`, `{0,5,0,6}`. Result/direction/combined bytes are
    96/64/160. Compare complete ordered matrices, not selected nonzero cells.
12. Cover all 512 three-by-three binary support patterns with independently
    generated dense linear coefficients and direct seed summation. Validate
    disjointness and full recovery for dense, disconnected and non-prefix shapes.
    Supported numerical zeros must remain in the pattern when they become
    nonzero at another parameter point; test a product at the origin and away.
13. Add focused tests for empty extents/rows, unsorted duplicates, snapshot and
    result ownership, const concurrency, malformed shapes/indices, getter bounds,
    non-finite gradients, overflow, exact budgets and failure then success.
    Keep exact integer-valued references exact; use no fitted tolerances.
14. Follow RED/GREEN: capture one missing-interface failure, implement its minimum
    behavior, then add boundary/error cases while green. Run new/affected cases
    only, strict warning/format/complexity checks, installed consumption and
    documentation checks. Inspect exact-head CI/Codacy/full review bodies and
    actual runtime logs; merge only after repeated final audits and tree equality.

## Subsequent required P04 work

The next native adapter clears all previous seeds/adjoints per sweep, supports
scalar and bounded vector colors, validates distinct registered independent
slots and current output bindings, and adds alias output seeds. It must not grow
a live graph's adjoint width or preserve Number handles beyond scope cleanup.

A financial provider must establish conservative dependencies from actual
observations, delayed payments and resolved projection/discount/curve routing,
including non-prefix dependencies. Compare exact fresh structure descriptors
covering axes, activity, parameterization/calibration policy and relevant branches;
rebuild or use dense fallback on mismatch. Numeric zero cannot establish support.

Finally measure cold analysis and reused complete requests before strategy
selection. Include recording/re-recording, coloring, width, reconstruction and
result storage. Keep weighted risk on one VJP. No generic forward strategy is
available until an actual adapter is independently validated. These boundaries
remain necessary before marking P04 complete.

## Open questions

None for this numeric boundary. Native binding and financial structure identity
contracts belong to their subsequent controlling specifications.
