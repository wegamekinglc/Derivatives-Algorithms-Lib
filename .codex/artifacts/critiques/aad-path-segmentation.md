# Native path segmentation critique

Verdict: Proceed with caveats.

## Blocking issues

None for the standalone fixed-state scalar core under its declared kernel
contract. This is not acceptance of a financial checkpoint driver.

## Significant concerns

- Saving only model factors loses script variables, cashflow state, live old
  fixings and vector history. Financial integration must supply complete state
  and measure its actual storage before P05 can close.
- Replaying cached local derivatives at a new point is invalid. Each segment,
  terminal objective and initialization must receive fresh registered inputs.
- Numeric endpoint agreement alone does not prove identical branches. Compare
  full declared branch traces, including a regression with equal endpoint values.
- Parameter contributions can arise in every segment and terminal/initial code.
  The independent oracle must detect overwriting, omission and double counting.
- Small paths can become slower. Keep full-graph execution as the ordinary
  default and measure total requests, including passive prepass and cleanup.
- A kernel can own opaque data. Component capacities cannot be labeled whole
  process memory. The financial adapter must complete model/evaluator accounting.

## Design decisions

Use one complete passive driver vector, fixed-size state and exact trace slots
for this core. This bounds and admits retained checkpoint allocation explicitly.
Do not alter the existing tape cursor checkpoint or add nested ownership.
Financial liveness and variable-size history are subsequent adapter decisions;
do not mark their requirements complete from this core's numerical tests.

## Open questions

None blocking core implementation. Which financial state representation meets
the memory target remains a measured decision in the next increment.
