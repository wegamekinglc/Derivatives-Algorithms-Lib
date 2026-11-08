# Active Codex Artifacts

This directory holds active decisions and evidence required by reproducible checks.
Keep completed delivery records in Git and pull-request history; reusable
current-state explanations belong in the published docs or focused references.

## Status and Evidence Scope

DAL-396 and DAL-425 are closed. Their final handoff follows the user's
unit-test/performance-gate scope; it does not qualify the full stochastic XCCY
numerical matrix. Historical financial FAIL results and unqualified flags
retain their original meaning. The controlled-execution and identity-collection
continuation is stopped, not a pending instruction to start more work.

Retained script/XCCY decisions, implementation checkpoints and private proof
references bind frozen source identities and saved numerical evidence. They
remain available for reproducibility, including exact-hash admission by
`dal-cpp/tests/script/xccy_acceptance_fixed_output.py`; task closure does not
make those inputs interchangeable with current source. Their original
“pending”, “blocked”, branch, CI and handoff statements describe the named
checkpoint, not live issue status or the current master. Reuse requires the
recorded source, dependency, configuration and executable provenance.

The API decisions preserve their design-baseline vocabulary and proposed
signatures. The [script calibration guide](../../docs/methodology/script_calibration_contract.md)
owns the implemented capability inventory, including
[dedicated stochastic XCCY](../../docs/methodology/script_calibration_contract.md#stochastic-xccy-model-and-phased-mc)
and [prepared basket calls](../../docs/methodology/script_calibration_contract.md#calling-prepared-xccy-mc-baskets).
An implemented interface does not establish full numerical acceptance.

## Current Maintenance

- [DAL-479 documentation and agent consistency](documentation/dal-479-documentation-agent-consistency.md)
  — Complete tracked-file inventory, current-source corrections, agent equality
  evidence and reproducible documentation checks.

## Controlling API Decisions

- [DAL-497 owning early-exercise PDE risk](api-notes/dal-497-early-exercise-pde-risk.md)
  — Active S0 schema, defaults, availability and C++/Python/Excel projections;
  implementation and qualification remain pending.
- [DAL-398 shared script contract](api-notes/dal-398-script-contract.md).
- [DAL-410 domestic-collateral Gaussian model](api-notes/dal-410-xccy-mc-model-decision.md).
- [Prepared mixed-mapping XCCY basket allocation](api-notes/dal-xccy-mixed-mapping-basket.md).

## Early-Exercise PDE Risk Controls

- [DAL-497 R1–R11 contract](specs/dal-497-early-exercise-pde-risk.md)
  — Fixed-plan full repricing, finite-scale Gamma, ownership, cumulative budgets
  and compatibility controlling S1–S5.
- [DAL-497 acceptance fixtures and gates](specs/dal-497-pde-risk-acceptance.md)
  — Frozen N1–N12 inputs/thresholds, B1–B10 and scoped performance acceptance.
- [DAL-502 numerical qualification](validation/dal-502-pde-risk-qualification.md)
  — Original S3 product FAIL/CRR INCOMPLETE checkpoint, preserved with its
  original source identities.
- [DAL-502 diffusion-tail repair](validation/dal-502-pde-tail-repair.md)
  — Accepted production repair, strict RED/GREEN, direct tests, independent
  row audit and scoped performance.
- [DAL-502 bounded reference follow-up](validation/dal-502-pde-risk-followup.md)
  — Current affected qualification: 9 PASS / 0 FAIL / 3 INCOMPLETE; mandatory
  TREE remains unqualified at the approved maximum. S4 stays paused.

The DAL-497 controls are active requirements, not product validation evidence.
Historical early-exercise #602 evidence below does not qualify DAL-497 risk.

## XCCY Implementation and Acceptance Controls

- [M1 exact Gaussian model](implementation/dal-xccy-m1-exact-model.md).
- [M2 observation and execution](implementation/dal-xccy-m2-observation-execution.md).
- [M3 moments and staged calibration](implementation/dal-xccy-m3-moments-staged.md).
- [M4 MC statistics and partials](implementation/dal-xccy-m4-mc-partials.md).
- [M5 public bindings](implementation/dal-xccy-m5-public-bindings.md).
- [Mixed-mapping basket core](implementation/dal-xccy-mixed-mapping-basket-core.md).
- [Basket public bindings](implementation/dal-xccy-basket-public-bindings.md).
- [Basket public example](implementation/dal-xccy-basket-public-example.md).
- [Independent Gaussian numerical acceptance](validation/dal-xccy-gaussian-numerical-validation.md).
- [Financial bias reference](validation/dal425-financial-bias-bound-reference.md).
- [Finite-stream transform reference](validation/dal425-finite-stream-bias-reference.md).
- [Joint-law applicability reference](validation/dal425-joint-law-applicability-reference.md).
- [Saved fixed-output error reference](validation/dal425-fixed-output-error-reference.md).

The independent acceptance record and references retain their bounded verdicts.
Do not turn a passed control, a bias indicator or a completed implementation
checkpoint into acceptance of the entire advertised matrix.

## Retained Private Execution Controls

- [Controlled execution support](implementation/dal425-controlled-support.md).
- [Fixed profile and outer candidate](implementation/dal425-controlled-profile.md).
- [Fixed caller allocation binding](implementation/dal425-controlled-caller.md).
- [Codacy repair scope](implementation/dal425-codacy-repair.md).

These notes describe the private tools and their limitations. They grant no
product execution, new allocation or successor run; the stopped collection
chain is not resumed by keeping its source and evidence.

## Upstream Implementation Controls

- [DAL-399 immutable contracts](implementation/dal-399-script-contract.md).
- [DAL-400 structural preparation](implementation/dal-400-script-preparation.md).
- [DAL-401 same-instrument MC](implementation/dal-401-script-mc.md).
- [DAL-402 Deposit prototype](implementation/dal-402-deposit-prototype.md).
- [DAL-403 Gaussian BondOption prototype](implementation/dal-403-bondoption-prototype.md).
- [DAL-405 curve calibration](implementation/dal-405-script-curve-calibration.md).
- [DAL-406 deterministic curve families](implementation/dal-406-curve-product-fast-paths.md).
- [DAL-407 public script bindings](implementation/dal-407-script-public-bindings.md).
- [DAL-408 deterministic XCCY cashflows](implementation/dal-408-xccy-script-cashflows.md).
- [DAL-409 staged XCCY calibration](implementation/dal-409-script-xccy-calibration.md).
- [DAL-411 Gaussian rate-option fast paths](implementation/dal-411-gaussian-fast-paths.md).
- [DAL-412 Gaussian script MC calibration](implementation/dal-412-gsr-script-mc-calibration.md).
- [DAL-414 GSR public bindings](implementation/dal-414-gsr-script-public-bindings.md).
- [DAL-418 accrual literal implementation](implementation/dal-418-rate-option-literals.md).
- [DAL-418 test complexity repair](implementation/dal-418-test-complexity.md).
- [DAL-418 publication checkpoint](implementation/dal-418-publication.md).
- [DAL-418 approved publication](implementation/dal-418-approved-publication.md).
- [DAL-419 Windows installed encoding](implementation/dal-419-installed-windows-encoding.md).
- [DAL-419 publication](implementation/dal-419-publication.md).

## Upstream Independent and Documentation Controls

- [DAL-404 P0 independent acceptance](validation/dal-404-p0-same-source-validation.md).
- [DAL-413 Gaussian numerical acceptance](validation/dal-413-gsr-script-numerical-validation.md).
- [DAL-418 accrual validation](validation/dal-418-rate-option-literals.md).
- [DAL-418 accrual review](reviews/dal-418-rate-option-literals.md).
- [DAL-418 complexity review](reviews/dal-418-test-complexity.md).
- [DAL-418 documentation decision](documentation/dal-418-rate-option-literals.md).
- [DAL-419 installed encoding validation](validation/dal-419-installed-windows-encoding.md).
- [DAL-419 installed encoding review](reviews/dal-419-installed-windows-encoding.md).

## Retired Records and Recovery

Completed DAL-436 documentation maintenance, DAL-469 diagnostic repair,
DAL-476 covariance repair and PR #651 review receipts are retired. Their
current behavior is described in the documentation audit above, the
[benchmark workflow](../references/benchmark-workflow.md), the
[script calibration guide](../../docs/methodology/script_calibration_contract.md)
and the [public example catalog](../../dal-public/README.md#examples).
Exact receipts remain in [master snapshot 2ee93eff](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/tree/2ee93eff5026b3d7e9f297017f6d4e352e675888/.codex/artifacts):

```bash
git show 2ee93eff5026b3d7e9f297017f6d4e352e675888:.codex/artifacts/documentation/dal-436-documentation-agent-consistency.md
git show 2ee93eff5026b3d7e9f297017f6d4e352e675888:.codex/artifacts/implementation/dal-469-benchmark-repair.md
git show 2ee93eff5026b3d7e9f297017f6d4e352e675888:.codex/artifacts/implementation/dal-476-basket-covariance-overflow.md
git show 2ee93eff5026b3d7e9f297017f6d4e352e675888:.codex/artifacts/reviews/pr651-full-review.md
```

DAL-379 market-IV delivery and DAL-387 benchmark-startup repair are complete.
Their implementation, validation and review receipts are retired from this
active directory. Exact pre-retirement files remain in
[master snapshot dd6ddc3e](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/tree/dd6ddc3e9dd1637634287bcbe8dd8d5bc7fc49a4/.codex/artifacts):

```bash
git show dd6ddc3e9dd1637634287bcbe8dd8d5bc7fc49a4:.codex/artifacts/implementation/dal-379-market-iv-ci-repair.md
git show dd6ddc3e9dd1637634287bcbe8dd8d5bc7fc49a4:.codex/artifacts/implementation/dal-387-benchmark-repair.md
git show dd6ddc3e9dd1637634287bcbe8dd8d5bc7fc49a4:.codex/artifacts/reviews/dal-379-market-iv-surface.md
git show dd6ddc3e9dd1637634287bcbe8dd8d5bc7fc49a4:.codex/artifacts/validation/dal-379-market-iv-surface.md
```

Earlier completed market-IV API/critique/probe controls are recoverable from
[Git snapshot d5b4f85b](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/tree/d5b4f85b77a85a6189c213c178aaf755da966b9c/.codex/artifacts).
That commit is available remotely; it is not an unpushed local recovery
dependency. The reusable API, defaults, compatibility and examples are in the
[market-IV guide](../../docs/models/market-iv-surface.md).
The comparison [benchmark guide](../../dal-python/benchmarks/README.md)
describes current driver behavior.

Completed early-exercise PDE controls remain in
[Git snapshot d8bb4c16](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/tree/d8bb4c1695cc6c963fd5bb4145ed59676a91c707/.codex/artifacts),
delivered through [PR #602](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/602).
The [Bermudan/American PDE guide](../../docs/methodology/pde/early-exercise.md)
owns the current method and interfaces.

Supported compiled double batching and LSMC checkpoints are documented in the
[simulation guide](../../docs/methodology/monte-carlo/simulation.md#explicit-compiled-double-batching)
and [LSMC guide](../../docs/methodology/monte-carlo/lsm.md#opt-in-training-checkpoints).
