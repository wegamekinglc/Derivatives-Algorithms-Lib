# Native path segmentation core

Status: active; first P05 increment, based on accepted PR #511 at
`ddea35911c8e5f3b759e70b7fe3c12a9c6dd0594`.

## Problem and boundary

The existing recording checkpoint keeps one initialization prefix while paths
are evaluated. It does not recompute discarded pieces of one long path.
The controlling full plan, section 5.5, requires fresh local derivatives,
complete boundary state, exact passive random inputs and correct parameter
accumulation across segments.

This increment implements the standalone native reverse core for a fixed-size
state transition supplied by an immutable kernel. The following financial
increment must adapt complete model/evaluator state, live observations,
cashflows, vectors and historical initialization. This core alone does not
complete P05 or enable segmentation in ordinary Monte Carlo/LSM calls.

## Kernel contract

A kernel declares step count, state width and a fixed number of branch-trace
slots per step. It provides passive-double and native-number versions of
initialization, transition and terminal objective. Both versions evaluate the
same formulas, smoothing and control flow. Each transition returns its complete
next state, direct scalar objective contribution and complete branch decisions.
Unused trace slots use a stable sentinel; traces are compared entry by entry,
not by a hash. The kernel owns or immutably borrows all other data for the call.
Active methods use only supplied fresh active inputs and passive immutable data.

Parameters and the full passive driver vector are copied once for each request.
Recomputation reads this vector and does not advance an RNG. A bridge-transformed
vector is therefore retained in full; an index alone is not recovery evidence.

## Functional requirements

1. Copy the request inputs, run passive initialization and every transition,
   retaining complete state at segment boundaries, direct contribution sums
   and exact branch traces. Validate declared shapes and all numeric values.
2. Evaluate the passive terminal objective; the returned value includes all
   direct transition contributions exactly once.
3. Reverse the terminal objective on freshly registered terminal state and
   parameters. Preserve its state seeds and direct parameter gradient.
4. Visit segments in reverse order. Register each boundary state and parameter
   vector again, recompute every local transition and local derivative, and
   compare the end state, direct contribution and full branch trace before
   setting any reverse seed. A mismatch rejects the whole request.
5. Seed segment outputs with the incoming state adjoints and seed its direct
   contribution with one. Accumulate each segment's parameter contribution;
   propagate its input-state adjoints to the preceding segment. Aliased state
   outputs accumulate rather than overwrite seeds.
6. Reverse fresh initialization once with the first state's adjoints and add
   its parameter contribution. Never keep and manually reconnect the same
   initialization dependency twice.
7. Results own passive value, gradient and execution statistics. No active
   number, iterator or snapshot borrowing survives recording closure.
8. Zero steps, zero state width and empty parameters remain defined. Positive
   segment length is required; a length greater than the path gives one segment.
   Dimension arithmetic and retained-checkpoint budget admission precede
   checkpoint allocation or kernel evaluation.
9. The checkpoint budget covers boundary states, stored contribution sums and
   branch traces. The separate tape budget uses actual allocated capacity and
   existing cleanup reservation. Report these components without claiming they
   include arbitrary opaque kernel storage or process RSS.
10. Every independent recording closes on success and exception. Restore the
    caller's scalar/vector mode, reject unsupported nesting before changing its
    graph, and allow a healthy request after mismatch, invalid data or budget
    failure. Concurrent requests use independent thread-local tapes.
11. Report passive steps, recomputed steps, segment count, reverse sweeps,
   retained checkpoint capacity, peak tape capacity and separate cleanup reserve. Full cost includes
    input copies, prepass, snapshotting, fresh recordings, reverse and cleanup.
12. Use native scalar reverse. Ordinary full-graph callers and capability flags
    retain their accepted behavior. No external backend or automatic selector
    is introduced by this increment.

## Acceptance

The three-step oracle `z_next = a*z + u`, `z0 = b`, `V = z3*z3`, with
`a=2`, `b=1`, `u=(1,-1,3)` yields value 169 and gradients `(390,208)`.
Check segment lengths 1, 2, 3 and larger than the path. Add an independent
multistate oracle with direct contributions and parameter-dependent
initialization, aliasing, empty shapes and repeated fresh parameter points.

Focused RED/GREEN cases cover exact and insufficient budgets, invalid shapes,
nonfinite values, branch mismatch even when end values agree, numeric mismatch,
failed-then-successful execution, mode restoration and concurrent detached
results. Strict OFF/combined compilation and an installed-only consumer follow.

Freeze accepted archive/header identity. Existing paths are reused when their
source and executable identity apply. Select only short and long fixed-state
paths plus one direct-contribution case for complete cost and storage evidence;
do not run an unrelated matrix. Keep the original two best-of-ten rounds and
noise rule. A memory improvement may cost time and must be reported as such.

## Remaining P05 work

Financial state flattening/liveness, same-smoothing evaluator replay, actual
model initialization, path-number/RNG semantics, resource decisions and the
complete financial request acceptance remain separate work. LSM and early
exercise need a dedicated contract. Shared prepared programs and opaque kernel
storage must be accounted for explicitly in that financial acceptance.
