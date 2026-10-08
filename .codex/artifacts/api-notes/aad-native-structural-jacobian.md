# Native structural Jacobian API

Audience: explicit C++ native callers with proven row supports and an independent
RecordingScope. Numeric planning remains in its tape-free header; native binding
and execution live in `dal/math/aad/structuraljacobiannative.hpp`.

```cpp
NativeStructuralInputs_ BindStructuralJacobianInputs(
    RecordingScope_* recording, const Vector_<Number_>& inputs);

Matrix_<> ExecuteStructuralJacobian(
    RecordingScope_* recording, const NativeStructuralInputs_& bindings,
    const StructuralJacobianPlan_& plan, const Vector_<Number_>& inputs,
    const Vector_<Number_>& outputs);
```

The immutable binding token exposes its input count and recording mode/width,
without exposing raw slot identities. It snapshots input identities and scope
identity; it owns no Number, tape or scope. Copies own metadata and const reads
are safe; execution remains on the scope's owning thread. Reassign moved-from
tokens before using them.

Required sequence: choose mode/width, enter scope, register inputs, StartRecording,
bind inputs before constructing any expression nodes or reverse events, build
outputs, FinishRecording, execute. Binding requires only root nodes and no events.
Its slot check uses the existing bridge, so no ordinary header/layout changes
are needed. A plan can survive cleanup; a binding cannot execute on a new scope.

Execution explicitly mutates recording adjoints/state through its pointer.
Supplied Number vectors need not mutate their handles. Additive seeding supports
aliased outputs and direct independent roots without graph extension. Repeated
execution clears every lane per block; a last partial block does not resize width.
An empty-color plan returns its exact zero shape after complete validation.

All output slots, including constants, must be materialized and live. Invalid
state, mode, count, input order/identity, duplicate roots, slots or non-finite data
raise DAL exceptions. Validation precedes seeding. A failed backend reverse uses
existing FAILED semantics; non-finite numeric harvest returns no partial result.

The plan's budget covers the simultaneous directions and full result. Slot
metadata and tape capacity remain outside it. No public Python/Excel or strategy
setting is added. Existing calls keep their behavior. Financial proof/identity is
still required before integration or default promotion.

Rejected alternatives: infer independence from a zero-argument node after graph
construction (solve events publish such nodes); add scope/Number fields for a
new registry; resize live widths; build payoff roots after READY; seed by
assignment; infer supports from a previous matrix.
