# Excel calibration and automatic Dupire requests

Status: implemented and locally verified; publication-head Windows runtime and
final F01 acceptance remain open. Native common/automatic request specifications
and the F01 integration audit control all numerical behavior. This adds
worksheet reachability and passive projections only. See the
[increment review](../reviews/aad-risk-request-excel.md) and
[affected-entry performance](../performance/aad-risk-request-excel.md).

## Common worksheet interface

- `CalibrationRiskRequest_New(name, settings)` accepts the existing two-column
  settings convention: `inputs`, `report_factors`, `numeric_payload_budget_bytes`.
  Omitted inputs select all; an explicit blank inputs row selects none. Semicolon
  IDs/factors follow existing scalar syntax; outputs and unknown keys reject.
- `CalibrationRiskRequest_Get_Settings(request)` returns configured rows only;
  all-default configuration spills one blank cell. Preserve absent versus empty.
- `CalibrationRiskPlan_New(name, calibration, request?)` owns the native plan.
  Getters return calibration, complete/selected quote coordinates and exact shape/
  numeric payload. Quote rows retain all native optional metadata, blank if absent.
- `CalibrationRiskResult_New(name, plan, parameters, direct?)` calls the native VJP.
  Getters return the plan, existing common raw quote-risk handle and one scalar
  row projection: Total, Calibration, Direct or Reported. Empty projection spills
  one blank cell; exact shape remains available from the plan.

## Automatic worksheet interface

Use an execution settings handle to keep positional arguments short:

```text
execution = DupireScriptRiskSettings_New(name, n_paths, valuation?, simulation?)
request   = DupireScriptRiskRequest_New(name, execution, quotes?, bindings?, direct?)
plan      = DupireScriptRiskPlan_New(name, product, modelData, calibration, component, request)
result    = DupireScriptRiskResult_New(name, plan)
```

The private Excel execution value owns paths and copies existing valuation/
simulation settings. Omitted simulation enables native AAD. Explicit false is
rejected by native planning. Settings getters return paths and existing typed
settings handles; they do not capture a date or execute.

Bindings are an optional two-column data range of exact nonnegative constant
ordinals and quote IDs, without a header. Reject bool/text numeric coercion,
fractional/overflowing ordinals, errors and NUL strings before Excel conversion.
Require exactly representable integers within size_t and at most 2^53-1.
Native planning validates source membership, values, uniqueness and exclusive
binding/external direct forms. Optional empty ranges represent no bindings.

Request configuration returns owning settings, quotes, a binding table and a
`has_direct` flag. A separate checked direct getter rejects when no external seed
exists: the existing repository formatter cannot publish a null handle.
Plan getters expose its quote plan, complete/required model
axis, copied settings/bindings and component/paths/combined payload metadata.
Result getters return existing scalar valuation and requested quote-risk handles
plus component/method/payload. Reuse their established projection/provenance
getters. Sealed model/product data is not exposed from a plan.

## Ownership, compatibility and acceptance

Extend the existing immutable calibration-value storage tags; introduce no new
wrapper framework. Share existing scalar request parsers and model-axis/empty-
matrix formatting without changing their diagnostics or normal execution paths.
All getters project native passive values; no observer/history/JSON/reverse work.
Numeric budgets count native retained arrays, including every complete quote
matrix and automatic required valuation gradient. No PV/currency is invented.

Add focused missing-API RED, native parity for Dupire and captured curve providers,
full/subset/empty settings, exact/short budgets, strict inputs and source recovery,
owning copies, passive getters and actual automatic flat/Merton execution. Keep
the accepted independent native mathematical oracles and their original steps/
tolerances. Test OFF/combined portable bindings and generated-registration drift;
verify raw XLL guards and runtime through actual Windows CI. Measure only changed
old Excel entries with the unchanged paired policy. Consolidate publication with
the locally committed Python automatic interface. Full F01 and P01/whole-PR/
final-head review/merge gates remain open; later stages require new PRs.

No user decision is required. Rejected alternatives: a long request argument
list, label-inferred dependencies, duplicate calibration math, fake scalar
results for plan metadata, and JSON serialization of immutable risk handles.
