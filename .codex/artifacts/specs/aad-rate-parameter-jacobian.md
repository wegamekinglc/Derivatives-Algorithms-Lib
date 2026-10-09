# Complete rate parameter Jacobian execution specification

Active execution increment after merged [#509](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/509). Source: the original full AAD plan,
sections 5.4, C.9 and D.4; accepted numeric/native structural execution; the
owning financial dependency provider. This increment is explicit execution;
measured automatic policy remains required to complete P04.

## Problem and observable result

The provider can prove curve-coordinate supports and plan row colors, but cannot
yet calculate rate-trade PVs and a compressed complete financial Jacobian.
Existing joint-native batches provide a reference, including base coupling,
but independently sweep trade/component cells. The new operation must register
all requested physical coordinates once, record complete current pricing, and
return every requested derivative with stable positional metadata.

## Functional requirements

1. Both dense and compressed operations return an owning passive complete matrix
   of shape `(trades.size(), inputAxis.size())`, including empty dimensions.
   Single-row results do not collapse to vectors. Preserve row order, input
   coordinate order, price metadata and each trade's actual PV currency.
2. Coordinates are native curve free-parameter ordinals. Validate component
   availability, representability, ordinal range, physical alias duplication,
   finite current values, matrix dimension limits and byte-extent overflow.
   Missing or malformed inputs cannot become zero columns.
3. Support the same seven closed pricing families and four exact native curve
   representations as the provider. Derived/custom curves or incomplete/cyclic
   active closures return explicit errors; do not fabricate complete matrices.
4. Resolve current single-currency and XCCY pricing roots using shared routing.
   Include every transitive base and preserve curve sharing. Register selected
   free parameters as independent inputs; keep other parameters constant while
   allowing unselected curves to transmit selected base derivatives.
5. Use the existing pricing formulas, cashflow schedules, fixing resolution and
   collateral/tenor/basis interpretation. Reuse passive validation and prepared
   geometry. Failed trade pricing identifies the positional row and instrument
   before a complete result is published.
6. Dense execution uses full row supports and does not depend on cached sparse
   identity. Avoid unnecessary canonical structure construction on this path.
7. Compressed execution recaptures the current complete descriptor from the
   supplied market/trades and stored input axis, verifies availability and
   complete equality, and rejects mismatch before native binding or seeds.
8. Same-structure numeric curve changes re-record derivatives and may reuse the
   plan. Numerical zeros never remove support. Trade/fixing/geometry/axis/base/
   route/layout changes follow the accepted provider invalidation contract.
9. Register all selected independent slots, start recording, bind inputs before
   graph arithmetic, record current outputs once actively, finish recording,
   and use the accepted native structural executor. Do not bind after curve
   arithmetic or retain live values/tapes in returned metadata.
10. Scalar mode has width one; vector mode uses the supported finite native
    width. Set mode before scope entry and restore it on exit. Each independent
    sweep clears all relevant seeds and adjoints, including unused tail lanes.
11. Report actual reverse direction and sweep counts; zero-color requests call
    no reverse but still validate current finite prices and financial semantics.
    Repeated positional outputs and expired/constant rows retain their rows.
12. Check all passive/active PVs and returned derivatives for finiteness before
    publication. Throw on failure without leaking a partially assembled result.
13. Independent concurrent requests use thread-local scopes. Exceptions and
    nested-scope rejection leave the next valid independent request usable and
    preserve caller mode/state on rejected nesting.
14. Enforce the optional numeric payload budget for result plus color-direction
    matrices consistently with the numeric plan, including reused plans. It is
    not a tape/RSS or metadata-allocation cap. Validate before those allocations.
15. Existing pricing, node/quote-risk, weighted-gradient and default strategy
    behavior remains unchanged. No quote-coordinate mapping, FX spot delta,
    currency conversion, forward adapter or AUTO policy is introduced here.

## Executable acceptance

16. Test the delayed-payment, layered non-prefix three-trade/five-coordinate
    fixture against the previously frozen independent analytic reference: all
    45 derivatives and nine PVs at three points. Compare the existing dense
    joint-native reference, without sourcing supports from numeric derivatives.
17. Check dense/compressed equality across scalar and finite vector widths,
    repeated requests, reordered/subset axes, simultaneously selected bases and
    derived curves, and a zero derivative that becomes nonzero at a fresh point.
18. Exercise seven trade and four curve families with complete matrices and
    suitable centered-difference checks. For base risk, rebuild the passive
    dependent graph when bumping; changing only a registered base handle is an
    invalid finite-difference oracle. Record step stability and normalized units.
19. Reject stale plans and invalid inputs/fixings/routes/active graphs before
    compressed seeds. Cover empty dimensions, constant/expired/repeated rows,
    nonfinite result rejection, budget boundaries and cleanup followed by a
    successful request. Run affected existing pricing/closure cases, strict
    OFF/combined builds and an installed-only public consumer.
20. Freeze affected complete-request performance scope before timing: compressed
    and uncompressible portfolios, cold and reused plans at fresh numeric points,
    equivalent dense outputs. Include validation, preparation, identity, binding,
    recording, all sweeps, recovery, storage and release. Only the two existing
    passive/joint-native paths affected by a changed shared object are repeated;
    reuse unchanged MC/PDE/solve/other portfolio evidence. Preserve calibrated
    two-round best-of-ten sampling and sustained regression thresholds.

Publish only after exact-head CI/Codacy and complete review-body/thread inspection,
actual focused cases in applicable runtime configurations, and guarded tested/
merged-tree equality. Automatic strategy selection follows in another PR and
must compare full cold/reuse costs rather than dimensions or sweep counts alone.

No blocking user choice remains. Proposed interfaces and design caveats are in
the neighboring API and critiques.
