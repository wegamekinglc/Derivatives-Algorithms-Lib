# Active Codex Artifacts

This directory contains only specifications and API decisions that still control
active work or current acceptance checks. Completed implementation, validation,
review and performance records belong to Git and pull-request history, not the
working tree. Reusable current-state explanations belong in `docs/` or the focused
references.

## Current Controls

- [DAL-497 owning early-exercise PDE risk](api-notes/dal-497-early-exercise-pde-risk.md)
- [DAL-497 PDE risk contract](specs/dal-497-early-exercise-pde-risk.md)
- [DAL-497 acceptance fixtures and gates](specs/dal-497-pde-risk-acceptance.md)
- [DAL-398 shared script contract](api-notes/dal-398-script-contract.md)
- [DAL-410 domestic-collateral Gaussian model](api-notes/dal-410-xccy-mc-model-decision.md)
- [Prepared mixed-mapping XCCY basket allocation](api-notes/dal-xccy-mixed-mapping-basket.md)

These controls retain the independent PDE fixed-plan/schema/acceptance limits,
shared script ownership and binding rules, and XCCY model/scalar-allocation
contracts. Their numerical and platform qualifications remain scoped to the
named fixtures and producers; task completion alone does not supersede a contract.

## Implemented contracts and qualification

Use the current-state guides for:

- [Market-grid local-volatility model/API](../../docs/models/local-volatility.md#owning-market-model-in-core-c),
  including Init freezing, resource admission and request identity domains.
- [Independent local-volatility qualification](../../docs/methodology/monte-carlo/market-iv-local-vol-validation.md),
  including N1-N7 gates, canonical grids, A/3 sampling and evidence reuse.
- [Compounded OIS](../../docs/ccy-curves/compounded-ois.md), including `/1`/`/2`
  schedules, frozen context, signed quote DV01 and independent FD checks.

The [numerical harness guide](../../tests/market-iv-local-vol/README.md) identifies
the sealed execution evidence and reproduction entry points. Frozen control
bytes remain with their original Git revisions and evidence archives; current
guides do not rebind their hashes, reopen completed budgets or claim a new run.
The [documentation index](../../docs/README.md) covers other implemented interfaces.
