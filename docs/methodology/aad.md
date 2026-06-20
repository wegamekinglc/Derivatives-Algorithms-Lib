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

| Function                | $g'(r)$                           |
|-------------------------|-----------------------------------|
| $\exp r$                | $v$                               |
| $\ln r$                 | $1/r$                             |
| $\sqrt{r}$              | $1/(2v)$                          |
| $\lvert r\rvert$        | $\operatorname{sgn} r$            |
| $\phi(r)$ (normal pdf)  | $-r\,\phi(r) = -r\,v$             |
| $\Phi(r)$ (normal cdf)  | $\phi(r)$                         |
| $\operatorname{erfc} r$ | $-\tfrac{2}{\sqrt{\pi}} e^{-r^2}$ |

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

## Memory: Checkpointing via Mark / Rewind

The tape grows with the number of operations, so long simulations would exhaust
memory if every step were kept. The algorithm uses a **checkpoint** discipline:

- A **mark** records a position on the tape.
- **Rewind** discards everything recorded after the mark, reusing that memory,
  without disturbing the adjoints already accumulated before the mark.

This lets a repeated computation (e.g. one Monte Carlo path) record, propagate,
and then rewind, so the tape size is bounded by the work of a *single* repetition
rather than the whole simulation. Propagation can therefore be partitioned into
ranges: from the end to the mark, and from the mark to the start.

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

1. Place the parameters $\theta$ on the tape once and mark.
2. For each path: rewind to the mark, simulate the path and evaluate the payoff
   (forward pass), seed the payoff adjoint to $1$, and run the reverse pass to the
   mark. Parameter adjoints **accumulate** across paths automatically.
3. After all paths: propagate from the mark to the start and divide the parameter
   adjoints by $P$.

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
respect to forward-rate nodes -- the input the underdetermined solver consumes.

### Piecewise-Linear Forward Curve — `Tape::DiscountPWLF_<T_, B_>`

```text
dal-cpp/dal/curve/ycpwlf.hpp
```

This is the primary curve type for the joint multi-curve AAD path. It
interpolates forward rates piecewise-linearly on the scalar type `T_` and
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

A sibling of Phase A's `Tape::Rate_<T_>` (which is bound to `YCCtx_<T_>` and
reads a single curve). `JointRate_<T_>` declares a pure virtual
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

$$\text{Clear}(\textit{tape}) \rightarrow
\text{RegisterIndependent}(x_k)\;\forall k \rightarrow
\text{NewRecording}(\textit{tape}) \rightarrow
\text{forward pass (build curves, price residuals)} \rightarrow
\text{per row } \{\text{ZeroAdjoints},\; \bar{r}_i = 1,\;
\text{PropagateToStart},\; \text{harvest } \bar{x}_j\}.$$

Under PWL_FWD every knot is free (no anchor exclusion), so the independent
registration covers all $2 \cdot n_{\text{knots}}$ forward-rate parameters
per declaration. The harvested adjoints form a dense
`XCurveJacobian_` (`dal-cpp/dal/curve/curvejacobian.hpp`) with exact structural
zeros where an instrument has no parametric dependence on a given knot.

## Summary

Reverse-mode AAD records the computational graph on a forward pass and applies
the chain rule backwards on a reverse pass. Its defining property — the complete
gradient at constant multiple of one function evaluation — makes full risk on
large portfolios feasible. Combined with checkpointing for memory and pathwise
adjoints for Monte Carlo, it is the engine for analytic-accuracy Greeks across the
library.

## See Also

- [Yield curve construction](yield_curve.md) — uses AAD-computed sensitivities
  during calibration.
- [Underdetermined search](underdetermined_search.md) — the calibration solver
  that consumes these Jacobians.
