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
registry/path range. The producer now establishes that provenance through an
owning prepared portfolio, captures one date and union history after all plans
and the admission callback, and retains explicit/global source metadata.
Fourteen new preparation tests verify late-trade rejection before reads, private
state, mutation isolation and failure recovery. The six exact factory model
types admit full-contract grouping; their actual shared-path risk oracles still
must pass. These tests do not prove scenario reuse or cross-group risk accumulation.

The shared weighted batch increment now proves scenario reuse within a group.
Nine focused cases cover independent nonzero-volatility batches with original
absolute paths, private historical vectors and scalar aliases, omitted poisonous
trades, zero-weight validation, overflow, nested native modes and failure recovery.
Actual counters verify one scenario and suffix reverse per path and one prefix
reverse per batch. The internal coordinator's seven additional cases now cover
cross-group accumulation, selected/empty inputs, task failure recovery and all
six native model families against every independent scalar risk. Original meshes,
owners, output order and one/four workers are checked. Aggregate capacity,
blocked attribution and complete binding acceptance still remain open.
Native/passive public weighted results now own values, axes and original resolved
trade/group metadata. Passive execution preserves the existing sharp evaluator
and checked BS paths, while native-empty inputs retain fuzzy prices. All six
families match independent prices/risks in both modes. Native weighted execution implements
aggregate runtime recording/scratch guards and known-shape startup admission before
history. Seven new cases verify prospective groups against completed groups,
finite budgets/peaks, zero-budget rejection, all selected private vectors, omission
of unselected evaluator state, capacity failure/recovery and sparse native vector
zero-hole bindings. Passive known private-vector admission now has an independent
historical-price oracle and rejects before reads with finite scratch limits,
ignoring zero tape limits. Owning native blocked attribution now has independent
all-column oracles across six families, original mixed meshes/owners and several
widths. Root/matrix/private historical-vector admission rejects known impossible
budgets before reads/tasks and finite budgets recover correct prices/risks. The
review fixes wrong original-row context on report overflow and shares owning
selection/result metadata without changing old single-product entry points.
Passive attribution now reuses private double batches without native tape or
an unused overflowing objective. Independent six-family/mixed-mesh row oracles,
sharp versus native-empty prices, exact zero-column payloads and known private
historical-vector rejection/recovery pass. The actual extended CI failure in the
finite weighted fixture is repaired with declared per-worker quotas rather than
a prior scheduling-dependent peak; all nine replay cases pass locally. Dedicated
equivalent-budget narrowing and attribution failure recovery now pass locally.
The retained late-width RED exposed a submission from an earlier group before
rejection; all selected group widths now validate first. Bindings and fresh
complete-head publication/performance gates remain required. Python construction,
typed requests/results and both valuations now pass 49 ownership, strict-input,
detached-lifetime and independent original-mesh cases. All 126 affected Python
risk cases pass through the installed extension. Excel's strict physical trade
table preserves original owner handles, with checked immutable result getters
and separate generated wrappers. Four new and 19 existing affected portable risk
cases pass; MSVC OFF/combined syntax and strict source/header checks pass. Actual
generated Windows runtime and all six expanded sanitizer jobs pass at
`9cc9fe0a`. The
complete OFF suite and all four extended linked/runtime diagnostic modes pass.
Installed consumers pass. The measured two-helper preparation-inlining repair
passes original single-request weighted timing and all 160 comparative cases,
with fixed caller CPU confirmation limited to two serial MC metrics. The
[performance report](../performance/aad-compatible-script-portfolio.md) retains
failed/noisy measurements, original-workload repair confirmation and
130 independent new-entry cost cases. One-trade and distinct-owner overhead
remains explicit future optimization work; sharing is not a general speed claim.

Implement weighted groups before blocked attribution; reuse the accepted
native root/replay primitives. Avoid speculative hashing, persistent caches or
automatic width selection in this PR. Current-state public documentation now
covers the implemented C++/Python/Excel construction, requests, results and
native/passive valuation. Final platform/performance acceptance remains distinct
from local implementation. The callback has
actual native/passive weighted aggregate admission. Blocked request/result integration must retain this
policy and failure boundary; the generic callback alone is not capacity acceptance
for another execution mode.

## Author questions

No user clarification is needed. The model snapshot boundary and result metadata
layout are implementation decisions under existing authority. If the factory
cannot freeze an accepted model family, document the concrete gap and repair it
before claiming that family's portfolio acceptance.
