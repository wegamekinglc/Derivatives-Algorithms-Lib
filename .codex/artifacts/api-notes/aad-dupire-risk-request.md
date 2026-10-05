# Automatic Dupire request API decision

Status: C++ API accepted locally and at exact publication head `e42c4835`;
all 35 checks pass. Request bindings remain an independent required increment. The
[spec](../specs/aad-dupire-risk-request.md) controls sealing, native methods,
required inputs, direct dependencies, budgets and acceptance.

## Public C++ surface

```cpp
struct DupireQuoteBinding_ {
    size_t constantOrdinal_ = 0;
    String_ quoteId_;
};

struct DupireScriptRiskRequest_ {
    int numPaths_ = 0;
    CalibrationRiskRequest_ quotes_;
    Vector_<DupireQuoteBinding_> directBindings_;
    std::optional<CalibrationDirectQuoteAdjoints_> direct_;
    ScriptValuationSettings_ valuation_;
    MonteCarloSettings_ simulation_ = DefaultRiskMonteCarloSettings();
};

DupireScriptRiskPlan_ PlanDupireScriptRisk(
    const Handle_<ScriptProductData_>& product,
    const Handle_<ModelData_>& model,
    const DupireCalibrationSnapshot_& calibration,
    const String_& component,
    const DupireScriptRiskRequest_& request);

DupireScriptRiskResult_ ValueByMonteCarloWithDupireRisk(
    const DupireScriptRiskPlan_& plan);
```

The required path count lives in the request and defaults to invalid zero, so a
caller must set it explicitly. Five required planning arguments avoid a long
positional execution/settings list. Keep declarations in
`dal-public/src/dupireriskrequest.hpp`; native public-layer helpers share scalar
axes and Dupire layout validation without changing dependency direction.

Plan getters: `Component()`, `QuotePlan()`, `CompleteInputAxis()`,
`RequiredInputAxis()`, `DirectBindings()`, `NumPaths()`, `ValuationSettings()`,
`SimulationSettings()` and `NumericPayloadBytes()`. Return const owning passive
metadata. Private sealed product/model/external seed storage has no live getter.

Result getters: `Valuation()`, `QuoteRisk()`, `Component()`, `Method()` and
`NumericPayloadBytes()`. `QuoteRisk()` is the common `CalibrationRiskResult_`,
so its selected/report projections reuse one implementation. The scalar
`Valuation()` contains the planned mandatory surface/direct columns with fixed
calibration provenance. Do not retain another copy of request seed arrays in
the result or imply a model-axis choice through quote selection.

```cpp
DupireScriptRiskRequest_ request;
request.numPaths_ = 257;
request.quotes_.inputs_ = Vector_<String_>{"quote:3", "quote:0"};
request.quotes_.reportFactors_ = Vector_<>{0.01, 0.01};
request.valuation_.evaluationDate_ = Date_(2026, 9, 12);
const auto plan = PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request);
const auto result = ValueByMonteCarloWithDupireRisk(plan);
const auto spreadRiskPerVolPoint = result.QuoteRisk().ReportedJacobian();
```

To declare a direct script dependency, add a `constantOrdinal_/quoteId_` binding
only when that constant stores the actual native spread value. An external
typed direct gradient is an alternative; supplying both rejects. Binding checks
do not infer semantics from the constant's display name. Direct contributions
are fixed-surface scalar partials, added once after the calibration mapping.

## Error, settings and compatibility decisions

Use `InvalidDupireRiskRequest` for path/type/carry/direct-binding violations,
with component, ordinal, ID or field context. Preserve native axis/source/
shape/finite diagnostics. Combined numeric budget failure identifies both
valuation and full quote components; report the planned total byte count.

The first automatic path seals exact native flat-rate Hybrid graphs and rejects
unsupported custom or stochastic/nonflat carry before serialization/work.
Legacy conditional extraction keeps its broader domain. Omitted evaluation
date is captured during planning. Omitted fixing snapshot uses global history
resolved/frozen during execution after valid preflight; explicit snapshots
remain immutable shared inputs. Do not claim standalone plans freeze later
global fixing changes.

## Python/Excel projection and alternatives

Both bindings construct the same request and native owning plan; required native
arguments come first. Python validates integer paths/ordinals/budget without
bool/enum coercion, copies strict finite inputs, and releases the GIL only for
callback-free native execution. Excel uses immutable request/plan/result handles,
strict raw scalar/range guards and detached matrix/axis getters. Worksheet result
construction accepts one plan; planning accepts the five required handles/text
arguments. Generated long-name/help/type registrations and XLL-owned recording
tests remain required. Exact names/settings-factory markup is decided in the
binding increment against its closest existing contract.

Rejected alternatives: reuse quote selection as model selection; borrow mutable
Hybrid handles after preflight; reconstruct axes from display labels; infer
direct dependencies automatically; store another projected report matrix; or
advertise a fixed-base Dupire mapping as joint stochastic calibration. Each
would break ownership, budget, coordinate or derivative semantics.
