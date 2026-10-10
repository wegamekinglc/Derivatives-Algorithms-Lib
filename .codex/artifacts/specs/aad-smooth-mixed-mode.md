# Native smooth forward-over-reverse prototype

Status: active specification; implementation and acceptance open.

## Source and problem

F04 in [the implementation plan](../plans/aad-implementation.md) requires a
native mixed-mode prototype. Merged #527 completes finite-step policy curvature.
Existing `EvaluateBumpOverAAD` differences native gradients and consequently
retains an outer-step error even for smooth polynomials. The new prototype must
compute directional second derivatives through native arithmetic, with no bump.

## Scope

Add an opt-in core C++ number and request driver. Compose forward directional
arithmetic with the existing native scalar reverse tape. Preserve ordinary
`Number_`, its first-order storage, its operation costs and its capability report.
Bindings and arbitrary financial-model adapters are subsequent work.

Supported primitives are arithmetic, exp, log, sqrt, pow, erfc, NCDF and NPDF.
Do not provide nonsmooth comparisons, abs/min/max, regression differentiation,
opaque reverse-event operators, checkpoint replay or independent nested tapes.
An ordinary native `Number_` cannot implicitly become the new number, and the
new number cannot implicitly convert to double.

## Mathematical contract

For a deterministic C2 scalar function f, point x and direction v, each number
retains native active components (a, da), with da = D_v a. Reverse propagation
from f yields its gradient; reverse propagation from D_v f yields H(x)v.
The result is a directional AD composition, subject to floating-point range and
rounding, rather than a finite-step estimator. No symmetry correction, automatic
step, sampling claim or arbitrary higher-order capability is introduced.

## Requirements

1. Own copies of point, directions and budgets before invoking the callback.
   Reject missing callbacks, dimensional mismatch, nonfinite point/directions,
   payload overflow and insufficient numeric-output budget before recording.
2. Return scalar value, full gradient, point, original directions, one directional
   derivative per row and an M-by-N Hessian-product matrix. All returned numeric
   components must be finite; results retain no active native numbers.
3. Expose smooth arithmetic as a distinct number type. Never prune a tangent
   merely because its current value is zero: x squared at zero still has a
   nonzero directional second derivative. Preserve tiny and signed directions.
4. Require positive arguments for log/sqrt and positive bases for active
   exponents. Passive integer powers admit negative bases; zero/one powers use
   constant/identity semantics, and zero to a negative power is rejected.
   Noninteger passive powers require positive bases.
5. Each public arithmetic result must have finite primal and directional values.
   Diagnose invalid domains and nonfinite gradients/products explicitly. Aliased
   operands and compound assignments preserve the chain rule.
6. With M positive directions, record M graphs at the same point: the first
   yields the gradient and its product, and later graphs yield their products.
   Report M callback evaluations and M+1 reverse sweeps. With no directions,
   perform one evaluation and one gradient sweep. Retain exact work counts.
7. Callbacks must represent the same deterministic scalar function using the
   supported arithmetic. Later primal values must equal the first; this guard
   does not certify general callback purity. Direction-dependent control flow
   and extracting passive values to bypass differentiation violate the contract.
8. Numeric-output budget covers `(1 + 2N + 2MN + M) * sizeof(double)`. Metadata,
   callback captures and temporary recording storage are excluded explicitly.
9. Scope native scalar adjoints, reject nested ownership and checkpoint/reverse
   events, and restore the caller mode on every exit. Recording capacity is
   capped across sequential graph reuse; report actual peak tape capacity and
   cleanup reserve without an RSS or aggregate-memory claim.
10. Domain, callback, capacity and reverse failures must clean up recording state;
    a later valid request must succeed. DAL exceptions identify the current direction;
    allocation failures preserve their standard exception type.
11. Expose scoped prototype capabilities verified by tests. Global native
    `higherOrder_` and independent-nesting capability remain false.
12. Existing bump-over-AAD and financial curvature semantics remain unchanged.

## Executable acceptance

- Establish an analytic quartic RED: a finite outer difference has nonzero
  step error and cannot satisfy the requested exact directional identity.
- Verify full gradients, directional derivatives and products against analytic
  polynomial Hessians, including cross terms, aliases, affine/constant outputs,
  zero-valued tangents, negative/tiny directions and empty directions/inputs.
- Check every supported unary/power family against independent scalar first and
  second derivatives, including normal density/CDF and erfc identities.
- Verify a smooth European Black-Scholes kernel against independent analytic
  Gamma/cross-Gamma and a separate native gradient reference.
- Exercise admission-before-callback, exact numeric budgets, capacity failures,
  callback exceptions, nested/checkpoint rejection, scalar/vector caller modes,
  owned snapshots, deterministic reuse and concurrent independent requests.
- Compile negative capability probes, strict OFF/combined probes and an installed
  core consumer. Complete current-head CI/Codacy/reviews and actual runtime logs.
- Select complete-request timing cases from the new smooth callers only, retaining
  calibrated paired sampling and raw build identities. No full benchmark matrix.

## Open questions

None requiring user input. Numerical evidence may narrow the supported domain;
it cannot weaken the analytic acceptance assertions or silently add operators.
