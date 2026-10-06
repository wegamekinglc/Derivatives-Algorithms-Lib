# Compatible native script portfolio risk

Status: active specification. Sealed C++ ownership, passive coordinate catalogs
and internal compatibility planning and whole-request preparation are implemented
and locally verified. Shared weighted group batches and internal weighted
replay/scatter are also locally verified. An internal owning weighted request plan
validates selection and numeric payload before history/tasks. Native weighted
startup recording/scratch admission and aggregate runtime guards are now locally
verified. Owning public C++ weighted results and passive shared execution are
implemented and locally verified against independent single-script calls for
all six families. Owning native C++ blocked attribution now passes independent
model/private risk, historical prefix, original-mesh and failure-recovery checks.
Known block-width capacities admit before history, with aggregate runtime guards.
Passive attribution, width-narrowing acceptance, bindings and final delivery gates
remain pending.
Delivery starts from merged PR #484, commit `1c9273c9`, in open PR
[#487](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/487).

## Source and problem

The user authorized the full AAD plan, native-only support, removal of existing
performance regressions, and correction of CI/Codacy/review issues before each
merge. The controlling plan's F02 task card C.6 requires compatible portfolio
aggregation and separate execution of incompatible trades with retained identity.
See the [implementation ledger](../plans/aad-implementation.md).

Single-script weighted risk and budgeted Jacobians are accepted. They prepare
one product, generate its scenario, and differentiate its outputs. Calling them
once per trade repeats model/path work even when the scenario contracts agree.
Combining scripts by name or joining their dates would introduce unproved risk
ownership and RNG changes. This increment must expose portfolio aggregation
while sharing only work whose equivalence is established.

Closest contracts are [weighted risk](aad-weighted-script-risk.md),
[blocked Jacobians](aad-blocked-script-risk.md), and the current
[AAD methodology](../../../docs/methodology/aad.md#budgeted-script-jacobians).
Preparation uses `PreparedScript_` and `ObservationPlan_`; each product currently
has its own evaluator, historical seed and observation-to-sample mapping.

## Goals and exclusions

Deliver owning C++/Python/Excel portfolio results for a fixed weighted objective
and ordered output attribution. Share a scenario and model recording inside a
compatible group; scatter group gradients into an explicitly owned global input
axis. Preserve the finite-sample function of the corresponding individual calls.

This increment does not implement union-mesh RNG remapping, persistent worker
caches, automatic block-width tuning, exercise-policy aggregation, structured
solver/PDE operators, or second-order risk. Different supported model owners
and incompatible sample contracts form separate groups. An unsupported trade
fails with its identity; incompatibility alone is not an unsupported request.

## Inputs, outputs and mathematical function

Inputs are an ordered nonempty list of trade IDs, product/model handles, a
positive integral path count, an immutable output/input selection, passive
weights or a Jacobian width, valuation/simulation settings, and optional budgets.
Capture one evaluation date and freeze the required historical environment for
the whole request. Model ownership is explicit through repeated model handles;
equal names, parameter values or serialized shapes do not establish ownership.

For selected scalar output `k`, belonging to trade `t(k)` and compatible group
`g(k)`, the price is the mean of that trade's prepared output over the original
absolute path indices. The weighted objective is the sum of those means times
the fixed passive weights. Its derivative is the weighted sum of the output
derivatives with respect to the global owned input coordinates. Shared model
coordinates accumulate contributions from all groups of that owner. Script
constant coordinates remain private to their trade.

Weighted results retain objective mean, ordered component means/weights,
one-by-input gradient, selected/complete axes, and provenance. Attribution
results retain ordered means and an output-by-input Jacobian. Both retain
trade/group identities, resolved sampling contracts, method, history, actual
widths/replays, generated scenario counts and peak admitted capacities. Getter
results are detached passive copies; no live tape, model or evaluator escapes.

## Functional requirements

1. **Validation and sealing.** Reject an empty portfolio, empty/duplicate trade
   IDs, null products/models, unsupported model families, invalid path counts,
   malformed selections/factors/weights and arithmetic overflow before history
   access or task submission. Use existing case-insensitive DAL ID semantics.
   Capture settings once, deep-freeze execution inputs before asynchronous work,
   and verify complete model/product axes against the preparation snapshot.
   Caller mutation or destruction after sealing cannot change the request.

2. **Stable identities.** Trade positions follow the supplied portfolio order.
   Assign model-owner ordinals by first occurrence of the same explicitly shared
   model handle. Use namespaced IDs such as `trade:0:payoff`,
   `trade:0:output:1`, `trade:0:constant:0`, and `model:0:parameter:0`.
   Retain the user trade ID and original script label separately. Output/input
   selection, weights, grouping, worker count and width do not renumber axes.
   Reordering the portfolio changes positions and is a different sealed request.

3. **Selection.** Omitted outputs select one payoff per trade in input order;
   explicit empty outputs and duplicate IDs are invalid. Distinct legal output
   aliases remain distinct rows. Omitted weights mean one per selected output;
   provided weights must have exactly that length and be finite. Negative and
   zero weights are valid. Zero weight does not excuse nonfinite selected output
   values. Explicit empty native inputs retain native fuzzy pricing; explicit
   passive mode selects no risks and retains the zero-column shape.

4. **Grouping proof.** Share a model/path only when model-owner identity,
   frozen model type/parameters/calibration inputs, evaluation date, sample dates,
   timeline, ordered complete sample definitions, observation semantics and
   slots, numeraire/payment definitions, RNG algorithm/dimension/stream/absolute
   index range, Brownian-bridge/factor ordering and simulation method agree.
   Compare semantic fields; pointer equality of observation plans, coincident
   slot numbers and model display names are insufficient. A hash may accelerate
   lookup but must be followed by full contract equality. No epsilon comparison
   may merge genuinely different timeline or model snapshots.

5. **History and evaluator isolation.** Resolve required fixings from one sealed
   request environment. Validate historical observation keys, source policy and
   frozen values before sharing a scenario. Retain each trade's historical
   scalar/vector seed, constants, variables and evaluator exclusively. Different
   private script state may consume the same scenario only after proving that
   no private state is shared. Unproved compatibility forms separate groups;
   do not merge states merely because variable names and values agree.

6. **Incompatible groups.** Deterministically form groups in first-trade order
   and keep trade order within each group. Different owners, time grids,
   observation layouts or numeraire/RNG contracts execute separately using their
   own original prepared plan. Preserve each group's original random dimension
   and absolute path indices. Do not form a union timeline or enlarge observation
   layouts to obtain compatibility in this increment. Scatter every result back
   into the requested global output order. Unselected groups may be omitted
   only after dependency proof; the complete identity axes remain available.

7. **Weighted execution.** Per group and path, generate one scenario, evaluate
   each selected trade with private state, and materialize a weighted native root
   that preserves alias/direct-input/constant semantics. Reverse each recorded
   path suffix once and each retained batch prefix once. Shared model leaves
   are registered once per group recording; trade constants are distinct leaves.
   Accumulate incompatible groups of the same model owner into the same global
   model columns after passive extraction. Do not double-reverse prefix state.

8. **Attribution execution.** Reuse fixed-width native block semantics. Each
   group replays its complete original path range for each of its output blocks;
   unused tail lanes are zero and changing width rebuilds the recording.
   For group `g` with `m_g` requested rows, the replay count is
   `ceil(m_g / width_g)`; generated scenarios total the path count times the sum
   of those group replay counts. Keep evaluator invocations separate from
   generated scenarios in diagnostics. Do not count a shared path once per trade.

9. **Shape and units.** Weighted raw gradient shape is `(1, n)`; attribution
   shape is `(m, n)`, including `n = 0`. Price normalization is exactly the
   existing path mean. No added discounting, FX conversion or portfolio netting
   convention is implied. Each input retains its native/physical unit and report
   factor; reported copies scale columns exactly once. Cross-currency objectives
   require explicit user weights/conversions in the scripts or model contract.

10. **Budget admission.** Preserve existing numeric-payload, recording-capacity
    and scratch-capacity meanings. Weighted retained numeric payload is
    `sizeof(double) * (1 + n + 2*m)`; attribution payload is
    `sizeof(double) * m * (1 + n)`, using checked arithmetic. Runtime scratch
    includes all private evaluator/seed capacities, group model/path buffers,
    roots, result/batch buffers, task slots and growth overlap. Recording includes
    retained tape blocks/tails plus protected replacement admission. Preflight
    known shapes and guarded model startup before history. Initial group execution
    is sequential; workers remain parallel inside a group, avoiding simultaneous
    group peaks. A width may narrow only under the existing equivalent-budget
    policy, preserving the requested maximum and reporting actual widths.

11. **Failure and recovery.** Runtime capacity exhaustion and nonfinite required
    values/derivatives fail the whole request with trade/group/output/input and
    capacity context. Drain submitted tasks, restore native recording state and
    publish no partial result. Do not silently retry with changed RNG, smoothing,
    paths or estimator. Earlier completed results remain usable; a later valid
    request must succeed after preparation, worker or reverse failure.

12. **Capability boundaries.** Support the six model families accepted by the
    existing weighted/Jacobian drivers where complete grouping proof exists.
    A family or plan without sharing proof still executes in an isolated group.
    Exercise and fully expired products retain the existing multi-output
    rejection boundary, now naming the offending trade. Preserve current scalar,
    weighted, Jacobian and LSM entry points and their existing default paths.

13. **Bindings.** C++ returns owning passive values. Python copies and validates
    typed sequences before releasing the GIL; reject booleans as numbers, fractional
    path counts and lossy ID coercion. Excel retains immutable portfolio/request/
    result handles and typed/raw cell guards; getters trigger no valuation or
    fixing lookup. Exact zero-column matrix shape remains available separately
    from the one-blank-cell Excel spill. Generate wrappers from Machinist markup.

## Independent acceptance

- **Ownership oracle:** shared spot `S=100`, trade A `2*S + X_A`, trade B
  `3*S + X_B`, constants `X_A=5`, `X_B=7`, weights `(2,-1)` give component means
  `(205,307)`, objective `103`, shared-spot derivative `1`, and private constant
  derivatives `(2,-1)`. Attribution rows are `(2,1,0)` and `(3,0,1)`.
  Repeat with equal constant names/values; columns must remain private. Repeat
  with two distinct but numerically equal model owners; spot columns must split.
- **Common-path oracle:** compare every selected mean and derivative against
  independent existing single-product calls using the same frozen history,
  absolute indices, smoothing and native/passive mode. Use nonzero volatility,
  irregular different grids, overlapping fixings and reordered output selections;
  deterministic zero-volatility cases alone cannot prove RNG equivalence.
- **Sharing observation:** instrument actual model path generation and evaluator
  calls. Compatible trades generate one scenario per group/path/replay; different
  grids/owners generate separate scenarios. Check exact group memberships and
  identity scatter, not only equal prices. Force hash collisions if hashing is used.
- **Execution matrix:** tree/compiled, one/four workers, native/passive/empty
  native columns, aliases/constants/direct inputs, widths one/two/non-divisor/full,
  zero/negative weights, six supported model families and delayed payments.
  Weighted VJP must match a passive weighted sum of independent Jacobian rows.
- **History:** one captured date/snapshot, duplicate keys, different historical
  values/source policies, private scalar/vector seeds and live fixing slots.
  Reject malformed requests and impossible initial budgets before observed
  history reads/submissions. Verify caller mutation and result lifetime.
- **Failure:** checked shape/payload overflow, nonfinite outputs at zero weight,
  budget exhaustion at startup and during workers, task drain, no partial result,
  prior-result immutability and successful follow-up request.
- **Binding boundaries:** focused Python tests; Excel typed tests and actual
  generated Windows raw exports in all existing diagnostic/profiling modes;
  installed consumers, wrapper regeneration and static documentation checks.

Numeric tolerances follow the existing independent oracles: exact deterministic
values where justified, otherwise at most the established `1e-10` price/risk
tolerance for matching finite-sample programs. Do not replace derivative
assertions with shape checks or use changed RNG paths to make them pass.

## Performance and delivery gates

The old single-product path must gain no portfolio buffers, axes, string lookup,
per-node metadata or per-path selection branch. Additive drivers own portfolio
costs. Benchmark full preparation, cloning/history, simulation, reverse, reduction
and result projection for one trade, compatible many-trade, mixed-group and
heterogeneous-owner requests. Compare weighted aggregation and several explicit
Jacobian widths against independent single-product execution of the same work.
Record groups, generated scenarios, evaluator calls, replay counts and admitted
peaks; do not claim a speedup from theoretical reuse alone.

At a stable merge head, run the existing 160 no-regression cases with unchanged
two rounds, ten alternating process samples per side per round, best-of-N and
4% policy, retaining raw failures and source/binary provenance. Focused tests run
after each behavior change; broad matrices run once at the delivery boundary or
when a new failure/coverage gap justifies them. Require exact-head CI, Codacy
annotations, paginated review audit and a guarded merge. This PR remains
unmergeable until behavior, consumers and these gates are accepted.

## Implementation slices and remaining estimate

1. Sealed ownership/axes and passive compatibility groups are locally complete.
   Twenty-one new tests cover original-handle identity, serialization, all six
   model families, every sample field, fixing policies and private scalar/vector
   history over repeated paths in both evaluators. Two existing single-script
   risk tests also pass. The foundation repair head `0fffbf74` passes all 35 CI
   checks, zero Codacy annotations and zero unresolved review threads. Newer
   preparation code still requires its own publication-head checks; portfolio
   execution and comparative performance acceptance remain pending.
   Internal move-only plans now defer history until every trade is planned and
   the whole-request admission callback returns. The producer captures one date
   and one union fixing snapshot, completes private trade state and constructs
   groups from the original owners/meshes. Fourteen new cases verify ownership,
   mutation isolation, source provenance, early rejection and failure recovery.
   This callback is an admission boundary, not an implemented budget policy.
2. Shared weighted group recordings now register each model leaf once, retain
   private constants/history/evaluators and reverse one suffix per path and one
   prefix per batch. Nine focused cases pass in both evaluators, including the
   analytic ownership oracle, nonzero-volatility independent absolute-path
   batches with both RNGs/bridge settings, reordered aliases, private vectors,
   zero-weight nonfinite outputs, weighted overflow and failure recovery.
   Actual scenario/evaluator/reverse counters are observed inside their loops.
   The internal replay coordinator now scatters shared inputs across incompatible
   meshes and distinct owners, preserves requested coordinates and normalizes
   once by the original path count. Seven public-layer internal tests verify
   one/four workers, selected-input validation before submission, empty native
   columns, skipped groups and submission/worker/derivative failure recovery.
   All six model families match every independent existing scalar-call risk in
   tree/compiled evaluation; BS also covers both original RNGs and mixed meshes.
   Internal weighted request planning now owns global selection/weights/factors
   and the exact checked retained numeric payload before history or tasks.
   Five new cases cover defaults, caller lifetime, repeated ordinals across
   distinct owners, aliases/zero weights, native-empty/passive shape and early
   malformed/budget rejection. Native weighted startup and aggregate runtime
   recording/scratch budgets are now verified. Public C++ requests/results own
   selected/complete axes, component/objective means, detached report matrices and
   resolved trade/group provenance. Native-empty versus passive-sharp execution,
   caller mutation, report overflow and failure recovery have public tests.
   Passive shared execution and known private-vector admission also pass
   independent single-script prices for all six supported families. Weighted
   failures retain group trades and selected output IDs without changing paths.
   Strict warning categories pass in OFF and combined ON syntax checks; this
   does not establish linked/runtime ON acceptance. The weighted/public C++
   increment is locally implemented; complete publication acceptance is pending.
3. Add blocked attribution and owning C++/Python/Excel surfaces with independent
   common-path and generated-export acceptance. About 1.5–2 person-days.
4. Review, focused repairs, complete performance/platform gates and current-state
   documentation at a stable head. About 1 person-day.

Remaining estimate is 3–5 person-days, including uncertainty in shared recording,
aggregate capacity admission and binding integration. This is
single-developer effort, not a calendar commitment or a merge-acceptance claim.

## Open implementation decisions

- Sealing now uses the supported factory's parameter validation and exact JSON
  round-trip snapshots, with original-handle registration before cloning. Binding
  construction must preserve these identities before converting its inputs.
- The full-equality planner compares every `SampleDef_` field, ordered bindings,
  observation key/value/slot meaning, settings and initialized model dimensions.
  Its views now come from a producer owning one sealed registry and path count.
  The six exact factory model types admit grouping only after full plan equality;
  future/unproved types remain separate by default. Actual shared-path risk
  acceptance still requires the independent model-family execution oracles.
- Planning and completion are split without changing existing preparation
  entry points. Whole-request admission precedes one union history capture,
  retaining original explicit/global provenance. Connect aggregate recording
  and scratch policy to this boundary, including every private evaluator/seed.
- Choose lookup acceleration only after measuring preparation cost. Start with
  deterministic full comparisons; a hash is not a compatibility certificate.

None of these decisions authorizes a change to path/coordinate identity, budget
scope, result units, performance policy or the unsupported exercise boundary.
