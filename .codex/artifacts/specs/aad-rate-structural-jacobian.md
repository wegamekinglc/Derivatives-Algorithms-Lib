# P04 financial structural dependency and identity

Status: active provider/identity implementation after accepted #508, merged as
`5f41297f550aa327a2eb04a43c9d8342f5e06461`.
Source: immutable full plan `de5dd8b20089223e6938dfd8100d463aa6d4a169`,
sections 5.4/C.9/D.4; source anchors in the provider preparation note.

## Problem and delivery boundary

Numeric colors and native slot identity cannot prove that a financial matrix
has the supplied supports. Establish conservative curve-coordinate dependencies
from the closed rate-pricing family registry and actual resolved routing/base
closure. Own complete structural identity so reuse can be validated before
compressed seeds. Keep default pricing and weighted one-VJP requests unchanged.

Deliver the provider/identity boundary first. Explicit complete financial
execution and measured strategy selection follow if combining them would make
this PR unreviewable. Do not mark P04 complete until all original requirements,
financial equivalence, invalidation and measured strategy acceptance pass.

## Inputs and outputs

Inputs are ordered rate trades, the current pricing market, and an explicitly
ordered curve-parameter axis. Each coordinate names a stable market component
key and a free-parameter ordinal verified against DescribeCurveFreeParameters.
The caller may interleave parameter blocks; supports use actual column positions.
Missing/out-of-range coordinates and duplicate physical independent coordinates
are invalid requests, not structural zeros. Referenced alias keys may resolve
to the same curve while the input axis represents each independent slot once.

An owning immutable plan exposes availability/reason, ordered axis metadata,
conservative row supports and the numeric color/recovery plan when available.
No Number, tape, raw curve pointer, convention pointer or mutable market handle
survives capture. A structural comparison validates a fresh capture against the
stored complete canonical records; a hash is optional addressing only. Keep
fresh structural capture separate from numeric plan construction: equality
validation must not needlessly recolor/build a second complete plan.

## Numbered requirements

1. Resolve consumed roots using the same closed family and route interpretation
   as actual pricing. Share the single-currency key enumeration and XCCY
   collateral/tenor/basis resolution; do not duplicate an independent family
   switch that can drift from the pricing kernel.
2. Verify complete exact-family base closure. Include every requested parameter
   of each consumed curve and reachable represented base. Initially use full
   curve blocks, not nominal maturity/prefix guesses or sampled derivative zeros.
3. Distinguish unavailable proof from a proven empty support. Unknown or derived
   custom curve classes, unresolved XCCY routes, missing consumed handles,
   cyclic/incomplete base graphs or unrepresentable parameters cannot produce
   a sparse promise. Preserve a reason and require the existing dense/unsupported
   behavior before any compressed seeding.
4. Canonicalize the graph by stable logical records and edges, not retained
   addresses. Validate aliases while live handles are owned. Physical equality
   during capture can resolve aliases; pointer equality alone cannot validate
   reuse at a later numerical point.
5. Own and compare complete ordered input/output identities, exact curve family,
   parameterization, interpolation, anchor/day basis, free parameter dates and
   components, base topology, resolved routes, trade family/terms and observation
   structure. Include relevant calibration policies for a calibrated-residual
   provider; this first trade-PV provider must not claim that capability.
6. Capture actual prepared payment/accrual/fixing geometry, fixing availability
   and valuation activity. Reuse the same prepared geometry for execution, or
   validate a fresh identical snapshot before recording. Calendar names alone
   do not establish equal generated geometry.
7. Numeric branch pruning is prohibited. Use a proven union of every curve root
   a supported pricing family can access, including a forecast whose current
   contribution vanishes. If this union is unavailable, use dense behavior.
8. Reuse only after complete equality. Axis order, output order, base/route,
   payment/fixing/calendar, observation/activity and parameterization mutations
   invalidate or rebuild before seeding. Curve numerical values may change under
   identical proven structure; re-record with fresh native inputs/bindings.
9. Keep plans caller-owned and bounded; no implicit global cache. Concurrent const
   metadata use is allowed, with independent request/scopes and immutable market
   snapshots. Plan/result ownership survives source handle cleanup.
10. Support metadata and native binding remain distinct. A matching descriptor
    does not revive a token from another scope, and a valid live token does not
    prove financial dependence. Never publish a partially recovered matrix.
11. Existing pricing/default strategies, rowWidths extraction semantics,
    weighted risk, quote units and binding behavior remain unchanged. Do not
    report forward capability until an actual supported tangent adapter exists.

## Executable acceptance

12. RED first for the missing provider. A small interleaved-axis market contains
    a delayed-payment IRS with a layered forecast, an unrelated deposit and an
    end-settled FRA with shared base/discount dependence. Expected full supports
    are derived from pricing roots/base closure, independently of derivatives.
13. Compare every PV and matrix entry with independent analytic flat-curve
    formulas and existing dense native calculations. Include a point where a
    supported discount derivative is zero and a fresh point where it is nonzero.
14. Mutate each ordered axis, output, consumed route, base edge, parameterization,
    calendar/payment/fixing and valuation activity independently. Reuse must
    invalidate/rebuild or use dense before any compressed execution.
15. Cover routed XCCY components/basis, alias keys, unregistered consumed curves,
    custom exact-type rejection, failed route, unsupported/ambiguous axis and
    cyclic/incomplete graph. Unavailable empty routing must not yield zero rows.
16. Test owning copies, detached metadata/results and independent concurrent
    requests. No retained raw pointers or active objects cross cleanup.
17. Run only the new/affected local suites and meaningful nearby pricing/closure
    cases, strict/format/docs/installed checks. If a shared pricing source changes,
    select only request paths affected by that source and preserve other evidence.
18. Freeze at most four new full-request cost rows after correctness: compressed
    and dense/unavailable-proof handling, cold and reused-plan fresh numerical
    point. Include dependency capture/equality, binding, recording, all sweeps,
    recovery/storage and release. Measure any altered existing path separately
    in the smallest representative selection; no full matrix reruns by habit.
19. Inspect exact-head CI/Codacy and full review bodies/threads, actual new cases
    in applicable runtime profiles and tested/merged tree equality before merge.

## Remaining design checks

Choose the smallest bridge that exposes actual prepared routing/geometry from
the existing pricing implementation without changing existing hot behavior.
Keep complete canonical records readily inspectable and factor local comparison
helpers rather than adding a parallel general serialization framework.

## Increment acceptance boundary

This PR implements requirements 1-9 for dependency capture and immutable identity,
plus the metadata/numeric-plan separation in requirement 10. The independent
analytic fixture validates all 45 entries against the existing joint dense
native path and verifies that conservative supports survive a zero derivative.
Complete compressed financial execution, detached financial result ownership,
execution-side invalidation and full-request compressed/dense strategy costs in
requirements 10, 13, 14, 16 and 18 remain P04 follow-up acceptance. Provider cost
rows measure capture/plan/equality only and must not be labeled financial
Jacobian execution or used to select a default strategy.
