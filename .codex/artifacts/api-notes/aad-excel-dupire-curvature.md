# Excel Dupire curvature API

Status: active design for native financial worksheet users. See the
[specification](../specs/aad-excel-dupire-curvature.md).

## Surface

Use immutable `Excel::StorableRiskValue_` aliases for the common native bump
request and public Dupire request/plan/result. Associate binding metadata
overloads with their value namespaces so include order does not require editing
the existing storable template. Raw numeric guards stay in a small input header;
shared passive cell-copy/shape helpers serve the new modules only. The common
request includes `dal-public/src/bumpoveraad.hpp`, a thin forwarding header for
the existing native request; production bindings never include core directly.

The five common functions are `BumpOverAADRequest_New(name, directions, steps,
settings=blank)`, and `Get_Directions`, `Get_Steps`, `Get_Settings`, `Get_Shape`.
Settings keys are `input_count`, `numeric_payload_budget_bytes` and
`recording_capacity_budget_bytes`. A nonempty matrix determines its own input
count; a supplied count must agree. For gradient-only evaluation use blank
directions/steps and `input_count=Q`. The constructor validates finite direction
entries, positive steps and row counts, leaving point-dependent admission to
the native financial plan. A zero row is not equivalent to zero direction rows.

The three request functions are `DupireScriptCurvatureRequest_New(name, risk,
bumps)`, `Get_Risk` and `Get_Bumps`. Both required handles are copied; no defaults
silently replace a missing handle.

The seven plan functions are `DupireScriptCurvaturePlan_New(name, product,
modelData, calibration, component, request)`, `Get_BasePlan`, `Get_Point`,
`Get_Directions`, `Get_Steps`, `Get_Shape` and `Get_Payload`. The six required
factory inputs follow the existing first-order plan convention. Payload is the
native admitted numeric byte count; this query performs no execution.

The ten result functions are `DupireScriptCurvatureResult_New(name, plan)`,
`Get_Base`, `Get_QuotePlan`, `Get_Point`, `Get_Gradient`, `Get_Directions`,
`Get_Steps`, `Get_HessianProducts`, `Get_Shape` and `Get_Execution`.
The quote-plan projection allows the existing
`CalibrationRiskPlan_Get_Inputs(quotePlan, complete=true)` to supply all native
coordinate labels, units and row/column metadata without a second formatter.

## Spill and ownership contract

Directions and products have M rows and Q columns. Steps are an M-by-one
column; point and gradient are Q-by-one columns. Each numeric getter returns
copied cells. Empty data spills as one blank cell, with the shape query retaining
the true zero-by-Q extent. Shape is one row containing M then Q. Settings are
three key/value rows; actual input count is always reported. Optional unset caps
are blank. Execution is four key/value rows: `method`,
`quote_gradient_evaluations`, `paths_per_evaluation`, `numeric_payload_bytes`.

The full raw product coordinate order comes from the complete quote plan;
reported/selected first-order results remain available through `Get_Base`.
Do not apply a first-order reporting factor, DV01 conversion or 1% volatility
scale to these products. Direction ordinals follow zero-based native row order.
The result is finite-step curvature through full recalibration, not exact
nested AD or a general higher-order capability.

Names are checked before construction. Getters reject null handles before
dereference. Output handles are replaced only after successful preparation or
execution. Constructor inputs, handle getters and numeric spills never expose
mutable aliases to the stored value. Every new handle rejects serialization.

## Typical worksheet chain

1. Create the existing Dupire calibration, matching Hybrid model and first-order
   request with fixed paths/date/settings and any explicit scalar quote bindings.
2. Create `BUMPOVERAADREQUEST.NEW` from direction rows and corresponding positive
   steps. A unit row selects a Gamma/cross-Gamma column; signed rows request HVPs.
3. Create `DUPIRESCRIPTCURVATUREREQUEST.NEW`, then the plan and result handles.
4. Query raw products and the returned quote plan's complete input metadata.
   Query the base first-order result separately when reported risks are needed.

## Errors and alternatives

Reject unsupported raw numeric kinds before conversion, with function/field and
worksheet location. Settings retain existing duplicate/unknown/NUL checks and
blank-versus-zero budget semantics. Native admission/execution errors retain
their stage and perturbation context. A bump recording cap is valid in the common
request but explicitly unsupported by this parallel Dupire adapter, including
zero. Request construction does not claim a worker-memory cap was installed.

An inferred input count from a blank range would lose the zero-by-Q contract;
the explicit optional setting avoids that ambiguity. A duplicated quote-axis
table or a refactor of legacy calibration formatters would widen this increment;
returning the existing quote-plan handle preserves its accepted contract.
No C++/Python behavior or numerical algorithm change is required.

Open questions: none.
