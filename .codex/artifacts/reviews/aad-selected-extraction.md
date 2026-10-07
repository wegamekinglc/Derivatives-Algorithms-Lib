# Selected extraction acceptance review

Verdict: **Approve** for the reviewed implementation and local acceptance.
Final-head publication gates remain mandatory before merge.
Scope: [P03 specification](../specs/aad-selected-extraction-block-policy.md),
the [ownership design](../designs/aad-selected-extraction.md) and PR #489.

## Findings

No open code findings. The compact fallback and subset-only outer guard pass
37 default and 50 selected/empty/width comparisons under predefined calibrated
sampling. The [performance report](../performance/aad-selected-extraction.md)
retains all earlier failures and the observation-window amendment.

Final code is not yet published. `e425e52d` passes all 35 checks, has zero Codacy
annotations and zero unresolved threads, but lacks the final compact fallback
and quota test. Require exact-head checks and two complete audits after publication.

## Correctness and methodology

Core packing preserves original ordinals and the full model prefix. Native empty
arrays keep their rows and recorded work; mappings remain immutable until tasks
drain. Admission uses the same mapping decision and numeric layout as replay.
Only actual capacity failures narrow widths. Full model/evaluator registration,
historical reseeding, original RNG offsets and reduction order remain intact.

The compact fallback is a deterministic pre-valuation memory proxy, not a measured
latency predictor or minimum-capacity guarantee. Compact nonempty requests retain
full private arrays and return only selected public columns. Wide and native-empty
requests retain packing. The specification and current methodology now describe
this boundary explicitly. Public C++/Python/Excel requests and defaults are unchanged.

## Tests and residual risk

Local evidence covers 41 unchanged core and 47 public cases. The new quota contract
is RED against frozen published production (43,692 required versus 43,684 allowed)
and GREEN locally; its quota is found only from complete requests. Wide weighted
and Jacobian bounds remain GREEN, including width two, padding and history aliases.
Thirteen freshly linked legacy executables retain accepted hashes.

Published-head logs verify 87 affected cases in each of six sanitizer modes,
94 relevant cases and installed consumers in each of four Windows modes, and
91 relevant C++ cases plus 1,166 Python cases in each of four extended modes.
The final compact test must appear in those actual final-head runtime logs too.

## Open questions

No user clarification or permission is needed. Measured widths retain default
maximum one, the explicit upper bound and capacity-only narrowing; no automatic
policy/API is justified by this matrix. Final publication/CI acceptance remains.
