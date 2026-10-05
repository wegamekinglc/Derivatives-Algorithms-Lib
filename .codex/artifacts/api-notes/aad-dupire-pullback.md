# F01 calibration boundary API decision

Status: core snapshot/scalar pullback, public Hybrid adapter and Python/Excel
bindings accepted through corrective publication `533b6602`. Common curve
integration and final F01 acceptance remain open. The
[specification](../specs/aad-dupire-pullback.md) defines the derivative and tests.

Introduce `dal-cpp/dal/model/dupirerisk.hpp` with passive quote/calibration
configuration, immutable `DupireCalibrationSnapshot_`, numeric
`DupireParameterAdjoints_` and `DupireQuoteRisk_`. Keep calibration inputs in a
configuration struct, with required base IVS and configuration first and the
optional name last. Existing `CalibrateDupireLocalVolSurface` stays unchanged.

`CalibrateDupireWithRisk(baseIvs, inputs, name = {})` copies settings, quotes and name,
samples the base IVS into a private passive representation and creates a checked
numeric calibration snapshot. Expose its surface and quote definition through
const getters. Add nonvirtual const rate/dividend getters to `IVS_` so the
snapshot can preserve existing deterministic carry without a clone or type test.

`PullbackDupireCalibration(snapshot, parameterAdjoints, directQuoteAdjoints = {})`
validates the seed's complete surface identity before making an independent
recording. Return separate raw calibration/direct/total quote matrices plus the
snapshot and method. A seed must carry the surface coordinates and values it
was computed against; a naked same-length vector is insufficient. Direct quote
seeds likewise carry the quote definition. Use immutable snapshot ownership
similar to existing `RateQuoteRiskProvenance_`; do not expose opaque active nodes.
Parameter and direct seed structs each carry an immutable calibration snapshot
and a numeric matrix. Parameter seeds require complete snapshot content equality.
Direct seeds compare only their ordered quote axes and quote values: a different
fixed base IVS can legitimately share the same direct quote definition.
`DupireQuoteRisk_` retains detached calibration/direct/total matrices, the snapshot,
`NativeAADCalibrationVJP`, raw `decimal-vol` and the fixed-input boundary.

Frozen base samples cover the original stencil and ATM band calls. Replay must
use the same arithmetic and lookup coordinates, fail on an unsampled query,
and preserve output spot/time orientation. Native additive output seeds handle
boundary aliases. The first VJP never allocates a dense calibration Jacobian.

The public Hybrid adapter takes an existing structured risk result, the
calibration snapshot and a local-vol component ID. It reconstructs/validates
the retained passive model data, uses typed component parameter extents and
requires the necessary selected model ordinals. It performs no second MC run.
`HybridModel_` sorts components by their typed name before constructing parameters;
the archive's input component order is not the runtime parameter order. Match
that ordering and verify it against the complete reconstructed model axis, with
an intentionally unsorted-component acceptance case.
Additional quotes used directly by a product must be supplied explicitly with
matching identity; the adapter cannot infer them from model labels.

## Public Hybrid boundary

Add `dal-public/src/dupirerisk.hpp` with these two operations:

```cpp
DupireParameterAdjoints_ ExtractDupireParameterAdjoints(
    const Script::RiskResult_& valuation,
    const DupireCalibrationSnapshot_& calibration,
    const String_& component);
DupireScriptQuoteRisk_ PullbackDupireScriptRisk(
    const Script::RiskResult_& valuation,
    const DupireCalibrationSnapshot_& calibration,
    const String_& component,
    const std::optional<DupireDirectQuoteAdjoints_>& direct = {});
```

The extraction helper is useful for portfolio seed accumulation before a single
core pullback. The complete helper returns a passive immutable object exposing
`Valuation()`, `QuoteRisk()`, `Component()` and `Method()`. Retain the complete
source result and the core quote result so price, raw model gradients, execution,
calibration identity and separate direct/calibration/total quote contributions
remain auditable. Required arguments precede the optional direct contribution.

Require scalar native mean-risk execution with a retained Hybrid JSON model
snapshot. Decode that snapshot, construct the ordinary numeric runtime model,
and verify every complete model coordinate's ordinal, label and numeric value.
Sort typed data components by the runtime name ordering and sum actual typed
component parameter counts. The selected component must be local-vol equity;
its complete surface grids and values must match the calibration. Validate each
selected model coordinate against the complete axis and require every surface
ordinal. Request/report ordering and reporting factors do not affect raw seeds.
No preparation, fixing read, worker submission or second valuation occurs here.

The composite method concatenates the retained valuation method and calibration
method. A source `NativeAADWithRetrainedPolicySecant` remains an explicitly
mapped model-policy secant approximation; quote-level retraining oracles and
the later full quote-bump estimator remain separate acceptance requirements.
Unknown methods and price-only execution fail before calibration recording.

Errors identify `InvalidDupireQuote`, `InvalidDupireCalibration`,
`DupireSnapshotMismatch` or `InvalidDupirePullback` with coordinates/field where
known. Unsupported nesting uses the existing recording diagnostic. Publish no
partial risk handle and retain prior successful handles on failure.

The [binding decision](aad-dupire-bindings.md) defines the implemented Python
keyword configuration and copies/read-only getters.
Excel follows `_New`, `MonteCarlo_...` and `_Get_...` naming with immutable
wrappers and no getter valuation. Detailed binding names are finalized only
after the core/Hybrid mathematical tests pass. No archive schema or general
calibration plug-in interface is introduced by this increment.
