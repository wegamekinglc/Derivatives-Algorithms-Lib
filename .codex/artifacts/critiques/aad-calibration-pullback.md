# F01 common calibration boundary critique

Verdict: Proceed with caveats.

Read the [specification](../specs/aad-calibration-pullback.md),
[API decision](../api-notes/aad-calibration-pullback.md), original F01 sections,
current inverse/joint methodology, provenance builders, quote aggregation,
Dupire APIs and nearby core/public/binding tests. The design addresses the
common operation and common result without changing the calibration map.

## Blocking issues

None remain in the revised design. Two necessary conditions must survive
implementation: the result has a shared representation, and curve identity can be
checked against retained complete source content. Typed overloads returning
unrelated existing results, or a wrapper containing only state hashes, would
not satisfy the original requirements.

## Significant concerns

1. Canonical curve records are case-sensitive UTF-8 bytes. DAL String_ has
   case-insensitive comparisons. Store/compare the record with std::string;
   test case-only changes. Reuse the existing JCS input to SHA-256 so capture
   neither changes existing fingerprints nor canonicalizes twice.
2. Default legacy provenance must not retain a large record. The proposed
   default-false capture flag is a storage preference, excluded from state
   identity. Inspect actual memory/cost and compare all four builders with
   frozen baseline binaries. Do not declare no regression from unchanged math.
3. A requested ANALYTIC mode can fall back to BUMPED, and some old result types
   do not retain the actual mode. The conservative curve method label avoids
   claiming an unknown mode. Keep the selected-solution mapping and all original
   recorded options/results. An EXACT solve does not prove that every retained
   pseudoinverse differentiates its nonlinear solution selection.
4. Sharing the aggregation transform must not add an extra output pass,
   metadata allocation or repeated source checks to its legacy hot path.
   An inline consumer helper can preserve the existing loop. Benchmark the
   resulting code; do not waive the unchanged two-round/ten-process/4% policy.
5. Joint native gradients require the existing consumed base graph and complete
   coupled inverse. Test actual legacy aggregation and independent recalibration,
   including layered graphs and signed portfolios. Independent single-curve
   results cannot stand in for a coupled joint result.
6. Dupire direct seeds intentionally permit equal quote definitions under
   different fixed bases. Preserve this through the shared provider. Curve direct
   identity is initially stricter and must be stated; display names omit raw
   quote values. Test calibration plus direct contributions to catch double adds.
7. Generic wrapping must not introduce a nested recording around Dupire. Curve
   mapping is passive and must preserve an unrelated active graph. Prove both
   behaviors, and recovery after provider errors, with actual numeric results.
8. Record capture requires reachability in Python config and each native Excel
   provenance factory. Preserve old exported test overloads and worksheet input
   prefixes. Keep the legacy generic-joint exclusion; do not expand that dispatcher
   incidentally. Compile generated registrations under actual MSVC/SDK.

## Minor notes

The source variant preserves original domain axes and avoids inventing new
label-derived coordinates. Curve M-by-one/N-by-one matrices make the common
numeric result explicit; examples must show the shape. Immutable tagged seed
implementations avoid near-duplicate classes. Copied binding projections and
serialization errors require their own tests rather than being inferred from
native const getters.

Capture can retain large numeric inverse text. Report its size and opt-in cost;
avoid automatic worksheet spills or eager JSON parsing on every pullback.
Guard dimension/count arithmetic before allocation. Full D04/F01/F02 request
planning, budgets and final three-language acceptance remain separate unfinished
requirements; this foundation is not full F01 completion.

## Counter-proposals and questions

Keep the existing inverse and source builders. Add opt-in capture before the
shared layer so numeric/identity changes can be reviewed independently.
No user decision is needed: the controlling plan determines the common result,
full identity, passive boundary and no-regression requirements. Revisit only
if executable evidence contradicts the proposed reuse or performance model.
