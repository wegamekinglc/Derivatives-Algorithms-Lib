# AAD recording lifecycle: implementation contract

Status: active design; the interfaces below are proposed until their executable
acceptance criteria pass. This specifies D01 and the lifecycle boundary needed by
D02/D03 in the [controlling plan](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/9dd9282bb2c517c838a6576a95c9b7a937e750af/.codex/artifacts/plans/aad-improvement-plan.md).

## Problem and compatibility constraints

`curve/tapeguard.hpp` currently rewinds the default tape both on entry and on exit.
An inner guard consequently destroys an outer graph without reporting the nested
use. Its destructor catches cleanup failures without retaining any evidence that
the tape is unusable. Simulation manages the same lifecycle through separate raw
calls. Native `ZeroAdjoints` currently clears scalar slots even in vector mode.

Move ownership and lifecycle into the AAD layer. Keep the existing independent
curve-Jacobian behavior, input registration order, path-prefix accumulation, public
valuation results, backend-specific numerical conventions, and capacity reuse.
The release configuration must not add fields to every node or checks to expression
evaluation, registration, or propagation inner loops for this feature.

The default tape remains thread local. The first implementation supports one
independent recording and one replaceable path checkpoint per thread. It does not
promise nested differentiation, independently activated tapes, or movable active
recordings. Unsupported nesting is an error before any tape mutation.

## Required operations and state

R01. A non-copyable, non-movable `AAD::RecordingScope_` owns an independent recording
on the calling thread's default tape. Construction validates the tape and checks
for an existing scope before rewinding. Inputs are registered before
`StartRecording()`, preserving current XAD/Adept/CoDiPack setup conventions.

R02. States distinguish input registration, graph recording, ready for reverse,
reversing, failed, and closed. `StartRecording()` establishes the backend's start
position. `FinishRecording()` makes the current graph available to reverse.
Operations reject incompatible states and identify the operation and constraint.
Expressions continue using the existing active type; callers must finish graph
construction before a sweep. Optional D02 diagnostics will enforce expression
lifetimes separately.

R03. Reverse operations distinguish the entire graph, the suffix after a checkpoint,
and the checkpoint prefix. They keep native consumed-intermediate clearing and
leaf accumulation. Repeated sweeps require caller-controlled seeding and clearing;
scope ownership must not silently change accumulation semantics.

R04. `Checkpoint_` is an opaque handle with recording identity, checkpoint generation,
owner thread/context, and scalar/vector width information. Creating a replacement
checkpoint invalidates the previous handle. Restore retains the prefix and its
accumulated adjoints, discards the suffix, and returns to graph-recording state.
Wrong-owner, replaced, closed-recording, and wrong-mode handles are rejected before
backend position access. The first implementation stores one backend mark, as the
current adapters do; it does not expose raw iterators as stable checkpoints.

R05. Full adjoint clearing covers every live scalar or vector slot. A distinct suffix
restore keeps prefix adjoints. These operations cannot substitute for each other.
Recording mode cannot change while a scoped graph is live. Public mode selection
must validate this at its boundary without adding per-node release checks.

R06. Normal callers use explicit `Close()` to rewind and report cleanup failure.
Close is idempotent after successful closure. Destruction performs noexcept fallback
cleanup so a business exception remains the primary exception. If cleanup fails,
the context is marked unusable; the next independent entry must rebuild it
successfully before admitting work. A failed rebuild rejects the entry. A failed
reverse also makes the scope unusable until close/reset; no partial result is
reported as a successful scope outcome.

R07. Operations validate the owner thread at the boundary. Scope lifetime and
destruction remain confined to that thread. Active objects must be destroyed before
thread exit. No release-mode lifetime scheme should dereference another thread's
expired thread-local context.

R08. The existing `TapeGuard_` becomes a compatibility wrapper around independent
scope ownership. Its curve users explicitly finish and close where possible.
Migrate analytic curve Jacobians first, ordinary Monte Carlo worker batches second,
and LSM replay after examining its separate training/pricing scopes. Per-path work
uses checkpoint windows within one worker scope, rather than creating a scope for
each path. Tasks are drained before owners and active objects leave their lifetime.

## Acceptance criteria

| ID | Executable evidence |
|---|---|
| A01 | A smooth scalar graph has the same value and derivatives under raw and scoped execution on native, XAD, CoDiPack, and Adept. |
| A02 | Attempted nested `RecordingScope_` and nested legacy `TapeGuard_` throw before rewind; the outer graph still produces its expected gradient. |
| A03 | Exceptions during registration, graph construction, reverse, and result extraction release ownership; a subsequent independent valuation succeeds. |
| A04 | Explicit close and normal destruction reset the recording while retaining reusable capacity where the backend supports it. |
| A05 | Three independently rebuilt suffixes accumulate at the checkpoint, then one prefix sweep gives the independently calculated total derivative. |
| A06 | Replaced, foreign, wrong-mode, and previous-recording checkpoint handles are rejected; valid prefix values survive suffix restoration. |
| A07 | Reverse before finish, registration/start after finish, and reverse after close are diagnosed without changing a valid outer recording. |
| A08 | Multi-mode clearing is verified with nonzero scalar fields and every vector channel, including non-specialized widths and leaf slots. |
| A09 | Owner-thread method misuse is rejected without accessing the other thread's backend; concurrent independent scopes on different workers succeed. |
| A10 | A controlled cleanup-failure test proves explicit error reporting, noexcept fallback, unusable-context retention, and successful or rejected recovery. |
| A11 | Existing curve-calibration, ordinary MC, LSM, task-drain, and portable binding tests pass after each migration. |
| A12 | Two-round nine-target gate passes under the unchanged 4% policy; changed curve/MC workloads additionally compare equal results and memory/capacity. |

Tests should establish externally observable contracts rather than duplicate private
state transitions. Use a narrow backend fault-injection seam only for cleanup failures;
the production default path must retain its ordinary backend dispatch and allocation
behavior. Record four-backend capability differences instead of assuming that a
backend consumes all intermediates or preserves repeated sweeps identically.

## Open implementation decisions

- Select the smallest common backend boundary for controlled cleanup-failure tests
  without exposing a user-facing callback or allocating a type-erased adapter in
  normal requests.
- Audit raw guard users in GSR/SLV calibration before enforcing ownership there;
  an existing nested business flow needs a numerical pullback or a separately
  verified context, rather than weakening the rejection rule.
- D02 will handle stale active-number slots and illegal raw graph mutations.
  D01's boundary checks alone must not be advertised as complete stale-node detection.
