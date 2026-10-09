# Structural Jacobian plans

`AAD::PlanStructuralJacobian` in
[`dal/math/aad/structuraljacobian.hpp`](../../dal-cpp/dal/math/aad/structuraljacobian.hpp)
owns conservative row supports and colors for reconstructing a full Jacobian
from compressed direction gradients. The numeric API accepts supplied gradients;
the native adapter below binds live inputs and executes the required VJPs.

## Supports and compressed directions

For an output-by-input Jacobian $J$, row support $S_i$ promises that
$J_{ij}=0$ whenever $j\notin S_i$. Supports describe structural dependence over
the intended computation, including supported coordinates whose derivative is
zero at the current parameter point. For example, both inputs belong to the
support of $x_0x_1$ even at the origin.

The caller must prove the supports. Use a conservative full row when its
dependencies are unknown. A support may include extra columns; this preserves
correctness but can increase direction count. Empty support means a proven zero
row. Neither numerical zeros nor nominal instrument maturity establishes proof.

The plan sorts and deduplicates each support without changing the ordered axes.
It visits rows in order and assigns the smallest available color. Same-color
rows have disjoint supports; greedy coloring need not be optimal. For color $c$,
the supplied direction is the complete VJP with a unit seed on every row of that
color. Its component at a supported column is therefore attributable to one row.
Recovery assigns these components and fills all excluded coordinates with zero.

The following matrix needs two colors, with rows 0 and 1 sharing color 0:

$$
J=\begin{pmatrix}2&3&0&0\\0&0&4&0\\0&5&0&6\end{pmatrix}.
$$

```cpp
#include <dal/math/aad/structuraljacobian.hpp>

const auto plan = Dal::AAD::PlanStructuralJacobian(4, {{0, 1}, {2}, {1, 3}}, {160});
Dal::Matrix_<> gradients(2, 4, 0.0);
gradients(0, 0) = 2.0;
gradients(0, 1) = 3.0;
gradients(0, 2) = 4.0;
gradients(1, 1) = 5.0;
gradients(1, 3) = 6.0;
const auto jacobian = Dal::AAD::RecoverStructuralJacobian(plan, gradients);
```

Here the gradients are independent analytic values for the displayed matrix.
In an AAD caller, clear the previous seeds/adjoints and execute each corresponding
VJP under the recording's existing mode/width contract. `ColorRows(color)` gives
the seed rows; the numeric API itself does not seed or reverse a tape.

## Shape, ownership and admission

`Inputs`, `Outputs`, `RowSupport`, `RowColor`, `ColorCount` and `ColorRows` expose
immutable metadata. Empty rows have no `RowColor` and use no direction. Input
and output counts must fit nonnegative Matrix int dimensions; empty axes retain
their exact shape, including 3-by-0 and 0-by-4. Incidence storage uses only
columns present in supports, so a wide unused axis needs no full-axis array.

Recovery requires exactly `ColorCount` rows and `Inputs` columns. Every supplied
gradient must be finite, including values excluded by a row's support. Wrong
shapes, invalid indices, overflow, insufficient budgets and non-finite gradients
raise DAL exceptions before a result is published.

The plan snapshots its descriptor. Copies own independent metadata; references
returned by its getters remain valid while that plan remains alive and unchanged.
Reassign a moved-from plan before using its getters or recovery. Concurrent const
use is supported. Recovery owns its result independently of the plan and gradients.

With $m$ outputs, $n$ inputs and $c$ colors, `ResultBytes` is $8mn$,
`DirectionBytes` is $8cn$ and `NumericPayloadBytes` is their checked sum.
`StructuralJacobianSettings_::numericPayloadBudgetBytes_` optionally caps this
simultaneous numeric payload. The example needs 96 result bytes and 64 direction
bytes, so budget 160 admits it and budget 159 rejects it. Budget zero is valid
for an empty payload. Plan metadata, allocation overhead, tape storage and other
caller data are outside this budget.

## Native AAD execution

[`dal/math/aad/structuraljacobiannative.hpp`](../../dal-cpp/dal/math/aad/structuraljacobiannative.hpp)
provides `BindStructuralJacobianInputs` and `ExecuteStructuralJacobian`. Choose
the recording mode before registering inputs, then bind immediately after
`StartRecording`, before constructing dependent expressions or reverse events.
Binding requires distinct live inputs, exclusively independent root nodes in the
current graph, and no reverse events. The following example reconstructs the
same complete 3-by-4 matrix:

