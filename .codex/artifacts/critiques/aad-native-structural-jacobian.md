# Native structural Jacobian critique

Verdict: Proceed with caveats.

## Findings resolved before implementation

- Zero-argument nodes alone do not prove independence: solver events publish
  dependent leaves. Binding requires RECORDING with exclusively zero-argument
  live nodes and no reverse events, then unique physical input identities.
- Scope identity must reject old/foreign tokens before retained identities can
  be dereferenced. Validate current live supplied slots before address/adjoint
  access. Ordinary Number lifetime assumptions remain explicit.
- Clear the complete graph once per block and add output seeds. Per-row clearing
  and assignment lose aliases or stale leaf contributions.
- Width is fixed before registration. Last-block padding must retain zero lanes;
  no new root node or width mutation belongs to READY execution.
- Reuse the existing identity/live-slot bridge and scalar adjoint addresses after
  validation. No old header extension, scope layout change or hot-path branch is
  justified. All 177 existing object identities should remain reusable.

## Significant concerns

Binding identity does not prove mathematical supports. Keep financial routing,
base closure, canonical descriptor equality, branch invalidation and default
selection open. Include exact zero dependencies at another numerical point.

Check all counts, slots and finite values before seed clearing. Preserve READY
after validation errors. A backend failure cannot promise rollback; a healthy
request after non-finite numeric harvest tests clearing when graph state remains
valid. Explicitly materialize constant output slots.

Independent analytic references cover full matrices, solve-event composition,
aliases, direct inputs and partial blocks. Shared immutable plans use per-thread
fresh bindings, never migrated active Numbers. New complete-request costs remain
informational until measured selection/integration is accepted.

## Author questions

None for this increment. Implement locally under the spec/API and RED/GREEN
contracts, then review all source and exact publication evidence before merging.
