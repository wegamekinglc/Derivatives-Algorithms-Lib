# P05 financial design critique

Verdict: Proceed with caveats.

## Blocking issues

None in the written design. The following are implementation acceptance gates,
not permissions to narrow the financial state silently.

## Significant concerns

1. Observation liveness is inclusive through the last consuming event. Reusing a
   slot at a sample before that sample's events finish can overwrite an older
   fixing still needed by the payoff. Test this exact overlap before integration.
2. Passive replay must execute the prepared fuzzy program, even though its storage
   uses double. Calling the ordinary passive preparation would substitute hard
   conditions. Interior fuzzy branches and vector blending require exact parity.
3. MAX/MIN and vector extrema have discrete selections even without an IF. Trace
   the actual selections and ties with bounded exact data; an endpoint comparison
   or a branch hash is insufficient to establish replay identity.
4. Vector reservation hints are not logical bounds. Include historical APPEND and
   indexed writes when proving shape; preserve length and padded-entry independence.
5. The typed step helper must preserve the exact raw spot at a time-zero sample.
   exp(log(spot)) is not a bitwise substitute at representable extremes.
6. Segment-local model formulas need fresh typed parameters. A cached active full
   initialization or full active scenario invalidates the claimed bounded tape.
7. Historical constant-variable dependence must be rebuilt only in InitialState.
   Restoring the same historical graph at every segment either retains the prefix
   or double-counts the initialization chain.

## Minor notes

Distinguish sample transitions from Gaussian increments in execution statistics.
A time-zero event consumes a sample transition but no random draw. An expired
product has a historical-only objective and zero path transitions; it must not
fail merely because the Black–Scholes timeline plan itself requires a future
sample. Price and all declared parameter columns still have a defined result.

## Counter-proposals

Use a complete persistent payment-variable state and terminal receiver initially;
direct contribution extraction is unnecessary and unsafe without dataflow proof.
Share typed dynamics and interpreter dispatch with existing code, using an opt-in
policy for compact reads and tracing. Do not duplicate financial formulas or the
interpreter to avoid examining their existing semantics.

## Author questions

None requiring user input. The initial explicit model, no-exercise contract,
smoothing, driver ownership and change-scoped acceptance follow the approved plan.
