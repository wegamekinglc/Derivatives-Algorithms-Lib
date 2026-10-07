# Selected portfolio gradient extraction and measured block policy

Status: implementation, 88 affected cases and calibrated paired performance pass.
Measured widths retain existing defaults/upper bounds; exact-head platform
acceptance and guarded publication remain.
Branch: `feature/aad-selected-extraction`, based on merged
[P02 #488](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/488),
`b7e69342d618b539ffc5d3964ed0d6ac2fd15934`. Its tree equals accepted
head `4a3229996fcafddd58fafc84f751d2e24bd1e166`: 35/35 checks, zero Codacy
annotations, zero unresolved threads and two guarded publication audits.

## Source and problem

The user authorized the complete native-only AAD plan without existing performance
or CI regressions, and requires separate PRs after accepted increments.
P02 is complete; the [implementation ledger](../plans/aad-implementation.md)
leaves selected extraction and measured width decisions in P03.

Baseline `ExtractGradients` in `dal-cpp/dal/script/portfoliobatch.cpp` reads every
model and private input into owning batch vectors/matrices. `VisitGradients` and
`AddGradient` in `dal-public/src/portfolioreplayinternal.cpp` discard unwanted
columns afterward. `AdmissionSlots` also reserves full gradients. A request for
one risk therefore retains numeric storage for many risks it never returns.

The baseline scatter infers the private-input axis offset from the model gradient's
column count. Packed selected gradients make that inference incorrect: selecting
one of four model inputs must not move the first private input from offset four
to offset one. Explicit original ordinals and packed positions must remain distinct.

## Goals and boundaries

Pack requested numeric gradients while preserving complete axes,
caller order, shared-owner contributions, private identities and owning results.
Keep full extraction for compact nonempty selections when the discarded scalar
gradient bytes do not exceed the fixed model/trade mapping-entry payload. Native
empty selection always packs zero columns. This deterministic pre-valuation cost
proxy protects the existing compact route; it does not promise minimum possible
scratch capacity for every selection or path count.
Begin with private implementation surfaces. Retain existing public C++/Python/Excel
requests and defaults. Register the full inputs required by the recorded model and
trades; this work does not prune the differentiation graph or change estimators.

Cross-request caches, structural sparsity, checkpoint redesign, second order and
hidden trial pricing/history are outside this PR. Measure widths after extraction
stabilizes. A new public performance policy requires measured benefit and a
separate documented API decision; absence of evidence preserves the existing policy.

## Inputs and outputs

Inputs are the sealed portfolio, original group/batch plan, ordered outputs and
selected complete-axis input positions, explicit maximum width and optional
tape/scratch budgets. Outputs retain means/objectives, selected gradients or
Jacobians, complete coordinate metadata, report factors and actual diagnostics.
Native-empty and passive zero-column results retain their distinct semantics.

## Requirements

R01. Build immutable mappings from complete axes to group-local model/private
ordinals outside block and batch loops. Admission and replay each admit their own
mapping under their respective capacity scopes; replay shares its mapping through
all blocks and batches. All-input selection
must have a compact identity route; explicit empty selection means zero numeric
columns. Neither may be inferred from an ambiguous empty index vector.
Compact nonempty selection may use the full-gradient reference route under the
documented discarded-byte/mapping-payload rule; public scatter remains selective.

R02. Extract each requested model/private column once per applicable batch/lane.
When packing is selected, do not materialize unrequested numeric columns.
Preserve every live trade's private state. Selecting a private risk of a trade that
does not contribute an output must produce zero without adding valuation/history.

R03. Scatter packed columns through their original ordinals and full model-input
prefix. Preserve requested order, distinct owners with equal values, shared-owner
summation and aliases. Never derive an original offset from packed matrix width.

R04. Retain model re-registration, historical reseeding, absolute RNG offsets,
original batch slots/reduction order and reversal counts. Gradient selection must
not change native smoothing or convert an empty native request into passive work.

R05. Admit mapping storage, selected numeric slots, worker buffers and replacement
headroom before history/submission. Continue accounting for full recorded inputs
and tape lanes. Preserve aggregate runtime quotas and overflow checks; allowances
must not be reported as measured peaks.

R06. Validate public coordinates against sealed complete axes before side effects.
Private mappings must be checked against their planned model/trade extents. Selected
nonfinite derivatives and report overflow fail with original coordinate context;
unselected derivatives must not become newly required.

R07. Retain `(m, 0)` native/passive matrices and detached results. Mapping references
live until all accepted tasks drain, including partial submission and failure.
The next request must recover independently on the same pool.

R08. Preserve explicit maximum width and capacity-only narrowing. More efficient
selected storage may make a wider width feasible; report the actual width and work.
Only capacity failures may narrow. Numerical errors must propagate unchanged.

R09. Keep the default all-input, weighted, passive and small-request routes within
the frozen regression rule. Keep the numerical batch kernel shared across entry
routes and avoid speculative dispatch layers or duplicated model kernels.

R10. Compare widths 1/2/3/8 with complete-request cost, actual paths/reversals and
peak capacities after extraction stabilizes. Any performance policy must be
deterministic, use pre-valuation information, retain an explicit upper bound and
project consistently into C++/Python/Excel. No hidden trial valuations are allowed.

## Executable acceptance and delivery

A01. Establish RED for a public resource contract before production changes: a
wide private-input portfolio requests a small reordered subset under a justified
quota. The full-gradient reference must expose the unwanted numeric storage.
Do not calibrate the assertion to the new implementation's own measured peak.

A02. Compare selected values/risks with independent complete-gradient/frozen
single-script references for all six native families, tree/compiled, one/four
workers, multiple original batches, widths 1/2/3 and padded tails.

A03. Cover a partial model prefix with private inputs from different trades,
shared/distinct owners, model-only/private-only/empty selections, aliases,
historical vectors, zero/signed weights and selected/unselected nonfinite risks.

A04. Exercise known-fit admission before history, finite aggregate quotas,
replacement headroom, submission/worker failure and retained-result recovery.
Demonstrate fewer numeric gradient bytes/read operations, not just fewer returned
columns. Preserve owning zero-column shapes and original work counters.

A05. Measure startup, full/default and sparse selections against merged #488,
including 257/8193/32785/131072 paths. Use two rounds of thirty alternating pairs,
minimum process observation and failure only above 4% in both rounds. Each process
warms one request, then separately times 256 complete public requests for compact
workloads with at most 1,024 paths; wide and larger workloads time one request.
The process observation is average duration per complete request, reduced with
minimum across processes. Both sides use one identical helper object. Every
request still prepares, admits capacity, initializes history, replays and builds
owning results; only the immutable sealed input is shared. Retain all constituent
request durations, failed runs and numerical/work checks.

The larger count and observation window are predefined before sampling because
the original ten/thirty-pair single-request and 32-request controls expose excessive
noise. Keep those original samples and controls in the evidence and ownership
design; do not rerun an unchanged candidate until it happens to pass. Both baseline
and head A/A absolute round deltas must stay within 4% before A/B acceptance starts.
If calibration fails, stop local sampling and use an independent environment.
Legacy timing reuse requires fresh-link and exact executable-hash proof.

A06. Run affected tests per increment and consolidate required compilers,
diagnostics, sanitizers, Windows, installed consumers and bindings at a stable
head. Fix all CI/Codacy/review findings, audit the final head twice and merge with
the SHA guard before the following phase.

Delivery: resource RED and mapping design; selected core extraction plus ordered
public scatter/admission; six-family and recovery coverage; width measurements;
stable-head acceptance. Estimate: 3–5 developer days, excluding CI queue time.
No user clarification blocks the existing private optimization scope. Exact mapping
ownership and the independent quota fixture are the first design/TDD tasks.
