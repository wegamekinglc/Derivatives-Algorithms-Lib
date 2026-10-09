# Dense planner and measured rate strategy API

Status: active; controlling [specification](../specs/aad-rate-jacobian-strategy.md).

`AAD::PlanDenseJacobian(size_t inputs, size_t outputs,
const StructuralJacobianSettings_& settings = {})` returns the existing owning
`StructuralJacobianPlan_`. It is usable by the accepted native binding/execution
API and avoids introducing a second executor or result type. Arguments describe
the matrix axes; the optional budget retains its existing numeric payload units.

Existing financial interfaces remain the comparable explicit strategies:
`RateTradeParameterJacobian` and `ExecuteRateStructuralJacobian`. The latter
requires a proven current descriptor and already rejects stale structure.

The planner rejects dimensions outside the matrix int range, overflowing numeric
extents, or insufficient combined payload before metadata allocation. Empty
axes remain valid. No Python/Excel function or generated enum is added by the
dense numeric planner; eventual financial binding work remains in the full plan.

Add the explicit overload
`RateTradeParameterJacobian(trades, market, inputAxis, cachedPlan, settings = {})`.
The requested axis belongs to the current request and may differ from the
stored axis. Matching proven metadata executes compressed reverse on a freshly
recorded graph. Unavailable or mismatched metadata executes dense reverse with
the current rows/axis. All results retain the existing owning result type and
actual direction/sweep counts. Settings and budgets apply to the strategy that
actually executes. A stale plan whose dense fallback exceeds the budget fails.
The strict `ExecuteRateStructuralJacobian` continues to reject stale plans.

The measured lightweight cases favor dense execution; do not introduce a global
automatic cutoff or hidden repeated live pricing. Ordinary requests incur no
structural capture or timer. The opt-in overload performs one capture and is
appropriate only after workload-specific measurement. Native forward is
unavailable for this financial path; theoretical direction counts cannot
authorize an unsupported mode.
