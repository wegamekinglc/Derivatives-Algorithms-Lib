# Native solve accuracy design critique

Verdict: Proceed with caveats.

## Significant concerns

- Owned-buffer deallocation uses the currently attached account. Allocate
  returned storage before scratch; copy doubles, then destroy numeric scratch
  before publishing. Moving numeric error vectors into reports is prohibited.
- Recording identity is lazy in the existing checkpoint implementation. Use the
  existing identity generator through a cold friend access method; do not
  substitute addresses or a competing counter into the checkpoint identity.
- ReverseEvent_ is mutating. Collection must bind to the actual invocation and
  scope, enforce policies without collection, and expose no partially accepted
  result after another event fails. Native graph failure requires cleanup.
- A private helper extraction may change existing executable bytes/inlining.
  Preserve old template bodies/layouts and measure only affected caller rows.
- Dense checks retain a physical transpose and dense residual work. Do not
  represent this as an efficient checked banded/PDE implementation.

## Acceptance controls

The spec's analytic/rational tests, actual multi-channel seeds, checkpoint
restore identities, exact budgets and failure refunds are mandatory. Zero seeds
must retain report columns. Detached entries must retain invocation metadata.
No remaining author questions require user clarification.
