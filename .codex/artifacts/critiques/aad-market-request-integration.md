# F01 market-request audit critique

Verdict: Proceed with caveats for the audit; write the concrete API/spec before
implementing the remaining request behavior.

Read the [integration audit](../plans/aad-market-request-integration.md),
D04 request/result spec/API, accepted Dupire/common contracts, public
`riskvalue.cpp`/`dupirerisk.cpp`, scalar preflight-failure tests and complete-chain
tests. The audit correctly preserves the open F01 scope and current-PR merge
boundary rather than treating common wrappers as complete request integration.

## Blocking issues for the eventual implementation contract

Resolve these before writing the behavior tests; no user decision is required.

1. Define one authoritative owner/meaning for the quote request and its stable
   IDs. Do not send quote IDs into core `PlanScalarRiskRequest`, which accepts
   only model/constant IDs. Keep public calibration ownership above the core
   dependency boundary. Avoid several overlapping wrappers representing the
   same calibration contributions without a distinct numerical role.
2. Specify exactly which matrices the result retains. Selection alone cannot
   reduce the payload estimate when the common provider's three full matrices
   remain owned underneath it. Choose whether reporting projects existing data
   or stores another matrix, and test the resulting exact budget boundary.
   Preserve D04's numeric-result budget scope; tape/workspace caps belong to F02.
3. Required propagation inputs and visible returned inputs have different roles.
   Decide whether required surface inputs are explicitly added to the returned
   valuation axis or retained internally. A caller's selected axis/order cannot
   change silently; expose the planned behavior and account for all retained
   numbers. Reuse the accepted complete source/axis checks after execution.
4. Plans cannot rely on caller-mutatable shapes/settings after validation. State
   how model/product/source choices are sealed before history observers, released
   GIL or task submission. If existing handles do not guarantee immutability,
   own a validated passive copy or reject unsupported input before work. A later
   mismatch rejection is necessary but does not prove the preflight budget held.

## Significant concerns

- Keep model valuation provenance `calibration=fixed` for its parameter partials.
  The combined quote result records the subsequent calibration mapping; rewriting
  the parameter result to differentiated calibration would break the accepted
  extractor contract and obscure whether a direct term was already included.
- Use actual PV currency groups for curves. A native parameter vector alone has
  no authoritative currency: do not infer one from quote display names or attach
  currency metadata without an explicit typed source supplied by the caller.
- Report factors are finite positive projections, with raw values unchanged.
  Keep the derivative of the existing scalar AAD estimator when selection is
  empty; do not use passive pricing merely to avoid recording or budget costs.
- Typed external direct seeds are already useful and checked. Automatic script
  bindings require a separate exact ordinal/value/uniqueness contract and an
  independent fixed-surface direct derivative reference. Same labels or numeric
  values cannot infer a dependency, and a total gradient cannot be added twice.
- The first Dupire result holds deterministic calibration inputs fixed. Any
  prospective model-carry restriction must be stated and tested on the opt-in
  request; do not retroactively change accepted legacy conditional mappings or
  advertise stochastic-rate joint-calibration correctness.
- Raw curves preserve the retained tolerance-scaled effective inverse and its
  selected-solution semantics. This request increment adds no transposed solve,
  recalibration or stronger exactness claim; those have later operator contracts.

## Minor notes and counter-proposal

Deliver one inspectable passive planner and scalar execution path first, then
strict Python/Excel parity. Share native validation and reuse existing numeric
providers. Keep old entries out of new planning/allocation so their unchanged
performance gates remain meaningful. Measure the new request separately.

The audit's acceptance matrix is appropriate, provided new tests distinguish
planning failure before history/workers from post-run extraction failure.
Retain the existing fixed-path, bump-step and tolerance protocols; source parity
and wrapper-copy checks do not replace an independent derivative oracle.
