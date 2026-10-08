# Rate structural dependency provider review

Verdict: Approve local implementation; exact-head publication gates pending.

## Findings

No open local correctness findings after the following reproduced repairs:

- A fixing exactly at valuation must be captured even when the historical-request
  list is empty; its presence changes projection into known fixing consumption.
- An unregistered XCCY consumed root cannot become an available zero row.
- Enum C strings need explicit nullable string encoding, otherwise overload
  resolution can encode every non-null enum name as boolean true. Day-basis and
  interpolation mutations now invalidate same-sized layouts.
- An unsupported input must not hide later missing, duplicated or out-of-range
  represented coordinates.
- Failed curve parameter inspection must preserve unavailable proof for input
  and consumed/base graphs; allocation failures still propagate normally.

The additive bridge in `dal-cpp/dal/curve/ratecashflowpricing.cpp` reuses actual
family roots, XCCY routing and coupon geometry. Every existing function body is
byte-identical source text after removing the added bridge and two includes.
The opaque immutable payload in
`dal-cpp/dal/curve/ratestructuraljacobian_internal.hpp` owns only axis strings,
supports, reason and canonical bytes. No live handle or active object is retained.

## Tests

Sixteen new tests and six nearby existing pricing/closure/prepared-request tests
pass, 22/22. The independent pre-implementation analytic reference checks all
45 matrix entries against the existing joint dense native path at three points,
including supported zero-to-nonzero derivatives. All seven closed trade families,
four curve families, alias/axis/base mutations, fixing activity, unavailable
proof, empty axes, moved-from metadata and detached/concurrent captures are covered.

Eight OFF/combined strict source/header checks pass. The existing missing-field
initializer warning in the shared source is reproduced at the accepted baseline;
only that source uses the same category waiver as CI. New sources/tests use all
enabled warnings as errors. Formatting, complexity and installed consumption
pass; maximum new-function CCN is 8. Source hashes pin reused successful checks.

The [scoped cost report](../performance/aad-rate-structural-jacobian.md) records
two affected existing requests and two informational provider rows. No other
accepted timing set is repeated. CI includes the new suite in every existing
focused sanitizer/diagnostic filter; actual execution remains a publication gate.

## Remaining acceptance

Inspect every exact-head CI/Codacy page, complete review bodies and unresolved
threads, verify actual new cases in applicable runtime profiles, repeat the final
audits and compare the tested/merged tree before guarded merge. Local success
does not prove those remote gates.

P04 is not complete. These entry points expose dependency capture/identity and
numeric plans; complete compressed financial recording/execution, fresh native
binding validation, detached financial results and measured strategy selection
remain required follow-up work. No new default strategy or financial speedup is
claimed. Conditional support validity still belongs to the represented closed
pricing families and immutable request snapshots.
