# Python request API decision

Status: active; common quote bindings implemented locally, automatic bindings
remain specified. The
[spec](../specs/aad-risk-request-python.md) controls ownership, parsing, native
semantics, GIL and compatibility acceptance.

## Common interface

```python
request = dal.CalibrationRiskRequest_(
    inputs=["quote:3", "quote:0"],
    report_factors=[0.01, 0.01],
    numeric_payload_budget_bytes=144,
)
plan = dal.CalibrationRiskPlan_New(boundary, request=request)
result = dal.CalibrationRiskResult_New(plan, parameter_adjoints, direct=direct)
per_vol_point = result.reported_jacobian
```

`CalibrationRiskRequest_` has readonly configuration properties matching its
keyword names. Plan properties are `calibration`, `complete_input_axis`,
`input_axis`, `selected_ordinals`, `numeric_payload_bytes`. Result properties are
`plan`, `quote_risk`, `jacobian`, `calibration_jacobian`, `direct_jacobian` and
`reported_jacobian`. Coordinate properties match native fields in snake_case;
optional source fields return None. Keep matrices as detached `DoubleMatrix_`
values, matching existing bindings.

## Automatic interface

```python
binding = dal.DupireQuoteBinding_(constant_ordinal=0, quote_id="quote:3")
request = dal.DupireScriptRiskRequest_(
    num_paths=257,
    quotes=dal.CalibrationRiskRequest_(inputs=["quote:3", "quote:0"]),
    direct_bindings=[binding],
    valuation=valuation,
)
plan = dal.DupireScriptRiskPlan_New(product, modelData, calibration, "Z_LOCAL", request)
result = dal.DupireScriptRiskResult_New(plan)
price = result.valuation.values[0]
spread_risk = result.quote_risk.reported_jacobian
```

All request fields are keyword-only; paths are required. Optional `direct`
accepts the existing common direct seed and excludes bindings. Settings use
existing native bound types; omitted simulation uses the native AAD default.
Plan/result property names follow their C++ getters in snake_case. `quote_plan`
is the common plan, and `quote_risk` is the common requested result. There is
no live product/model getter. Factory names follow type `_New` conventions,
with the five required planning arguments and one required execution plan.

## Sharing and compatibility

Add focused request binding translation units/initializers. Move scalar ID,
factor and size_t parsing into one private reusable helper with explicit type,
field and diagnostic context. Keep old scalar parsing behavior/messages exactly;
do not modify scalar semantic planning or defaults. Reuse existing strict string,
enum, integer, settings and copying helpers. Do not duplicate provider dispatch,
axis building, identity checks or projections.

Use explicit constant field/identifier labels in the shared parser. Dynamic
type/field assembly caused the first old-entry performance failure; fixed labels
preserve stable diagnostic content/timing and pass the unchanged full gate.
The source file/line trace follows the private helper's location. Common local
functionality, installed parity and performance are verified; own-head CI and
automatic bindings remain open.

Release the GIL only after all typed input conversions, around callback-free
native work. Projection getters stay passive and ordinary Python-owned data
copies. Rejected alternatives: calling Python constructors for validation;
passing dictionaries through implicit casts; borrowing request lists; allocating
cached selected matrices; guessing PV/currency; or accepting untyped binding pairs.

No user clarification is needed. Deliver common requests first, then automatic
requests, with native/install parity and old-entry performance evidence per increment.