```cpp
#include <dal/math/aad/structuraljacobiannative.hpp>

using namespace Dal;
using namespace Dal::AAD;
const auto plan = PlanStructuralJacobian(4, {{0, 1}, {2}, {1, 3}}, {160});
const auto mode = SetNumResultsForAAD(true, 2);
RecordingScope_ recording;
Vector_<Number_> inputs(4);
for (size_t column = 0; column < inputs.size(); ++column)
    recording.RegisterInput(inputs[column], static_cast<double>(column + 1));
recording.StartRecording();
const auto bindings = BindStructuralJacobianInputs(&recording, inputs);
const Vector_<Number_> outputs = {
    2.0 * inputs[0] + 3.0 * inputs[1],
    4.0 * inputs[2],
    5.0 * inputs[1] + 6.0 * inputs[3]
};
recording.FinishRecording();
const auto jacobian = ExecuteStructuralJacobian(
    &recording, bindings, plan, inputs, outputs);
```

Execution checks READY state, recording identity, fixed mode/width, counts,
input order and live slots, and finite input/output values before clearing
seeds. Every output must have a live slot: a materialized constant such as
`Number_(5.0)` is valid; a default-constructed Number without a slot is rejected.
Direct inputs, intermediate nodes, aliased outputs and recorded solver outputs
are supported. Unit seeds accumulate for aliases.

The scalar mode executes one color per reverse. Vector mode executes up to the
existing width per reverse, clearing all graph adjoints before each block.
Unused lanes in the final block stay zero; execution does not resize the width
or add graph nodes. An empty-color plan validates the complete request and
returns the exact zero shape without reversing or clearing existing adjoints.
Validation errors preserve existing seeds. A backend reverse failure follows
the recording's FAILED semantics; non-finite harvested gradients publish no
partial result.

`NativeStructuralInputs_` owns binding metadata and exposes `Inputs`,
`VectorAdjoints` and `Width`. It owns no Numbers, scope or tape. Copies own their
metadata; reassign moved-from tokens before use. Execution belongs to the
recording's owner thread and requires live Numbers under the ordinary Number
lifetime contract; diagnostic builds additionally check generations. A token
cannot execute after scope cleanup or on another recording, even if storage
addresses are reused. The returned matrix owns its data after cleanup.

Binding identity establishes which live slots are used; the caller still proves
mathematical supports for the ordered axes. Reuse a numeric plan at another
parameter point only when its support proof remains valid, and re-record with
fresh live inputs and bindings. Independent threads may share a const plan and
use their own scopes. The numeric payload budget above excludes binding
metadata and tape capacity. Native execution is explicit and does not select a
default risk strategy.

## Rate-trade dependency provider

[`dal/curve/ratestructuraljacobian.hpp`](../../dal-cpp/dal/curve/ratestructuraljacobian.hpp)
provides `CaptureRateStructuralJacobian`, `PlanRateStructuralJacobian` and
`SameRateStructuralJacobianStructure` for the closed deposit, FRA, future, OIS,
IRS, basis-swap and XCCY trade families. Capture takes ordered trades, the
current `RatePricingMarket_` and an explicit ordered input axis. A
`RateCurveParameterCoordinate_` contains a market component key and its
zero-based free-parameter ordinal, using the same layout as
`DescribeCurveFreeParameters`. LogDF anchors are pinned; PWLF left/right
parameters are distinct coordinates.

The provider resolves the same consumed curve roots as pricing and follows their
complete exact-family base closure. Each row conservatively includes every
requested coordinate belonging to a consumed curve or reachable base. It uses
full component blocks, including a forecast with a currently zero contribution.
Interleaving component coordinates preserves their actual input column positions.
For example, with axis `[C, D, A, E, B]`, bases `C -> B` and `E -> A`, an IRS
using forecast C/discount A has support `[0, 2, 4]`; a deposit discounted on D
has `[1]`; an FRA using forecast E/discount A has `[2, 3]`. These rows need two
greedy colors.

```cpp
#include <dal/curve/ratestructuraljacobian.hpp>

const Dal::Vector_<Dal::RateCurveParameterCoordinate_> axis = {
    {"C", 0}, {"D", 0}, {"A", 0}, {"E", 0}, {"B", 0}
};
const auto descriptor = Dal::CaptureRateStructuralJacobian(trades, market, axis);
const auto plan = Dal::PlanRateStructuralJacobian(descriptor);
```

Here `trades` and `market` are the caller's current immutable request snapshots.
Check `Available()` and `Reason()` before constructing a numeric plan when
unavailable proof is an expected outcome. Unknown or derived curve classes,
missing consumed curves, incomplete base graphs, unresolved XCCY routing,
unregistered XCCY consumed roots and unrepresentable parameter layouts report
unavailable proof. `RowSupport` and plan construction reject it. Missing input
keys, duplicate physical independent coordinates and out-of-range represented
parameter ordinals are invalid requests and throw, including later malformed
coordinates following an unsupported curve. Referenced alias keys are allowed
when each physical independent input appears only once on the axis.

