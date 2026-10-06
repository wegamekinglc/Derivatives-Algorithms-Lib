# Compatible portfolio API decisions

Status: sealed C++ portfolio construction and coordinate inspection exist.
Internal whole-request preparation and shared weighted group batches also exist.
The replay coordinator, native/passive weighted capacity admission and owning
public C++ weighted/attribution results also exist. Python construction, immutable
requests/results and both value functions are implemented and locally verified.
Excel construction, typed requests/results and shared checked getters are
implemented; affected portable contracts and Windows syntax checks pass locally.
The standard installed C++ consumer and 126 affected installed Python cases
pass. The full OFF suite, all four extended diagnostic configurations, four
actual Windows modes and six expanded sanitizer jobs pass at `9cc9fe0a`.
The preparation-inlining repair passes affected functionality and all 160
comparative cases; its final publication commit needs fresh delivery acceptance
under the active
[portfolio specification](../specs/aad-compatible-script-portfolio.md).

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

## Implemented ownership and coordinates

`Script::ScriptPortfolioData_` is a storable containing ordered trade IDs, product
snapshots, model snapshots and a model-owner registry. An entry repeats an
explicit model owner when its original model handle repeats. Assign owners
before copying/cloning; numerical equality cannot collapse distinct owners.
Product/model execution data are frozen at construction. The supported factory
validates model parameters; exact JSON serialization deep-copies each unique
owner once and verifies its dynamic type. Unsupported snapshots fail with the
offending trade ID. Internal preparation captures one request date/history.

