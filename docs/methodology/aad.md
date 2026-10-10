# Automatic Adjoint Differentiation (AAD)

This note describes the mathematics behind the reverse-mode automatic
differentiation used throughout the library to compute risk sensitivities
(Greeks). It explains *why* the method works and *how* the algorithm proceeds,
independent of any particular implementation.

## The Problem

A pricing routine evaluates a scalar function

$$
y = f(x_1, x_2, \dots, x_n)
$$

where the inputs $x_i$ are market data and model parameters (rates, vols, spots)
and $y$ is a price or risk number. We want the full gradient

$$
\nabla f = \left( \frac{\partial y}{\partial x_1}, \dots, \frac{\partial y}{\partial x_n} \right),
$$

i.e. all first-order sensitivities of the output to every input.

Three classical approaches:

| Method              | Cost of full gradient             | Accuracy                        |
|---------------------|-----------------------------------|---------------------------------|
| Finite differences  | $(n+1)$ function evaluations      | Truncation + cancellation error |
| Forward-mode AD     | $\propto n \times$ one evaluation | Machine precision               |
| **Reverse-mode AD** | $\propto 1 \times$ one evaluation | Machine precision               |

Reverse mode is the key result: it produces the **entire gradient at a cost that
is a small constant multiple of a single function evaluation, independent of the
number of inputs $n$**. For a derivatives book with thousands of risk factors
this is the difference between a tractable and an intractable computation.

## The Computational Graph

Any closed-form evaluation of $f$ decomposes into a sequence of elementary
operations (a *Wengert list*). Each intermediate result $v_k$ is produced by an
elementary operation $\varphi_k$ acting on earlier values:

$$
v_k = \varphi_k\!\left(v_{i} : i \prec k\right),
$$

where $i \prec k$ denotes "$v_i$ feeds $v_k$". The inputs $x_1,\dots,x_n$ are the
leaves and the output $y = v_N$ is the root. This induces a directed acyclic
graph (DAG): edges carry the **local partial derivatives** $\partial v_k /
\partial v_i$.

## The Chain Rule, Run Backwards

Define the **adjoint** of each node as the sensitivity of the final output to
that node:

$$
\bar{v}_k \equiv \frac{\partial y}{\partial v_k}.
$$

The root seeds the recursion with $\bar{y} = \partial y/\partial y = 1$. The
multivariate chain rule says the adjoint of a node is the sum, over all its
direct consumers, of the consumer's adjoint times the local derivative along the
connecting edge:

$$
\bar{v}_i = \sum_{k : i \prec k} \bar{v}_k \, \frac{\partial v_k}{\partial v_i}.
$$

Evaluating this relation in **reverse topological order** (from the root back to
the leaves) computes every adjoint in a single sweep. When the sweep reaches the
leaves, $\bar{x}_i = \partial y/\partial x_i$ — the gradient we wanted.

The asymmetry between forward and reverse mode is exactly this: forward mode
propagates one input's perturbation through the whole graph (so $n$ inputs need
$n$ sweeps), whereas reverse mode propagates one output's sensitivity back to all
inputs (so one output needs one sweep).

## The Two-Pass Algorithm

1. **Forward pass.** Evaluate the function normally. As each elementary
   operation executes, record onto a *tape* (a linear log of the graph): the
   operation's local partial derivatives with respect to its arguments, and a
   reference to where each argument's adjoint is accumulated.

2. **Reverse pass.** Initialise all adjoints to zero except the output
   ($\bar{y} = 1$). Walk the tape from the last operation to the first. At each
   node, push its accumulated adjoint into its arguments using the recorded local
   derivatives:

   $$
   \bar{v}_i \mathrel{{+}{=}} \bar{v}_k \, \frac{\partial v_k}{\partial v_i}.
   $$

   Because the tape is traversed in reverse and adjoints accumulate additively,
   each edge of the DAG contributes exactly once and the chain-rule sum above is
   formed correctly.

## Local Derivatives of Elementary Operations

The reverse pass needs only the local partial derivative of each elementary
operation. These are fixed analytic facts. For binary operations with result
$v$:

| Operation   | $\partial v/\partial l$ | $\partial v/\partial r$ |
|-------------|-------------------------|-------------------------|
| $l + r$     | $1$                     | $1$                     |
| $l - r$     | $1$                     | $-1$                    |
| $l \cdot r$ | $r$                     | $l$                     |
| $l / r$     | $1/r$                   | $-l/r^2$                |
| $l^{\,r}$   | $r\,v/l$                | $v\,\ln l$              |
| $\max(l,r)$ | $\mathbb{1}_{l>r}$      | $\mathbb{1}_{r>l}$      |
| $\min(l,r)$ | $\mathbb{1}_{l<r}$      | $\mathbb{1}_{r<l}$      |

For unary functions with result $v = g(r)$:

| Function               | $g'(r)$                           |
|------------------------|-----------------------------------|
| $\exp r$               | $v$                               |
| $\ln r$                | $1/r$                             |
| $\sqrt{r}$             | $1/(2v)$                          |
| $\lvert r\rvert$       | $\mathrm{sgn} r$                  |
| $\phi(r)$ (normal pdf) | $-r\,\phi(r) = -r\,v$             |
| $\Phi(r)$ (normal cdf) | $\phi(r)$                         |
| $\mathrm{erfc} r$      | $-\tfrac{2}{\sqrt{\pi}} e^{-r^2}$ |

Storing $v$ where it appears (e.g. for $\exp$) lets the reverse pass reuse the
forward result rather than recompute it.

## Vector-Valued Outputs

For a function with $m$ outputs $y_1,\dots,y_m$, the same machinery yields the
full Jacobian. Each node carries an adjoint *vector* of length $m$ rather than a
scalar, and the reverse-pass update becomes

$$
\bar{v}_i^{(j)} \mathrel{{+}{=}} \frac{\partial v_k}{\partial v_i} \, \bar{v}_k^{(j)}, \qquad j = 1,\dots,m.
$$

Seeding the $j$-th output adjoint to $1$ (others $0$) and propagating recovers the
$j$-th row of the Jacobian; doing all $m$ together in one sweep recovers the whole
Jacobian at the cost of one reverse pass with vector arithmetic.

An owning [structural Jacobian plan](aad-sparsity.md) colors caller-proven
conservative row supports and recovers the full matrix from supplied compressed
directions. Numerical zeros do not establish structural independence.

## Memory: Checkpointing via Mark / RewindToMark

The tape grows with the number of operations, so long simulations would exhaust
memory if every step were kept. The algorithm uses a **checkpoint** discipline:

- A **mark** records a position on the tape.
- **`RewindToMark`** discards everything recorded after the mark, reusing that memory,
  without disturbing the adjoints already accumulated before the mark.

`RewindToMark(tape)` discards nodes recorded after the current mark and is called
before each Monte Carlo path, so it clears the preceding path's recording between
paths. `Rewind(tape)` resets the tape to the beginning of its recording and is used
before a fresh simulation or calibration recording.

This lets a repeated computation (e.g. one Monte Carlo path) record, propagate,
and then rewind, so the tape size is bounded by the work of a *single* repetition
rather than the whole simulation. Propagation can therefore be partitioned into
ranges: from the end to the mark, and from the mark to the start.

### Independent Recording Ownership

`AAD::RecordingScope_` in `dal/math/aad/recording.hpp` owns an independent
recording on the calling thread's default tape. Entry activates and rewinds the
tape; successful closure rewinds it while retaining reusable backend capacity.
The scope is neither copyable nor movable, and its lifetime, operations, and
destruction belong to the creating thread. Separate threads can own separate
recordings. A nested scope throws before changing the outer tape. The curve
`TapeGuard_` delegates to this ownership boundary.

Register inputs before `StartRecording()`, build the graph, and call
`FinishRecording()` before a reverse operation. Repeated sweeps use explicit
clearing and fresh seeds. Extract passive results before calling `Close()`:

```cpp
AAD::RecordingScope_ recording;
AAD::Number_ input;
recording.RegisterInput(input, 3.0);
recording.StartRecording();
AAD::Number_ output = input * input;
recording.FinishRecording();
recording.ClearAdjoints();
AAD::Adjoint(output) = 1.0;
recording.Reverse();
const double derivative = AAD::AdjointValue(input);
recording.Close();
```

`Close()` reports cleanup errors and is idempotent. During exception unwinding,
the destructor performs fallback cleanup without replacing the business
exception. A backend or cleanup failure remains available through
`AAD::LastRecordingCleanupFailure()` and makes the thread's scoped context
unusable until the next entry successfully rebuilds the tape. A failed rebuild
rejects that entry. A failed reverse rejects further graph/reverse work in its
scope; closing it still releases ownership and retains the recovery requirement.
Curve Jacobian, node-risk, ordinary MC, and LSM replay callers close explicitly
after extracting passive results.

`MakeCheckpoint()` captures one prefix boundary during graph recording and
returns a copyable opaque `Checkpoint_`. A replacement invalidates the previous
token. `Restore(checkpoint)` discards the suffix, retains prefix adjoints, and
returns to graph-recording state. Finish each suffix before
`ReverseSuffix(checkpoint)`. Its contributions accumulate at the prefix; one
`ReversePrefix(checkpoint)` propagates them to the registered inputs. Extract
still-needed suffix values as passive doubles before restoring or closing their
recording. Tokens from a previous recording, another
thread, a replaced checkpoint, or another mode are rejected before position use.

Select native scalar/vector mode with `SetNumResultsForAAD` before creating the
scope, and keep the returned mode guard alive until the scope closes. Mode
selection inside an owned scope is rejected. Native `ZeroAdjoints` and scoped
`ClearAdjoints` clear both scalar fields and every vector channel, including
leaves; suffix restoration preserves prefix adjoints instead. A raw mode change
that violates the scoped width is diagnosed before scoped graph/reverse work.

Active numbers and tape positions become invalid when their recording is
discarded. Ownership checks protect nesting between scoped callers; raw tape
operations still require the caller's existing lifetime discipline. Existing
raw registration/propagation operations remain available to compatibility
callers, but must not replace marks or discard graphs managed by scoped
checkpoint methods. The scope does not validate every expression or make raw
iterators stable checkpoints.

### Native Active-Number Lifetime Diagnostics

Configure with `DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS=ON` to check native
`Number_` operands and adjoint access. The default is `OFF`; default builds
retain the number/node layouts and omit these per-number checks and counters.
Scoped ownership, state and checkpoint checks remain enabled independently.
DAL's only AAD implementation is native; the diagnostic setting changes its ABI.

Each binding captures a tape-lifetime identity, recording epoch, slot generation
and scalar/vector layout. Full clear or rewind invalidates the old recording;
suffix restoration invalidates discarded suffixes while preserving the prefix
and its accumulated adjoints. Reusing an address does not validate an old
binding. Checks precede old node access and expression-result allocation;
assignment precondition failures preserve the destination and valid graph.
Mode mismatches and exhausted counters are reported before incompatible access
or destructive reset. Rejected independent registration/rebinding preserves the
old cached primal and binding. Partial allocation failures require a successful reset.

`Value` only reads the cached primal. Explicit double assignment,
`RegisterIndependent` or `PutOnTape` can bind that value as a new independent;
copying alone preserves the old binding. The checks reject numbers from a
foreign or exited thread without reading its expired tape. They do not protect
arbitrary dangling C++ references or direct mutation of internal block lists.
Use the tape recording/reset APIs and applicable sanitizers.

The option changes diagnostic layouts. Its definition propagates through
`DAL::cpp`, `DAL::public`, bindings and installed exports; consumers must use
the exported targets and matching headers/libraries. The installed package
reports the setting as `DAL_CPP_AAD_LIFETIME_DIAGNOSTICS`.

### Native Tape Storage

The native `Tape_` in `dal-cpp/dal/math/aad/tape.hpp` owns separate block lists
for nodes, local derivatives, operand-adjoint pointers, and multi-result
adjoints. `RecordNode<N_>` reserves one node and, for nonzero `N_`, `N_`
derivative and pointer entries. Multi-result mode also reserves and zeroes the
node's result-adjoint slots. These allocations use the receiving tape object,
including for a caller-owned tape.

All arities share the same allocator. The three-input specialization uses
compiler-specific attributes to prevent inlining the allocation call; it
does not change the storage or recording rules. A block list
advances when the next reservation does not fit, reuses an existing next block
after rewind, and allocates a block when needed. Mark and rewind therefore
support storage reuse without promising allocation-free AAD evaluation.
`dal-cpp/tests/math/aad/test_tape.cpp` covers independent stream rollover,
reuse, aliased operands, multiple results, and caller-owned tapes.

`AAD::MeasureTape(tape)` in `dal/math/aad/statistics.hpp` explicitly scans a
native recording for its node and edge counts. It reports three storage measures:

- `liveBytes_`: node objects, recorded edge derivatives/pointers, and logical
  vector-adjoint slots for the current uniform recording mode.
- `occupiedBytes_`: storage through each block-list cursor, including skipped
  block tails and any retained cursor in an inactive storage stream.
- `capacityBytes_`: currently allocated block arrays and owned reverse-event
  storage, including capacity retained after rewind. Event storage includes
  descriptors, the lazy owner, event-table capacity and numeric caches. Scalar
  list bookkeeping and allocator overhead are excluded.

`reverseEvents_` counts recorded operators and `reverseEventCapacityBytes_`
reports their currently owned storage separately. `reverseScratchPeakBytes_`
records the largest simultaneous numeric scratch reservation above a retained
event cache during reverse, including admitted allocations that subsequently
fail. It resets on complete clear/rewind. The scalar
`liveBytes_`, `occupiedBytes_` and `blocks_` measures exclude event storage;
the scratch high-water value is not another retained-capacity contribution.

`blocks_` counts current blocks, rather than cumulative allocations. In a window
that only grows storage, the block-count increase measures new block allocations;
it does not count allocations elsewhere in a valuation. Empty tapes retain one
block per storage stream. A snapshot is neither a high-water counter nor process
RSS. Capture it at the relevant graph boundary to observe current capacity, and measure RSS
separately. Call the scan on the owning thread while recording and reverse work
are stopped. It maintains no per-node counters in normal execution.

The native `tape_perf` and `jacobian_perf` executables accept `--diagnostics` to
print these snapshots outside timed loops. Use their default invocation for
throughput comparisons. Tape cases include active/passive constants and vector
widths 1, 4, 10, 16, and 64, with analytic value/gradient checks. The historical
`100K nodes` case labels remain for regression continuity; the diagnostic node
count describes the actual fused graph, including active constants.
Jacobian cases retain the synthetic full-clearing reference and additionally
call `HarvestCurveJacobian` for 23-by-24 and 95-by-96 Jacobians. Proven-prefix
harvesting uses a dependency range established by the fixture itself.

### Recorded Dense Linear Solves

