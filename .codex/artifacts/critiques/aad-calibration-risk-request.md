# Calibration-coordinate request critique

Verdict: Proceed with caveats.

Read the full [spec](../specs/aad-calibration-risk-request.md),
[API note](../api-notes/aad-calibration-risk-request.md), market-request audit,
native common provider and scalar D04 request/result implementation. This
contract makes source ownership, retained payload and raw/report projections
concrete for the first request increment; full F01 remains open.

## Blocking issues

None for implementing the common passive request increment. Automatic Hybrid
valuation still needs its concrete sealing/execution tests before acceptance;
the common planner cannot prove that work complete.

## Significant concerns

- Quote IDs are source-scoped. Include independent snapshots with equal labels
  in parameter mismatch tests and retain complete case-sensitive curve source
  checks. Preserve Dupire's quote-only direct identity across different fixed
  base IVSs; a direct partial is not a derivative of the fixed calibration base.
- Three full matrices must be counted for subset and empty selections. Tests
  must reject one byte below that full count without touching an unrelated live
  recording, and preserve that graph's independent derivative.
- Projection overflow must reject publication for every selected contribution;
  checking only total could hide cancellation of two overflowing reported terms.
- Curves have no typed quote values. Optional absence is required; do not make a
  NaN sentinel, unitless zero or guessed captured-record value.
- Empty selection must still perform the native VJP and retain its full method
  and contributions. Exercise Dupire's native independent-scope guard to
  distinguish a false no-op implementation from the intended behavior.
- All arithmetic must be checked before creating axis vectors or result matrices.
  A pure byte-count helper gives executable overflow coverage without giant
  fixtures. Do not introduce another calibration inverse or change tolerances.

## Minor notes and counter-proposals

Use one private projection helper for the four getters. Keep axis construction
domain-specific only where native coordinate metadata differs; source dimensions,
selection, factor validation and budget logic are shared. New getters may allocate
their detached return values; document that these are outside the retained-result
budget and do not measure them as part of a prepared VJP call.

## Author questions

No user clarification required. Implement the specified common layer with RED /
GREEN evidence, then review the automatic Hybrid plan and binding parity before
closing F01 or attempting the current-PR merge.
