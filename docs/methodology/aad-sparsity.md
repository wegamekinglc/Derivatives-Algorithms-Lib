# Structural Jacobian plans

`AAD::PlanStructuralJacobian` in
[`dal/math/aad/structuraljacobian.hpp`](../../dal-cpp/dal/math/aad/structuraljacobian.hpp)
owns conservative row supports and colors for reconstructing a full Jacobian
from supplied compressed direction gradients. It builds numeric metadata and
recovers matrices; the caller provides the actual VJP execution.

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
