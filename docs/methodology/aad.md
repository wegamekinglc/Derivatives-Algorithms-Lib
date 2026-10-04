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
`ReversePrefix(checkpoint)` propagates them to the registered inputs. The XAD
prefix adapter rewinds the suffix before reverse, so extract suffix values as
passive doubles before that call. Tokens from a previous recording, another
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
The option requires native AAD; configuration rejects XAD, CoDiPack or Adept.

Each binding captures a tape-lifetime identity, recording epoch, slot generation
and scalar/vector layout. Full clear or rewind invalidates the old recording;
suffix restoration invalidates discarded suffixes while preserving the prefix
and its accumulated adjoints. Reusing an address does not validate an old
binding. Checks precede old node access and expression-result allocation;
rejected stale expression assignment preserves the destination and valid graph.
Mode mismatches and exhausted counters are reported before incompatible access
or destructive reset. Partial allocation failures require a successful reset.

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
registered zero to the payoff; alternative backends always use this addition.
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

The recording contract that produces a correct Jacobian on all four backends is
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
the backend-neutral per-row zero/seed/propagate/harvest loop. The harvested adjoints form a dense
`XCurveJacobian_` (`dal-cpp/dal/curve/curvejacobian.hpp`) with exact structural
zeros where an instrument has no parametric dependence on a given knot.

## Backends

The library compiles with one of four AAD backends selected at build time:

- **native** — the in-tree reference tape (`dal-cpp/dal/math/aad/tape.hpp`,
  `dal-cpp/dal/math/aad/node.hpp`), always available.
- **Adept** (`DAL_USE_ADEPT_AAD`) — `adept::Stack`-based tape.
- **XAD** (`DAL_USE_XAD_AAD`) — `xad::adj<double>` tape.
- **CoDiPack** (`DAL_USE_CODIPACK_AAD`) — `codi::RealReverseUnchecked` tape.

CoDiPack gives each operating system thread its own underlying tape and DAL
wrapper through native thread-local storage. Recording and reverse propagation
therefore run independently of the Python GIL, and the tape is destroyed when
its owning thread exits. Active values and tape positions are thread-affine and
must not be transferred between threads.

All four expose the same `Number_` / `Tape_` surface through facade functions in
`dal-cpp/dal/math/aad/aad.hpp`, so caller code is backend-neutral. The
differences that matter at the call site are the recording contract and the
gradient-zeroing semantics between single-result reverse sweeps.

### Load-Bearing Recording Contract

A correct Jacobian on all four backends requires this exact ordering:

$$
\text{Rewind}(\textit{tape}) \;\rightarrow\;
\text{RegisterIndependent}(x_k)\;\forall k \;\rightarrow\;
\text{NewRecording}(\textit{tape}) \;\rightarrow\;
\text{forward pass} \;\rightarrow\;
\text{per output row } \bigl\{\,\bar{y}_i = 1,\;\text{PropagateToStart},\;\text{harvest},\;\text{zero each leaf}\,\bigr\}.
$$

Each step has a backend-specific reason to be in this position:

- **Rewind** resets the tape's write cursor to the start so the next recording
  reuses the already-allocated node blocks, avoiding the free/re-allocate cycle
  of `Clear` on every iteration. The reused storage is overwritten in place by
  the next forward pass, so no stale data leaks into the new sweep.
- **RegisterIndependent** stamps each input as a tape leaf that subsequent
  operations differentiate. It must run *before* `NewRecording` opens the
  recording window on XAD (see below); running it after silently drops the input
  and yields an all-zero Jacobian column.
- **NewRecording** marks the start of the live recording so the reverse sweep
  terminates at the right point.
- **Zeroing between rows** is **not** uniform across backends. On native the
  inline-zeroing `PropagateOne` clears each consumed intermediate adjoint, so
  only the parameter leaves must be zeroed by the caller after harvest; on the
  other backends a full `ZeroAdjoints` pass before each row is still required.
  Skipping the between-row zero is the single most common source of corrupted
  multi-row Jacobians.

### Per-Backend Zeroing Semantics

- **Native propagation precision.** Every nonzero adjoint propagates, including
  very small values. Only exact zero seeds are skipped: a small intermediate
  adjoint can be multiplied by a large local derivative later in the sweep.
  Multi-result channels follow this rule independently, so a zero channel does
  not evaluate `0 * Inf` when another channel is active. NaN and nonzero infinite
  seeds remain observable rather than being discarded by a threshold comparison.

- **Native.** `PropagateOne` (`dal-cpp/dal/math/aad/node.hpp`) zeroes each
  consumed node's adjoint inline after propagating it to its parents, so the
  intermediate graph starts clean for the next reverse sweep without a separate
  pass. Leaf parameter nodes (`n_ == 0`) are *not* consumed by `PropagateOne`
  and would accumulate across rows; the shared harvester
  (`dal-cpp/dal/curve/aadjacobian.cpp`) zeroes each harvested leaf adjoint in
  place immediately after reading it, which
  is O(nParams) per row instead of the O(all nodes) `ZeroAdjoints` sweep. The
  `ZeroAdjoints` facade is still defined for callers that need a full sweep
  outside this pattern.

