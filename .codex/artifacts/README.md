# Active Codex Artifacts

This directory holds DAL specifications, designs, API notes, critiques, reviews,
plans and performance reports while they control ongoing work.

Keep completed delivery records in Git and pull-request history. Put reusable
current-state explanations in the published docs or focused Codex references.

## Current Artifacts

- [DAL-379 early-exercise PDE independent validation](validation/dal-379-early-exercise-pde.md)
  — Retained for final acceptance: static numerical/budget evidence and contact
  probes are reusable, while native Windows XLL execution is pending CI.
  The mixed-link DSO/AAD failure is a baseline limitation, outside the P3 repair.

- [DAL-379 early-exercise PDE focused code review](reviews/dal-379-early-exercise-pde.md)
  — Targeted Approve at `318ccc57` clears the P2 blank-settings code blocker.
  Three raw OPER/XLL entry tests still require Windows CI compilation/execution;
  portable/shim checks do not substitute. Keep this record until final acceptance;
  the CRR style observation remains non-blocking.

The current API, method, defaults, budgets and language examples are documented
in the [Bermudan/American PDE guide](../../docs/methodology/pde/early-exercise.md).
Completed API, numerical-critique and implementation controls are retained in
Git history rather than registered as active work.

Supported compiled double batching and LSMC training checkpoints are documented in
the [simulation guide](../../docs/methodology/monte-carlo/simulation.md#explicit-compiled-double-batching)
and [LSMC guide](../../docs/methodology/monte-carlo/lsm.md#opt-in-training-checkpoints).
