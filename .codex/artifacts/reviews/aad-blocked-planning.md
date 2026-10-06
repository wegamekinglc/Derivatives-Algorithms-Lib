# Native adjoint block planning: first increment

Verdict: Comment Only. Passive planning is locally verified; the complete
recording-budget/replay/Jacobian consumer contract remains open for this PR.

## Implemented behavior

`dal-cpp/dal/math/aad/adjointblocks.hpp` and `.cpp` provide an owning constant-size
block plan. Matrix dimensions, lane limits, worker/result-slot geometry and all
byte products are validated before any numeric allocation. Result payload is
`8*m*(n+1)` for eight-byte doubles. Known scratch lower bounds reserve active
root objects for concurrent workers and mean/gradient payloads for every batch
result slot, including finished slots retained before final aggregation.

A supplied lower-bound budget narrows the width or rejects width one. Block
descriptors are generated on demand, with fixed width and an explicit live-row
count for the padded tail; no vector of all block descriptors is allocated.
The planner touches no tape, recording state, model, history or worker pool.

The scratch figure is a payload lower bound for those named objects. It does
not yet admit actual vector capacity, tape blocks, model/path/evaluator buffers
or allocation overlap. The later budget guard must cover those capacities
before this PR can claim an enforced prepared-script workspace budget.

## RED/GREEN evidence

Evidence root is the established `dal-aad-evidence-20261004-8886c083/evidence`.

- `aad-blocked-plan-red-02.{json,log}`: the tracked tests fail to compile against
  the missing planner interface, with the exact compiler command and exit one.
- `aad-blocked-plan-green-{off,combined}-02.log`: four cases pass unchanged per
  mode. They cover fixed-width tail blocks, exact result/minimum-scratch budgets,
  width narrowing and width-one rejection, matrix/channel/worker boundaries,
  maximum lane width, explicit zero limits, product/sum/final-width byte overflow
  and successful differentiation of a pre-existing live graph
  after successful/rejected passive planning.
- `aad-blocked-plan-build-01.json`: freshly compiled planner/test units, cached
  accepted unchanged native/Google Test support. Combined enables lifetime,
  profiling and focused ASan/UBSan; it is not a fully instrumented-library claim.
- `aad-blocked-plan-warnings-01.json`: GCC 14 OFF/combined syntax checks pass
  under the unchanged strict flags. Production/tests meet complexity eight.

## Remaining acceptance

Complete script axis preflight, allocation-site inventory and aggregate budget
admission; native root/seed oracles; frozen common-path replay and owning
Jacobians; C++/Python/Excel consumers; stable-head platform/performance/review
acceptance. Existing scalar/weighted hot paths are not edited in this increment.
The preceding #483 is already merged and accepted; its gates are not open work.
