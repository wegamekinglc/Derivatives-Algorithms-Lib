# Native recorded solve API decisions

Status: active F03 recording increment. All three overloads and a numeric
RHS-only contribution are implemented locally. Forty-two new cases pass,
covering analytic/reference gradients, aliases, multiple RHS, vector channels,
serial/shared solves, checkpoint/reset ownership and failure invalidation.
Exact storage admission and release pass locally; performance and final CI
acceptance remain open. This note
specifies the complete required surface.
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

## Active resource implementation

The prototype is pushed at `c79bcc58`, whose 35 checks pass. The subsequent
local resource implementation admits finite budgets before owned allocation.
MeasureTape and tape-budget admission now include a fifth domain for reverse
events, with exact descriptor/table/cache capacity and reverse scratch peaks.
Current head CI does not validate this unpublished source increment.

For n rows and m RHS columns, the retained variable payload consists of LU
`n*n*sizeof(double)`, swaps `n*sizeof(int)`, X `n*m*sizeof(double)`, active A
bindings `n*n*sizeof(Number_)` when present, active B bindings
`n*m*sizeof(Number_)` when present, and output bindings
`n*m*sizeof(Number_)`. Add the actual event descriptor, lazy owner and event-table
capacities. Measure allocated vector capacities, including replacement overlap,
rather than assuming logical extents describe every allocation.

Per-channel reverse scratch includes collected seeds and transpose/RHS
contributions, each `n*m*sizeof(double)`, and a matrix contribution
`n*n*sizeof(double)` only for active A. Forward snapshots and intermediate
construction copies also affect peak capacity. These formulas identify required
storage; actual allocation measurements establish admission and peak evidence.
Returned X container storage belongs to the caller; its scalar output slots
already belong to the tape. Distinguish that caller buffer from retained event
output bindings in the resource report.

Admission runs before owned allocation and refunds unsuccessful allocations.
An event-specific cold allocation context may reuse existing buffer accounting;
ordinary node recording/propagation needs no added resource hook. Resource
ownership must survive adjoint clear and prefix reuse, follow suffix destruction,
and release before scalar reset. Existing request buffer scopes and worker tape
readmission must remain compatible; no silent uncharged cache or escaped budget
reference is acceptable.

The controlling resource RED is
`AADLinearSolveTest.TestFiniteCapacityIncludesCacheAndResetReturnsToBaseline`.
Its original admission-guard failure is retained; GREEN now shows cache-inclusive
MeasureTape/budget equality, retained cache after clear/reverse and exact release
on scope close. Tests additionally cover exact peak/one-byte-short budgets,
passive binding and scratch omission, replacement overlap, constructor/copy/
buffer allocation failures, suffix release, detached readmission, parent scratch
rejection and recovery, aligned/zero allocations and finite vector widths.
Cross-thread and foreign-live-slot rejection precede output publication. The
reverse-event capability contract is updated under its own RED/GREEN; actual
final-head platform/performance acceptance is still required before merge.
