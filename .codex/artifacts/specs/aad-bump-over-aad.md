# Native bump-over-AAD directional curvature

## Source and problem

Stage D/F04 of the [implementation plan](../plans/aad-implementation.md) requires
Gamma, cross-Gamma and Hessian-vector requests. Native `Number_` records first
derivatives; its `higherOrder_` capability is false. There is no owning native
driver for central differences of fresh AAD gradients. This first F04 delivery
adds that driver without claiming exact higher-order AD.

## Scope

The input is one deterministic scalar native callback, a numeric point, direction
rows and a positive explicit step per row. Each callback invocation must rebuild
its complete active graph from the supplied inputs and ordinary constants. It
must hold all external state fixed and must not retain active inputs or roots.
The first delivery covers smooth kernels and composition with native recorded
operators. MC path capture, financial request axes/report scales, full quote
recalibration, LSM policy choices, bindings and mixed mode have later deliveries.
No stochastic or nonsmooth estimator guarantee follows from the generic API.
The callback receives the driver's scope pointer for existing recorded operators;
it must leave lifecycle/checkpoint/mode ownership with the driver. Check scope
state and reject any checkpoint creation after callback return before using the
returned active root. Each fresh scope starts with zero checkpoint generation;
every valid restore requires a checkpoint created in that same scope.

## Requirements

1. For a point of length N and M directions, return the base value, base native
   gradient and an M-by-N matrix. Row k estimates H(x) v[k] as
   `(g(x + h[k] v[k]) - g(x - h[k] v[k])) / (2 h[k])`. Direction rows keep caller
   order, signs and magnitude. They are not normalized or deduplicated.
2. Unit direction k provides Gamma in component k and cross-Gamma in component i.
   Arbitrary directions provide HVPs without an N-by-N allocation. Results retain
   owning copies of the point, directions and steps, plus method/execution data.
   No symmetry averaging or accuracy certification is performed.
3. Snapshot the callback, point and entire request before the first evaluation.
   Validate callback availability, N within matrix integer range, direction shape,
   finite inputs/directions, exactly M finite positive steps, and nonzero direction
   rows. Empty directions allow a base value/gradient request; an empty point is
   valid only with zero directions and still evaluates a scalar constant.
4. Validate all plus/minus points before invoking the callback. Use the same bump
   arithmetic during validation and execution. Each nonzero direction component
   must change the point on both sides and remain finite. Reject swallowed,
   underflowed or overflowing bumps instead of silently dropping coordinates.
5. Evaluate the base once, then each plus/minus pair in row order: exactly 1+2M
   independent native recordings and reverse sweeps. Clear seeds, support direct
   input aliases and constants, and reject nonfinite values, gradients or products.
   Avoid overflow solely from the central-difference numerator or `2h`; reject a
   nonzero central quotient that cannot be represented as a nonzero finite double.
6. Reject nested independent recording before disturbing its graph. Use scalar
   adjoints internally and restore the caller's prior adjoint mode on every exit.
   Callback, recording, allocation and numeric failures publish no partial result;
   later requests recover. Independent concurrent callers have no shared workspace.
7. A numeric payload budget admits exactly
   `sizeof(double) * (1 + 2N + 2MN + M)` before evaluation, with checked arithmetic.
   It covers the result's value, point, gradient, directions, products and steps,
   excluding owner headers/metadata and temporary storage. A separate optional
   recording-capacity budget covers retained tape capacity plus cleanup reserve
   throughout the sequential request. Report peak tape bytes and cleanup reserve.
8. Method identity is `BumpOverNativeAAD`; native tape higher-order capability
   remains false. On smooth kernels the central estimator has O(h²) truncation
   error in exact arithmetic; floating-point and callback errors are amplified
   as h decreases. There is no automatic step, Richardson correction or h/2 run.
9. Ordinary first-order pricing, tape layout and existing entries stay unchanged.
   Test/performance selection must stay within the new driver and its direct
   recording/operator boundary; no unrelated full local matrix is required.

## Acceptance

- A missing-interface RED precedes the first quadratic GREEN. Independent
  polynomial value/gradient/Hessian oracles verify Gamma, both cross orientations,
  non-unit signed HVPs, unused coordinates and owning results.
- A quartic verifies h/h2/h4 second-order convergence. Additional smooth functions
  and a recorded solve verify graph rebuilding at each perturbed point.
- Admission tests assert zero callback invocations for every invalid request;
  include empty requests, NaN/Inf, shape, step, range and exact budget boundaries.
- Exercise aliases/constants, scalar/vector caller modes, nested rejection,
  plus/minus callback failures and recovery, caller mutation after admission,
  independent concurrent callers and floating-point extreme quotients.
- A focused CMake target, strict C++17 OFF/combined probes and installed-only
  consumer pass. CI must actually execute the new suite in diagnostic/MSVC profiles.
- An isolated allocation-probe executable injects failure at each measured
  request allocation, checks caller-mode restoration and verifies a subsequent
  request. Keep global allocation replacement outside the ordinary test binary;
  execute the probe alongside the thirteen core cases in all fourteen profiles.
- Reject callback checkpoint creation and checkpoint/restore at base, plus and
  minus phases before inspecting its output; verify mode restoration and recovery.
- Measure only two selected request shapes (4 inputs/1 direction and 32 inputs/
  3 directions), reporting full-request cost, work counts, payload and limits.
  Existing caller bodies remain unchanged; retain source/binary provenance.

## Open questions

None for this increment. Financial smoothing/common paths, quote calibration
curvature and Frozen/RetrainedBump nested steps require their own active controls.
