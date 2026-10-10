# Excel Dupire curvature implementation review

Verdict: Comment Only; local correctness is green, publication gates pending.

## Findings

No open local correctness finding. The common settings NUL-key error was exposed
by a focused RED case and repaired before accepting the eight request cases.
Legacy calibration headers are consumed through the project include path; no
legacy template or shared numerical implementation is changed.

## Reviewed contract

The 25 generated worksheet functions preserve typed ownership, distinct input
and output locals, strict numeric/settings admission and full raw quote order.
Empty directions keep zero-by-Q dimensions without inventing a zero row. Query
copies remain passive. The returned existing quote plan supplies complete axis
metadata without duplicating its formatter. Selected/report-scaled first-order
projections do not alter raw gradients or products. Native planning/execution
retain their point validation, finite-step method, history and worker semantics.

The common recording cap remains reusable; this Dupire adapter rejects every
provided cap. Combined numeric and first-order budgets remain separate, and
worksheet ownership/copy costs are explicitly excluded. Output assignment is
atomic on constructor, planning and worker failure. Archive serialization fails
explicitly for all new handles.

## Tests

Eight common request tests and twelve Dupire tests pass against the accepted
installed public/core libraries. Evidence includes signed analytic Gamma in
tree/compiled engines, separately recalibrated mixed-payoff gradients, selected
reports, empty directions, exact 720/719 and 304/303 budget boundaries,
pre-work malformed admission, injected base/plus/minus failures, deterministic
recovery and preserved caller graph/seeds. OFF/combined strict checks cover the
changed portable translation units and the financial boundary bridge.

Windows tests call all 25 exports, existing complete quote metadata and raw
integer/bool/text/error/nonfinite/NUL/blank/zero-budget cases. They have been
authored but are not yet accepted as runtime passes. Registration checks require
the complete name, argument types/order, nonvolatile flag and help contract.

## Open questions and residual risk

None in the API contract. Real Windows execution, generated-file drift,
paired scoped cost observations, Codacy, full external review and both final
exact-head audits remain publication gates. Do not merge from this local review.
