# Python calibration and automatic Dupire requests

Status: active binding contract, specified before implementation. Common quote
bindings are locally implemented; automatic bindings and publication acceptance
remain open. The accepted
[common request](aad-calibration-risk-request.md) and locally verified
[automatic C++ request](aad-dupire-risk-request.md) define native behavior.
The [ledger](../plans/aad-implementation.md) keeps full F01/merge gates open.

## Problem and scope

Python reaches the common typed calibration pullback and the manual Hybrid
chain, but cannot construct native quote requests or automatic script plans.
Expose those owning values through the existing module without another planner,
normalization, source identity algorithm or cached report representation.
Deliver common requests first, then automatic execution over the same bindings.
Excel parity, multiple outputs, blocked sweeps and later operators remain separate.

## Requirements

R1. Bind `CalibrationRiskRequest_`, `CalibrationQuoteCoordinate_`,
`CalibrationRiskPlan_`, `CalibrationRiskResult_`, `DupireQuoteBinding_`,
`DupireScriptRiskRequest_`, `DupireScriptRiskPlan_` and `DupireScriptRiskResult_`.
All exposed properties are readonly. Owning values support copy/deepcopy through
native copies; lists, coordinates, settings and matrices returned by getters
are detached passive values. Plans expose no sealed live model/product or AST.

R2. Request constructors are keyword-only. Common configuration uses `inputs`,
`report_factors` and `numeric_payload_budget_bytes`, defaulting to None. Preserve
None versus explicit empty selection. Copy list/tuple inputs; reject strings,
generators, sets and arbitrary object coercion. Reject bool/enums for numeric
factors/budgets, embedded NUL and enum/string coercion for IDs. Positive finite
factors, uniqueness, source membership and complete retained budgets are checked
by the native planner before work. Keep invalid-factor timing compatible with
existing scalar requests when sharing their parsing helpers.

R3. Budgets and constant ordinals accept nonnegative integral `__index__` values
within size_t, excluding bool/enums. Paths require integers in 1..INT_MAX;
reject float, text, bool, enums, negatives and overflow with field/type/range
context. Share strict generic parsing with scalar requests while preserving
existing scalar messages, defaults and validation timing. Do not call a Python
constructor/module attribute to perform native configuration parsing.

R4. Common plans/results project the native metadata and all four detached
Jacobian getters. Curve quote value/strike/maturity are None; Dupire block fields
are None. Complete versus selected axes, selected ordinals, raw native shapes,
method/unit/boundary and exact payload remain inspectable. No generic value/PV
or currency is invented from external parameter seeds.

R5. Required boundary/plan/parameter/source handles are checked explicitly with
field/type context. Optional requests/settings/direct values accept the proper
native bound type or None; reject dictionaries, unrelated types and implicit
coercion. Preserve full parameter/curve direct identity and Dupire quote-only
direct identity, including the allowed different fixed base IVS.

R6. A direct binding is a keyword-only immutable `constant_ordinal`/`quote_id`
value. Automatic requests require explicit `num_paths`; optional `quotes`,
`direct_bindings`, `direct`, `valuation` and `simulation` are keyword-only.
Bindings accept only a copied list/tuple of typed binding values. None means
none. Default native simulation enables AAD; explicit price-only settings remain
invalid for automatic planning. Bindings/external direct are mutually exclusive
under the native planner. Labels and equal values do not infer dependencies.

R7. Planning/execution factories reuse the native sealed plans. Preserve complete
mandatory inputs independent of quote selection, exact combined budget, fixing
snapshot timing, native mean normalization, expired/mixed methods and fixed
calibration valuation provenance. Required native arguments precede optional
keyword configuration. Getters never perform JSON parsing, history lookup,
reverse or another valuation.

R8. Finish every Python conversion/copy with the GIL held. Release it only around
callback-free native planning/VJP/execution after owning required inputs. The
automatic planner rejects custom archive types before serialization. Never
release around a Python IVS callback, borrowed object inspection or list copying.
Native tape ownership remains operating-system-thread local.

R9. Keep old signatures, defaults, source getters, raw/report values and errors.
Freeze old modules/production/binding inputs before sharing existing parsers.
Run original paired two-round/ten-process/4% changed-binding/default workload
acceptance. New request costs are additional informational rows. Preserve all
failed runs and all original mathematical oracle steps/tolerances.

## Acceptance

- Missing request/factory RED before implementation; strict field/type/range,
  semantic planning, copy/deepcopy and detached readonly property tests.
- Common Dupire and all four captured curve providers, both inverse modes:
  full/subset/empty, exact/one-byte-short full budgets, native metadata/ordinals,
  wrong owner/domain/direct sources, signed/zero seeds and report overflow.
- Exact independently installed C++ parity for coordinates, selected/raw/report
  matrices, payload and methods. Retain accepted native independent recalibration
  and actual portfolio oracles; wrapper parity does not replace them.
- Automatic flat/Merton, direct binding/external direct, required ordinals,
  owned models/settings/fixings, empty smoothing, expired/mixed methods,
  failure/recovery and no second mean normalization.
- Concurrent native calls, real GIL heartbeat, passive unrelated-tape getters
  and valid execution after errors. No fabricated observer or shape result.
- Workspace and fresh standalone OFF/combined modules, Python 3.9-compatible
  tests, proportional installed/Windows/own-head CI acceptance, full original
  performance protocol and updated current-state guides/changelog.

## Open questions

No user decision required. Use native planning for semantic constraints and
existing DAL Python factory/property naming. Publish only accepted increments;
full F01 still requires Excel request parity and final integration.
