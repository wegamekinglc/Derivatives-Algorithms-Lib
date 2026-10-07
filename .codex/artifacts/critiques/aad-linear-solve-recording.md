# F03 recorded solve critique

Verdict: Proceed with caveats.

Read the [specification](../specs/aad-linear-solve-recording.md),
[API decisions](../api-notes/aad-linear-solve-recording.md), accepted numeric
operator, Tape/Number/BlockList/RecordingScope/native-capability code and nearby
lifecycle/native tests. This critique controls implementation, not acceptance.

## Blocking issues

None in the specified behavior. The following gates remain mandatory before
acceptance; a scalar analytic prototype alone does not satisfy them.

## Significant concerns

1. Leaf output slots retain adjoints in ordinary propagation. Events must consume
   every output/channel themselves while preserving independent input sums.
   Interval inclusion at an event-end checkpoint must be tested both ways.
2. Block-end End() and a newly canonicalized mark can represent the same ordinal
   with different iterators. Canonicalize event boundaries before storing them;
   exact block-limit tests must catch an unbounded reverse iterator walk.
3. OFF Number has only value/node and cannot detect stale address reuse. Promise
   checked event-owned lifetimes, and retain the existing caller diagnostics
   distinction instead of inventing a guarantee or adding per-node metadata.
4. TapeCapacityBudget currently accounts for four scalar block lists. Admission
   must include event/cache capacities and destruction/replacement accounting.
   The spec explicitly forbids accepting a silent bypass.
5. A failure in a later channel/event can leave earlier contributions present.
   Graph invalidation must prevent documented reads/reuse until reset; cache
   ownership alone is insufficient failure recovery.
6. Uniformly deleting unsafe Tape copy/move changes OFF compile-time behavior.
   Document that graph ownership restriction, inspect all repository call sites
   and installed consumers, and preserve supported Number/scope operations.
7. Empty-event dispatch, new adjoint-read guards and reset paths can affect short
   requests even with unchanged scalar loops. Fresh affected timing is required;
   prior hash-identity evidence cannot close this increment's performance gate.

## Minor notes

Keep numeric pivot/range policies identical to the accepted operator. Future
residual/condition diagnostics and symmetric/PDE/calibration semantics stay
explicitly open. Do not label the scalar reference study a general speedup.

## Counter-proposals and questions

Prefer lazy out-of-line event machinery and explicit scalar interval boundaries
to node tags or a global registry. A passive-A overload should expose only the
required transpose/RHS contribution. No user clarification is needed.
