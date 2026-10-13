# WP-4c design critique

Verdict: Proceed with caveats.

## Blocking issues

None before implementation. Acceptance remains conditional on real baseline RED,
fresh-prefix parity, capacity/mode recovery and all exact-head remote gates.

## Significant concerns

- AAD input leaf adjoints survive prefix reversal. Clear each harvested model
  and script-constant leaf between batches; test historical active inputs.
- Cleanup capacity is observed after recording.Close(). A ring slot can be
  reduced before its worker exits; keep cleanup statistics in worker-owned slots
  and fold them after draining, rather than storing incomplete values in reused
  risk slots. Preserve tape-capacity scope/mode destruction order on exceptions.
- Batch indices must retain their absolute training+validation+replicate offsets.
  RQMC keys belong to one replicate; no state may survive into another policy.
- The independent fresh-segment oracle may enable degree selection when moving
  the pricing offset with validation paths. Use a sole degree-one candidate and
  prove identical results/policy on the baseline before accepting this oracle.
- Generic plan support must retain ordinary explicit-template callers and the
  cancellation/callback lifetime guarantees already accepted in #561.

## Minor notes and counter-proposals

No guaranteed speedup is established for coarse BS batches. Prefer common
ownership and bounded storage; reject a measured throughput regression. Preserve
the legacy LSMC plan rather than combining persistence with a new granularity.

## Author questions

None requiring user input. Keep default training paths unchanged.
