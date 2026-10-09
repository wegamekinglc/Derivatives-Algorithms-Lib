# Native path segmentation API

Status: active; controlled by the [specification](../specs/aad-path-segmentation.md).

## Surface

The opt-in C++ header is `dal-cpp/dal/math/aad/segmentedpath.hpp`.
`SegmentedPathDimensions_` contains `steps_`, `stateSize_` and `traceSlots_`.
`SegmentedPathTransition_<T_>` contains `state_`, `contribution_` and
`branchTrace_` (unsigned 64-bit decisions, not numerical derivatives).

`SegmentedPathKernel_` supplies `Dimensions()` and passive/native overloads of
`InitialState(parameters)`, `Advance(step, state, parameters, drivers)` and
`Terminal(state, parameters)`. Both overloads obey one mathematical contract;
concrete kernels should share their formulas through a typed private helper.

`ExecuteSegmentedPath(kernel, parameters, drivers, settings = {})` returns an
owning `SegmentedPathResult_` with `Value()`, `Gradient()` and `Execution()`.
`SegmentedPathSettings_` takes a positive `segmentSteps_` (default 64), an
optional checkpoint-capacity budget and an optional tape-capacity budget.
Replay comparison uses a fixed small relative/absolute tolerance and exact
trace equality; callers cannot loosen it to hide a different execution.

## Semantics

Initialization may depend on parameters; the terminal objective may also depend
directly on parameters. A transition contribution is added once to the scalar
objective. State columns and parameter columns follow caller order. Retained
driver values are passive and do not receive gradient columns.

The checkpoint budget includes the actual allocated boundary/trace/contribution
payload. It does not include arbitrary immutable data inside a user kernel.
The tape budget preserves existing actual-capacity and cleanup semantics.
Execution reports `peakTapeBytes_` and `cleanupReserveBytes_` separately: admission
reserves cleanup capacity in addition to ordinary retained allocations. A peak
payload alone is not an admissible budget. These are not process memory metrics.
Active kernels must return materialized native numbers, including constants;
construct constants from a double rather than returning an unbound default number.

Errors identify segmented-path admission or replay, the segment/step where
applicable, and the failed shape, numeric, trace or budget constraint. Failure
returns no partial result and preserves the next independent request.

## Compatibility and later surfaces

The ordinary Monte Carlo API retains full-path execution. C++ kernel callbacks
are not exposed directly through Python or Excel. Financial settings, complete
axes/provenance and bindings are designed after the financial adapter exists.
No new active type, native-forward capability or nested recording is implied.
