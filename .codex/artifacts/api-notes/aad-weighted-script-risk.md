# Weighted script risk API decisions

This active note controls the first F02 PR after merged #480. The
[specification](../specs/aad-weighted-script-risk.md) controls the complete
weighted delivery; the first commit supplies its native root, not a valuation API.

## Native root increment

`AAD::WeightedPayoffRoot(const Vector_<Number_>& outputs,
const Vector_<double>& weights)` returns an active scalar objective inside the
caller's recording. Both sequences are required, nonempty and equally sized.
Weights are passive and finite. Every component is validated even at weight
zero. The expression preserves caller order and always records a suffix-local
root, including when all components are prefix nodes or registered inputs.

Use ordinary expression edges to accumulate aliases; never assign component
adjoints individually. Seed only the returned root and use the existing suffix
and prefix reverse lifecycle. No global adjoint mode, allocator or tape layout
changes are needed. The existing `PayoffRoot` and scalar driver remain unchanged.
The helper is synchronous and does not retain caller containers. Its active
return value must stay inside the live recording; it is not a passive result.

Example within a live recording:

```cpp
const Vector_<AAD::Number_> outputs = {x * y, x + y, AAD::Number_(5.0)};
AAD::Number_ root = AAD::WeightedPayoffRoot(outputs, {2.0, -1.0, 0.5});
recording.FinishRecording();
AAD::Adjoint(root) = 1.0;
recording.ReverseSuffix(checkpoint);
recording.ReversePrefix(checkpoint);
```

Empty/mismatched dimensions, nonfinite weights/components and weighted overflow
raise `Exception_`. Component failures identify their ordinal. Request-level
selection/budget errors later use `ScriptError_` with stable output IDs.

## Planned owning boundary

The preflight increment fixes these native names in
`dal/script/weightedrisk.hpp`:

- `RiskOutputCoordinate_` stores canonical `id_`, actual scalar label and slot.
  `ScriptRiskOutputAxis(indexedProduct)` lists scalar slots; the receiver is
  `payoff`, other slots are `output:<ordinal>`. Vector storage is not an output.
- `WeightedRiskRequest_` composes `RiskRequest_ selection_` with optional passive
  `weights_`. Composition avoids implicit conversion to a scalar request that
  would silently drop weights; Python/Excel options can remain flat.
- `PlanWeightedRiskRequest(indexedProduct, completeInputAxis, evaluationDate,
  request, enableAad)` returns an owning read-only `WeightedRiskPlan_` with ordered
  selected/complete output axes, normalized weights, complete input axis,
  canonical input request, date, method flag and exact numeric payload bytes.
  Date/method are required planning inputs; this is preflight, not an owning
  prepared product/model execution boundary.
- `WeightedRiskResultPayloadBytes(components, inputs)` checks
  `sizeof(double) * (1+inputs+2*components)` before allocation. Reuse
  `PlanScalarRiskRequest` for input/report validation with output/budget controls
  temporarily cleared, then enforce the larger weighted payload.
- `ValidateWeightedRiskPreparedAxes` rechecks selected/full slot identity and
  input values/order after preparation, with no caller-owned mutable buffers.

Reject missing receiver, unindexed products, exercise, invalid valuation dates
and fully expired event calendars before preparation. A product may use vector
operations internally; only indexed scalar slots enter the output axis.

Keep additive `WeightedRiskRequest_`, `WeightedRiskResult_` and
`ValueByMonteCarloWithWeightedRisk` surfaces. One request structure groups ordered
output IDs, optional weights, input IDs, report factors and payload budget.
An output-axis query returns owning passive IDs/labels/slots from the indexed
product. These names are proposed and not yet implemented by the root increment.

The final result separates the weighted value/gradient from component means and
passive weights. Its gradient is always `(1,n)`. Reuse scalar input selection and
provenance helpers, without routing ordinary scalar callers through weighted
metadata. Default weights and output selection resolve before history/workers.
The retained numeric budget is `sizeof(double) * (1+n+2*k)`.

Python uses keyword options, strict parsing, copies before releasing the GIL and
detached arrays. Excel uses immutable handles, raw cell guards and generated
Machinist functions. Neither binding exposes the active root helper.

## Alternatives and remaining work

Repeated scalar valuations duplicate forward work. Assigning each selected
output's seed overwrites aliases and mishandles prefix accumulation. A new vector
tape layout adds complexity unnecessary for one fixed weighted objective.

Next implement output identity/preflight and prepared batch integration; then
owning projection/provenance, common-path oracles, bindings and affected scalar
cost checks. Blocked Jacobians and portfolio timelines remain separate F02 work.
