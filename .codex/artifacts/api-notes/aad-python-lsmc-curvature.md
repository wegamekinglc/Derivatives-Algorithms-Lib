# Native LSMC financial API

## Surface and rationale

C++ includes `dal-public/src/lsmccurvature.hpp` and calls
`PlanBlackScholesLsmc(product, simulation, valuation)`, then
`ValueByBlackScholesLsmcWithCurvature(plan, point, paths, bumps)`.
Optional simulation and valuation follow the product; defaults reuse existing
risk Monte Carlo settings. Python uses keyword-only planning options:

```python
simulation = dal.MonteCarloSettings_(
    enable_aad=True, compiled=True, smooth=2.0,
    lsmc_training_paths=256, lsmc_policy_risk_mode="Frozen")
plan = dal.BlackScholesLsmcPlan_New(
    product, valuation=valuation, simulation=simulation)
point = [100.0, 0.2, 0.05, 0.0, *plan.script_constants]
result = dal.BlackScholesLsmc_Get_Curvature(plan, point, 129, bumps)
policy = result.base_policy
```

Reuse `BumpOverAADRequest_` and `MonteCarloSettings_`; do not add a second
policy-risk settings schema. Planning seals execution settings and history;
evaluation replaces only the explicit raw numeric coordinates. There is no
model-data or Python callback argument. Planning does not train the policy.

## Passive results

`BlackScholesLsmcCurvatureResult_` retains `plan`, `value`, `gradient`,
`parameter_labels`, `point`, `directions`, `steps`, `hessian_products`,
`base_policy`, `simulation` and `execution`. Policy elements are readonly
`LsmcExercisePolicy_` values in live EXERCISE order; return detached containers
for all coefficient/normalization/power arrays. Plan event dates and time line
allow interpretation of the retained contract.

Execution preserves method, 1+2M gradient requests, paths per replicate,
training/validation/replicate counts, numeric payload, recording cap and maximum
batch tape/cleanup reserve. Retained simulation records the policy mode and
inner relative step independently from the outer request.

## Errors and compatibility

Reject wrong object types, bool/enums/coercion, malformed sequences, invalid
positive path counts and wrong bump types before releasing the GIL. Copy all
typed inputs first; native finite/domain admission precedes worker submission.
Native errors retain base/direction/sign, domain and budget context. Explicitly
disabled AAD, expired/no-exercise preparation and unsupported settings fail.
Numerical admission occurs after sealed history preparation and never re-reads
external history. Recording caps can fail after policy training.

No existing signature, default, method or capability flag changes. Rejected
alternatives are a generic objective callback, Python model trampoline, and
repeating ordinary Frozen valuations at outer points: the last retrains the
policy and changes the quantity being estimated. Excel remains a later stage.
