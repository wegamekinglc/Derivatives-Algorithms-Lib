# Compatible portfolio API decisions

Status: proposed surfaces for the active
[portfolio specification](../specs/aad-compatible-script-portfolio.md).
Names and examples below are not callable DAL APIs yet.

## Current boundary and audience

`ValueByMonteCarloWithWeightedRisk` and
`ValueByMonteCarloWithJacobianRisk` in `dal-public/src/value.hpp` take one product,
one model, a path count and settings. Python and Excel mirror this boundary.
`PreparedScript_` owns one immutable prepared product but constructs private
evaluator/history state for execution. Keep these entry points and their costs.

The new audience is a user who needs total portfolio risk or selected trade
attribution with shared model inputs. Portfolio callers must be able to inspect
which trades share scenarios and which execute separately, and match returned
coordinates to the original trades without relying on display labels.

## Proposed owning surfaces

Use a `ScriptPortfolioData_` storable containing ordered trade IDs, product
snapshots, model snapshots and a model-owner registry. An entry repeats an
explicit model owner when its original model handle repeats. Assign owners
before copying/cloning; numerical equality cannot collapse distinct owners.
Freeze product/model execution data when constructing the portfolio. Evaluation
captures the date and required fixing snapshot independently for each request.
Reject objects that cannot be frozen using the supported model-data boundary.

Proposed C++ entry points take the portfolio and path count first, followed by
request, valuation and simulation settings. Use separate weighted and attribution
methods rather than a mode flag with incompatible output types:

```cpp
// Proposed signatures; declarations are not present in DAL yet.
PortfolioWeightedRiskResult_ ValuePortfolioByMonteCarloWithWeightedRisk(
    const Handle_<ScriptPortfolioData_>& portfolio, int numPath,
    const PortfolioWeightedRiskRequest_& request = {},
    const ScriptValuationSettings_& valuation = {},
    const MonteCarloSettings_& simulation = DefaultRiskMonteCarloSettings());

PortfolioJacobianRiskResult_ ValuePortfolioByMonteCarloWithJacobianRisk(
    const Handle_<ScriptPortfolioData_>& portfolio, int numPath,
    const PortfolioJacobianRiskRequest_& request = {},
    const ScriptValuationSettings_& valuation = {},
    const MonteCarloSettings_& simulation = DefaultRiskMonteCarloSettings());
```

Request types reuse the existing input/output/report-factor selection vocabulary.
Both add portfolio-wide recording/scratch capacity limits; attribution keeps an
explicit maximum block width, default one. Weighted requests keep passive
weights. Do not add portfolio fields to the legacy scalar or single-script request
types. Omitted outputs mean every trade's payoff; explicit empty outputs fail.

New results own numeric data and trade/group metadata. Selected/complete axes
use the namespaced IDs defined in the specification. Expose trade IDs and
model-owner ordinals separately from risk labels. Execution metadata contains
group membership and scenario counts; it must distinguish scenario generation
from trade evaluator invocations. Getters retain existing detached-copy behavior.

## Binding projection

Python proposes `ScriptPortfolio_New(trade_ids, products, modelData)` with equal
nonzero sequence lengths and strict element types. Construction copies/seals all
execution inputs. A repeated Python model object establishes a repeated owner;
two different model objects remain separate even when their contents agree.
Evaluation uses `PortfolioMonteCarlo_ValueWithWeightedRisk` and
`PortfolioMonteCarlo_ValueWithJacobianRisk`; request, valuation and simulation
remain keyword-only. Immutable request/result properties return detached copies.

Excel proposes `ScriptPortfolio_New(name, trades)`, where `trades` is a physical
three-column table of trade ID, product handle and model handle. Resolve handles
before assigning owners, reject malformed/ragged cells without coercion, and
store a sealed immutable portfolio. Use the same evaluation names and ordinary
immutable request/result factories/getters. Include a shape getter for zero risk
columns, as in the current Jacobian surface. Machinist owns generated exports.

Typical Python use, shown as pseudocode until the bindings exist:

```python
portfolio = ScriptPortfolio_New(["A", "B"], [trade_a, trade_b], [model, model])
request = PortfolioWeightedRiskRequest_(
    outputs=["trade:0:payoff", "trade:1:payoff"], weights=[2.0, -1.0],
    inputs=["model:0:parameter:0", "trade:0:constant:0", "trade:1:constant:0"])
result = PortfolioMonteCarlo_ValueWithWeightedRisk(
    portfolio, 4096, request=request, valuation=valuation, simulation=simulation)
```

The two trades share a model owner; their sampling contracts still determine
whether they share a scenario. Changing the weights does not change ownership.

## Errors, compatibility and rejected alternatives

Errors identify the field and offending trade/group/coordinate. Null handles,
duplicate IDs, mismatched sequence/table lengths, invalid path counts and
unsupported exercise/expired trades fail before tasks. Nonfinite selected
outputs fail even with zero weight. Budget errors retain their budget kind and
required/admitted capacities. No result is returned on a partial failure.

Do not overload the legacy single-product method to interpret a collection:
that would complicate binding conversion and its default allocation path.
Do not identify inputs by model/constant label alone. Do not choose a union
timeline or silently substitute a RNG dimension to improve sharing. Do not expose
an active model/evaluator handle as the portfolio result.

## Decisions required before implementation

Confirm the existing model-data snapshot/factory code can deep-freeze every
accepted family and preserve original-handle ownership. Finalize result metadata
layout without adding fields or runtime work to old results. Validate strict
Python/Excel construction against analogous risk request parsers. Names may be
adjusted to fit registration conventions before implementation; mathematical
identities and failure semantics remain controlled by the specification.
