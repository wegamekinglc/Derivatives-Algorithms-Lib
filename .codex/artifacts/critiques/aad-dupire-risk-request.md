# Automatic Dupire request critique

Verdict: Proceed with caveats.

Read the full [spec](../specs/aad-dupire-risk-request.md),
[API decision](../api-notes/aad-dupire-risk-request.md), common request contract,
scalar D04 source/tests, accepted Hybrid extractor/source/oracles, native Hybrid
component ordering/numeraire validation and immutable fixing settings.

## Blocking issues

None for the proposed staged planner/execution implementation. F01 and the
current PR remain open until execution/binding/oracle/performance acceptance.

## Significant concerns

- Model handles are const shared pointers to objects with publicly mutable
  backing fields. Test deep sealing, including nested surface/correlation data;
  copying handles or verifying only after execution does not satisfy preflight.
- Reject custom dynamic archive implementations before serializing. The explicit
  known flat-rate component set supports the required first case without claiming
  arbitrary Hybrid graph immutability. Legacy extraction must keep its scope.
  The implementation also checks the exact nested surface type. A genuine RED
  shows a custom surface archive callback running before that guard; GREEN
  rejects before the callback, with zero writes.
- Require full surface columns regardless of quote subset/empty selection and
  include those valuation numbers in the combined payload. Result storage must
  not accidentally retain another external direct seed matrix through the plan.
- Same values or names do not prove a structural direct dependency. Only explicit
  ordinal/quote bindings declare it; validate both-side uniqueness and native
  values, then compare against fixed-surface independent derivatives.
- The native scalar risk is already mean-normalized. Do not average again when
  extracting constant/surface seeds. Keep `calibration=fixed` in that result and
  use the combined method to describe the following VJP, including policy secants.
- Planning freezes the evaluation date but intentionally does not read global
  history. Preserve the documented execution-time global snapshot semantics and
  use real fixing/compilation/submission observers for preflight-error coverage.
- Refactoring shared axis/layout helpers can change legacy binaries even if
  arithmetic is identical. Freeze affected old objects first and retain actual
  paired old-entry measurements; new-cost measurements are not that gate.

## Minor notes and counter-proposals

Deliver missing-API RED and inspectable passive plan tests first; add execution
only when mapping/sealing/budget decisions pass. Reuse the common quote result
for all projections and source ownership. Use checked scalar/quote payload
helpers plus checked addition. Keep arbitrary workspace caps and multi-output
choices with F02 rather than introducing an unfinished generic planner framework.

## Author questions

No user decision required. Concrete Python/Excel names and settings factories
are a later local API/critique stage, while native behavior and ownership remain
shared. Keep all original oracle steps/tolerances and failure evidence.