The immutable descriptor owns `InputAxis`, `OutputAxis`, supports and complete
structural records. Copies share this payload; reassign moved-from descriptors
before use. It retains no curve, convention, market handle, Number, tape or raw
pointer. Metadata survives source cleanup and permits concurrent const use.
Empty input/output axes keep their requested shape. Availability proves
dependence; it does not guarantee that current pricing succeeds, for example
when a required historical fixing is missing.

Before reuse, capture the current request again and compare its descriptor with
`plan.Descriptor()` using `SameRateStructuralJacobianStructure`. Equality checks
ordered identities, curve definitions/free-parameter layouts, base and alias
topology, resolved routes, trade/convention fields, actual generated payment,
accrual and observation geometry, valuation time and fixing availability. A
fixing exactly at valuation is included even when the historical-request list
is empty. Curve numerical values may change under matching structure; trade or
fixing scalar changes may conservatively invalidate reuse. Shape, names,
addresses and hash equality alone are insufficient.

Capture does not color a second plan merely to validate a cache hit. Only cold
or invalidated requests construct numeric color/recovery metadata. Capture and
planning provide passive metadata; the execution functions below record current
pricing and return its derivatives.

## Complete rate-trade Jacobians

[`dal/curve/rateparameterjacobian.hpp`](../../dal-cpp/dal/curve/rateparameterjacobian.hpp)
provides explicit dense and compressed execution:

```cpp
#include <dal/curve/rateparameterjacobian.hpp>

const auto dense = Dal::RateTradeParameterJacobian(trades, market, axis);
const auto descriptor = Dal::CaptureRateStructuralJacobian(trades, market, axis);
const auto plan = Dal::PlanRateStructuralJacobian(descriptor);
Dal::RateJacobianExecutionSettings_ settings;
settings.vectorAdjoints_ = true;
settings.adjointWidth_ = 2;
const auto compressed = Dal::ExecuteRateStructuralJacobian(
    trades, market, plan, settings);
```

Both results own `prices_`, `jacobian_`, `inputAxis_` and `outputAxis_`. The matrix
has one row per positional trade and one column per requested free parameter;
empty dimensions and repeated trade rows retain their shape. Prices keep each
trade's actual PV currency. Derivatives use native curve parameter units and
that row's PV currency; the operation performs no quote mapping, FX spot delta
or currency conversion.

The seven closed deposit, FRA, future, OIS, IRS, basis-swap and XCCY swap families
use the existing pricing formulas, schedules, fixings and routing. Curves must use the
four exact native representations: piecewise-constant forwards, piecewise-linear
forwards, log-discount nodes or zero-rate nodes. Every transitive base is rebuilt
actively, preserving shared curves. Selected free parameters are independent;
an unselected curve still transmits risk from a selected base.

Dense execution records full row supports. Compressed execution recaptures the
current complete structure and requires equality with the supplied plan before
binding or seeding. Matching numeric curve changes re-record current derivatives;
changed trade terms, geometry, fixings, layouts or routing require a fresh plan.
Unavailable proof or stale identity raises an exception. Choose dense execution
explicitly when appropriate; neither function selects an AUTO strategy.

The default is scalar adjoints with width one. Vector adjoints use the requested
native width and report actual `reverseDirections_` and `reverseSweeps_`.
Proven zero rows need no reverse. Each call owns its recording scope and restores
the caller's mode, including on failure. Independent threads may share a const
plan and use separate calls; nesting inside an active recording is rejected.

`numericPayloadBudgetBytes_` caps the result and direction matrices together,
including reused plans. It excludes tape storage, price/axis metadata and RSS.
Invalid or duplicate physical coordinates, failed pricing, non-finite PVs or
derivatives, and insufficient budgets raise exceptions without publishing a
partial matrix.

Compare complete request costs when choosing between the two functions: include
capture for cold plans, current identity validation for reused plans, preparation,
recording, all reverse blocks, recovery, result storage and cleanup. Fewer reverse
directions alone do not establish a speed advantage.

## Reuse and risk semantics

Before reusing a plan, establish that its ordered axes and conservative supports
remain valid for the current computation. Observation changes, curve routing,
parameterization or newly accessible control-flow branches can invalidate a
support. A matching matrix shape or previous numerical zero is insufficient.
Rebuild with proven supports or use a dense method when proof is unavailable.
The numeric plan provides no automatic graph analysis or structure cache.

A fixed-weight gradient needs one direct VJP; constructing a full Jacobian adds
work to that request. See [weighted risk](aad.md#weighted-script-risk-results).
Full request cost includes dependency analysis, actual recording/reverse,
reconstruction and result storage; reduced direction count alone does not
establish a production speedup.
