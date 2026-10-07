# Selected extraction critique

Verdict: **Proceed with caveats** for the active
[P03 specification](../specs/aad-selected-extraction-block-policy.md).
P02 worker reuse and native reset are accepted in
[merged #488](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/488).

## Blocking issues

No user-scope blocker. The independent resource RED fixture and mapping ownership
must be concrete before production implementation. A fixture that subtracts bytes
from the new selected request's own peak would reject its own implementation and
would not establish a stable resource contract.

## Significant concerns

- Packed model columns cannot supply the original private-input prefix. Store
  or derive the full prefix from sealed metadata and test partial model selection
  together with another trade's private risk.
- Numeric extraction can shrink while recorded input/tape costs remain full.
  Update gradient-slot admission separately from model/evaluator/tape admission.
- Native-empty extraction must preserve native valuation and reversal semantics.
  Empty packed arrays do not identify passive execution.
- Share immutable mappings across workers with a lifetime extending through task
  draining. Avoid duplicating mappings per batch or materializing unselected
  numeric gradients behind another interface.
- Preserve default all-input performance. P02 retained failed dispatch/kernel
  experiments; a new abstraction needs complete-request evidence, not intuition.
- A feasible width and a faster width are different decisions. Measure after
  extraction and retain defaults unless a stable policy has sufficient evidence.

## Recommended order

Establish the resource contract and original-ordinal mapping first. Implement
selected storage, scatter and known-fit admission together. Cover empty/native,
ownership, recovery and all six families before width-policy decisions.

No external approval is required for the existing private optimization scope.
A public policy extension needs a documented API/parity decision before coding.
