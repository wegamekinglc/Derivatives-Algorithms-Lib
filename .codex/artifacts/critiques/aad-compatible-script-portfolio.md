# Compatible portfolio design critique

Verdict: **Proceed with caveats**.

This local design review covers the active
[specification](../specs/aad-compatible-script-portfolio.md) and
[API proposal](../api-notes/aad-compatible-script-portfolio.md), against
`PreparedScript_`, `ObservationPlan_`, `SampleDef_`, nearby preparation/simulation
tests and the current weighted/Jacobian methodology. It is not implementation,
performance or merge acceptance.

## Blocking issues

No unresolved specification blocker. The design must retain separate groups for
incompatible supported trades and explicit coordinate ownership. Removing either
requirement would reopen a blocker under the approved F02 plan.

## Significant concerns and implementation gates

1. **Model owners can disappear during cloning.** Register repeated original
   handles before making snapshots. Two cloned objects from the same owner must
   scatter into one coordinate axis; two distinct originals with equal values
   must keep two axes. Add both tests before the owner registry implementation.
2. **A date union changes the finite-sample function.** The current model
   initialization derives path dimensions from the prepared timeline. Equal
   terminal-date distributions do not prove equal Sobol/MRG path realizations.
   Keep exact original grids and run nonzero-volatility independent per-trade
   oracles; a deterministic price alone cannot admit mesh sharing.
3. **Slot equality does not establish observation equality.** Compare every
   ordered sample-definition field and canonical observation/date meaning.
   Different discount maturities or differently ordered indices must split
   groups even when vector lengths agree. If hashing is introduced, collision
   tests must prove full comparison is still performed.
4. **History/evaluator state must remain private.** Prepared historical seeds
   depend on the trade's script and constants. Equal fixing values do not permit
   sharing variables, vector stores or seeds. Keep separate state and require
   an isolation test in tree and compiled execution before sharing a scenario.
5. **Incompatible groups may share input ownership.** One model owner can have
   several timelines. Different recordings then scatter into the same model
   columns. Tests must detect missing contributions, duplicated prefix reverse
   and accidental division by the number of groups.
6. **Memory savings cannot hide private evaluator costs.** Admission must include
   every resident evaluator/history seed, not just one model/path per group.
   Sequential initial group execution bounds group overlap; worker parallelism
   still requires aggregate reservation and task-drain tests at finite budgets.

## Minor notes and smaller useful scope

The first implementation slice now establishes owning identities, passive axes
and a deterministic full-equality group planner. Local tests exercise distinct
and repeated owners, all sample fields and private scalar/vector state over
reused tree/compiled evaluators. The planner consumes views from one sealed
registry/path range; the producer establishing that contract is still pending.
These tests do not prove scenario reuse or cross-group risk accumulation.

Implement weighted groups before blocked attribution; reuse the accepted
native root/replay primitives. Avoid speculative hashing, persistent caches or
automatic width selection in this PR. Keep valuation/binding proposals visibly
marked as unimplemented; current-state documentation covers only sealed C++
construction and passive coordinate inspection.

## Author questions

No user clarification is needed. The model snapshot boundary and result metadata
layout are implementation decisions under existing authority. If the factory
cannot freeze an accepted model family, document the concrete gap and repair it
before claiming that family's portfolio acceptance.