`Dal::AAD::LinearSolve` in `dal/math/aad/linearsolve.hpp` composes a dense
`AX = B` solve with ordinary native expressions in an explicit recording scope.
The three overloads accept active A/active B, passive A/active B or active
A/passive B. A fully passive solve uses the numeric
[linear-solve operator](matrix.md#dense-linear-solve-pullback).

```cpp
#include <dal/math/aad/linearsolve.hpp>
#include <dal/math/aad/native.hpp>

AAD::RecordingScope_ scope;
AAD::Number_ input;
scope.RegisterInput(input, 3.0);
scope.StartRecording();
SquareMatrix_<> a(1);
a(0, 0) = 2.0;
Matrix_<AAD::Number_> b(1, 1);
b(0, 0) = input;
auto x = AAD::LinearSolve(&scope, a, b);
AAD::Number_ objective = x(0, 0) * x(0, 0);
scope.FinishRecording();
scope.ClearAdjoints();
AAD::NativeOperations_::SetSeed(objective, 1.0);
scope.Reverse();
const double gradient = AAD::NativeOperations_::ReadAdjoint(input); // 1.5
scope.Close();
```

The recording owns factors, the solution and slot bindings independently of
caller matrices. A solve records zero-edge output slots and one reverse event.
Each scalar/vector channel solves `A^T Lambda = W` and accumulates
`bar-B += Lambda`, `bar-A += -Lambda X^T` into active entries. Aliases add their
contributions. Passive A omits the matrix contribution. Output seeds are
consumed; input accumulation and immutable caches survive repeated sweeps.
The existing full, suffix and prefix reverse methods preserve operator ordering.
Suffix restoration releases discarded caches before rewinding slots; closing
the scope releases all event storage. Tape copying and moving are unsupported.

Finite tape budgets admit event descriptors, table capacity, caches and reverse
scratch before allocation and refund failed allocations. Table growth includes
the old and new storage while both exist. The budget peak includes admitted
reservations even if a later allocation fails. Returned X container storage
belongs to the caller's buffer context; its slots belong to the tape. Retained
caches suspend request-buffer accounting, while reverse scratch can obey both
the tape and request-buffer ceilings. Those scratch measurements overlap and
must not be added as separate RSS contributions.

The call requires the scope's owner thread and RECORDING state, compatible
dimensions, live active inputs, finite values and valid pivot tolerance.
Numerical policies match the numeric operator. Construction/reverse failure
invalidates the graph and rejects adjoint reads until reset; a subsequent
independent scope can recover. Optional lifetime diagnostics additionally reject
stale source epochs/generations. Default builds retain the existing raw Number
lifetime contract. This overload family provides first-order dense-entry
derivatives. Packed symmetric and banded parameters use the
[coordinate overloads](#recorded-solve-coordinates). Higher order and public user
callbacks require separate support.

`LinearSolveWithDiagnostics` in `dal/math/aad/linearsolvediagnostics.hpp`
accepts the same three activity combinations and pivot tolerance. Its
`DiagnosedLinearSolveResult_` owns `solution_` and passive `diagnostics_` with
the [numeric diagnostic definitions](matrix.md#optional-solve-diagnostics).
It reuses one factorization for the solution, diagnostics and repeated reverse;
diagnostic values create no native nodes and are not differentiated.

```cpp
#include <dal/math/aad/linearsolvediagnostics.hpp>

auto result = AAD::LinearSolveWithDiagnostics(&scope, a, b);
AAD::Number_ objective = result.solution_(0, 0) * result.solution_(0, 0);
const double reciprocal = result.diagnostics_.reciprocalConditionInfinity_;
```

The detached report remains readable after input-container mutation, checkpoint
restoration or recording close. Its output Numbers follow the ordinary tape
lifetime contract. Cached diagnostic values and construction scratch belong to
the tape account; the returned report is copied into caller-budget storage
before output publication. Allocation or diagnostic inverse-range failure
invalidates the graph and refunds uncommitted storage. The ordinary `LinearSolve`
retains its existing cache and work. Diagnostic computation adds the numeric
diagnostic cost and one caller-owned vector of per-RHS errors.

### Recorded Solve Accuracy

`LinearSolveWithAccuracy` in `dal/math/aad/linearsolveaccuracy.hpp` records a
dense solve with an explicit [accuracy policy](matrix.md#explicit-forward-and-transpose-accuracy).
It supports the same three activity combinations as the ordinary dense solve.
The returned `CheckedLinearSolveResult_` owns `solution_`, forward `diagnostics_`
and an opaque `event_` identity. The event retains one checked numeric cache;
its physical transpose and declared limits remain fixed across reverse sweeps.

```cpp
#include <dal/math/aad/linearsolveaccuracy.hpp>

const LinearSolveAccuracyPolicy_ policy{1e-12, 1e-12};
auto result = AAD::LinearSolveWithAccuracy(&scope, a, b, policy);
AAD::Number_ objective = result.solution_(0, 0) * result.solution_(0, 0);
scope.FinishRecording();
scope.ClearAdjoints();
AAD::NativeOperations_::SetSeed(objective, 1.0);
const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
const auto& errors = reports.Report(result.event_).transposeBackwardErrors_;
```

The transpose check uses the actual seeds accumulated from ordinary expressions
and later events. Report rows identify RHS columns; report columns identify AAD
channels, including zero channels. Scalar mode and vector width one are distinct.
Each owning entry retains its event/recording identity, unique reverse invocation
identity and mode. Entries follow execution order. Repeated reverse returns a
new invocation; earlier reports remain detached historical values.

`ReverseSuffixWithSolveAccuracy` and `ReversePrefixWithSolveAccuracy` take the
scope and checkpoint explicitly. They report only events executed in that
window. Looking up an absent or discarded event throws. Restoring a suffix never
reuses its event identity. Reports remain readable after source destruction,
restore and scope close; their active solution Numbers retain the tape lifetime
contract.

The ordinary `scope.Reverse()` also enforces checked-event limits, without
collecting reports. A policy, numerical, contribution or allocation failure
invalidates the graph and returns no collection, including reports prepared by
earlier events in the same sweep. Existing native adjoints are not rolled back;
reads remain rejected until cleanup.

Returned diagnostics, report matrices and entry storage obey caller buffer
budgets. Event caches and reverse scratch obey tape budgets; scratch can also
overlap the caller ceiling. The optional interface adds physical residual checks
and report storage. It supplies first-order dense derivatives; packed parameters
use the [checked coordinate interface](#recorded-coordinate-accuracy).

### Recorded Solve Coordinates

`dal/math/aad/linearsolvecoordinates.hpp` adds `AAD::LinearSolve` overloads
for a `LinearSolveCoordinates_` layout, packed parameters and a dense RHS.
The [numeric layout](matrix.md#symmetric-and-banded-solve-coordinates) defines
parameter order: lower-triangle row order for symmetric matrices, or the actual
in-band entries in row order, excluding boundary padding. The three activity
combinations are active parameters/active RHS, passive parameters/active RHS
and active parameters/passive RHS. Fully passive calls use
`CoordinateLinearSolvePullback_` directly.

```cpp
#include <dal/math/aad/linearsolvecoordinates.hpp>
#include <dal/math/aad/native.hpp>

using namespace Dal;

AAD::RecordingScope_ scope;
AAD::Number_ coupling;
scope.RegisterInput(coupling, 1.0);
scope.StartRecording();
const auto layout = LinearSolveCoordinates_::Symmetric(2);
const Vector_<AAD::Number_> parameters = {3.0, coupling, 2.0};
Matrix_<> rhs(2, 1);
rhs(0, 0) = 1.0;
rhs(1, 0) = 2.0;
auto x = AAD::LinearSolve(&scope, layout, parameters, rhs);
AAD::Number_ objective = 3.0 * x(0, 0) - x(1, 0);
scope.FinishRecording();
scope.ClearAdjoints();
AAD::NativeOperations_::SetSeed(objective, 1.0);
scope.Reverse();
const double gradient = AAD::NativeOperations_::ReadAdjoint(coupling); // -1.4
scope.Close();
```

One event owns the numeric cache and the packed active bindings. It publishes
one zero-edge slot per solution entry, without expanding parameters to a dense
active matrix or recording fixed out-of-band zeros. Symmetric off-diagonal
gradients add both physical-entry contributions; they are not averaged.
Repeated parameters and aliases shared with the RHS or surrounding expressions
accumulate into their original slots. An active parameter whose value is zero
still receives its derivative.

Scalar and vector sweeps, output-seed consumption, checkpoints, thread ownership,
failure invalidation and tape/caller budgets follow the recorded dense-solve
contract. Source containers may change after capture; retained factors, solution
and bindings remain owned by the event. Passive parameters omit parameter
contraction, including its unused overflow checks. Only exact-zero seed channels
skip reverse work; requested nonfinite contributions invalidate the recording.

For p parameters and m RHS columns, the event retains O(p) parameter bindings
and contracts gradients in O(pm), with O(p+nm) reverse outputs. LU factors remain
dense, so factorization costs O(n^3) and transpose substitution costs O(n^2m).
These ordinary coordinate overloads provide first-order derivatives. Physical
accuracy checks and owning reports use the optional interface below.

### Recorded Coordinate Accuracy

`dal/math/aad/linearsolvecoordinateaccuracy.hpp` adds
`AAD::LinearSolveWithAccuracy` for a layout, packed parameters, dense RHS and
explicit accuracy policy. Parameters and RHS can both be active, or either can
be passive. It returns the same `CheckedLinearSolveResult_` as dense checked
solves, so full, suffix and prefix collection can include both event families.

```cpp
#include <dal/math/aad/linearsolvecoordinateaccuracy.hpp>

AAD::RecordingScope_ scope;
AAD::Number_ offDiagonal;
scope.RegisterInput(offDiagonal, 1.0);
scope.StartRecording();
Vector_<AAD::Number_> parameters{AAD::Number_(3.0), offDiagonal, AAD::Number_(2.0)};
Matrix_<> rhs(2, 1);
rhs(0, 0) = 1.0;
rhs(1, 0) = 2.0;
auto checked = AAD::LinearSolveWithAccuracy(
    &scope, LinearSolveCoordinates_::Symmetric(2), parameters, rhs,
    LinearSolveAccuracyPolicy_{0.0, 0.0});
AAD::Number_ objective = 2.0 * checked.solution_(0, 0) - checked.solution_(1, 0);
scope.FinishRecording();
scope.ClearAdjoints();
AAD::NativeOperations_::SetSeed(objective, 1.0);
const auto reports = AAD::ReverseWithSolveAccuracy(&scope);
const auto& errors = reports.Report(checked.event_).transposeBackwardErrors_;
// offDiagonal's adjoint is -1.0; errors(0, 0) is zero.
scope.Close();
```

The event owns one [checked physical coordinate cache](matrix.md#checked-coordinate-accuracy),
captured policy and O(p) active parameter bindings. Symmetric off-diagonal
gradients sum both physical contributions; zero-valued parameters retain risk.
Reverse contracts directly into p parameter contributions without a dense
matrix gradient. Passive parameters omit unused parameter work and overflow.
The physical factorization, condition estimate, transpose and residual work
remain dense. Packing does not provide a sparse or tridiagonal solver.

Forward limits apply before output publication. Transpose limits apply to the
actual seeds accumulated through expressions and later events, including
ordinary `scope.Reverse()`. Per-invocation report axes, identities, historical
ownership and failure behavior follow [recorded solve accuracy](#recorded-solve-accuracy).
The report covers m RHS columns by actual AAD channels, including zeros.
No derivative clipping, automatic regularization or relaxed pivot policy occurs.

Returned diagnostics, solution containers and reports obey caller buffer
budgets. Retained cache/bindings and reverse scratch obey tape budgets; report
allocation precedes scratch so the caller peak includes their overlap. A failed
capture publishes no outputs. A failed reverse returns no partial collection
and invalidates adjoint reads, while earlier successful reports stay readable.
The interface provides first-order physical-system derivatives. Equation roots
and sampled PDE steps use the owning operators below; bindings have separate
interfaces.

### Owning Implicit-Root Linearization

`ImplicitRootLinearization_` in `dal/math/optimization/implicitroot.hpp` maps
adjoints of a supplied root candidate to its equation inputs. For the square
equation $R(\theta,q)=0$, capture the residual, complete parameter Jacobian
$J=R_\theta$ and input Jacobian $K=R_q$ together at the supplied point. Then
solve $J^T\lambda=w$ and return $\bar q=-K^T\lambda$. Add any direct objective
dependence on $q$ separately.

The caller supplies the candidate and local root branch. The operator does
not run a nonlinear solver or differentiate a finite number of its iterations.
For a stationarity equation, supply its complete derivative, including
residual-Hessian terms. A Gauss–Newton approximation defines a different map.

The equation is evaluated once at owning copies of the candidate and inputs.
The captured result owns its point, residuals, accuracy policy, input Jacobian
and one checked parameter-Jacobian cache. The equation object and original
containers may be destroyed afterward. Repeated and concurrent const reverse
requests use independent results and scratch.

```cpp
#include <dal/math/optimization/implicitroot.hpp>

class QuadraticEquation_ final : public Dal::ImplicitRootEquation_ {
public:
    Dal::ImplicitRootEvaluation_ Evaluate(const Dal::Vector_<>& theta,
                                          const Dal::Vector_<>& q) const override {
        return {Dal::Vector_<>{theta[0] * theta[0] - q[0]},
                Dal::SquareMatrix_<>(1, 2.0 * theta[0]), Dal::Matrix_<>(1, 1, -1.0)};
    }
};

const QuadraticEquation_ equation;
const Dal::ImplicitRootAccuracyPolicy_ accuracy{Dal::Vector_<>{1e-12}, 1e-12};
const Dal::ImplicitRootLinearization_ root(equation, Dal::Vector_<>{2.0},
                                          Dal::Vector_<>{4.0}, accuracy);
const auto risk = root.Reverse(Dal::Matrix_<>(1, 1, 1.0));
// risk.inputs_(0, 0) is 1/4; the candidate -2 gives -1/4.
```

Each finite nonnegative `residualAbsoluteLimits_` entry is an inclusive limit
in that equation's units. `Residuals()` and `Policy()` retain the observations
and limits. Admitting a nonzero residual gives a linearization at that candidate;
it does not certify root or sensitivity error. A zero limit requires the
observed residual to be exactly zero. Physical Jacobian condition is reported
separately by `ReciprocalJacobianConditionInfinity()`.

With $n>0$ parameters, $k\geq0$ inputs and $m>0$ independent root-seed columns,
`Reverse` accepts an $n\times m$ matrix and returns $k\times m$ `inputs_` plus
$m$ actual `transposeBackwardErrors_`. Each transpose solve must meet the
declared finite limit in $[0,1]$. Zero input rows retain the seed/report column
axis and still perform the requested accuracy checks. Invalid shape/range,
singular Jacobians, unsupported normalized inverse range in condition
measurement, failed accuracy or overflowing requested risks reject the request.
Nonzero contraction operands whose product rounds to zero also reject, even
when the exact final sum would be representable. Representable subnormal
products remain supported. The transpose report does not certify contraction
rounding. A rejected reverse leaves the owning cache usable for subsequent
supported requests.

Capture uses dense factorization and condition work, with retained storage
$O(n^2+nk+n+k)$. Reverse costs $O(m(n^2+nk))$ and returns $O(km+m)$ storage,
with $O(n)$ local transpose scratch. It reuses the factorization and contracts
input risks directly. Caller buffer budgets include actual retained capacity
and overlapping scratch. This interface uses owning double-valued points and
results.

### Recorded Implicit Roots

`AAD::ImplicitRootWithAccuracy` in `dal/math/aad/implicitroot.hpp` records the
same complete equation linearization in an explicit native scope. It accepts
a passive candidate, `Vector_<AAD::Number_>` equation inputs and the explicit
accuracy policy. Its `CheckedImplicitRootResult_` owns a vector of output
Numbers, passive forward diagnostics and a shared `SolveAccuracyEvent_` token.

Using the quadratic equation above:

```cpp
#include <dal/math/aad/implicitroot.hpp>
#include <dal/math/aad/native.hpp>

Dal::AAD::RecordingScope_ scope;
Dal::AAD::Number_ q;
scope.RegisterInput(q, 4.0);
scope.StartRecording();
auto root = Dal::AAD::ImplicitRootWithAccuracy(
    &scope, equation, Dal::Vector_<>{2.0}, Dal::Vector_<Dal::AAD::Number_>{q}, accuracy);
Dal::AAD::Number_ objective = 3.0 * root.parameters_[0] + q;
scope.FinishRecording();
scope.ClearAdjoints();
Dal::AAD::NativeOperations_::SetSeed(objective, 1.0);
const auto reports = Dal::AAD::ReverseWithSolveAccuracy(&scope);
const double risk = Dal::AAD::NativeOperations_::ReadAdjoint(q); // 7/4
scope.Close();
```

The candidate selects the local branch and supplies the exact forward values.
Each Number input declares a differentiable equation slot; repeated aliases
sum their contributions. Direct objective input dependence composes through
ordinary expressions. Zero equation inputs remain supported. One event owns
the numeric cache and input/output bindings. It evaluates the equation once
at owning doubles and retains no callback or nonlinear iteration graph.
The recording must still be active when evaluation returns; a callback that
ends recording causes capture to fail before any root output is published.

Forward `diagnostics_` owns `residuals_`, `policy_` and
`reciprocalConditionInfinity_`. The captured-point interpretation and complete
stationarity Jacobian requirements above also apply here. For each AAD channel,
reverse solves one transpose RHS formed from all parameter seeds. Its actual
error report is therefore **1-by-channel-width**, including exact-zero channels.
Root, dense and coordinate events share full, suffix and prefix report collection,
checkpoint ordering and invocation identities. Ordinary `scope.Reverse()` also
enforces the declared transpose limit. Historical reports and diagnostics remain
readable after restoration or close; output Numbers follow the tape lifetime.

Returned vectors, diagnostics and invocation reports obey caller budgets.
Capture first constructs the validated numeric linearization in the caller
context, then deep-copies it into the event without another factorization.
This keeps callback-managed buffers outside event ownership; their side effects
remain caller-owned if the callback throws. Caller peak includes staged bindings,
numeric construction and the overlap with returned outputs/diagnostics.
Retained cache/bindings and reverse scratch obey tape budgets; with a caller
budget active, the same scratch also counts toward its peak alongside reports.
These overlapping measurements describe the same allocation. Failed capture
publishes no root outputs. Failed reverse, unsupported numerical range or
nonfinite accumulated input risk invalidates the recording and returns no
partial collection. The interface provides first-order C++ equation derivatives.

### Owning Sampled PDE Theta Steps

`PDE::SampledThetaStepPullback_` in `dal/math/pde/sampledthetastep.hpp`
differentiates one discrete theta step on a fixed physical grid. Its
`SampledThetaStepInputs_` contains finite increasing locations `x_`, interior
`rates_`, `drifts_` and nonnegative `variances_`, positive `dt_`, `theta_` in
$[0,1]$, and an $n\times m$ `oldValues_` matrix with $n\geq3$ and $m>0$ layers.
Each coefficient vector has $n-2$ entries. Rates and drifts may be signed.
The grid must fit the library's integer matrix dimensions.

The nonuniform three-point stencil forms

$$
L=\mu D_x+\tfrac12 vD_{xx}-rI,\qquad
A=I-\Delta t\,\theta L,\qquad E=I+\Delta t\,(1-\theta)L.
$$

Interior equations solve $Au=Eu_{old}$. Boundary rows of $A$ are identity;
`externalBoundaries_` explicitly selects each endpoint's RHS from the old
endpoint or its row in `externalValues_`. External values have shape
$2\times m$ when either endpoint is external. An all-old request may omit
them; every supplied nonempty external matrix must have that shape and finite
entries, including its unused side. The default boundary flags are both false
and the default theta is $1/2$; `dt_` must be supplied.

```cpp
#include <dal/math/pde/sampledthetastep.hpp>

Dal::PDE::SampledThetaStepInputs_ inputs;
inputs.x_ = {0.0, 1.0, 2.0};
inputs.rates_ = {0.1};
inputs.drifts_ = {0.2};
inputs.variances_ = {0.4};
inputs.dt_ = 0.2;
inputs.oldValues_ = Dal::Matrix_<>(3, 1, 1.0);
const Dal::PDE::SampledThetaStepPullback_ step(
    inputs, Dal::LinearSolveAccuracyPolicy_{1e-14, 1e-14});
Dal::Matrix_<> seeds(3, 1);
seeds(1, 0) = 1.0;
const auto risk = step.Reverse(seeds);
// step.Solution()(1, 0) is 103/105; risk.rates_ has one interior entry.
```

One $n\times m$ seed matrix defines one objective summed over all output
layers. Reverse solves $A^T\lambda=w$ using the same adjacent-pivot
tridiagonal LU, including second-upper fill from row swaps. Let $P$ remove
endpoint rows. Old-state risk is $E^TP\lambda$, plus each old-derived
endpoint's $\lambda$; external endpoint risk is its $\lambda$ and unused
external risks are zero. Generator contributions are
$\Delta t\lambda_i[\theta u_j+(1-\theta)u_{old,j}]$, summed over layers.
They map directly to interior rate, drift and variance coordinates; `dt_`
and `theta_` return scalar aggregate risks. Chain steps by passing each
`oldValues_` risk to the preceding reverse and summing shared coefficient risks.
At theta zero the solver retains no factors, while theta risk can remain nonzero.

`Solution()`, `ForwardBackwardErrors()` and `Policy()` belong to the owning
cache. Each reverse returns detached `SampledThetaStepAdjoints_` containing
old/external matrices, the three coefficient vectors, scalar time/theta risks
and one actual `transposeBackwardErrors_` entry per layer. The caller buffers
may be changed or destroyed after capture. Copies own independent buffers;
copy assignment first captures a complete temporary cache, so allocation or
capacity failure leaves the destination unchanged. The temporary uses the same
owned-buffer capacity as a copied cache; self-assignment allocates nothing.
Moves preserve the destination and leave the source assignable/destructible.
Concurrent const reverse requests own independent results and scratch.

Both accuracy limits must be finite in $[0,1]$ and are inclusive. Physical
componentwise errors use the [solve residual definition](matrix.md#optional-solve-diagnostics)
with at most three entries per row, including the actual transposed edges.
The optional normalized pivot tolerance defaults to 64 machine epsilons and
must lie strictly between zero and one, including for theta zero. Singular
or rejected pivots, failed accuracy, nonfinite arithmetic and nonzero products
or quotients rounded to zero reject unsupported numerical range. Representable
subnormal results remain supported. A failed reverse leaves the cache reusable.
Accuracy and pivot admission do not certify condition, discretization or
sensitivity error.

Factor work/storage are $O(n)$; capture and each reverse are $O(nm+n)$ with
linear retained/result/scratch buffers. Caller capacity budgets count actual
buffers and their overlap and refund failed requests. This C++ numeric operator
holds the mesh and sampled coefficient-provider mapping fixed. Variance risk
maps to volatility risk by the upstream $2\sigma$ factor. Native recording uses
the interface below; Python/Excel bindings are separate capabilities.

### Recorded Sampled PDE Theta Steps

`AAD::SampledThetaStepWithAccuracy` in `dal/math/aad/sampledthetastep.hpp`
records an owning theta-step event in a live `RecordingScope_`. The numeric
configuration supplies the passive grid, boundary-source flags and field values.
`SampledThetaStepBindings_` selects complete active replacements: Number vectors
`rates_`, `drifts_`, `variances_`; Number matrices `oldValues_`, `externalValues_`;
and optional Number scalars `dt_`, `theta_`. Empty containers or absent scalars
use the corresponding numeric value. Active replacements supply their own
primals, so the overwritten passive field can be empty or otherwise unused.
An active old-state matrix defines the layer count $m$. Each resulting
coefficient vector contains $n-2$ entries, independent of the layer count.
Any declared external matrix has two rows and $m$ columns, including unused sides.
The native call requires at least one active field; fully passive steps use
the numeric operator.

```cpp
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/sampledthetastep.hpp>

Dal::AAD::Clear(*Dal::AAD::Tape());
auto mode = Dal::AAD::SetNumResultsForAAD(false, 1);
Dal::PDE::SampledThetaStepInputs_ inputs;
inputs.x_ = {0.0, 1.0, 2.0};
inputs.drifts_ = {0.2};
inputs.variances_ = {0.4};
inputs.dt_ = 0.2;
inputs.oldValues_ = Dal::Matrix_<>(3, 1);
for (int row = 0; row < 3; ++row)
    inputs.oldValues_(row, 0) = row + 1.0;
inputs.externalBoundaries_ = {true, true};
inputs.externalValues_ = Dal::Matrix_<>(2, 1);
inputs.externalValues_(0, 0) = 4.0;
inputs.externalValues_(1, 0) = 5.0;
Dal::AAD::RecordingScope_ scope;
Dal::AAD::Number_ rate;
scope.RegisterInput(rate, 0.1);
scope.StartRecording();
Dal::AAD::SampledThetaStepBindings_ active;
active.rates_ = {rate};
auto step = Dal::AAD::SampledThetaStepWithAccuracy(
    &scope, inputs, active, Dal::LinearSolveAccuracyPolicy_{1e-14, 1e-14});
Dal::AAD::Number_ objective = 2.0 * step.solution_(1, 0) + rate;
scope.FinishRecording();
scope.ClearAdjoints();
Dal::AAD::NativeOperations_::SetSeed(objective, 1.0);
const auto reports = Dal::AAD::ReverseWithSolveAccuracy(&scope);
const double risk = Dal::AAD::NativeOperations_::ReadAdjoint(rate); // 163/735
const auto& errors = reports.Report(step.event_).transposeBackwardErrors_;
scope.Close();
Dal::AAD::Clear(*Dal::AAD::Tape());
```

The result owns an $n\times m$ Number matrix, passive `diagnostics_` with one
forward backward error per layer and the policy, and an opaque `event_` token.
Each reverse channel seeds the whole output matrix for one objective; coefficient
and scalar risks sum its layers. Actual transpose reports have $m$ rows and the
scalar/vector channel width columns. Ordinary `scope.Reverse()` enforces the
same limits without returning reports. Full, checkpoint suffix/prefix and raw
mark windows use the existing event ordering. Independent output nodes include
the copied endpoints; passing outputs as the next active `oldValues_` composes
serial rollback. Shared aliases sum every formal contribution. Upstream
`variance = sigma * sigma` carries the volatility chain factor.

The event captures its bindings and numeric cache once; source mutation or
destruction cannot change the calculation. All declared active slots must belong
to the recording, including unused external sides, whose risks remain zero.
The reverse computes the complete numeric coordinate risks before scattering
active slots: unsupported range in an inactive coordinate still rejects.
Exact-zero channels skip that numerical work and retain zero physical reports.
The mesh and sampled coefficient-provider mapping remain passive. Accuracy limits
do not bound discretization or sensitivity error; no condition estimate is given.

Tape capacity includes the owned cache, declared bindings, output bindings and
reverse scratch. Caller capacity includes staged capture, returned outputs,
diagnostics and owning reports. Reverse scratch also counts against an active
caller budget; these measurements overlap. Storage is linear in grid size and
layers, with per-channel scratch reused. Checkpoint restoration releases discarded
payloads while the tape may retain its descriptor-table capacity. Close releases
the event storage. Passive diagnostics and successful historical reports survive
restore and close; Numbers follow the normal tape lifetime contract. Capture or
reverse failure invalidates the recording, refunds buffers and returns no partial
result or report collection. Independent workers use separate recordings.

For a complete financial chain, see
[fixed-grid European PDE prices and adjoints](pde/aad.md). The example records
terminal payoffs, discounted external boundaries and volatility-square
coefficients, then validates discrete risks and continuum grid convergence
separately.

### Native Production Profiling

`DAL_ENABLE_AAD_PROFILING=ON` enables the C++ diagnostics in
`dal/math/aad/profiling.hpp`. The default is OFF. The definition propagates
through exported CMake targets, and the installed package reports
`DAL_CPP_AAD_PROFILING`. Use matching headers and libraries. This option changes
profiling/task helper layouts; it leaves native number, node and tape layouts
unchanged and is independent of lifetime diagnostics.

An explicit, thread-affine scope collects one request:

```cpp
#include <dal/math/aad/profiling.hpp>

AAD::ProfilingData_ data;
const auto result = [&] {
    AAD::ProfilingScope_ profile(&data);
    return Script::MCSimulation<AAD::Number_>(prepared, modelData, paths);
}();
// Inspect data after the request and all its tasks have finished.
```

`ProfilingAvailable()` identifies the build setting. Creating a scope in an
OFF build throws. An ON build without an explicit scope skips clocks, tape
scans and array measurement callbacks. Default OFF production paths omit the
diagnostic calls and task collectors. A scope borrows its report; keep the
report alive until scope destruction and task draining. Nested scopes require
separate reports. Read or copy completed reports after their scopes end.
Unwinding and unfinished tasks leave `complete_` false. Measurement overflow
sets `invalidMeasurement_`; missing CPU clocks remain explicitly unavailable.

Ordinary MC and LSM record preparation, worker initialization, path
forward/recording, payoff, suffix/prefix reverse, waiting and reduction.
LSM also records training, regression and pricing replay. `taskGroups_` owns
the individual task reports, including regression helper tasks.
Scope wall time is an interval; worker interval sums are cumulative work,
not request latency. `WAIT` includes tasks executed by `ActiveWait`.
Phase self wall time excludes nested spans in the same report; it does not
exclude task windows belonging to another report. Phase intervals overlap and
must not be added as independent costs.

CPU fields use actual thread CPU clocks on supported platforms. Scope
`cpuNanoseconds_` includes its nested work. `selfCpuNanoseconds_` excludes
same-request task scopes executed inside it on the same thread. Summing
available self CPU values across a completed task tree avoids that duplicate
counting and describes the observed scope intervals, including diagnostics.
It excludes pool/framework work outside those intervals. Independent profiling
requests are not subtracted from each other. CPU values and wall values have
distinct availability and inclusion rules.

Tape samples occur after AAD initialization and after each path payoff,
before reverse/rewind. `highWater_` retains componentwise maxima of these
observations; its fields need not come from one simultaneous snapshot.
Sampling is not a per-instruction peak tracker. `blockAllocations_` counts
successful native block-list array allocations during the scope; reuse does
not increment it. `allocatedArrayBytes_` excludes allocator/list overhead.

`memory_` separates observed path, selected workspace, result and LSM regression
arrays. Live bytes use array sizes; capacity bytes use retained capacities.
Path measurements include sample objects and their owned vectors. Workspace
measurements include Gaussian buffers, exposed evaluator variables/vectors and
fuzzy replay arrays. Regression measurements include coexisting training and
validation rows and backward working arrays. These are selected array payloads:
private evaluator seeds/stacks, model/RNG caches, solver temporaries and
diagnostic storage are excluded. Their maxima are not a complete valuation
memory total or process RSS. Memory metadata collection has no separate timed
phase; its cost is included in the enclosing scope/window.

The existing `script_mc_perf` executable adds an explicit JSON-lines mode:

```bash
script_mc_perf --production-profile short 8192 aad compiled cold 4 0 0 1
script_mc_perf --production-profile local-vol 8192 aad compiled phases 1 16 0 1
script_mc_perf --production-profile lsmc-bs 8192 aad tree warm 1 0 512 3
```

Arguments are scenario, pricing paths, `double|aad`, `tree|compiled`,
`cold|warm|phases`, outputs, surface grid, training paths and repetitions.
Scenarios are `short`, `long`, `local-vol`, `lsmc-bs` and `lsmc-local-vol`.
Ordinary cases accept 1/4/16/64 different strike outputs; they execute
sequential single-output requests with the same model inputs and Sobol paths.
AAD channel width remains one. Passive requests report zero active parameters
and no risks. LSM cases accept one output and keep the Frozen policy; the
existing `--lsmc-replay` interface retains its policy options and workloads.

Cold timing includes fresh model/product data and script preparation. Warm
timing reuses prepared inputs after an untimed full request; it still includes
worker/model initialization. Phases use the cold boundary and explicit scopes.
All modes run validation before timing, so cold is not process-first startup.
The short BS fixture validates each output's price and four risks against
analytic fixed-path formulas. Long/local-vol fixtures use common-path finite
differences at two steps; LSM uses tree/compiled fixed-path comparisons.
Independent checks use up to 256 pricing paths. Each timed result also matches
an untimed request at the full path count, with every requested risk checked.
Startup messages go to stderr; request/result/scope/phase records go to stdout.
Use uninstrumented Release binaries for throughput comparisons and measure
profiling overhead separately. External process resource tools include the
untimed validation and warm-up when reporting lifetime peak RSS.

## Pathwise Adjoints in Monte Carlo

A Monte Carlo price is an average over $P$ simulated paths,

$$
V = \frac{1}{P}\sum_{p=1}^{P} g\big(\omega_p; \theta\big),
$$

where $\theta$ are the model/market parameters and $g$ is the discounted payoff
on path $\omega_p$. Differentiation commutes with the (finite) average:

$$
\frac{\partial V}{\partial \theta} = \frac{1}{P}\sum_{p=1}^{P} \frac{\partial g(\omega_p; \theta)}{\partial \theta}.
$$

This is the **pathwise adjoint** estimator. The algorithm is:

1. For each batch recording, place the parameters $\theta$ on the worker's
   tape, record shared model initialization and any parameter-dependent
   historical state, then mark.
2. For each path: rewind to the mark, simulate the path and evaluate the payoff
   (forward pass), create or reuse a path-local payoff root, seed its adjoint
   to $1$, and run the reverse pass to the mark. Adjoints before the mark
   **accumulate** across paths automatically.
3. After the batch's paths: propagate from the mark to the start and divide
   the parameter adjoints by the total simulation path count $P$. Sum batch
   contributions without a second normalization.

In [prepared script evaluation](script_engine.md#historical-state-and-recording-lifetime),
historical fixings are sealed doubles, but script expressions using them can
depend on active parameters. Each recording replays those expressions locally
before the mark and each path restores the resulting typed seed. The native
backend reuses the payoff as the path-local root only when the post-mark range
is nonempty and the payoff is its current terminal node. Otherwise it adds a
registered zero to the payoff.
The fallback creates a path-local root even for a pre-mark seed or a passive
constant, preserving accumulated seed adjoints and providing a valid reverse
range when the post-mark recording would otherwise be empty. Historical
decisions use hard branches; fuzzy smoothing applies to future events,
including future conditions on known fixings. Tree and compiled execution share
this recording contract: compiled history is hard bytecode with settled payments
discarded, and observation loads use the same sealed plan. Parameter-dependent
historical state remains typed rather than being inlined as its current double
value. Future fuzzy weights remain live through optimization; compare AAD primal
values with fuzzy double using the same epsilon, not exact double at a future
discontinuity.

The result is the full gradient of the Monte Carlo price — every Greek for every
parameter — for the cost of roughly one extra simulation, regardless of how many
parameters there are. Because each thread keeps its own tape and the per-path
work is independent, the scheme parallelises with no synchronisation during the
forward or reverse passes; thread results are summed at the end.

## Smoothing Discontinuous Payoffs

The pathwise estimator differentiates the payoff path by path, which requires the
payoff to be (almost everywhere) differentiable in the parameters. Discontinuous
payoffs — digitals, barriers — have a derivative that is zero almost everywhere
and a Dirac mass at the discontinuity, so the naive pathwise derivative is biased
(it misses the jump). The library addresses this with **fuzzy evaluation**:
indicator functions $\mathbb{1}_{S > K}$ are replaced by a smooth approximation
over a small spread $\varepsilon$,

$$
\mathbb{1}_{S>K} \;\approx\; \Psi\!\left(\frac{S-K}{\varepsilon}\right),
$$

with $\Psi$ a smooth sigmoid-like transition. This regularises the payoff so the
adjoint captures the (smoothed) sensitivity through the discontinuity, trading a
small bias for a finite, low-variance derivative.

## Tape-Layer Primitives for Curve Calibration

Beyond the scalar arithmetic operators and special functions that record
derivatives for pricing, the library provides **templated curve types** under
`namespace Dal::Tape` that extend the tape into yield-curve construction itself.
Each records the dependence of discount factors on the curve's free parameters
so that the reverse sweep produces a Jacobian of calibration residuals with
respect to the selected curve coordinates -- the input the underdetermined solver
consumes.

### Piecewise-Constant Forward Curve — `Tape::DiscountPWC_<T_, B_>`

```text
dal-cpp/dal/curve/ycconst.hpp
```

This curve stores one typed right-hand forward value per knot and a typed cumulative
integral. Date comparisons, segment selection, and elapsed-day weights remain passive;
the forward values, integral, exponential, and optional base multiplication stay on
`T_`. The passive specialization delegates to `PiecewiseConstant_` for its public
arithmetic contract, while the active specialization records derivatives through the
same piecewise-constant integral.

### Piecewise-Linear Forward Curve — `Tape::DiscountPWLF_<T_, B_>`

```text
dal-cpp/dal/curve/ycpwlf.hpp
```

This curve interpolates forward rates piecewise-linearly on the scalar type `T_` and
integrates to log-discount factors, so every discount-factor read records the
dependence on the $2 \cdot n_{\text{knots}}$ forward-rate parameters
(`fLeftT_`, `fRightT_`). The base type `B_` is a second template parameter:
`B_ = DiscountCurve_<double>` for baseless curves (base treated as a constant);
`B_ = DiscountCurve_<T_>` for base-layered curves, where the base's own
parameters also carry adjoints and the reverse sweep propagates OIS
sensitivities through the base multiplication into the discount-curve free
nodes.

The forward-to-log-DF integration reproduces the four-branch
`PiecewiseLinear_::IntegralTo` logic (below first knot, beyond last knot,
on-knot shortcut, in-range partial trapezoid) with `double` knot abscissae and
`T_` forward values. The running integral is stored in the `Vector_<T_>`
`sofarT_` member, recomputed by `UpdateT()` whenever the forward parameters
change.

### Log-Discount Curve — `Tape::DiscountLogDF_<T_, B_>`

```text
dal-cpp/dal/curve/yclogdf.hpp
```

The curve stores typed node log DFs but passive year fractions. `LogDfInterpolation_`
selects log-linear, natural-cubic, or mixed geometry and applies the resulting passive
weights to the typed ordinates. Passive pricing, AAD pricing, pre-anchor behavior, and
right-tail secant extrapolation therefore share one evaluation definition. Natural-cubic
weights are globally supported; the mixed scheme is local in its linear head and global
within its cubic tail.

All three curve templates accept `B_ = DiscountCurve_<double>` for a passive or absent
base and `B_ = DiscountCurve_<T_>` for a base built in the same recording. The latter
propagates cross-curve adjoints through base composition regardless of representation.

### Joint Multi-Curve Routing — `Tape::JointCurveBlock_<T_>`

```text
dal-cpp/dal/curve/jointycctx.hpp
```

The multi-curve analogue of the single-curve `Tape::YCCtx_<T_>`. It holds one
`const DiscountCurve_<T_>*` per collateral and one per forward tenor, and
provides `Discount(collateral)` and `Forward(tenor, collateral)` reads that
mirror `CurveBlock_`'s routing (including the OIS fallback and the
forward-to-discount fallback). The pointer maps are non-owning references to
curves built in the same `Gradient` call. Unlike `YCCtx_<T_>`, which is bound to
a single curve, `JointCurveBlock_<T_>` enables the multi-curve reads (discount
at one curve, forecast at another) that IBOR-projection instruments require.

### Projection-Capable Rate Base — `Tape::JointRate_<T_>`

```text
dal-cpp/dal/curve/jointrate.hpp
```

A sibling of the single-curve `Tape::Rate_<T_>` (which is bound to `YCCtx_<T_>`
and reads a single curve). `JointRate_<T_>` declares a pure virtual
`T_ operator()(const JointCurveBlock_<T_>& block)` so each subclass can read
both a discount curve AND a forecast curve in the `T_` domain. The three
projection-capable subclasses are:

- `DepositRateProj_<T_>` -- single-period forecast read,
- `ForwardRateProj_<T_>` -- covers FRA and Future (convexity adjustment stays
  `double`),
- `SwapRateProj_<T_>` -- swap par rate with float-leg forecast reads and
  fixed-leg discount reads.

This hierarchy prices the IBOR projection slice of a joint calibration, where
$\text{forecast} \neq \text{discount}$. The OIS-discount slice (where
$\text{forecast} = \text{discount}$) does not need these and rides the
inherited `Swap_::PrecomputeT<T_>`.

### Recording Contract for the Joint Path

The native recording contract for a correct Jacobian is
the same as the single-curve path:

$$\text{Rewind}(\textit{tape}) \rightarrow
\text{RegisterIndependent}(x_k)\;\forall k \rightarrow
\text{NewRecording}(\textit{tape}) \rightarrow
\text{forward pass (build curves, price residuals)} \rightarrow
\text{per row } \{\bar{r}_i = 1,\;
\text{PropagateToStart},\; \text{harvest } \bar{x}_j,\;
\text{zero each } \bar{x}_j\}.$$

Independent registration follows `CurveParameterLayout_`: PWC contributes one value per
knot, PWL contributes interleaved left/right values, and log-DF contributes future-node
ordinates while its pinned storage anchor is excluded. `HarvestCurveJacobian` performs
the per-row seed/propagate/harvest/leaf-clear loop. Consumed intermediate seeds
clear during native reverse. The harvested adjoints form a dense
`XCurveJacobian_` (`dal-cpp/dal/curve/curvejacobian.hpp`) with exact structural
zeros where an instrument has no parametric dependence on a given knot.

## Native AAD

DAL uses its built-in scalar/vector tape exclusively. `Number_` and `Tape_`
in `dal-cpp/dal/math/aad/` have no external-backend aliases or selection paths.
Each operating system thread owns its default tape, which is destroyed at thread
exit. Active values and tape positions remain thread-affine, independently of
the Python GIL. See [configuration and migration](../installation.md#native-aad-configuration).

### Native Operations

`AAD::NativeOperations_` in `dal-cpp/dal/math/aad/native.hpp` provides checked
seed/channel access and a passive capability description. Recording services
call the established native tape functions directly; there is no backend
inheritance, selector, virtual dispatch or per-node capability lookup.

| Contract                                | Native support                                |
|-----------------------------------------|-----------------------------------------------|
| Scalar and repeated fixed-graph reverse | Yes, while the graph remains valid            |
| Interval reverse/prefix accumulation    | Yes                                           |
| Scoped lifecycle validation             | Yes                                           |
| Vector adjoint channels                 | Up to `ADJ_SIZE`                              |
| Active-number lifetime diagnostics      | Available; default OFF                        |
| Recorded reverse events                 | Dense/coordinate solves, scalar/vector sweeps |
| Independent nesting                     | Not implemented                               |
| Higher-order active mode                | Not implemented                               |

`SetSeed(number, seed, channel)` replaces a seed; `AddSeed` accumulates it,
including multiple weights for the same output reference. `ReadAdjoint`
returns a passive double. The optional channel defaults to zero. In vector
mode these operations access the vector array, including channel zero;
the legacy `Adjoint` scalar field is separate.

`ClearSeeds(number)` clears all channels of that number under the current mode.
It validates the number once and preserves other nodes' adjoints. Blocked output
seeding uses this operation to clear reused or aliased roots before setting the
diagonal seeds.

For example, $u=xy$, $v=x^2+y$ at $(x,y)=(2,3)$ has Jacobian rows $(3,2)$
and $(4,1)$. Seeds $(2,-1)$ compute the weighted gradient $(2,3)$:

```cpp
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>

AAD::RecordingScope_ recording;
AAD::Number_ x, y;
recording.RegisterInput(x, 2.0);
recording.RegisterInput(y, 3.0);
recording.StartRecording();
AAD::Number_ u = x * y;
AAD::Number_ v = x * x + y;
recording.FinishRecording();
recording.ClearAdjoints();
AAD::NativeOperations_::SetSeed(u, 2.0);
AAD::NativeOperations_::SetSeed(v, -1.0);
recording.Reverse();
const double dx = AAD::NativeOperations_::ReadAdjoint(x); // 2
const double dy = AAD::NativeOperations_::ReadAdjoint(y); // 3
recording.Close();
```

Clear gradients before an independent seed vector. `ActiveRoot(payoff,
activeZero)` applies the existing payoff-root convention when an output is a
constant, direct input or prefix alias; its second argument must be an active
zero on the same recording. It does not silently register a new parameter.

For several outputs on one live recording, include
`<dal/math/aad/weightedroot.hpp>` and call
`WeightedPayoffRoot(outputs, weights)`. This materializes one scalar weighted
objective; seed its root once, then reverse the suffix and prefix normally.
Ordinary expression edges accumulate distinct slots that alias the same node.
The root remains path-local when components are constants, direct inputs or
prefix outputs. Weights are passive, finite and ordered like the components.
Both sequences must be nonempty and equally sized. Nonfinite components are
rejected even at weight zero, and a nonfinite weighted sum raises an error.
This is a native recording helper; structured scalar valuation requests still
select the single default payoff.

Native output preflight is available in `<dal/script/weightedrisk.hpp>`.
`ScriptRiskOutputAxis` reads an indexed product's scalar slots. The receiver's
ID is `payoff`; other scalar slots use `output:<ordinal>`, with the actual
variable label stored separately. Vector storage is excluded.
`PlanWeightedRiskRequest` resolves ordered choices and finite passive weights,
reuses input/report checks and enforces the exact retained numeric payload
`sizeof(double) * (1 + inputs + 2 * components)`. Its owning read-only plan
captures the valuation date and native/price-only choice. Prepared-axis validation
checks identity again after preparation. This preflight does not run a valuation
or resolve historical fixings.

`ValidateAdjointMode(multi, width)` checks a proposed mode without changing
the tape. Scalar width must be one; vector width must be positive and at most
`ADJ_SIZE`. Select actual mode with `SetNumResultsForAAD` before scope entry.
Invalid modes/channels fail before seed mutation. Diagnostic ON also validates
ownership and lifetime before node access. OFF rejects missing nodes/vector
storage, but cannot generally detect stale or wrongly rebound numbers.

### Recording and Gradient Clearing

For a fresh independent Jacobian, rewind, register inputs, start recording,
perform the forward pass, then seed/reverse/harvest each output row.
`NewRecording` is a native no-op; the scoped API still uses it as an explicit
phase boundary. Rewind discards activity while retaining reusable capacity;
re-register numeric inputs before the next independent graph.

Every nonzero adjoint propagates, including subnormal values; only exact zero
seeds are skipped. Vector channels follow this rule independently, so a zero
channel does not evaluate `0 * Inf` when another channel is active. NaN and
nonzero infinite seeds remain observable.

`PropagateOne` consumes each intermediate adjoint after propagating it to its
parents. Leaf parameter nodes retain accumulated contributions. The curve
Jacobian harvester clears each independent leaf after extraction, costing
O(nParams) per row. General independent repeated VJPs use `ZeroAdjoints` or
the scoped `ClearAdjoints`, which clear the actual scalar/vector storage.
A suffix restore preserves the valid prefix and its accumulated adjoints;
a whole-graph gradient clear must not replace that operation.

Before closing or restoring a suffix, extract still-needed prices and gradients
as passive values. Discarded activity cannot be used in a later reverse.
Failure recovery and checkpoint validation follow the scoped lifecycle rules
above.

### Public Result Validation

Public Monte Carlo valuation requires both the reported mean and requested
sensitivities to be finite. An invalid sensitivity raises `InvalidRisk` with the
output and input names; a non-finite aggregate mean raises `InvalidPayoff`.
This validation does not make an undefined local derivative mathematically valid.
For example, the native derivative of `sqrt(0)` is infinite. A finite reported value is
therefore insufficient evidence of differentiability at an endpoint.

### Structured Scalar Risk Results

`ValueByMonteCarloWithRisk` in `dal-public/src/value.hpp` runs one valuation and
returns a passive `Script::RiskResult_`. Its default simulation enables native
AAD. Supplying `MonteCarloSettings_` with `enableAad_=false` selects price-only
execution; omitted inputs then select no risk columns. The existing dictionary
valuation entries retain their defaults and execution path.

The output ID is `payoff`; its raw Jacobian always has shape `(1, n)`, including
`(1, 0)` for an explicit empty input selection. Omitted native inputs select all columns;
explicit IDs select and reorder them. Model IDs are `model:<ordinal>` and script
constant IDs are `constant:<ordinal>`. These IDs are local to the stored complete
axis definition and numeric snapshot. Display labels do not determine extraction.
Coordinates retain native units and optional physical units. BS and correlated
BS units follow their typed parameter layout; other model families and arbitrary
script constants retain an unknown physical unit. No currency or financial
scale is inferred from a display name. Explicit empty native inputs preserve the
smoothed native estimator; they do not switch to passive hard decisions.

`Jacobian()` returns raw derivatives. `ReportedJacobian()` returns a separate
matrix with each requested positive finite reporting factor applied once.
`LegacyValues()` returns raw `PV`/`d_...` values and rejects colliding display
keys; equal model/script display names remain distinct in structured columns.
Getters expose no active numbers and do not rerun calculation.

`numericPayloadBudgetBytes_` bounds returned value and raw-Jacobian numeric
storage. It excludes metadata, source storage, worker/tape memory and getter
copies. Path count, selection, factors and budget are validated before date
capture, history resolution, compilation or worker submission. Nonfinite
requested values/derivatives or reported multiplication overflow fail before
publishing a result.

The result retains the resolved evaluation date, model-data JSON snapshot,
complete numeric coordinate axis, product definition, simulation settings and
the actual observation keys/frozen historical values from preparation. Native
LSM `RetrainedBump` uses the method label `NativeAADWithRetrainedPolicySecant`;
its policy secant is not represented as a wholly analytic derivative. Paths per
replicate and actual pricing-replicate count are explicit. An expired result
is labelled `Expired` and its stored path count is the requested count. No
uncomputed standard error is supplied.

The lower-level `Script::ProjectMonteCarloRiskResult` in
`dal/script/riskresults.hpp` converts an existing `SimResults_` without valuation
or a reverse sweep. It divides the payoff sum by the positive path count once;
existing mean gradients are copied unchanged. Its provenance is caller-supplied
conversion metadata and cannot certify execution. The public valuation entry
constructs provenance from its own sealed preparation.

Python exposes keyword-only `RiskRequest_` and `MonteCarlo_ValueWithRisk`.
Result properties are read-only; matrix/container getters return detached
copies. Excel uses immutable request/result handles and `RISKRESULT.GET.*`
getters. A zero-column Jacobian spills one blank Excel cell; `GET.SHAPE` reports
the exact `(1, 0)` extent. See the [C++ guide](../public-api.md#structured-script-risk),
[Python guide](../python/README.md#structured-script-risk) and
[Excel guide](../excel/script-settings.md#structured-script-risk).

### Weighted Script Risk Results

`ValueByMonteCarloWithWeightedRisk` in `dal-public/src/value.hpp` returns an
owning passive `Script::WeightedRiskResult_` for one fixed weighted objective.
Select scalar slots using `request.selection_.outputs_`; omitted outputs select
`payoff`. Omitted weights are ones. Signed and zero weights are allowed, while
every selected component must remain finite even at weight zero. Empty, repeated
or unknown outputs, incompatible weights, exercise and fully expired products
fail before historical resolution or worker submission.

The driver evaluates the script once per path, gathers the requested slots and
constructs one weighted native root. Each path reverses its suffix once; each
batch reverses its prefix once. Constants, direct inputs, historical values and
aliases use the same recording lifecycle. The scalar objective specialization
has no weighted buffers or per-path selection branch.

`OutputAxis()`, `Weights()` and `ComponentMeans()` preserve requested order.
`WeightedValue()` is the objective mean; `Jacobian()` has shape `(1, n)` and
contains its mean gradient. Native gradients are not divided again during
projection. `ReportedJacobian()` applies selected input factors to a detached
copy. The complete input axis and actual preparation provenance are retained.
The result owns all these passive values and exposes no recording state.
Nonfinite projection errors identify the weighted objective's ordered component
IDs and weights; risk errors also identify the selected input coordinate.

The exact retained numeric budget is
`sizeof(double) * (1 + n + 2 * components)`: objective value, raw gradient,
component means and weights. Metadata, source snapshots, temporary worker/tape
storage and detached getter copies are outside this budget. Explicit empty
native inputs preserve AAD smoothing and the `(1, 0)` shape; disabling AAD
explicitly selects price-only execution and rejects nonempty risk selection.
See the [C++ example](../public-api.md#weighted-script-risk).

Python exposes `WeightedRiskRequest_`, `MonteCarlo_ValueWithWeightedRisk` and
`Product_Get_RiskOutputs`. Typed requests/settings are copied before releasing
the GIL, and the passive result's matrix/container getters return independent
copies. The [Python reference](../python/README.md#weighted-script-risk) describes
the keyword-only entry and read-only properties.

Excel uses immutable weighted request/result handles. Output-axis and component
tables retain IDs/labels/slots and selected weights/means, while raw/reported
gradients keep one row. Stored snapshot getters share the scalar result's
conversion rules and perform no valuation. See the
[worksheet reference](../excel/README.md#weighted-script-risk).

### Budgeted Script Jacobians

`ValueByMonteCarloWithJacobianRisk` returns the means of ordered scalar script
outputs and their full selected-input Jacobian. `JacobianRiskRequest_` reuses
the scalar input/output/report-factor selection, adds `maxBlockWidth_` (default
one), and accepts optional recording and scratch capacity limits. Omitted
outputs select `payoff`; explicit empty outputs are invalid. Aliased outputs
remain distinct rows. Exercise and fully expired products are unsupported.

Rows and columns follow the requested IDs. For `m` outputs and `n` inputs,
`Values()` has length `m` and `Jacobian()` has shape `(m, n)`, including `(m, 0)`.
The values and gradients are finite-sample means; no additional discounting or
normalization is applied. `ReportedJacobian()` returns a separate matrix with
each column multiplied by its report factor.

The native driver records a fresh fixed-width graph for each output block and
replays the complete requested path range. Each block uses the same sealed
product, model, date, resolved history, RNG stream and absolute path indices.
Path suffixes reverse per path; the retained prefix reverses once per batch.
Alias, constant, direct-input and prefix roots have independent seeds, and
unused tail lanes are zero. Width one uses vector mode with one channel.
`Execution()` records actual widths, replay attempts, executed paths and peak
admitted recording/scratch capacities during replay. For example, four outputs
at width three use two width-three blocks and twice the requested forward paths.

The three budgets have different scopes:

- `selection_.numericPayloadBudgetBytes_` bounds retained raw numeric values
  and risks: `sizeof(double) * m * (1 + n)`. Axes, snapshots, execution metadata
  and detached getter copies are excluded.
- `recordingCapacityBudgetBytes_` bounds aggregate tape payload: allocated
  node, derivative, argument-pointer and vector-adjoint blocks, including
  unused tails and resident cached blocks. Each concurrent worker also needs
  protected replacement headroom for cleanup; unused headroom is excluded
  from the reported payload peak.
- `scratchCapacityBudgetBytes_` bounds admitted result/batch buffers, roots,
  numeric model/path/evaluator capacities and fixed worker/task-slot storage
  across concurrent workers. Growth includes old/new allocation overlap.
  Immutable preparation/selection metadata, strings, allocator bookkeeping,
  third-party RNG state, process RSS and detached getter copies are excluded.

Result extents and initial bounds are checked before history or tasks. With a
capacity limit, model-aware admission constructs guarded startup buffers after
the observation/timeline plan is known and before reading history. It accounts
for all concurrent workers, known model initialization, path arrays, scalar
variable counts, nested fuzzy stores and historical evaluator shapes. When past
events exist, vector seed admission conservatively uses the parsed vector
capacity bounds, including current/replacement seed overlap. A smaller width
is chosen when needed. These probes evaluate no simulated path and read
no fixing. Startup admission divides the remaining scratch budget equally among
concurrent workers. A previous execution's measured peak depends on scheduling
and does not guarantee sufficient startup quota for each worker on a later call.
History-dependent/control-flow growth remains guarded at allocation
boundaries during replay. Runtime exhaustion drains submitted tasks and returns
an error identifying the block/output and capacity cause; it publishes no partial
matrix and does not retry. Earlier completed results remain usable.

Explicit empty native inputs preserve fuzzy AAD prices. Explicit passive mode
selects no risk inputs, performs one forward replay, reports zero recording
payload and retains the `(m, 0)` shape. The result, axes, provenance and diagnostic
containers own passive values. See the [C++](../public-api.md#budgeted-script-jacobians),
[Python](../python/README.md#budgeted-script-jacobians) and
[Excel](../excel/README.md#budgeted-script-jacobians) interfaces.

### Sealed Script Portfolio Coordinates

`Script::ScriptPortfolioData_` in `dal/script/portfolio.hpp` owns an ordered
trade table with independent product snapshots and immutable model snapshots.
Model owners are assigned before cloning: repeating the same original model
handle shares an owner, while two different handles with equal names and values
retain distinct owners. Trade IDs must be nonempty and unique under DAL's
case-insensitive string comparison. JSON serialization preserves the owner table.

`ScriptPortfolioRiskAxes` in `dal-public/src/portfoliorisk.hpp` returns an owning
passive coordinate catalog. All model parameters come first in owner order,
followed by each trade's private constants. IDs are `model:<owner>:parameter:<p>`
and `trade:<t>:constant:<c>`; matching constant labels do not merge columns.
Scalar outputs retain the original local slots and receive IDs such as
`trade:<t>:payoff` and `trade:<t>:output:<slot>`.

```cpp
#include <dal-public/src/portfoliorisk.hpp>
#include <dal/model/blackscholes.hpp>

using namespace Dal;
using namespace Dal::Script;

const Handle_<ModelData_> model(new BSModelData_("model", 100.0, 0.2));
const Vector_<Cell_> dates{Cell_("X"), Cell_(Date_(2027, 1, 1))};
const Handle_<ScriptProductData_> a(
    new ScriptProductData_("A", dates, {"5", "pay PAYS 2 * SPOT() + X"}));
const Handle_<ScriptProductData_> b(
    new ScriptProductData_("B", dates, {"7", "pay PAYS 3 * SPOT() + X"}));
const Handle_<ScriptPortfolioData_> portfolio(
    new ScriptPortfolioData_("portfolio", {{"A", a, model}, {"B", b, model}}));
const auto axes = ScriptPortfolioRiskAxes(portfolio);
// Four shared Black-Scholes parameters, followed by A.X and B.X.
const auto& localToGlobal = axes.TradeInputPositions();
```

`TradeInputPositions()` maps each trade's local model/constant axis to the global
input positions. `OutputTrades()` maps each output row to its trade position;
the original user IDs remain available through `portfolio->TradeIds()`.
Axis inspection parses and indexes the sealed scripts without resolving an
evaluation date, reading fixings, submitting tasks or performing valuation.
The catalog retains ordinary numbers and strings after its portfolio is destroyed.

### Weighted Script Portfolio Risk

`ValuePortfolioByMonteCarloWithWeightedRisk` in `dal-public/src/value.hpp` values
a sealed portfolio using ordered global output/input IDs. Omitted outputs select
one payoff per trade and omitted weights mean one. Provided weights may be zero
or negative, must be finite and must match the selected output count. Selected
values must be finite even at zero weight. The objective is the weighted sum of
component path means, with no additional discounting or currency conversion.

```cpp
// Use the sealed portfolio from the preceding example.
#include <dal-public/src/value.hpp>

PortfolioWeightedRiskRequest_ request;
request.selection_.outputs_ = Vector_<String_>{"trade:0:payoff", "trade:1:payoff"};
request.weights_ = Vector_<double>{2.0, -1.0};
request.selection_.inputs_ = Vector_<String_>{
    "model:0:parameter:0", "trade:0:constant:0", "trade:1:constant:0"};
ScriptValuationSettings_ valuation;
valuation.evaluationDate_ = Date_(2026, 1, 1);
const auto risk = ValuePortfolioByMonteCarloWithWeightedRisk(
    portfolio, 4096, request, valuation);
const double objectiveMean = risk.WeightedValue();
const auto gradient = risk.Jacobian(); // one row, three selected columns
const auto reported = risk.ReportedJacobian();
const auto& groups = risk.Execution().groups_;
```

The call freezes one evaluation date and one union historical snapshot. Trades
share a model and scenario only when owner identity and complete original sampling,
observation, numeraire and simulation contracts agree. Incompatible groups retain
their own time grids, random dimensions and original absolute path indices;
there is no union timeline. Selected trades always have private constants,
historical seeds and evaluator state. Groups run sequentially, with parallel
path batches within a group. Native execution reverses one weighted suffix per
path and each retained batch prefix once.

Each group or native output block submits at most the admitted worker count.
A job constructs its model, RNG, Gaussian/scenario buffers and private evaluators
on its executing thread, then reuses their capacity across its assigned original
batches. Every native batch starts a fresh recording, re-registers parameters and
constants, initializes model coefficients and historical seeds, and repositions
the RNG to the original absolute offset. Batch results retain separate slots and
reduce in their original order. All worker state is destroyed after tasks drain;
it does not persist across requests, groups or output blocks.
Requests with only one batch per worker use the direct batch entry.

Default settings use native AAD. Explicit empty native inputs retain the smoothed
estimator and a `(1, 0)` gradient. Setting `simulation.enableAad_ = false` selects
sharp price-only execution, permits only omitted/empty inputs and performs no
reverse work. Passive execution ignores the recording-capacity budget. Both
modes retain the complete unscaled coordinate catalogs and requested component
order. Report factors must be positive and finite; reported gradient copies scale
selected columns once. A nonfinite report projection rejects the whole call.

The owning result retains component means/weights, selected/complete axes,
original trade IDs, model-owner ordinals and per-trade execution snapshots.
`Execution()` reports group membership, complete original sampling definitions,
simulation settings, actual scenario/evaluator/reverse counts and capacity peaks.
Scenario counts measure generated paths; evaluator counts measure selected
trade evaluations. Matrix getters return detached copies without history reads
or additional Monte Carlo work.

`selection_.numericPayloadBudgetBytes_` bounds the retained numeric payload at
`sizeof(double) * (1 + n + 2*m)`. `recordingCapacityBudgetBytes_` and
`scratchCapacityBudgetBytes_` apply to the whole request. Known startup shapes,
including all selected private history/vector capacities, are admitted before
historical reads. Runtime growth and overlapping replacements remain guarded.
Batch gradient buffers pack requested model and private input columns when
discarded scalar gradient bytes exceed the fixed model/trade mapping payload.
Compact nonempty sources retain full private extraction and project the requested
columns afterward; explicit native empty selection retains zero numeric columns.
Their original ordinals and complete model-input prefix preserve global coordinate
identity and requested order. All recorded inputs remain registered, so selecting
fewer returned risks reduces numeric buffer storage while retaining the full recorded graph.
Capacity or worker failures drain accepted tasks and publish no partial result.
Exercise and fully expired trades retain the multi-output rejection boundary.

### Script Portfolio Jacobians

`ValuePortfolioByMonteCarloWithJacobianRisk` returns separate output means and
an owning `(m, n)` risk matrix over the same sealed portfolio and global axes.
The entry uses `PortfolioJacobianRiskRequest_`; omitted outputs select
every trade payoff. `maxBlockWidth_` is an explicit positive maximum bounded by
the native adjoint capacity, with a default of one.

```cpp
// Use the same sealed portfolio and valuation date.
PortfolioJacobianRiskRequest_ attributionRequest;
attributionRequest.selection_.outputs_ = Vector_<String_>{
    "trade:1:payoff", "trade:0:payoff"};
attributionRequest.selection_.inputs_ = Vector_<String_>{
    "trade:1:constant:0", "model:0:parameter:0", "trade:0:constant:0"};
attributionRequest.maxBlockWidth_ = 2;
const auto attribution = ValuePortfolioByMonteCarloWithJacobianRisk(
    portfolio, 4096, attributionRequest, valuation);
const auto outputMeans = attribution.Values();
const auto sensitivities = attribution.Jacobian(); // two rows, three columns
const auto& attributionGroups = attribution.Execution().groups_;
```

Native attribution includes explicitly empty input selection, which retains the
smoothed estimator and an `(m, 0)` matrix.
Each compatible group replays its original path range once per output block.
Model leaves are shared inside each recording; constants, historical state and
evaluators stay private to the trades needed by that block. Independent roots
preserve output aliases and unused tail lanes stay zero. Only selected derivative
columns must be finite. Raw and reported matrix getters return detached copies;
report factors scale columns once and overflow rejects the entire result with
the original failing trade/output context.

A width-one block uses the native scalar adjoint channel and retains its one-row
Jacobian shape. Wider blocks use vector channels, including zero tail lanes.
Requested maxima, admitted widths and per-batch reversal counts keep the same
meaning. Startup admission remains conservative for a width-one recording.

The retained numeric payload is exactly `sizeof(double) * m * (1+n)`.
Recording and scratch limits apply across the sequential groups and concurrent
original path batches. Known result/task buffers, root lanes, full extracted
model/private matrices and historical vector shapes admit before historical
reads. Capacity-only admission can narrow the maximum block width while keeping
the estimator and original paths. `Execution()` retains the requested maximum;
each group reports its actual widths, replay attempts, scenario/evaluator/reverse
counts and whole-request capacity peaks. For a group with `m_g` rows and admitted
width `w_g`, scenario generation totals `numPath * ceil(m_g / w_g)`.

With `simulation.enableAad_ = false`, attribution uses sharp passive pricing and
permits only omitted/empty risk inputs. It returns `(m, 0)`, evaluates each
selected group once over its original paths, ignores recording limits and reports
no actual native widths or reversals. Passive rows accumulate independently, so
finite output rows remain valid even when their unused aggregate would overflow.
Known private historical shapes still admit against the whole scratch limit before
reads, and runtime capacity/nonfinite failures drain tasks and retain trade/output
context.

### Discrete Dupire Calibration Pullback

The C++ functions in `dal/model/dupirerisk.hpp` map numeric local-volatility
node adjoints to additive implied-volatility spread quotes. Quote rows are
strikes and columns are maturities. Spreads use absolute decimal volatility;
`0.01` is one volatility point. Surface rows are spots and columns are model
times, matching `LocalVolSurfaceData_`.

The public header `dal-public/src/dupirerisk.hpp` also accepts `BSModelData_`
as a flat base IVS, preserving its spot, rate and dividend yield. Python exposes
the same frozen boundary with built-in and custom IVS inputs; see the
[Python quote-risk interface](../python/README.md#dupire-quote-risk).

```cpp
#include <dal/model/dupirerisk.hpp>
#include <dal/model/ivs.hpp>

AAD::MertonIVS_ base(100.0, 0.2, 0.08, -0.1, 0.15);
DupireRiskInputs_ inputs{{75.0, 105.0, 135.0}, {0.4, 1.2},
                         Matrix_<>(3, 2, 0.0), {60.0, 100.0, 140.0}, 10.0,
                         {0.5, 1.0}, 0.5};
const auto calibration = CalibrateDupireWithRisk(base, inputs);
const auto& surface = *calibration.Surface();
DupireParameterAdjoints_ seeds{
    calibration, Matrix_<>(surface.vols_.Rows(), surface.vols_.Cols(), 1.0)};
const auto risk = PullbackDupireCalibration(calibration, seeds);
// TotalAdjoints() differentiates the sum of surface nodes in this example.
```

`CalibrateDupireWithRisk` copies configuration and the display name before
sampling. Its immutable snapshot owns the base IVS samples needed by the
existing central-difference stencil and ATM band selection, deterministic
carry, complete quote/grid definition and numeric calibrated surface.
Subsequent pullbacks use those samples after the original IVS changes or is
destroyed. The derivative holds these inputs fixed and follows the discrete
`1e-4` relative strike/time stencil with the original base-band boundary copies.

The caller supplies numeric surface adjoints. A separate native recording
replays calibration, validates its primal surface and adds each output seed,
including copied boundary aliases. The seed's complete calibration identity
must match; dimensions and display names alone cannot establish compatibility.
When ordinary active replay fails the primal check, a checked replay uses scalar
call prices with the original active-expression derivatives after checking
call-level rounding disagreement. It propagates the discounted Black-leg
rounding bounds through the maturity difference, strike slope, curvature and
carry numerator to bound the local volatility. Both the active value and a
fresh scalar calibration must lie inside this interval before the scalar
local-volatility value becomes the replay primal, with derivative one along
the active expression. This covers contraction across call prices and stencil
operations while preserving the numeric calibration. Unresolved intervals and
larger disagreements fail explicitly; the final surface check remains
relative/absolute `1e-12` before reverse propagation.
Zero and negative seeds are supported. Nonfinite quotes/seeds, invalid grid
spacing, unresolved or nonpositive call curvature and nonpositive local
variance fail explicitly. Independent nested recordings remain unsupported.

An optional `DupireDirectQuoteAdjoints_` carries the quote definition and an
additional numeric contribution. Its ordered quote axes and values must match.
`CalibrationAdjoints()`, `DirectAdjoints()` and `TotalAdjoints()` preserve the
separate contributions to `C_quote^T g_surface + g_direct`. No path averaging or
reporting conversion is applied here. Results own ordinary numeric matrices,
the calibration snapshot, method `NativeAADCalibrationVJP` and unit `decimal-vol`.

The owning common C++ boundary in `dal-public/src/calibrationrisk.hpp` and its
Python factories also
accept this snapshot and preserve these layouts, direct-quote identity,
method and units. It returns the same passive result type as captured curve
provenance; see the [common calibration interface](../yield-curves/jacobian-risk.md#common-passive-c-calibration-pullback).

### Hybrid Valuation to Dupire Quotes

`dal-public/src/dupirerisk.hpp` connects an existing `ValueByMonteCarloWithRisk`
result to its calibration snapshot. Select the local-volatility component by
name and use the snapshot's surface when constructing the Hybrid model:
`NewDupireModelData(name, calibration, index, currency, factor, maxStep=1/12)`
provides a detached one-factor model with `equity` and `rate` components,
preserving the frozen spot and deterministic carry. Python and
[Excel](../excel/README.md#dupire-quote-risk) expose the same convenience.

```cpp
#include <dal-public/src/dupirerisk.hpp>

// valuation was produced with calibration.Surface() in component "equity".
const auto quoteRisk = PullbackDupireScriptRisk(valuation, calibration, "equity");
const auto& quoteGradient = quoteRisk.QuoteRisk().TotalAdjoints();
const auto& retainedPrice = quoteRisk.Valuation().Values();
```

The adapter restores the result's retained numeric model snapshot, checks its
complete model axis and the selected component's surface, and maps raw model
ordinals to spot-major/time-minor seeds. Runtime component ordering determines
the mapping; display labels and report factors cannot replace it. All surface
inputs must be selected, including coordinates whose computed risk is zero.
Missing inputs, changed grids/values, inconsistent model coordinates,
price-only execution and unsupported valuation methods fail explicitly.

The operation performs a separate calibration reverse after the existing
valuation. It does not run Monte Carlo or reread history. Direct quote inputs
are **PV adjoints**, with any cashflow discount already included; they are
added once without another discount or path normalization. To combine compatible
trades, extract each seed with `ExtractDupireParameterAdjoints`, sum their raw
matrices after checking `calibration_.Matches`, and call the core pullback once.

The wrapper retains the passive valuation and quote result. Its method joins
the source method with `ThenNativeAADCalibrationVJP`. An expired source produces
zero calibration risk. A `NativeAADWithRetrainedPolicySecant` source keeps that
mixed-method label: multiplying its model-coordinate secant by the calibration
Jacobian does not establish a full quote-bump/recalibrate/retrain estimator.

### Automatic C++ Dupire Risk Requests

`dal-public/src/dupireriskrequest.hpp` plans and executes one scalar Hybrid
valuation followed by the frozen Dupire quote pullback:

```cpp
#include <dal-public/src/dupireriskrequest.hpp>

DupireScriptRiskRequest_ request;
request.numPaths_ = 257;
request.quotes_.inputs_ = Vector_<String_>{"quote:3", "quote:0"};
request.quotes_.reportFactors_ = Vector_<>{0.01, 0.01};
request.valuation_.evaluationDate_ = Date_(2026, 9, 12);
const auto plan = PlanDupireScriptRisk(product, model, calibration, "equity", request);
const auto result = ValueByMonteCarloWithDupireRisk(plan);
const auto perVolPoint = result.QuoteRisk().ReportedJacobian();
```

The passive owning plan exposes the complete input axis, every required surface
coordinate in native order, quote selection and numeric payload before history
resolution or worker submission. It seals product/model/settings data, including
nested surfaces and correlations. Mutating or destroying caller data does not
change later execution. This entry accepts exact native Hybrid graphs with BS
or local-vol equities, constant correlation and one flat domestic rate provider.
Target spot, dividend, rate, surface grids and surface values must match the
calibration. Custom archive types are rejected before serialization.

An explicit `DupireQuoteBinding_` associates a script constant ordinal with a
source-scoped quote ID. Both must be unique, and their native values must agree.
The caller declares that dependency; matching display names cannot infer it.
Required constant columns follow all mandatory surface columns in binding order.
Their fixed-surface partials are added once after calibration mapping. Checked
external `CalibrationDirectQuoteAdjoints_` are an alternative; supplying both
forms rejects. External Dupire direct identity compares quote axes and values,
independently of the fixed base IVS.

`request.quotes_.numericPayloadBudgetBytes_` bounds the combined retained numeric
result: one scalar value, S surface derivatives, B bound-constant derivatives
and three full Q-quote contribution matrices, or `8 * (1 + S + B + 3 * Q)` bytes.
Subset and empty quote selections retain the same payload for fixed bindings.
The budget excludes source/archive/metadata, plan/input storage, getter copies
and temporary worker/tape/VJP arrays; it is not a process-memory limit.

An omitted evaluation date is captured during planning. Explicit immutable
fixing snapshots are shared; omitted global history is resolved and frozen at
execution after successful preflight. Empty quote selection preserves the
native smoothed estimator and all mandatory valuation derivatives. The scalar
valuation retains fixed-calibration provenance; the combined method preserves
expired and mixed-policy labels. Result getters expose passive values and
detached report projections without history lookup, valuation or another reverse.

### Passive vs Active Tape

Native recording is unconditional. Code that needs a value-only pass (e.g. a baseline pricing run
without differentiation) should use a plain `double` evaluation rather than
relying on tape passivity.

## Segmented path recomputation

The opt-in C++ header `dal/math/aad/segmentedpath.hpp` supplies
`ExecuteSegmentedPath` for a fixed-size state transition. It computes a scalar
objective and its parameter gradient while keeping a tape for one segment at a
time. Ordinary Monte Carlo and script entry points retain their full-path
execution. The kernel interface is a C++ surface.

A `SegmentedPathKernel_` declares its step count, state width and number of
branch-trace slots per step. Its passive-double and native-number overloads
provide initialization, a state transition with a direct objective contribution,
and a terminal objective. Both overloads must evaluate the same formulas,
smoothing and branch decisions. Sharing a typed formula helper keeps that
contract explicit. Active callbacks use supplied fresh active inputs and passive
immutable data; active constants must be materialized from doubles.

For parameters $p$, retained passive drivers $u_k$, complete state $s_k$ and
direct contributions $c_k$, the objective is

$$
s_0 = I(p),\qquad s_{k+1}=F_k(s_k,p,u_k),\qquad
V=G(s_L,p)+\sum_{k=0}^{L-1}c_k(s_k,p,u_k).
$$

The request copies parameters and the complete driver vector once. It performs
a passive prepass, saving state at each segment boundary, segment contribution
sums and exact branch decisions. Reverse execution first records the terminal
objective using fresh terminal state and parameter inputs. It then visits
segments backwards, freshly records their local transitions and propagates
incoming state adjoints. Each segment's direct contribution receives seed one;
each segment's parameter adjoints are added to the same detached gradient.
Finally, a fresh initialization recording propagates the first state adjoints
to the parameters once. Aliased state outputs accumulate their seeds.

Recomputation verifies the complete end state and segment contribution with a
fixed relative/absolute tolerance of $10^{-12}$, and compares branch-trace
entries exactly, before seeding that segment. Numeric equality alone is not a
branch proof. Each trace slot represents a declared decision; unused slots use
a stable sentinel. Drivers are read from the retained vector, including all
bridge-transformed inputs. Callbacks must not redraw randomness or retain
active numbers between independent recordings.

For example, a kernel with $s_0=b$, $s_{k+1}=a s_k+u_k$ and $G=s_3^2$ at
$(a,b)=(2,1)$ and $u=(1,-1,3)$ returns value 169 and gradient $(390,208)$
for segment lengths 1, 2, 3 or greater than the path. The call takes immutable
settings and returns an owning passive result:

```cpp
AAD::SegmentedPathSettings_ settings;
settings.segmentSteps_ = 2;
const auto result = AAD::ExecuteSegmentedPath(kernel, {2.0, 1.0}, {1.0, -1.0, 3.0}, settings);
const double value = result.Value();
const auto& gradient = result.Gradient();
```

The caller's kernel must validate its parameter and driver schema before using
indexed inputs. The engine validates declared state/trace shapes and finite
values. Zero steps, empty state and empty parameters are defined; segment length
must be positive. Unsupported independent nesting is rejected before changing
an outer recording. Success and failure restore the caller's adjoint mode.

`checkpointCapacityBudgetBytes_` admits retained boundary vectors, their vector
headers, contribution sums and branch traces before evaluation. Actual vector
capacity is checked after allocation. `recordingCapacityBudgetBytes_` includes
the existing tape's retained allocation and the native cleanup reservation.
`Execution()` reports passive/recomputed steps, segments, reverse sweeps,
`checkpointBytes_`, `peakTapeBytes_` and separate `cleanupReserveBytes_`.
Peak payload alone is insufficient for admission: ordinary tape allocation must
also leave room for the cleanup reserve. Warm tape capacity remains charged.
These component counts exclude driver/parameter copies, returned results,
temporary kernel vectors, arbitrary kernel data and allocator overhead; they
are not a process memory limit.

Segmentation trades a passive prepass, snapshots and recomputation for a smaller
local tape. Segment length controls that tradeoff; a small path can cost more
without reducing allocated tape blocks. The focused `tape_perf --segmented-path`
benchmark checks short/long paths and direct contributions against independent
price/gradient oracles and reports complete cold and reused-tape request costs
and capacities.
No automatic strategy is selected.

The complete boundary state is the caller's responsibility. A financial kernel
must include all model/evaluator variables, cashflow/vector state, historical
initialization dependencies and observations still needed by later events.
Saving a model factor alone does not satisfy that contract. The Black–Scholes
adapter below supplies that state for compiled scripts. Early-exercise/LSM
checkpoint execution is unsupported.

### Compiled Black–Scholes paths

Include `dal/script/segmentedpath.hpp` to explicitly prepare and evaluate one
fixed Gaussian path with `Script::BlackScholesSegmentedPath_`. Preparation fixes
the timeline, observation identities, historical fixing snapshot, compiled
program and smoothing. Parameters are fresh numerical inputs in the existing
Black–Scholes order, `spot, vol, rate, div`, followed by `ConstVarNames()`.
`ParameterLabels()` names that complete gradient axis. The returned value is
the prepared payoff variable selected by `PayOffIdx()`.

```cpp
Script::ScriptValuationSettings_ valuation;
valuation.evaluationDate_ = Date_(2026, 10, 1);
const Script::ScriptProductData_ product(
    "", {Cell_("SCALE"), Cell_(Date_(2027, 1, 7))},
    {"2", "pay PAYS SCALE * FIX(EQ[DAL196_TEST])"});
auto prepared = std::make_shared<const Script::BlackScholesSegmentedPreparation_>(
    Script::PrepareBlackScholesSegmentedScript(product, valuation));
const Script::BlackScholesSegmentedPath_ path(prepared);
AAD::SegmentedPathSettings_ settings;
settings.segmentSteps_ = 64;
const auto result = path.Evaluate({100.0, 0.2, 0.03, 0.01, 2.0}, {0.3}, settings);
```

The caller supplies exactly `SimDim()` finite Gaussian entries after any random
transform or Brownian bridge. The request retains that vector and never draws
during replay. A time-zero sample emits the original spot exactly and consumes
no draw. `Dimensions().steps_` counts samples, which can include time zero.
The immutable `AAD::BlackScholesStepPlan_` also exposes these resumable model
samples independently through `Advance(sampleId, logSpot, parameters, gaussian)`.

Every boundary retains log spot, persistent script scalars, vector lengths and
entries, and observations whose last consuming event has not finished. Vector
bounds are proved from the prepared acyclic statements, including history and
both branches. Historical constant dependence is freshly recorded once in
initialization. Cumulative payment variables remain readable by later events;
delayed payments use the prepared discount maturities. Each step computes only
its own typed model coefficients and sample, without retaining a full active
model initialization or scenario.

Passive and active passes use the same prepared fuzzy bytecode. Exact trace
slots record hard decisions, smoothing intervals, fuzzy branch regimes, extrema
choices including ties, and vector reduction lengths/choices. Skipped instructions
retain zero sentinels. Ordinary compiled execution uses a separate compile-time
policy with tracing disabled.

`PrepareBlackScholesSegmentedScript` enables compiled native AAD and accepts
valuation settings, an optional fixing snapshot, optional product settings and
optional smoothing. It also resolves and compiles historical-only products,
which have zero future transitions but can retain script-constant dependence.
The factory returns a move-only `BlackScholesSegmentedPreparation_` with a private
constructor. Only this preparation type enters the kernel. `Prepared()` exposes
a const ordinary-script view for independent comparison; generic `PrepareScript`
results cannot establish the model's provenance and cannot be passed directly.
The kernel rejects EXERCISE, validates model/script
parameter and driver shapes, and shares only immutable data across requests.
Its owning value/gradient result survives tape cleanup. Budgets and mode/nesting
rules follow `ExecuteSegmentedPath` above.

The fixed-path C++ adapter does not select a Monte Carlo strategy or change
ordinary script valuation. Capacity statistics cover the declared tape and
checkpoint components; immutable preparation, drivers, results and kernel
scratch require separate accounting. Growing vector state or wide exact traces
can erase a capacity advantage. The explicit
`tape_perf --financial-segmented-path --case running --mode segmented-warm`
benchmark compares complete requests for short, running-observation and
live-fixing/payment paths in cold and reused-tape regimes.

For the 2,048-sample reference cases at segment length 64, tape plus checkpoints
uses about 25% less capacity. An independent C++ heap probe finds about 38% lower
request peaks and about 6.2% lower totals after including immutable preparation,
adapter storage and caller inputs. Warm requests take about 2.6–2.7 times the full
graph time. The 16-sample case has no total-memory advantage. These measurements
describe an explicit memory/latency tradeoff, not a strategy-selection rule.

### Segmented Black–Scholes Monte Carlo

Include `dal/script/segmentedmontecarlo.hpp` for the explicit
`Script::EvaluateBlackScholesSegmentedMonteCarlo` request. With the immutable
path prepared above:

```cpp
Script::SegmentedMonteCarloSettings_ mc;
mc.firstPath_ = 7;
mc.useBb_ = true;
mc.path_.segmentSteps_ = 64;
const auto mean = Script::EvaluateBlackScholesSegmentedMonteCarlo(
    path, {100.0, 0.2, 0.03, 0.01, 2.0}, 128, mc);
```

`MeanValue()` and `MeanGradient()` are normalized by the positive path count.
`ParameterLabels()` names the fixed-path model/script axis. All result data is
owned after task draining and tape cleanup. Ordinary `MCSimulation` retains its
raw price sum and normalized-risk convention.

Settings select sobol, mrg32 or irn, optional Brownian bridge and an optional
Sobol-only `scrambleKey_`. `firstPath_` is an absolute zero-based offset applied
with `SkipNormalTo`. `normalPrecision_` accepts `Default`, `Fast` or `Precise`;
`Default` uses Fast for every generator. Select `Precise` to retain the previous
MRG32/IRN normal conversion; see [normal precision](monte-carlo/sampling.md#monte-carlo-normal-precision).
There is no additional 2048 Sobol offset. Each path draws
its transformed Gaussian vector once and retains it through replay. Time-zero
samples consume no Gaussian; zero-dimensional and historical-only products
allocate no RNG; their offsets obey size_t range admission without a Sobol
direction limit. Their inclusive last path index must fit, so a single path at
SIZE_MAX is valid; requests with drivers retain RNG exclusive-end limits.
Historical constant dependence, vectors, old fixing observations
and readable cumulative/delayed payments follow the compiled fixed-path semantics.

Fixed 32-path batches accumulate in path order; individual batch sums are reduced
in batch order and normalized once. Numerical results are bitwise reproducible
across worker counts for the same build, platform and floating-point environment.
At most `min(pool threads, 64, batch count)` exclusive request-local lanes execute
in a wave. Their RNG/Gaussian storage survives drained waves, so IRN seeking moves
forward instead of restarting from its seed per batch. IRN skip work still depends
on lane count. Inputs/settings are snapshotted before submission; concurrent
callers do not share lane storage.

Invalid inputs, settings, ranges, nesting and checkpoint plans are rejected before
task/result allocation. Recording-capacity admission occurs on each executing
thread. Submission or worker failure drains all accepted tasks before propagating
the error. Double-precision batch/aggregate sums must be finite; an ideal
higher-precision mean can exist even when this sum contract rejects it.

`Execution()` records count/offset, generator/bridge/shift/precision settings, batch/segment
sizes, admitted lane count and maximum per-path tape/checkpoint/cleanup payload.
Those maxima can depend on earlier retained tape capacity and are not process
limits or simultaneous aggregate peaks. Complete MC memory also includes
preparation, adapter, lanes, coordinator and results. Fixed-path measurements above
do not establish MC memory savings. The focused command
`tape_perf --financial-segmented-mc --case running --mode segmented --threads 4`
measures complete MC requests; `full` selects the ordinary native MC comparison.
Segmentation remains explicitly selected.

For 128 Sobol paths over 2048 daily fixings at h=64, focused Release measurements
on an i9-13900HX show warm C++ allocation payload peaks of 7,119,391/6,440,755 bytes
(ordinary/segmented, one thread) and 16,896,567/13,258,387 bytes (four threads).
The approximately 9.5%/21.5% reduction costs about 8.1x/7.3x complete-request warm
latency. Short 16-step requests show negligible warm memory improvement and
about 3.4–8.6x latency. Cold short four-thread probes can use more memory as
different workers create retained tapes. These observations include preparation,
adapter, request storage, tapes and consumed results; they exclude allocator
metadata, direct C allocations, stacks, common runtime infrastructure and RSS.
Shared-host paired sampling and per-worker retained capacity limit generalization.

## Smooth native forward-over-reverse prototype

`AAD::EvaluateForwardOverReverse` in `dal/math/aad/forwardoverreverse.hpp`
computes a smooth scalar function's gradient, directional derivatives and
Hessian-vector products without an outer difference step. Its callback uses
the distinct `ForwardOverReverseNumber_`: each value contains a native active
primal and a native active directional derivative. Reversing the latter gives
the Hessian product. Returned values own numeric storage and survive tape cleanup.

```cpp
AAD::ForwardOverReverseRequest_ request;
request.directions_ = Matrix_<>(1, 1, 1.0);
const auto result = AAD::EvaluateForwardOverReverse(
    [](AAD::RecordingScope_*, const Vector_<AAD::ForwardOverReverseNumber_>& x) {
        return x[0] * x[0] * x[0] * x[0];
    },
    {2.0}, request);
// Value 16, gradient 32, directional derivative 32, Hessian product 48.
```

Direction rows preserve their order and signs; product columns follow input
coordinates. Zero and tiny directions are valid. With M positive directions,
the driver records M graphs and performs M+1 scalar reverse sweeps; an empty
direction matrix performs one evaluation and one gradient sweep. Execution
metadata reports these counts, the `NativeForwardOverReversePrototype` method,
actual peak tape capacity and cleanup reserve. The optional numeric-output
budget covers `(1 + 2N + 2MN + M) * sizeof(double)`; metadata, callback captures
and temporary recording storage are excluded. A separate recording-capacity
budget bounds native tape allocation, including cleanup reserve, across graph
reuse. These are capacity measurements, not RSS or total process memory.

Supported primitives are arithmetic, `exp`, `log`, `sqrt`, `pow`, `erfc`, `NCDF`
and `NPDF`. Constants require explicit construction when returned directly.
Log/sqrt arguments and active-exponent bases must be positive. Passive integer
powers admit negative bases and smooth zero-base powers; powers zero and one
have constant and identity semantics. All values, directional derivatives and
reverse outputs must be finite within floating-point range.

Callbacks must represent the same deterministic C2 function for every direction.
The driver checks repeated scalar values; this does not certify purity. Extracting
`Value()` or `DirectionalDerivative()` to bypass active arithmetic, or branching
on a direction, violates this contract. Comparisons, abs/min/max, regression,
opaque reverse events, checkpoints and independent nesting are unsupported.
The caller's scalar/vector mode is restored after success or failure.

`ForwardOverReverseCapabilities()` describes only this opt-in prototype.
Ordinary native `higherOrder_` and independent nesting remain false. This core
C++ surface does not differentiate arbitrary existing financial adapters or
provide Python/Excel projection. Existing finite-step curvature entries retain
their own estimator semantics.

## Directional curvature with bump-over-AAD

`AAD::EvaluateBumpOverAAD` in `dal/math/aad/bumpoveraad.hpp` takes a scalar native
callback, a numeric point and a `BumpOverAADRequest_`. Direction rows and positive
steps are explicit. The result owns the base value and gradient, the point,
directions, steps and an M-by-N matrix whose row k estimates

$$
H(x)v_k \approx \frac{g(x+h_kv_k)-g(x-h_kv_k)}{2h_k}.
$$

A unit direction selects one Hessian column: its diagonal entry is Gamma and
the other entries are cross-Gammas. Arbitrary signed, unnormalized directions
produce HVPs without allocating a dense Hessian. The driver performs exactly
1+2M fresh recordings and reverse sweeps, including the base gradient. Empty
directions request only that base value and gradient.

```cpp
#include <dal/math/aad/bumpoveraad.hpp>

using namespace Dal;
AAD::BumpOverAADRequest_ request;
request.directions_ = Matrix_<>(1, 2, 0.0);
request.directions_(0, 0) = 1.0;
request.steps_ = {1e-3};
const auto result = AAD::EvaluateBumpOverAAD(
    [](AAD::RecordingScope_*, const Vector_<AAD::Number_>& x) -> AAD::Number_ {
        return 3.0 * x[0] * x[0] + 2.0 * x[0] * x[1] + 5.0 * x[1] * x[1];
    },
    {2.0, -1.0}, request);
// HessianProducts()(0, 0) is 6; HessianProducts()(0, 1) is 2.
```

The callback receives the owned recording scope for composing existing recorded
operators. It must rebuild its graph from the supplied inputs, keep captured
external state fixed and leave recording lifecycle, checkpoints and adjoint mode
to the driver. The driver rejects callback checkpoint creation before reading
the returned node, including callbacks that restore the checkpoint. Retaining
active numbers across calls is invalid. The driver
snapshots the callback and numeric request, uses scalar adjoints and restores the
caller's mode on success or failure. Nested independent recordings are rejected.

All directions are checked before any callback: finite nonzero rows, finite
positive steps and finite plus/minus bumps that change every nonzero coordinate.
Nonfinite values, gradients and products are rejected. Scaled central division
avoids overflow caused solely by the numerator or `2h`, and rejects nonzero
quotients outside the representable nonzero finite range.

`numericPayloadBudgetBytes_` admits exactly `sizeof(double) * (1+2N+2MN+M)`
owning numeric bytes. Headers, metadata and temporaries are excluded. The separate
`recordingCapacityBudgetBytes_` covers retained tape capacity plus cleanup reserve;
execution reports peak tape bytes and the reserve separately, excluding RSS.

This is a central finite-difference estimator of native first derivatives. On
smooth kernels its truncation error is O(h²); rounding and callback errors grow
as steps shrink. There is no automatic step, h/2 refinement, normalization or
symmetry correction. `higherOrder_` remains false. Stochastic common paths,
nonsmooth payoffs, calibration curvature and policy responses require explicit
caller methodology; the generic driver does not certify those estimators.

### Common-path segmented Monte Carlo curvature

`Script::EvaluateBlackScholesMonteCarloCurvature` in
`dal/script/montecarlocurvature.hpp` composes complete segmented native AAD
Monte Carlo gradients with the same explicit direction/step request. It takes
a sealed `BlackScholesSegmentedPath_`, the full parameter point, positive path
count, `BumpOverAADRequest_` and optional `SegmentedMonteCarloSettings_`.
Columns follow `ParameterLabels()`: spot, volatility, continuously compounded
rate, dividend yield, then prepared script constants. No coordinate scaling,
direction normalization or Hessian symmetry correction is applied.

```cpp
#include <dal/script/montecarlocurvature.hpp>

AAD::BumpOverAADRequest_ bumps;
bumps.directions_ = Matrix_<>(1, static_cast<int>(parameters.size()), 0.0);
bumps.directions_(0, 0) = 1.0;
bumps.steps_ = {0.25};
const auto risk = Script::EvaluateBlackScholesMonteCarloCurvature(
    kernel, parameters, 8192, bumps, settings);
const double spotGamma = risk.HessianProducts()(0, 0);
```

Each row differences the mean first-order gradient over exactly the same
absolute path interval, generator, scramble key, bridge and normal precision.
The kernel, point, bumps and MC settings are snapshotted before submission;
all plus/minus model domains are checked before any MC task. Each first-order
request retains the segmented path's bounded recording and fixed-batch ordered
reduction. The execution count `1+2M` is a count of gradient requests, not
reverse sweeps: every path can run multiple segment reverses.

The result owns `Base()` (mean value, gradient, labels and MC execution),
`Point()`, `Directions()`, `Steps()`, requested `Settings()` and the curvature
matrix. `Prepared()` retains immutable contract, valuation date, historical
seed, timeline, compiled program and smoothing after the original inputs and
kernel are destroyed. These fields identify the exact declared estimator.

`numericPayloadBudgetBytes_` has the same owning-double formula as the generic
driver. It excludes retained preparation, labels, metadata, temporary gradients,
tasks and random buffers. The minimum of the bump request and MC path recording
caps applies to every executing path; the checkpoint cap remains per path.
`Execution()` records the effective recording cap and maximum per-path tape,
checkpoint and cleanup reserve across all gradient requests. These maxima do
not measure concurrent aggregate capacity or RSS. Nested independent recording
entry is rejected; the caller's adjoint mode is restored after success/failure.

These outputs are finite-step secants of the compiled first-order estimator.
Fuzzy condition kernels are piecewise linear/triangular, while extrema can
remain hard; a positive smoothing width does not imply a globally C2 payoff.
For a hard vanilla call, the common-path spot secant estimates the change in
pathwise delta across the strike interval. Step bias and finite-path error
remain distinct. Smooth polynomial convergence does not establish convergence
order for nonsmooth products, unbiased Gamma, calibration curvature or exercise
policy responses. Native `higherOrder_` remains false.

### Native LSMC policy curvature

`Script::EvaluateBlackScholesLsmcCurvature` in `dal/script/lsmccurvature.hpp`
takes an owning `shared_ptr<const PreparedScript_>`, a complete numeric point,
positive pricing path count and explicit `AAD::BumpOverAADRequest_`. The point
contains spot, volatility, rate and dividend yield, then the prepared script
constants. The preparation must originate from the concrete native
`AAD::BlackScholes_<double>` model, contain live EXERCISE events and enable native
AAD. Preparation retains the original model's dynamic type without retaining its
state. Hybrid models and custom subclasses are rejected before training even
when their observations are compatible; the supplied point replaces only the
four Black–Scholes parameter values. Tree and compiled programs use the
preparation's smoothing and sampling.

`Frozen` trains one baseline policy and retains its coefficients, basis,
normalization and selected degrees for all outer points. Path states and script
constants remain active, including continuation predictions and historical
initialization. This is curvature conditional on that fitted policy. Repeating
ordinary Frozen valuations at bumped points instead trains new policies and
therefore estimates a different quantity.

`RetrainedBump` trains at each outer point. Its first-order estimator combines
the native partial derivative with the existing policy-only price secant. The
inner step is `lsmcPolicyBumpRelative_ * max(1, abs(coordinate))` at that point;
model lower-domain boundaries use the existing one-sided fallback and script
constants use central inner bumps. The outer product differences that complete
gradient estimator. Its method is `BumpOverRetrainedNativeLsmcPolicySecant`;
Frozen uses `BumpOverFrozenNativeLsmcAAD`.

All evaluations share absolute training, validation and pricing blocks, RNG,
bridge, normal precision and RQMC seeds/replicate keys. Constants replay the
sealed historical program before training and pricing. The result owns the
base mean `Value()`, full `Gradient()`, `Point()`, `Directions()`, `Steps()`,
`HessianProducts()`, immutable `Prepared()` and `BasePolicy()`. Execution records
`1+2M` gradient requests and the training/validation/pricing/replicate counts.

Numeric output budgets follow the generic owning-double formula and exclude
retained preparation, policy, metadata and temporary work. Recording caps apply
independently to each replay batch. Maximum batch tape and cleanup reserve are
capacity measures, not aggregate concurrent memory or RSS. Nested recording is
rejected; caller and worker adjoint modes are restored on every exit. Every
outer model domain and inner policy-bump representation is checked before
training. Failures identify the base or direction/sign.

These are finite-step products of a finite-path estimator. Piecewise smoothing
and hard policy fitting do not guarantee a globally C2 function, symmetric
Hessian, unbiased Gamma or quadratic step convergence. Outer-step bias, inner
policy-secant error and sampling dispersion must be assessed separately.
Native `higherOrder_` remains false. Language bindings and other model families
are outside this C++ entry.

### Recalibrated Dupire quote curvature

`EvaluateDupireQuoteCurvature` in `dal/model/dupirecurvature.hpp` estimates
Gamma, cross-Gamma and HVPs in the complete decimal-vol spread coordinates of
a `DupireCalibrationSnapshot_`. It accepts a native scalar objective, the
snapshot and an explicit `AAD::BumpOverAADRequest_`. Objective inputs contain
local-volatility nodes in strike-major order, followed by all quote spreads in
strike-major order. An objective can therefore depend on both the calibrated
surface and the quotes directly.

At every base and perturbed quote point, the driver rebuilds the numerical
calibration, differentiates the objective and runs a fresh calibration pullback:

$$
g(q)=Dc(q)^T\partial_c\phi(c(q),q)+\partial_q\phi(c(q),q).
$$

Product rows difference these complete quote gradients using the submitted
directions and steps. Rebuilding the calibration and its pullback includes
calibration curvature; holding the original Jacobian fixed would omit it.
`RecalibrateDupireWithRisk(snapshot, spreads)` also exposes the passive rebuild.
It retains sealed base-IVS samples, carry, quote axes, grids and fixed bands,
preserves the surface name and leaves the original snapshot valid. Neither
recalibration nor `ValidateDupireQuoteRecalibration` resamples an external IVS.

The result owns the base `Value()`, complete quote `Gradient()`, `Point()`,
`Directions()`, `Steps()`, `HessianProducts()` and recalibrated `Calibration()`.
`Execution()` counts one calibration, objective reverse and calibration reverse
per quote-gradient evaluation: exactly 1+2M, including the base. Empty direction
sets request the base only. Output columns use raw quote units; report-scale
Hessians require both coordinate conversion factors.

All plus/minus numeric and stencil domains are admitted before the first
objective. The numeric budget uses the same owning-double formula as the generic
driver with N equal to the quote count; retained snapshots and temporary arrays
are separate. The recording cap applies independently to each sequential
objective recording and calibration pullback. Reported peak tape capacity and
cleanup reserve exclude RSS. Nested driver entry is rejected and caller adjoint
mode is restored after success or failure.

The callback follows the sequential native scalar contract and keeps captured
data fixed. It cannot contain parallel simulation or another independent
recording. These are finite-step estimates of the implemented fixed-band Dupire
method, whose numerical call stencils introduce rounding error. Quote
interpolation folds can invalidate a bumped stencil when they coincide with a
calibration node; such requests reject. Smaller steps do not guarantee better
accuracy. This entry does not provide Monte Carlo quote curvature, exercise
policy responses or native mixed-mode differentiation; `higherOrder_` is false.

### Recalibrated rate quote curvature

`dal-public/src/ratecurvature.hpp` provides `NewRateCalibration(spec)` for
single-curve, same-currency joint, staged cross-currency basis and joint
cross-currency calibration definitions. The owning
`RateCalibrationSnapshot_` retains copied native instruments, deep-copied fixed
native curve bases, raw quotes, solved free parameters and calibration
provenance. YC instrument order is normalized and frozen inside each declaration;
duplicate curve names retain distinct joint declaration keys. Recalibration
uses these sealed inputs rather than later changes to caller handles.

Staged cross-currency snapshots expose basis quotes and basis parameters only;
the domestic/foreign curve blocks remain fixed dependencies. Joint cross-currency
snapshots expose domestic curve, foreign curve and basis blocks in the native
residual/parameter order. Projection routes follow each declaration's actual
tenor and collateral. YC instrument groups are normalized into solver order;
XCCY instruments retain declaration order. Joint currency keys are
`domestic:<ordinal>:<name>` and `foreign:<ordinal>:<name>`, with zero-based
declaration ordinals preventing collisions from repeated names. Prefixes apply
only to the sealed copy; basis keys remain `basis:<name>`.
FX spot remains fixed outside the
quote axis. Fixed native curve graphs and legacy single-curve block fallback
are preserved by deep copies. Missing fixing snapshots are resolved once from
required historical dependencies and retained for every replay, including
historical floating-rate and FX-reset fixings.

`EvaluateRateQuoteCurvature(objective, snapshot, request)` accepts a deterministic
native scalar objective over the concatenation of free curve parameters and
complete raw decimal quotes, in `snapshot.Provenance().Axis()` order. At every
base and perturbed point, it solves the calibration again, recomputes the native
analytic residual Jacobian and effective inverse, differentiates the objective
and applies the common calibration pullback. Direct quote derivatives enter once.
The returned gradient therefore differentiates the whole scalar function; its
central gradient secants include the changing calibration map. The ordinary
first-order `FrozenCalibrationEffectiveInverse` boundary alone does not provide
that curvature.

Only EXACT square calibration systems are accepted. Every solve must return an
available analytic Jacobian/inverse whose solver-scaled product satisfies the
identity check. Cross-currency snapshots apply one residual refinement to the
fresh native effective inverse before checking it and retaining it in provenance:
$E \leftarrow E + E(I-JE/\mathrm{tolerance})$.
The strict identity bound is unchanged; unavailable or inaccurate inverses reject.
Rectangular systems can depend on a moving solver chart, and
approximate systems require their own optimality-condition derivatives; neither
is admitted by this entry. Unsupported custom instrument or fixed-curve
subclasses reject before their virtual behavior runs. Unsupported fixed curve
block subclasses also reject before their virtual behavior runs.

`RecalibrateRateWithRisk(snapshot, completeQuotes)` exposes the same passive
rebuild and preserves the complete parameter/quote axis. Curvature results own
`Value()`, `Point()`, `Gradient()`, `Directions()`, `Steps()`,
`HessianProducts()`, `BaseCalibration()` and `Execution()`. With M directions,
there are exactly 1+2M fresh calibrations and objective reverse sweeps; zero
directions request the base only. A basis direction gives Gamma and cross-Gamma
coordinates without constructing a dense Hessian.

The numeric budget uses `AAD::BumpOverAADPayloadBytes(Q, M)` for the owning
quote bump/result payload. It excludes calibration definitions, solver matrices,
objective temporaries, allocator overhead and RSS. The recording limit applies
separately to caller-thread analytic recalibration and objective recording,
including retained tape capacity and cleanup reserve. All numeric bumps are
admitted before the objective. Solver feasibility is checked point by point;
errors name base/direction/sign and the failing stage. Active outer recordings
reject, and caller adjoint mode is restored after success or failure.

These are finite-step smooth-function estimates. Choose steps using independent
price differences and a convergence sweep, rather than assuming smaller is
better. Captured objective data must remain fixed, and the callback cannot run
parallel simulations or open another independent recording. Native
`higherOrder_` remains false.

### Native rate-trade quote curvature

`EvaluateRateTradeQuoteCurvature(trades, snapshot, request, settings)` in
`dal-public/src/ratecurvature.hpp` prices a native portfolio and applies the
recalibrated quote-curvature chain above. It supports deposits, FRAs, futures,
IRS, basis swaps, OIS and XCCY swaps with all four exact square calibration
families. It returns `Currency()` and a nested `Curvature()` result with the
value, quote gradient, directional Hessian products, base calibration and
execution evidence.

The scalar objective is the weighted sum of native trade PVs. Empty
`settings.weights_` means unit weights; supplied weights must be finite and match
the trade count. Negative and zero weights are allowed. Every row is validated,
including zero-weight and expired XCCY rows, and repeated instrument IDs remain
separate rows. Expired XCCY rows retain term/market admission, including finite
spread and positive notionals, before zero PV. Configuration admission uses
the same native swap constructor as calibration instruments.
All trades must have the same actual PV currency. XCCY uses its configured
domestic currency; this entry performs no portfolio currency conversion.
Non-XCCY consumed curves and their bases must match the PV currency.

The snapshot privately owns the solved native market. The adapter copies the
trades and captures immutable cashflow geometry, curve constants and fixing
observations before differentiating. Free coordinates follow the sealed
calibration provenance, including distinct joint declaration keys. Staged XCCY
fixed roots are available for pricing but remain outside the free risk axis.
Each objective recording reconstructs the complete active curve graph and sums
the PVs before one reverse sweep; it does not construct a trade Jacobian.

`settings.fixings_` supplements the snapshot's saved calibration fixings.
Conflicting observations reject, including inconsistent reciprocal FX values.
An explicit snapshot provides all additional trade history without global
fallback. If absent, only missing required historical observations are captured
from global fixings once. Later caller or global-fixing changes cannot alter
the captured objective. Native same-time fixing rules still apply.

The generic numeric and recording admission rules apply. Invalid numeric
requests and active outer recordings reject before objective preparation or
global fixing reads. With M directions, the adapter performs exactly 1+2M
calibrations and objective reverse sweeps. Raw quote units and the owning bump
payload budget retain their existing meanings; the payload budget excludes
portfolio preparation and solver storage. These are finite-step estimates
through full recalibration, and native `higherOrder_` remains false. Python
projects this financial entry through `RateCalibration_New` and
`RateTradeQuoteCurvature`; see the
[Python interface](../python/README.md#rate-quote-gamma-and-hessian-products).
Excel bindings are not provided.

### Common-path C++ Monte Carlo quote curvature

`dal-public/src/dupirecurvature.hpp` provides a financial plan that evaluates
the complete native MC-to-Dupire quote gradient at every perturbed quote point.
It composes the existing first-order financial chain outside native scalar
callbacks, allowing parallel valuation with fresh calibration pullbacks.

```cpp
#include <dal-public/src/dupirecurvature.hpp>

DupireScriptCurvatureRequest_ request;
request.risk_.numPaths_ = 257;
request.risk_.valuation_.evaluationDate_ = Date_(2026, 9, 12);
request.bumps_.directions_ = Matrix_<>(1, 6, 0.0);
request.bumps_.directions_(0, 3) = 1.0;
request.bumps_.steps_ = {2e-4};
const auto plan = PlanDupireScriptCurvature(product, model, calibration,
                                           "equity", request);
const auto result = ValueByMonteCarloWithDupireCurvature(plan);
const double quoteGamma = result.HessianProducts()(0, 3);
```

Directions use the full raw strike-major decimal-vol spread axis. Unit rows
select Gamma and cross-Gamma columns; signed rows select HVPs. First-order quote
selection and report factors affect `Base().QuoteRisk()` projections, while
`Gradient()` and `HessianProducts()` retain all raw quote coordinates. The result
owns its base financial result, input axis, point, directions, steps and products.
`Execution()` reports the method, `1+2M` quote-gradient evaluations, paths per
evaluation and admitted numeric payload. Empty directions evaluate the base only.

`RecalibrateDupireScriptRisk(plan, spreads)` also exposes the owning first-order
rebuild. It retains the sealed IVS samples, grids, carry, other Hybrid components
and correlation, replacing the named component's local-vol surface. Explicit
direct quote bindings update their original scalar definition rows with
round-trip double values; each new valuation differentiates those constants.
The calibration VJP and direct partial are recomputed and combined once.

Curvature planning checks every perturbation before simulation and freezes the
evaluation date and historical fixing snapshot. Repeated evaluation and all
base/plus/minus gradients use the same path count, RNG, bridge, normal precision,
compilation and smoothing settings. Fully expired European contracts return
zero without historical reads or worker submission. Caller adjoint mode is
restored after success or failure; active outer recordings reject entry.

The bump numeric budget admits the generic owning-double payload plus the base
financial result's declared payload. With Q quotes, M directions, S mandatory
surface derivatives and B bound constants, this conservative accounting rule
is `sizeof(double) * (2 + S + B + 5Q + 2MQ + M)`. Snapshots, metadata, temporary
plans, worker tapes and allocator overhead are excluded. The independent
first-order quote budget remains in `request.risk_.quotes_`.

This adapter accepts European native Hybrid requests and rebuildable scalar
direct bindings. External first-order direct seeds, EXERCISE policies and bump
recording-cap requests reject explicitly. A caller-thread tape cap cannot bound
parallel worker tapes. The estimator retains the first-order smoothing semantics
and fixed-band calibration stencils; smaller steps can amplify rounding or
sampling error. It provides finite-step curvature estimates, with native
`higherOrder_` still false.

## Examples

The runnable [AAD Black example](../../dal-cpp/examples/aad) compares passive
pricing, native AAD and analytic price/gradient references. It treats forward,
volatility, numeraire, strike and expiry as independent coordinates, so the
expiry derivative holds forward and numeraire fixed.

After registering those five inputs on a `RecordingScope_`, its repeated
evaluation uses a checkpoint to retain the inputs and their accumulated seeds:

```cpp
scope.StartRecording();
const auto checkpoint = scope.MakeCheckpoint();
for (int i = 0; i < rounds; ++i) {
    scope.Restore(checkpoint);
    AAD::Number_ price = BlackTest(fwdAad, volAad, numeraireAad, strikeAad, expiryAad, isCall);
    const double pv = AAD::Value(price);
    scope.FinishRecording();
    AAD::NativeOperations_::SetSeed(price, 1.0);
    scope.ReverseSuffix(checkpoint);
    // Use pv before the next restore discards this suffix.
}
scope.ReversePrefix(checkpoint);
const double delta = AAD::NativeOperations_::ReadAdjoint(fwdAad) / rounds;
const double vega = AAD::NativeOperations_::ReadAdjoint(volAad) / rounds;
scope.Close();
```

`BlackTest` is local to the example. The executable checks its price and all
five derivatives against independent analytic formulas. Repetition timing is
illustrative and is not a paired performance acceptance result.

This loop evaluates a deterministic formula; the production pathwise estimator
uses the same prefix/suffix discipline in `dal-cpp/dal/script/simulation.hpp`.

## Summary

Reverse-mode AAD records the computational graph on a forward pass and applies
the chain rule backwards on a reverse pass. Its defining property — the complete
gradient at constant multiple of one function evaluation — makes full risk on
large portfolios feasible. Combined with checkpointing for memory and pathwise
adjoints for Monte Carlo, it is the engine for analytic-accuracy Greeks across the
library.

## See Also

- [Yield curve construction](../yield-curves/construction.md) — uses AAD-computed sensitivities
  during calibration.
- [Underdetermined search](underdetermined_search.md) — the calibration solver
  that consumes these Jacobians.
