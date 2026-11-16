# Active Codex Artifacts

This directory contains only specifications and API decisions that still control
active work or current acceptance checks. Completed implementation, validation,
review and performance records belong to Git and pull-request history, not the
working tree. Reusable current-state explanations belong in `docs/` or the focused
references.

## Current Controls

- [DAL-538 market-IV to nonflat F/D local-vol MC API decision](api-notes/dal-538-market-iv-local-vol-mc.md)
- [DAL-538 staged implementation and acceptance plan](plans/dal-538-market-iv-local-vol-mc.md)
- [DAL-542 N5 resolution A/3: current MC protocol and explicit recovery](specs/dal-542-n5-resolution-a.md)
- [DAL-526 compounded OIS observation and quote-risk contract](specs/dal-526-compounded-ois-observation-quote-risk.md)
- [DAL-497 owning early-exercise PDE risk](api-notes/dal-497-early-exercise-pde-risk.md)
- [DAL-497 PDE risk contract](specs/dal-497-early-exercise-pde-risk.md)
- [DAL-497 acceptance fixtures and gates](specs/dal-497-pde-risk-acceptance.md)
- [DAL-398 shared script contract](api-notes/dal-398-script-contract.md)
- [DAL-410 domestic-collateral Gaussian model](api-notes/dal-410-xccy-mc-model-decision.md)
- [Prepared mixed-mapping XCCY basket allocation](api-notes/dal-xccy-mixed-mapping-basket.md)

The implemented behavior and supported interfaces are described by the current
[documentation](../../docs/README.md). Historical completion or validation text
in a controlling document describes its named baseline, not live branch status.
