# Black–Scholes segmented Monte Carlo API

## Audience and surface

C++ consumers include `dal-public/src/montecarlocurvature.hpp`. Python consumes
the same public boundary through `DAL::public`. The preparation is closed to
the concrete native Black–Scholes implementation; model creation callbacks and
active Python scalars are excluded.

| C++                                                                               | Python                                                                                 |
|-----------------------------------------------------------------------------------|----------------------------------------------------------------------------------------|
| `PlanBlackScholesMonteCarlo(product, valuation, smoothing)`                       | `BlackScholesMonteCarloPlan_New(product, *, valuation=None, smoothing=...)`            |
| `ValueByBlackScholesSegmentedMonteCarlo(plan, point, paths, settings)`            | `BlackScholesMonteCarlo_Get_Risk(plan, point, num_path, *, settings=None)`             |
| `ValueByBlackScholesMonteCarloWithCurvature(plan, point, paths, bumps, settings)` | `BlackScholesMonteCarlo_Get_Curvature(plan, point, num_path, bumps, *, settings=None)` |

Use keyword-only `SegmentedMonteCarloSettings_` with `rsg`, `use_bb`,
`first_path`, `scramble_key`, `normal_precision`, `segment_steps`,
`checkpoint_capacity_budget_bytes` and `recording_capacity_budget_bytes`.
Expose a flattened Python view of the native nested path settings; C++ retains
the core structure. Reuse the single existing bump request type.

## Typical use

```python
plan = dal.BlackScholesMonteCarloPlan_New(product, valuation=valuation)
point = [100.0, 0.2, 0.03, 0.01, *plan.script_constants]
settings = dal.SegmentedMonteCarloSettings_(first_path=7, segment_steps=64)
risk = dal.BlackScholesMonteCarlo_Get_Risk(plan, point, 129, settings=settings)
curvature = dal.BlackScholesMonteCarlo_Get_Curvature(
    plan, point, 129, bumps, settings=settings
)
```

The explicit point avoids hiding which constant/model values each secant uses.
Plan creation has no sampling/bump request: it intentionally resolves and seals
history before later evaluations. Invalid numerical requests subsequently touch
neither global history nor a new preparation. This keeps the existing core
preparation interface and its callers unchanged.

## Ownership and errors

The plan retains a copied contract and sealed native kernel. Financial results
retain both the native numerical result and the plan. Python exposes readonly
properties with detached containers and settings. Known observations reuse the
existing passive `RiskObservationSnapshot_` type. Settings/copy/deepcopy cannot
modify the estimator retained by an earlier result.

Strict conversion errors name the public operation and offending field. Native
errors retain direction/plus/minus and budget context. `num_path` follows the
existing positive public Python path-count contract; `first_path` and byte
budgets use checked unsigned native ranges. Requested sampling is retained;
execution reports the effective recording cap and native work/resource fields.

No existing signature/default changes. Excel projection is a later work package.
No automatic strategy, bump step, normalization or Hessian symmetry repair is
introduced. Ordinary native `higherOrder_` remains false.