- **Adept.** Adept's `compute_adjoint` zeroes only the LHS adjoint of each
  consumed statement and then accumulates into the operands; operands whose
  gradients are never cleared keep residual values across sweeps. In a
  single-result reverse-sweep loop, row 2's seed would land on row 1's operand
  residue and corrupt the Jacobian. The `ZeroGradientArray` helper
  (`tape.hpp`) clears the live gradient array while keeping
  `gradients_initialized_` true, which satisfies the `compute_adjoint` `THROW`
  guard ("Adept gradients are not initialized"). `Dal::AAD::ZeroAdjoints`
  routes to `ZeroGradientArray` on this backend, so callers that use the facade
  are safe; callers that bypass it must replicate the semantics.

  Gradient capacity can also grow after seeding: later recording windows may
  register more simultaneously live variables than the initialized array holds.
  `Tape_::EnsureGradientCapacity` in `dal-cpp/dal/math/aad/tape.hpp` ensures
  sufficient storage before DAL adjoint reads, writes, and reverse sweeps.
  Growth preserves accumulated adjoints and zeroes only the added storage;
  reinitializing the whole array would erase contributions from earlier paths.
  The explicit `ZeroAdjoints` call between independent output rows remains
  necessary.

- **XAD.** `registerInput` must run *before* `NewRecording` opens the recording
  window: registering an input after `NewRecording` silently drops it and
  yields an all-zero Jacobian column. The `RegisterIndependent` facade asserts
  the tape is active (`clearAll` does not deactivate a tape constructed with
  `activate=true`), so a passive tape fails loudly at registration time rather
  than producing a silent zero column. `ZeroAdjoints` maps to
  `xad::Tape::clearDerivatives`.

- **CoDiPack.** `RegisterIndependent` calls `tape.registerInput` on the active
  tape; `ZeroAdjoints` calls the no-argument `clearAdjoints`, which zeroes up
  to the largest created index and leaves the statement graph intact. Both are
  safe between sweeps.

### Public Result Validation

Public Monte Carlo valuation requires both the reported mean and requested
sensitivities to be finite. An invalid sensitivity raises `InvalidRisk` with the
output and input names; a non-finite aggregate mean raises `InvalidPayoff`.
This validation does not make an undefined local derivative mathematically valid.
Endpoint conventions still belong to the selected backend. For example, the
pinned CoDiPack backend assigns a zero local derivative to `sqrt(0)`, whereas
the native backend produces an infinite derivative. A finite reported value is
therefore insufficient evidence of differentiability at an endpoint.

### Passive vs Active Tape

XAD and CoDiPack distinguish an *active* tape (records statements) from a
*passive* tape (does not). The native backend has no notion of a passive tape —
recording is unconditional — and Adept's activity is governed by its
`Stack` base. Code that needs a value-only pass (e.g. a baseline pricing run
without differentiation) should use a plain `double` evaluation rather than
relying on tape passivity, which is backend-dependent.

## Examples

The recording contract of the previous section is exercised end to end by the
AAD benchmark program, which prices a Black payoff and reads back every Greek
from a single reverse sweep. See
[`dal-cpp/examples/aad/`](../../dal-cpp/examples/aad) for a runnable version;
its core backend-neutral sweep is:

```cpp
// from dal-cpp/examples/aad/aad.cpp
#include <dal/platform/platform.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/vectors.hpp>

using namespace Dal;
using Dal::AAD::Number_;

Dal::RegisterAll_::Init();
AAD::Clear(*AAD::Tape());

Number_ fwdAad(fwd), volAad(vol), numeraireAad(numeraire), strikeAad(strike), expiryAad(expiry);
PutOnTape(fwdAad);
PutOnTape(volAad);
PutOnTape(numeraireAad);
PutOnTape(strikeAad);
PutOnTape(expiryAad);
AAD::NewRecording(*AAD::Tape());

Number_ priceAad = BlackTest(fwdAad, volAad, numeraireAad, strikeAad, expiryAad, isCall);
Adjoint(priceAad) = 1.0;
AAD::PropagateToStart(*AAD::Tape());

const double pv    = Value(priceAad);   // price
const double delta = Adjoint(fwdAad);   // dP/dFwd
const double vega  = Adjoint(volAad);   // dP/dVol
// numeraire, strike, and expiry adjoints are read the same way
```

The same program benchmarks repeated evaluation of this deterministic Black
formula. Its timing loop reuses the tape and divides accumulated adjoints by
the repetition count:

```cpp
// from dal-cpp/examples/aad/aad.cpp
Number_ priceAad{0.0};
for (int i = 0; i < nRounds; ++i) {
    AAD::Rewind(*AAD::Tape());
    priceAad = BlackTest(fwdAad, volAad, numeraireAad, strikeAad, expiryAad, isCall);
    Adjoint(priceAad) = 1.0;
    AAD::PropagateToStart(*AAD::Tape());
}
const double delta = Adjoint(fwdAad) / nRounds;   // benchmark repetition average
```

This loop does not simulate Monte Carlo paths. The production pathwise estimator
uses the mark/rewind discipline described above in `dal-cpp/dal/script/simulation.hpp`.

The example also benchmarks the same payoff with the XAD, CoDiPack, and Adept
backends side by side; only the recording and zeroing calls differ, as described
under *Backends* above.

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
