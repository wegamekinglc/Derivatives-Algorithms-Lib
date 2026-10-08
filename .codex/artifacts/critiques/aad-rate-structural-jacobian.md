# Financial structural provider critique

Verdict: Proceed with caveats. Implement the provider/identity boundary locally
under RED/GREEN; complete all canonical records and invalidation cases before publication.

Key correctness threats are incomplete transitive base closure, treating failed
XCCY routing as empty dependence, axis aliases, reusing changed cashflow/fixing
geometry, and letting numeric branch outcomes prune a supported component.
All must reject/rebuild or conservatively include before seeding.

The existing anonymous pricing helpers and request-local geometry establish the
semantic source of truth. A new independent family switch would simplify linking
but duplicate that truth. Prefer an additive cold capture bridge and keep old
request behavior untouched; if its containing object changes, verify the affected
pricing costs instead of asserting all old object identities are unchanged.

The stored/current descriptor comparison must use full canonical equality. An
axis fingerprint or existing quote-state fingerprint does not automatically
serve this purpose: the latter can encode numeric values, while structural reuse
allows new numeric points under a proved conservative union.

The additive cold bridge now reuses exact-family closure and request-local
geometry. Canonical records live in a private immutable payload, with graph
ordinals assigned by ordered axis/root traversal rather than pointer-map order.
No live handles or active objects survive capture. The independent analytic
fixture was frozen before implementation; all 45 entries agree with the existing
joint native reference, including a zero-to-nonzero supported coordinate.

RED cases exposed two subtle boundaries: supplied fixing at valuation with an
empty historical request list, and an unregistered XCCY consumed root producing
an apparently available empty row. Both now pass after production fixes. Keep
provider proof separate from primal pricing success: missing historical values
can preserve a conservative support proof while later execution rejects pricing.

Local review additionally reproduced and repaired later malformed coordinates
hidden by an unsupported input and parameter-inspection failure escaping the
unavailable-proof boundary. Explicit C-string encoding distinguishes enum names
and unset fields; day-basis/interpolation mutations are tested.

Publication still requires final-source cost/installed checks and complete PR
review. P04 remains open for complete compressed financial execution,
fresh binding validation and measured strategy selection.
