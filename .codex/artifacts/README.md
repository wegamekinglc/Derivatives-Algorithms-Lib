# Active Codex Artifacts

This directory holds DAL specifications, designs, API notes, critiques, reviews,
plans and performance reports while they control ongoing work.

Keep completed delivery records in Git and pull-request history. Put reusable
current-state explanations in the published docs or focused Codex references.

## Current Artifacts

- [DAL-379 market IV surface API decision](api-notes/dal-379-market-iv-surface.md)
  — Active P2 C++/Python/Excel contract for price/decimal-IV quotes, constrained
  SSVI, analytic derivatives, Dupire export and full-refit bump responses.
  Numerical risk review found no design blocker; targeted certificate, conversion,
  derivative, refit and actual-budget validation remain for implementation.

- [DAL-379 market IV surface numerical critique](critiques/dal-379-market-iv-surface.md)
  — Active P2 review with full-domain algebra, independent high-precision probes,
  budget checks and concrete implementation acceptance cases; implementation and
  product numerical validation remain.

- [DAL-379 market IV surface implementation](implementation/dal-379-market-iv-surface.md)
  — Active P2 delivery record for constrained fitting, owning snapshots, analytic
  derivatives, full refit and all bindings; contains directed numerical/budget
  evidence and the explicit native Windows verification boundary.

Completed DAL-379 controls are archived at Git snapshot `d8bb4c1695cc6c963fd5bb4145ed59676a91c707`:
[benchmark implementation and repair](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/d8bb4c1695cc6c963fd5bb4145ed59676a91c707/.codex/artifacts/implementation/dal-379-benchmark-ci-repair.md),
[independent validation](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/d8bb4c1695cc6c963fd5bb4145ed59676a91c707/.codex/artifacts/validation/dal-379-early-exercise-pde.md),
and [full review history](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/d8bb4c1695cc6c963fd5bb4145ed59676a91c707/.codex/artifacts/reviews/dal-379-early-exercise-pde.md).
Final-head CI, native Windows OPER/XLL checks and acceptance are tracked in
[PR #602](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/602)
and the DAL-379 issue, rather than active stage records.

The current API, method, defaults, budgets and language examples are documented
in the [Bermudan/American PDE guide](../../docs/methodology/pde/early-exercise.md).
Completed API, numerical-critique and implementation controls are retained in
Git history rather than registered as active work.

Supported compiled double batching and LSMC training checkpoints are documented in
the [simulation guide](../../docs/methodology/monte-carlo/simulation.md#explicit-compiled-double-batching)
and [LSMC guide](../../docs/methodology/monte-carlo/lsm.md#opt-in-training-checkpoints).
