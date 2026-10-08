# Fixed-grid European PDE caller API

Status: active financial acceptance after merged #505; deliver in a separate PR.

## Audience and boundary

Provide a self-contained C++ example and a support header without test-framework
assertions, shared with financial acceptance tests. This composes the current
public sampled-step API, not a new dal-public pricing facade or Python/Excel
product. Existing production APIs and library headers stay unchanged.

## Caller-owned recording

Use a small configuration value for grid/time counts. The financial fixture's
physical domain, evaluation spot, dividend and expiry are named passive values,
and r/sigma/K are explicit registered `Number_` arguments. The recording helper
takes `RecordingScope_*` and the configuration/registered parameter collection;
it runs only between `StartRecording` and `FinishRecording`. It returns two active
price handles, one event handle per step and detached forward diagnostic values.

The caller owns tape clearing, scalar/vector mode, scope inputs, reverse seeds,
report extraction and `Close`. No hidden `Clear`, `SetNumResultsForAAD` or reverse call
inside the recording helper. Every terminal/boundary cell, including zero,
uses a valid current recording slot. One recorded zero expression can serve all
zero cells, exercising legal aliases with zero upstream parameter derivative.

Read active prices and adjoints while the scope is valid; return detached doubles
for output/reporting after `Close`. Retain event handles only as lookup identities
for owning reports. Do not use active handles after the scope is closed.

## Example flow

1. Initialize the repository registry and clear the current thread tape.
2. Select two bounded native channels; register r=.05, sigma=.20, K=110.
3. Start recording; record the two-layer fixed-grid damped theta chain.
4. Finish recording, clear adjoints and seed call/put in separate lanes.
5. `ReverseWithSolveAccuracy`; extract price/rho/vega/strike and actual reports.
6. Close the scope, clear the tape, then leave the channel mode and print results.

Use the standard example table/float-format conventions. Add a small CTest smoke
registration through the existing example target list. Installed-consumer
verification compiles this caller against the installed accepted DAL public
headers/library, with only the local support header on its private include path.

## Limits and error behavior

Configuration validates a positive fixed schedule and physical evaluation node.
Parameter branch selection is rebuilt at each point; the declared acceptance
fixtures keep strike away from every grid node. Existing public APIs own sample,
policy, current-slot, owner-thread, range and recording-failure validation.
The example propagates a clear failure and exits nonzero, with scope cleanup
before a mode exits. It introduces no new mixed-mode, adaptive mesh, exercise
projection, condition estimator, input-coordinate or caller capacity semantics.

No open API questions for this bounded example/test increment.
