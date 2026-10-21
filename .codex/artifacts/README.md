# Active Codex Artifacts

This directory holds DAL specifications, designs, API notes, critiques, reviews,
plans and performance reports while they control ongoing work.

Keep completed delivery records in Git and pull-request history. Put reusable
current-state explanations in the published docs or focused Codex references.

## Current Artifacts

- [DAL-398 script contract and calibration API v1](api-notes/dal-398-script-contract.md)
  — Active definition/header/binding schema, persistence, public error/report contracts
  and C++/Python/Excel mappings for the DAL-396 implementation chain. P0 admission is
  limited to Deposit and Gaussian BondOption; product implementation and validation follow.

- [DAL-387 benchmark startup repair](implementation/dal-387-benchmark-repair.md)
  — Scheduled comparison import-order failure, focused RED/GREEN evidence,
  build selection and actual driver verification; awaiting independent review.

- [DAL-379 market IV Codacy repair](implementation/dal-379-market-iv-ci-repair.md)
  — Incremental PR #604 complexity/length refactor; preserves the reviewed
  numerical contracts and raw-hook order. Includes scoped comparison evidence
  and the new-head native Windows validation boundary.

- [DAL-379 market IV surface independent validation](validation/dal-379-market-iv-surface.md)
  — Pending final acceptance only: the appended PRICE revalidation at `525b11de`
  clears the numerical blocker; the initial RED and all independent evidence
  remain intact. Four native Windows OPER/XLL regressions still need final-head CI.

- [DAL-379 market IV surface focused code review](reviews/dal-379-market-iv-surface.md)
  — Appended scoped Approve at `24662d9a`: the Excel raw-hook order code blocker
  is cleared; six constructors validate before conversion, strict bool diagnostics
  remain and two new exported-entry storage regressions are registered.
  The first-round review and PRICE clearance are retained. All four native
  Windows OPER/XLL regressions remain unexecuted locally; portable/shim evidence
  does not substitute. Method/interface docs and CHANGELOG are now complete;
  final-head CI and acceptance remain with the orchestrator.

## Recoverable completed records

P2 API, critique, implementation and completed numerical/control probes are
retired from active work. Their exact pre-retirement bytes, together with the
full validation/review, are preserved in local Git snapshot
`d5b4f85b77a85a6189c213c178aaf755da966b9c` (not yet pushed):

```bash
git show d5b4f85b77a85a6189c213c178aaf755da966b9c:.codex/artifacts/api-notes/dal-379-market-iv-surface.md
git ls-tree -r --name-only d5b4f85b77a85a6189c213c178aaf755da966b9c .codex/artifacts
```

The snapshot also preserves the reviewed Python regressions; the current
`dal-python/tests/test_market_iv_surface.py` remains byte-for-byte identical.
Use the snapshot paths to recover completed probes referenced by historical logs.
The reusable current contract, defaults, compatibility and language examples
are in the [market-IV surface guide](../../docs/models/market-iv-surface.md).

Completed P3 controls are archived at Git snapshot `d8bb4c1695cc6c963fd5bb4145ed59676a91c707`:
[benchmark implementation and repair](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/d8bb4c1695cc6c963fd5bb4145ed59676a91c707/.codex/artifacts/implementation/dal-379-benchmark-ci-repair.md),
[independent validation](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/d8bb4c1695cc6c963fd5bb4145ed59676a91c707/.codex/artifacts/validation/dal-379-early-exercise-pde.md),
and [full review history](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/d8bb4c1695cc6c963fd5bb4145ed59676a91c707/.codex/artifacts/reviews/dal-379-early-exercise-pde.md).
P3 was delivered through merged
[PR #602](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/602).
Its completed checks do not certify the new P2 entry points.

The current API, method, defaults, budgets and language examples are documented
in the [Bermudan/American PDE guide](../../docs/methodology/pde/early-exercise.md).
Completed API, numerical-critique and implementation controls are retained in
Git history rather than registered as active work.

Supported compiled double batching and LSMC training checkpoints are documented in
the [simulation guide](../../docs/methodology/monte-carlo/simulation.md#explicit-compiled-double-batching)
and [LSMC guide](../../docs/methodology/monte-carlo/lsm.md#opt-in-training-checkpoints).
