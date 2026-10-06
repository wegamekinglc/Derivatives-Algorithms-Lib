# Budgeted script Jacobian API decisions

Status: active design based on merged #483 (`d1600a15`); no implementation or
consumer acceptance is inferred. See the [contract](../specs/aad-blocked-script-risk.md).

## Audience and existing surfaces

C++ users request owning numerical results, Python users require stable
two-dimensional arrays, and Excel users need immutable requests and spill tables.
Existing `ValueByMonteCarloWithRisk` returns one payoff gradient;
`ValueByMonteCarloWithWeightedRisk` returns one fixed-weight gradient and
component means. Neither operation implies that a full Jacobian was computed.

## Additive surfaces

Use `JacobianRiskRequest_` with `RiskRequest_ selection_`, positive
`maxBlockWidth_` (conservative initial default one), and optional recording
payload/numeric scratch limits. Keep the existing numeric result budget in
`selection_`; avoid a fourth overlapping definition of retained result bytes.
Define recording/scratch coverage and aggregate-worker semantics before making
these limit fields public. Width zero is invalid, not an automatic-mode sentinel.

`ValueByMonteCarloWithJacobianRisk(product, model, paths, request, valuation,
simulation)` follows the accepted six-argument request style. C++ optional
settings remain trailing. Python uses keyword-only choices and the same request
semantics. Excel exposes `JacobianRiskRequest_New` and
`MonteCarlo_ValueWithJacobianRisk` with the existing strict raw-input guards.

The owning result exposes output means, raw/reported `(m,n)` matrices, selected
and complete row/column axes, sealed preparation provenance and actual execution
width/replay/budget evidence. Reuse compatible passive metadata and table
formatters. Decide whether the scalar `RiskResult_` can gain an internal generic
constructor or needs an owning wrapper; preserve its public layout/getters and
avoid making untyped scalar function pointers ambiguous.

## Typical request

```cpp
JacobianRiskRequest_ request;
request.selection_.outputs_ = Vector_<String_>{"output:2", "payoff"};
request.selection_.inputs_ = Vector_<String_>{"model:0", "model:1"};
request.maxBlockWidth_ = 2;
const auto result = ValueByMonteCarloWithJacobianRisk(
    product, model, 1024, request, valuation, simulation);
// Rows follow the two requested output IDs; columns follow spot and volatility.
```

This is a proposed example, not a currently available API.

## Errors and compatibility

Omitted outputs select payoff, matching scalar/weighted conventions; explicit
empty outputs fail. Empty inputs remain valid. Repeated IDs fail while distinct
IDs aliasing one node remain independent rows. Errors name the field, stable
row/input identity and violated limit. Runtime capacity failures identify the
budget category and requested/required bytes; no incomplete result is returned.

Reported scales multiply columns only. They do not change native seeds or
simulation. A single row is not flattened in Python/Excel. Existing handles,
getter ownership, date/history rules and native-versus-passive estimators remain
compatible. New requests cannot silently select LSM, quotes or portfolio modes.

## Rejected shortcuts

Changing `numAdj_` after recording leaves pointers with incompatible storage.
Looping existing scalar entry points re-queries history and hides replay costs;
freeze preparation once instead. Returning one weighted gradient under a
Jacobian name loses attribution. Checking memory after allocation does not
enforce a limit. Per-worker maxima cannot stand for concurrent request capacity.
An unmeasured wide default prematurely implements P03's tuning policy.

## Open decisions

The first implemented native helper is `AAD::PlanAdjointBlocks(outputs, inputs,
settings)` in `dal-cpp/dal/math/aad/adjointblocks.hpp`. It returns fixed-width
block descriptors lazily and validates result payload plus a named scratch
lower bound. Settings distinguish concurrent root owners from retained batch
result slots. This is an internal planning building block; full script selection,
actual capacity admission and the proposed public Jacobian entry remain open.

The implemented `TapeCapacityBudget_` owns aggregate synchronized tape-payload
reservations. `TapeCapacityScope_` admits the owning thread's cached default tape
and surrounds its recording scope. Allocation tickets reserve before block
allocation; detached workers remain charged. Replacement during clear includes
old/new overlap. These helpers enforce tape payload only, excluding allocator
metadata, and expose admitted current/peak bytes. Cold worker initialization is
attached before the default tape is created, so initial blocks are admitted
before allocation and partial-construction failures refund their reservations.
The two-argument scope with `reserveCleanup=true` protects the largest single
replacement payload for each attached worker. Growth cannot consume this
headroom; replacement can use it, including concurrent worker cleanup. The
producer's known minimum includes initial payload plus cleanup headroom for
every worker. Unused headroom is excluded from actual payload high-water bytes.

`SeedAdjointBlock` requires preallocated root vector capacity, fixed vector mode
(including width one), bounded descriptors and an active zero. It uses existing
path-local root materialization and explicitly clears padded seed lanes.
See the [capacity/root review](../reviews/aad-blocked-capacity-roots.md).

`BufferCapacityBudget_` and `BufferCapacityScope_` admit actual tracked `Vector_`
capacity and model/component heap objects before allocation, with synchronized
worker/coordinator accounting and transient growth overlap. Fixed evaluator
payload is reserved before construction. Text and exception bookkeeping are
excluded. Fresh request buffers must be destroyed while their budget is attached;
cross-request cached-buffer admission remains deferred. The installed consumer
checks allocations originating inside the shared library under a caller scope.
The allocator changes underlying standard-library iterator types, so consumers
must rebuild and use DAL iterator aliases. See the
[scratch review](../reviews/aad-blocked-scratch-capacity.md).

`Detail::EvaluateAADBlockBatch` now reuses the shared native batch loop with
vector seeds and ordered raw channel harvesting. Its caller owns padded numeric
batch slots. The worker's fixed admission includes current and historical
evaluators. `BufferCapacityScope_::ForWorker` can share an attached coordinator
budget, requires stack-ordered close and preserves normal nested-scope rejection.
See the [batch review](../reviews/aad-blocked-batch-execution.md).

The batch attaches its tape budget before selecting vector mode and enables
cleanup headroom. `Detail::EvaluateAADBlockReplay` now owns complete path replay,
ordered batch reduction, task draining, raw numeric matrices and actual width/
replay/path/capacity evidence. It captures selections/settings before submission
and returns no partial matrix on failure. The first driver does not retry after
runtime exhaustion. See the [request replay review](../reviews/aad-blocked-request-replay.md).

Public ordered-ID planning, full model/product/history snapshots, complete known
model-specific minimum preflight, axes/report factors/provenance and C++/Python/
Excel consumers remain unimplemented. Public delivery acceptance remains open.
