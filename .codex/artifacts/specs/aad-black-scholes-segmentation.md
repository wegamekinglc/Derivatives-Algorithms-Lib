# P05 Black–Scholes compiled-path segmentation design

Status: active financial implementation after the accepted merge of #512.

## Function and axes

For one frozen Gaussian vector, evaluate the prepared compiled script with its
prepared native smoothing and return the final payment receiver and its gradient.
Input columns are spot, volatility, rate, dividend yield, then the prepared script
constant variables in their existing order. A new numeric parameter point rebuilds
every local derivative. The prepared timeline, fixings, bytecode, smoothing, driver
vector and output identity remain fixed during a request.

The first financial kernel is exact one-factor Black–Scholes with no EXERCISE.
It is explicitly selected. Ordinary full-graph execution stays the default;
unsupported observation/sample definitions, exercise products and settings fail
before recording. Only the dedicated Black–Scholes factory can construct the
move-only preparation wrapper accepted by the kernel. A generic prepared script
cannot establish model provenance from its sample-definition shape, even when
it references just one asset of a multi-asset model. The wrapper exposes a const
ordinary preparation view and retains no active model or parameter values.
The integration must use owning detached results and the existing numeric risk
chain for any subsequent calibration pullback.

The explicit segmented preparation entry additionally resolves and compiles
historical-only products. Ordinary preparation's expired-product shortcut stays
outside this opt-in path. Zero future samples must still rebuild historical
constant-variable dependence from the supplied fresh numerical inputs.

## First implementation slice: resumable model samples

Introduce an opt-in immutable Black–Scholes step plan owning passive timeline and
sample-definition data. It accepts fresh typed parameter values and a log-spot
boundary, and emits one complete typed sample and the next log spot. It stores no
active model prefix or full active scenario. A time-zero sample emits the original
spot without an exp/log round trip and consumes no Gaussian element.

Share drift, standard-deviation, numeraire and discount-factor formulas with the
ordinary Black–Scholes initializer. Preserve operation order and domain checks.
Do not change the model inheritance hierarchy or add optional state to every model.
Validate timeline, outputs and discount maturities when constructing the passive
plan; validate parameter domains and finite emitted values for each fresh request.
An out-of-range sample or malformed driver vector identifies the offending input.

The first failing test compares emitted spot, numeraire, observation and delayed
discount values against independent closed formulas at irregular sample times.
Further tests compare all four derivatives with analytic derivatives and the
existing full graph, restore an interior log-spot boundary, preserve time-zero
exact spot, reject invalid inputs, recover after failure and use fresh points.

## Financial boundary layout

Flatten log spot, every persistent evaluator scalar, vector logical lengths and
the conservative maximum number of vector entries, and live model observations.
Padding is materialized zero and must not add parameter dependence. Lengths are
passive discrete state with validated exact integer values; actual vector contents
retain their derivatives. All boundary coordinates are checked by the core.

Compute observation live intervals from model sample production to the last future
event use. Assign reusable slots with inclusive interval overlap: an observation
used by an event at the sample where a new one arrives must survive until that event
finishes. Known historical observations are read from the sealed plan. Preserve
several events mapped to one sample in their original order.

Keep cumulative payment variables in the boundary because later expressions may
read or modify them. The first adapter uses zero direct segment contribution and
the final receiver as the terminal objective. Historical statements are rebuilt
once from fresh constant-variable inputs in InitialState, so the initialization
pullback propagates their parameter dependence exactly once.

Prove vector bounds from the acyclic prepared node tree that produces the sealed
compiled program, including
historical APPEND, indexed writes and both fuzzy branches. Existing capacity hints
are allocation hints, never admission proofs. Reject unsupported or unproved
streams early. Growing vector products remain supported only within proved bounds
and may have no memory benefit; report that boundary without an automatic switch.

## Compiled evaluator policy

Add a separate compile-time policy for compact observation reads and exact replay
trace collection. Default compiled dispatch retains its existing ordinary policy.
Keep the bytecode and financial operations shared; do not clone the interpreter or
route ordinary instructions through std::function. Event-local stacks and fuzzy
scratch reset at the existing boundaries and are not persistent path state.

Trace hard IF outcomes, fuzzy comparison intervals and IF regimes, and MAX/MIN selection including ties and
vector reductions. Use statically bounded trace slots with sentinels for skipped
instructions; never a probabilistic hash. Recursive branch execution retains the
same event trace identity and compact read policy. Passive and active execution
use identical bytecode and smoothing. Core replay rejects a mismatch before seeds.

## Gaussian and lifecycle contract

Generate or accept the full passive Gaussian vector once per fixed path and retain
the post-transform vector, including Brownian bridge output. Replay reads exact
sample-associated offsets; it never redraws. Admit supported RNG transforms
explicitly and test their common path numbering. Request owns driver/input copies;
the immutable prepared product and model plan outlive all segment operations.

Reject nested use before touching the caller's graph. Restore prior scalar/vector
mode on success and failure. Check checkpoint and tape capacity budgets before
unbounded allocations, retain cleanup reserve reporting, and return results that
survive worker tape rewind. Concurrent requests share immutable plans only.

## Acceptance and bounded performance scope

Tests cover older fixing reuse across boundaries, delayed payments later read,
historical scalar/vector state depending on script constants, several events at
one sample, smoothing boundaries/ties, fresh points, h=1, uneven and h>L splits,
analytic/full-native/common-driver directional oracles, malformed requests,
budgets, mismatch recovery, mode preservation and concurrency.

After correctness, select one short request, one long running-observation request
and one long live-fixing/payment request. Compare full and segmented execution in
cold and retained-tape regimes. Include passive prepass, copies, replay, reverse,
cleanup, final result/reduction and the complete resource breakdown. Count shared
immutable plan/program storage consistently and report per-worker peak capacity
separately from process RSS. Select a segment length before collecting acceptance
samples. A useful long case must demonstrate a measured capacity reduction;
latency tradeoffs remain visible and segmentation remains explicit.

Rebuild and compare only affected ordinary Black–Scholes and compiled-script paths.
Reuse accepted evidence for unrelated curve/PDE/RNG arithmetic/LSM code. Preserve
the repository's two rounds, ten observations and calibrated noise rule for each
selected cost row; expand only for a concrete failure or uncovered changed caller.
CI and complete current-head external reviews close the financial PR before merge.
