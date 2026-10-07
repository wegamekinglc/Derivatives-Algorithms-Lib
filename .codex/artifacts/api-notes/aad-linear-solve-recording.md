# Native recorded solve API decisions

Status: active F03 recording increment; no new API is implemented yet.
Controlling [specification](../specs/aad-linear-solve-recording.md).

## Audience and surface

Core C++ callers composing dense solves with native Number expressions use
`Dal::AAD::LinearSolve(RecordingScope_*, matrix, rhs, relativePivotTolerance)`.
Required arguments precede the optional tolerance, whose default matches the
accepted numeric `LinearSolvePullback_`. The explicit scope pointer shows the
recording mutation and prevents silently creating an independent recording.

Provide three type-distinguished overloads: active/active, passive/active,
active/passive. Return owning active X, preserving RHS/output order. No bool
activity mask or long positional setting list is needed. Passive/passive callers
keep the numeric operator. Adding this core method does not add a valuation,
Python or Excel request; those remaining full-plan surfaces require their own
typed result/method and budget acceptance.

Typical lifecycle: register inputs, start the scope, construct scalar matrix
producers, call LinearSolve, construct consumers, finish, clear, seed and reverse.
Use existing NativeOperations seed/read methods. The same Reverse/Suffix/Prefix
entry points must handle events; an extra user-managed pullback is insufficient.

## Ownership and compatibility

An internal event owner lives on the tape and is created only for the first
recorded operation. It owns callbacks/caches and canonical scalar boundaries.
Number copies remain aliases of their original slot. Tape itself is a
single-owner graph, not a duplicable value: current BlockList copying retains
iterators into original storage and diagnostic builds already prohibit it.
Delete Tape copy/move in both modes when adding unique event ownership. Record
this compiler-visible correction in the changelog and check installed consumers.
Do not change Number/TapNode size or scalar arithmetic/allocation loops.

Source Number lifetime checks remain conditional as documented today. Event
bindings own slot identity and validate owner/live interval in both modes; do not
promise to diagnose a preexisting stale OFF Number whose slot address was reused.

## Errors and capabilities

Errors identify LinearSolve, the offending scope/input/seed/pivot/range/budget
constraint and failed graph recovery. Failed graphs cannot yield publishable
adjoints; reset is required. Selectively passive inputs preserve the same solve
while omitting unused gradient allocations. Reverse-event capability becomes true
only after full first-order event acceptance; nesting/higher order stay false.

## Rejected alternatives

- Recording elimination repeats scalar intermediates and misses the operator goal.
- Adding a virtual callback to every TapNode burdens all existing requests.
- Separate special reverse calls silently return incorrect gradients when callers
  use existing native reverse functions.
- Referencing caller matrices or consuming the decomposition on first reverse
  breaks mutation isolation and repeated sweeps.
- Pretending that cache bytes belong to the four existing scalar block lists
  bypasses finite capacity admission; event payload needs explicit accounting.

No user decision is pending. Review/performance can require an internal storage
revision without weakening this surface or its lifecycle/resource contract.
