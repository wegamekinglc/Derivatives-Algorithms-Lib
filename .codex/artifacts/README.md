# Active Codex Artifacts

This directory holds DAL specifications, designs, API notes, critiques, reviews,
plans and performance reports while they control ongoing work.

Keep completed delivery records in Git and pull-request history. Put reusable
current-state explanations in the published docs or focused Codex references.

## Current Artifacts

- [DAL-379 early-exercise PDE API decision](api-notes/dal-379-early-exercise-pde.md)
  — Active C++/Python/Excel contract for deterministic-carry BS Bermudan/American
  prices, owning diagnostics, exercise regions, budgets and independent acceptance.
  Default fully implicit and full-region capacity/budget policy revised with both
  B1 counterexamples and refinement arithmetic; targeted B1 re-review cleared the
  budget blocker. Implementation, independent validation and focused code review are
  recorded below; the XLL P2 code blocker is cleared, with Windows CI and documentation pending.

- [DAL-379 early-exercise PDE numerical critique](critiques/dal-379-early-exercise-pde.md)
  — Initial numerical evidence preserved with an appended targeted B1 re-review:
  budget blocker cleared; independent acceptance, penalty, boundary and actual
  storage-envelope checks are covered by the implementation and validation records below.

- [DAL-379 early-exercise PDE implementation](implementation/dal-379-early-exercise-pde.md)
  — Core, public, Python and Excel implementation at `0b903a02`, with original
  numerical, budget, compatibility and performance evidence and explicit Windows limits.

- [DAL-379 early-exercise PDE independent validation](validation/dal-379-early-exercise-pde.md)
  — Accepted static evidence provenance verified; minimal current/baseline comparison
  identifies the existing mixed-link DSO/AAD failure. Eight synthetic contact probes
  pass with a failing truncation mutation control; default mixed configuration and
  Windows XLL runtime evidence remain open for the orchestrated follow-up stages.

- [DAL-379 early-exercise PDE focused code review](reviews/dal-379-early-exercise-pde.md)
  — Targeted Approve at `318ccc57` clears the original P2 blank-settings blocker;
  the initial review and evidence are preserved. Dedicated validator, generated
  entry and three Windows raw regressions reviewed; native Windows execution is
  still pending CI. CRR style remains non-blocking; documentation is pending.

Supported compiled double batching and LSMC training checkpoints are documented in
the [simulation guide](../../docs/methodology/monte-carlo/simulation.md#explicit-compiled-double-batching)
and [LSMC guide](../../docs/methodology/monte-carlo/lsm.md#opt-in-training-checkpoints).
