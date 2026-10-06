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
- `capacityBytes_`: all currently allocated block arrays, including capacity
  retained after rewind. List bookkeeping and allocator overhead are excluded.

`blocks_` counts current blocks, rather than cumulative allocations. In a window
that only grows storage, the block-count increase measures new block allocations;
it does not count allocations elsewhere in a valuation. Empty tapes retain one
block per storage stream. A snapshot is neither a high-water counter nor process
RSS. Capture it at the relevant graph boundary to observe a peak, and measure RSS
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

| Contract                                | Native support                     |
|-----------------------------------------|------------------------------------|
| Scalar and repeated fixed-graph reverse | Yes, while the graph remains valid |
| Interval reverse/prefix accumulation    | Yes                                |
| Scoped lifecycle validation             | Yes                                |
| Vector adjoint channels                 | Up to `ADJ_SIZE`                   |
| Active-number lifetime diagnostics      | Available; default OFF             |
| Independent nesting/reverse events      | Not implemented                    |
| Higher-order active mode                | Not implemented                    |

`SetSeed(number, seed, channel)` replaces a seed; `AddSeed` accumulates it,
including multiple weights for the same output reference. `ReadAdjoint`
returns a passive double. The optional channel defaults to zero. In vector
mode these operations access the vector array, including channel zero;
the legacy `Adjoint` scalar field is separate.

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