`ScriptPortfolioRiskAxes` in `dal-public/src/portfoliorisk.hpp` returns
`PortfolioRiskAxes_`, with read-only `InputAxis`, `OutputAxis`,
`TradeInputPositions` and `OutputTrades` accessors. Shared model columns precede
private trade constants. Inspection indexes products without history/date/task
access; it does not produce valuation or group execution results. See the
[current C++ example](../../../docs/methodology/aad.md#sealed-script-portfolio-coordinates).

`Script::Detail::PlanScript` returns a move-only plan owning its private product
and initialized passive model. It reads no history and cannot execute until
`CompleteScriptPreparation` consumes it with a nonnull frozen snapshot. Explicit
valuation snapshots cannot be substituted; original global source metadata is
preserved when completing with the captured global environment.

`Script::Detail::PrepareScriptPortfolio` owns the sealed handle and copies
settings before invoking callbacks. It plans every trade, invokes one
whole-request admission callback, captures the union of historical dependencies
once, completes private trade state and returns an owning `PreparedPortfolio_`
with the path count and compatible groups. These are internal integration
surfaces. The callback establishes where budget policy runs. Weighted replay
preparation now uses it for aggregate startup admission; this generic function
does not itself select a budget policy. No public Monte Carlo portfolio result or
worker scheduling is exposed by these preparation functions.

`Script::Detail::EvaluatePortfolioWeightedBatch` accepts one compatible group,
an original absolute path range and validated global output coordinates with
passive weights. It generates one scenario per path, evaluates only selected
trades with private state and reverses one weighted root. Its owning passive
result contains raw component/objective/model/private-constant sums and actual
scenario/evaluator/suffix/prefix counters. The model is registered once; private
constants remain separate leaves. Each batch reverses its retained historical
prefix once. It validates finite selected values even at zero weight and restores
recording/mode state after failures. This is an internal execution primitive,
without whole-request admission, selected-input projection or public results.
Optional shared scratch/tape budget pointers attach worker allocations and fixed
private evaluator payloads before construction. Recording capacity retains cleanup
replacement headroom. The coordinator owns and shares these budgets across groups
and workers.

`Dal::Detail::EvaluatePortfolioWeightedReplay` validates prepared axes and the
entire output/input-position selection before submitting tasks. It runs compatible
groups sequentially and original path batches in parallel, then scatters shared
model leaves through the global owner mapping and constants through private trade
positions. It preserves requested output/input order and normalizes all sums once
by the captured path count. Unselected groups are skipped; empty selected inputs
retain native pricing. Required derivatives are checked before reduction. Its
passive internal result owns component/objective means, selected gradients,
per-group work counters and actual recording/scratch peaks. Optional aggregate
limits guard coordinator, worker and batch/result capacities, including growth
overlap and task slots. Result replacement stays inside the worker attachment;
accepted tasks drain after submission/worker/capacity failures. This internal
coordinator feeds the public owning weighted result. Passive execution uses
the existing checked BS path and double-batch driver with private sharp evaluators,
never creating native roots or registering tape leaves. Only omitted/empty passive
inputs are accepted; native-empty inputs retain fuzzy pricing. Scenario and
evaluator counts remain distinct in both modes.

`Dal::Detail::PlanPortfolioWeightedRequest` is an internal owning request plan
over a sealed portfolio. It defaults to every trade payoff, resolves ordered
global output/input IDs, finite passive weights and positive reporting factors,
and enforces the checked `sizeof(double) * (1 + n + 2*m)` numeric payload before
history or tasks. It keeps native-empty and passive-zero-column selection distinct.
Complete axes remain unscaled; only selected coordinates receive report factors.
The plan owns the sealed handle, selections, original axes and input request after
caller mutation/destruction. It reuses schema-independent scalar request constraints
without relaxing legacy ordinal-ID validation. This plan is not a public valuation
entry or recording/scratch admission policy.

`PreparePortfolioWeightedReplay` owns a copy of this plan and the settings and
connects its native/passive selections to whole-request preparation. With finite capacity
limits, it uses prospective full-contract groups over the original planned meshes
and one pending historical environment. Completed groups still require executable
state and resolved historical equality. Admission reserves coordinator/result/task
capacities and divides the remaining capacity across concurrent workers. Guarded
model initialization and every selected private evaluator/history vector shape run
before historical reads. Unselected private evaluators are omitted. Placeholder
history admits known shapes; actual historical arithmetic and later growth remain
guarded at runtime. Passive startup admits every selected double evaluator and
known private vector seed, including the checked BS path. It ignores tape limits.
`EvaluatePortfolioJacobianBatch` now reuses the weighted group recording and
private evaluator lifecycle with fixed-width independent output roots. It owns
full model/private-constant gradient matrices, leaves tail lanes zero and reports
actual scenario/evaluation/reversal work. Weights do not scale attribution rows.
`PlanPortfolioJacobianRequest` reuses the owning global selection plan while
enforcing the distinct checked `sizeof(double) * m * (1+n)` retained payload.
`PreparePortfolioJacobianReplay` admits prospective original-mesh group blocks
before history. Result/selection/task capacities stay resident while remaining
capacity is divided across workers, including full model/private matrices, root
lanes and known historical vector shapes. Capacity-only narrowing retains the
requested maximum and returns admitted group widths. Native replay executes
original absolute batches for each group block, deterministically scatters rows
and selected owner/private columns, and normalizes once. Passive attribution uses
the existing private double batches with zero internal objective weights, keeping
independent row sums finite without evaluating an unused aggregate. It runs each
selected group once, returns zero risk columns, records one attempt/no native
widths and ignores tape limits. Its known-shape scratch admission uses the actual
passive worker/slot types. Dedicated narrowing tests compare requested width two
with explicit width one under the identical finite budgets and an independent
single-product oracle. All selected group widths validate before any submission;
unused groups retain zero width.

## Valuation surfaces

The C++ weighted and native/passive attribution entries exist. Both take
the portfolio and path count first, followed by
request, valuation and simulation settings. Use separate weighted and attribution
methods rather than a mode flag with incompatible output types:

```cpp
// Implemented weighted entry.
PortfolioWeightedRiskResult_ ValuePortfolioByMonteCarloWithWeightedRisk(
    const Handle_<ScriptPortfolioData_>& portfolio, int numPath,
    const PortfolioWeightedRiskRequest_& request = {},
    const ScriptValuationSettings_& valuation = {},
    const MonteCarloSettings_& simulation = DefaultRiskMonteCarloSettings());

// Implemented native/passive attribution entry.
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

The weighted result owns numeric data and trade/group metadata. Selected/complete axes
use the namespaced IDs defined in the specification. Expose trade IDs and
model-owner ordinals separately from risk labels. Execution metadata contains
group membership and scenario counts; it must distinguish scenario generation
from trade evaluator invocations. Matrix getters construct detached one-row
copies from the retained gradient; const axis/metadata accessors borrow from the
owning result. This preserves the exact weighted retained numeric payload without
retaining a second gradient. Report projection overflow fails before publication.
Per-trade provenance uses the shared scalar capture helper without changing legacy
scalar, weighted or Jacobian execution. The result retains no active state.

The native attribution result owns `Values()` and one retained `(m,n)` matrix,
with detached raw/reported matrix getters. It shares owning axis/provenance
storage with the weighted result and retains no weights or duplicate matrix.
Execution records the requested maximum and per-group actual widths, attempts and
scenario/evaluation/reversal work. Report overflow identifies the original failing
row's trade/output, including when a model input belongs to several trades.

## Binding projection

Python exposes `ScriptPortfolio_New(trade_ids, products, modelData)` with equal
nonzero sequence lengths and strict element types. Construction copies/seals all
execution inputs. A repeated Python model object establishes a repeated owner;
two different model objects remain separate even when their contents agree.
Evaluation uses `PortfolioMonteCarlo_ValueWithWeightedRisk` and
`PortfolioMonteCarlo_ValueWithJacobianRisk`; request, valuation and simulation
remain keyword-only. Immutable request/result properties return detached copies.

Excel exposes `ScriptPortfolio_New(name, trades)`, where `trades` is a physical
three-column table of trade ID, product handle and model handle. Resolve handles
before assigning owners, reject malformed/ragged cells without coercion, and
store a sealed immutable portfolio. Use the same evaluation names and ordinary
immutable request/result factories/getters. Include a shape getter for zero risk
columns, as in the current Jacobian surface. Machinist owns generated exports.

The Excel projection uses `PortfolioWeightedRiskRequest_New` and
`PortfolioJacobianRiskRequest_New` with immutable typed result handles. Shared
`PortfolioRiskResult_Get_*` getters accept either completed portfolio result kind
through a checked storable handle. `Get_Objective` requires weighted results.
`Get_Values` returns id/label/slot/mean/weight; attribution weights are blank.
`Get_Shape` distinguishes weighted `(1,n)` from attribution `(m,n)`, including
zero columns. Execution rows retain group/owner positions, dimensions, requested
and actual widths, work and whole-request peaks. Sampling uses a long-form table
of group/sample/field/ordinal/subordinal/value; vector lengths and empty nested
rows remain explicit. Trade-specific provenance/history/product/model snapshot
getters use strict zero-based original trade ordinals and perform no valuation.
Raw Windows checks reject malformed physical tables, text/bool ordinals and path
counts before generated conversion. Typed functions remain portable contracts;
generated Windows exports and runtime acceptance are separate required gates.

Typical Python use (with previously constructed products/model/settings):

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

## Remaining implementation decisions

All six accepted families pass snapshot/coordinate and independent native/passive
weighted and blocked common-path tests. Weighted result metadata is
additive and adds no fields or runtime work to old results. Validate strict
Python/Excel construction against analogous risk request parsers. Names may be
adjusted to fit registration conventions before implementation; mathematical
identities and failure semantics remain controlled by the specification.
