# Native coordinate accuracy local review

Verdict: Approve for local implementation scope. Remote completion gates remain open.

## Findings

No remaining local finding. Reviewed the full production diff, new public header,
four new test files, shared event helper, controlling specification/API/critique,
current-state methodology and changelog. Numeric coordinate/cache algorithms
retain the accepted #499 bytes. Unchecked native calls retain their semantics;
their shared point contribution helper receives an inline hint, measured below.

The checked payload template preserves dense binding types and field order;
packed instantiation owns O(p) Number bindings around one checked numeric cache.
It contracts symmetric pairs and zero-valued coordinates directly. Passive
parameters skip unused contraction/overflow. All inputs are validated before
publication; source-container mutation does not affect retained values/bindings.

Both payload families use the same TLS collector, publication path and opaque
event/invocation identities. Report allocation precedes event scratch, so caller
ceilings overlap correctly. Failure invalidates adjoint reads and exposes no
partial new collection; detached historical reports remain readable.

## Tests and quality

- The first repository harness confirms missing-header RED, then one analytic
  GREEN before importing edges. Eighteen new and twenty-five affected dense
  tests pass (43 distinct). No numerical assertion or resource ceiling is weakened.
- Independent native Cramer and three-step differences cover symmetric and
  nonsymmetric pivoting systems, every packed/RHS gradient and zero RHS seeds.
- Scalar/vector 1/4/8 channels, all three activities, aliases, ordinary producers/
  consumers, repeated reverse, mixed event windows/restoration and ownership pass.
- Exact inclusive 2^-55 error, below-limit rejection, nonfinite capture/seeds,
  large finite risks, omitted/paired overflow and failure/recovery pass.
- Resource cases prove four packed Number bindings rather than sixteen dense
  bindings, 176-byte full and 144-byte RHS-only reverse scratch, bounded channel
  scratch, exact report/caller peaks and one-byte-short refunds.
- Repair retests the analytic case plus five accumulation/alias/channel/overflow
  boundaries. Eight affected ordinary/packed helper boundaries also pass, giving
  51 distinct local cases across initial and repair acceptance, not a full suite.
- Both affected production units compile strictly in OFF and combined lifetime/
  profiling ON. The direct header was independently checked in both profiles.
  Maximum production CCN is 4, new tests 6; clang-format validation passes.
- The repaired archive is freshly installed; find_package(dal-cpp)/DAL::cpp
  consumer passes 1/1, including detached report access after recording close.
- [Scoped performance](../performance/aad-native-coordinate-solve-accuracy.md)
  retains and fixes the initial sustained dense cached regression. All eighteen
  final actual helper-caller rows pass; four new optional costs are separate.

## Open questions and merge readiness

No local API question remains. Required remaining gates are exact published-head
CI/Codacy/review and actual logs proving all eighteen new cases in each of fourteen
sanitizer, extended and MSVC configurations. Repeat full paginated audits and
guard the merge with that head SHA. This delivers native coordinate accuracy;
implicit-root/PDE behavior and bindings remain later increments.
