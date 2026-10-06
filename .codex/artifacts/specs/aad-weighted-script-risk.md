# Weighted prepared-script risk: F02 first delivery

Status: active contract for the new F02 PR, based on merged #480 (`079c9d52`).
The native root, preflight, prepared C++ valuation and Python boundary pass
focused OFF and combined checks. Excel, cost acceptance and final publication
CI remain open. This specification covers
the fixed-weight part of F02; blocked Jacobians, portfolio preparation and the
complete plan remain separate.

## Source and problem

The controlling detailed AAD plan, sections 4.2, 7.1, B.2 and C.6, distinguishes
one weighted gradient from a complete output-by-input Jacobian. The user requires
native AAD only and subsequent development in new PRs after #480 is merged.

Current source boundaries:

- `dal-cpp/dal/script/event.hpp` already exposes indexed scalar `VarNames()` and
  the default `PayOffIdx()`; another mutable output registry is unnecessary.
- `dal-cpp/dal/script/preparation.hpp` seals the product, observation plan,
  historical state, compiled program and simulation settings.
- `dal-cpp/dal/script/simulation.hpp` materializes a path-local `PayoffRoot`,
  reverses the suffix per path and the accumulated prefix once per batch.
  Its payoff sum is normalized later; its parameter gradients are already means.
- `dal-cpp/dal/script/riskresults.hpp` and `.cpp` supply passive owning scalar
  results, stable input IDs, report-only scales and overflow-safe numeric budgets.
- `dal-public/src/riskvalue.cpp` validates scalar requests before history/workers,
  checks prepared axes and records actual execution provenance.

Scalar structured requests reject outputs other than `payoff`. Repeating a
valuation for each output and summing gradients repeats forward work. A fixed
weight vector permits one scalar objective and one reverse per path.

## Goals and delivery limits

Deliver native weighted risk for several scalar outputs of one prepared,
non-exercise script, with C++/Python/Excel parity, independent numerical oracles
and unchanged legacy single-output cost/behavior. Retain complete component
identity and passive weights with the result.

The first delivery excludes vector-valued slots, EXERCISE/LSM, fully expired
products, Jacobian blocks, cross-trade timeline merging, adaptive workspace
reuse, quote-weight differentiation and second order. Unsupported requests fail
explicitly. These cases remain in the full F02/later-stage ledger, rather than
being represented by zero-filled successful results. F01 scalar quote-request
semantics remain; weighted quote integration needs its own explicit contract.

## Inputs and outputs

Inputs are a product/model snapshot, positive path count, immutable valuation
and simulation settings, an ordered optional output-ID selection, finite passive
weights, the existing input-ID/report-scale choices and a numeric payload budget.

Publish an owning passive output-axis query. The default receiver retains ID
`payoff`; other indexed scalar slots use `output:<ordinal>`. Keep the actual
variable name as a separate label and the slot ordinal as identity metadata.
No vector slot becomes a scalar output by accidental numeric conversion.

The result owns the ordered component IDs/labels/slots, weights, component means,
weighted mean, a `(1, selected-input-count)` raw gradient, selected/complete input
axes, reported gradient and exact execution/source provenance. It exposes no
active number, checkpoint, borrowed evaluator or mutable caller buffer.

## Numbered requirements

R01. Use additive weighted request/result entry points. Existing scalar overloads,
default settings, result keys, errors and `RiskResultPayloadBytes(1,n)` remain
compatible. Their ordinary execution must not construct weighted metadata,
component arrays, extra roots or matrices only to discard them.

R02. Resolve output IDs using DAL `String_` semantics before preparation/history
resolution or worker submission. Omission selects the default payoff; explicit
empty, repeated or unknown selections fail. Preserve caller order. Reject a
missing/default-receiver sentinel, unsupported slot kind or unsupported product.
Check that indexed output identity remains equal after preparation.

R03. Omitted weights mean one for every selected component. Explicit weights have
exactly the selected output count. Negative and exact-zero weights are valid;
NaN/infinity, wrong rank/type or dimension mismatch fail before work. Copy caller
choices before callbacks or a Python GIL release. Weights do not participate in
the active input axis; a market-dependent holding must be part of the script.

R04. The requested finite-sample objective is
`mean_p(sum_i(weight_i * final_scalar_slot_i(p)))` for the sealed prepared
program and requested RNG/path range. A variable is its actual script value;
do not discount it again or invent PV/currency units. Preserve the existing
payoff meaning when the sole selected output is `payoff` with weight one.

R05. Evaluate the prepared forward program once per path. Materialize the
weighted expression as one path-local root using the existing prefix/suffix
boundary. Do not overwrite adjoints for different slots that alias one active
node. Negative, zero, constant, parameter and pre-checkpoint outputs must all
respect the same recording lifecycle.

R06. Start each batch with compatible registered model/script parameters and a
fresh checkpoint. Restore the suffix per path, seed the weighted root once,
reverse the suffix once per path, preserve prefix accumulation and reverse the
prefix once per batch. No leaf/seed from a preceding request may remain. Follow
the existing exception recovery and task-drain contract.

R07. Preserve paths, RNG/BB, smoothing, compiled/tree behavior, batch planning
and ordinary aggregation order. Component/weighted values are means. Native
gradients already normalized in the batch driver are not divided by path count
again. Explicit no-input selections retain native payoff semantics.

R08. Native and price-only settings stay explicit. Price-only weighted evaluation
has a `(1,0)` gradient and rejects nonempty risk input requests. Do not silently
replace a fuzzy native program with a hard/passive program when no columns are
extracted. No generic method name promises unsupported execution.

R09. Every retained requested component value must be finite, including components
with zero weights. Diagnose nonfinite weighted sums and selected raw/reported
risks with component/input identity. Never publish a partial result. A later
valid request must recover after forward, reverse, allocation or worker failure.

R10. Reuse stable input-axis validation and selection without merging display
labels or introducing another ordinal convention. Missing inputs select all
native coordinates; explicit empty selects none. Preserve input order, strictly
positive finite report factors and exact one-time reporting. Keep raw risks raw.

R11. First-delivery retained numeric payload is
`sizeof(double) * (1 + n + 2*k)`: one weighted value, `n` gradient entries,
`k` component means and `k` weights. Check each sum/product for overflow and the
exact budget before history/workers or numeric storage allocation. This is a
numeric payload budget, not a tape/worker/process memory guarantee. Scalar
requests keep their original formula; later blocked Jacobians add separate
enforced workspace/tape budgets.

R12. Provenance records actual component IDs, passive weights, ordered full input
axis, product/model snapshots, valuation/fixing policy, captured observations,
RNG/BB, paths, worker/batch semantics, smoothing and compiled/tree execution.
Identify weighted native or price-only evaluation honestly. No estimator error,
quote calibration or arbitrary currency grouping is inferred from variable names.

R13. Keep immutable output/result handles detached from temporary planning and
subsequent executions. All getters read stored passive data; repeated reported
getters do not rescale the raw result. Mutating/destroying caller input containers
after construction cannot change requests/results.

R14. C++, Python and Excel agree on shapes, axis identity, input order, numeric
payload, weights, raw/report scaling, errors and ownership. Python choices are
keyword arguments; matrices keep two dimensions. Excel raw guards reject
booleans, text, missing cells and incompatible shapes before generated numeric
conversion. Use shared binding helpers and Machinist for any new public enums
or worksheet surfaces; do not hand-edit generated files.

R15. Legacy `PV`/`d_...` projection remains the scalar contract. A new weighted
result is not labelled as the old default payoff merely because it has one row.
If an explicit compatibility projection is added, require the sole canonical
payoff output and weight one, and verify exact axes/normalization first.

R16. Keep numerical, lifecycle, output-selection and binding changes reviewable.
Do not introduce workspace reuse, vector-tape layout changes, block-width
heuristics or allocator optimization in the weighted-root implementation.

## Executable acceptance

A01. Analytic fixture: `f1=x*y`, `f2=x+y`, `f3=5`, `x=2`, `y=3`, weights
`(2,-1,0.5)` gives value `9.5` and gradient `(5,3)`. Validate raw/reference
values independently in native OFF/combined and tree/compiled modes.

A02. Distinct slots aliasing `x*y` with weights `(2,3)` give value `30` and
gradient `(15,10)`. Add a direct registered-input output, a literal constant,
historical/pre-checkpoint values, negative/zero weights and two consecutive
different weight requests. The second request must contain no first-request seed.

A03. Compare weighted native risk with independently priced component requests
and a common-path finite-difference objective on smooth script/model cases.
Retain the original oracle step and rel/abs `1e-10` policy where specified; no
post-failure step/tolerance change. Repeat requested output/input permutations,
one/four workers, report factors and all/subset/empty input selections.

A04. Validate unknown/repeated/empty outputs, wrong/nonfinite weights, unsupported
exercise/expired/vector cases, invalid paths/settings and exact/one-byte-short/
overflow budgets before observed history evaluation or worker submission.
Check zero-weight invalid component values and selected nonfinite risks.

A05. Inject forward/worker/reverse failure where supported by existing test hooks;
prove tasks drain, no partial handle escapes, recording ownership is restored,
and an ordinary next scalar/weighted request succeeds. Run affected diagnostic
tests and focused sanitizer coverage, rather than unrelated full local suites.

A06. Fresh C++/Python/Excel parity uses independent native fixtures and passive
copied results, not two wrappers calling the same unexamined implementation.
Check strict raw parsing, copied buffers, installed ownership, real GIL behavior,
generated registration and zero generated drift for the new surfaces.

A07. Compare existing affected scalar entries under the unchanged paired policy:
two rounds, ten alternating process samples per side, minimum reduction and
failure above 4% in both rounds. Freeze inputs; do not overlap sampling with
builds/tests/Git. Compare new weighted requests against equivalent component
gradient sums for 1/4/16/64 outputs; report forward/reverse counts, memory and
whole-request cost. New capability measurements are separate from old gates.

A08. Publish one stable new-PR increment, inspect its exact-head applicable CI,
paginated review threads/status contexts and requirement evidence. Do not mark
full F02, Stage B or the overall plan complete after weighted VJP alone.

## Implementation order and open questions

Resolve passive output identity/preflight first with independent failing tests.
Then add the scalar weighted root and compare analytic/common-path references;
then owning results/provenance and three-language boundaries. Preserve the old
single-output route and verify its affected costs before publication.

No user answer is required. Exact public type/function names and the shared
input-selection helper boundary are engineering choices to settle in the API
note before coding. Existing scalar quote requests and the subsequent blocked/
portfolio/tape-budget extensions remain explicitly controlled by their specs.
