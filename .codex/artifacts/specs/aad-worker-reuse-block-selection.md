# Native AAD worker reuse, block policy and selected extraction

Status: P02 worker reuse is implemented locally in
[#488](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/488).
Focused numerical/resource tests pass; performance and final-head platform
acceptance remain open. P03 extraction/block policy follows in a new PR after
#488 merges. Delivery follows accepted portfolio PR
[#487](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/487),
merge `bf52e3868a3e72a5f43b8c023a12aeee095a46cd`. Its tree equals accepted
head `47c33a12`: all 35 checks and both publication audits pass. Actual Windows
runtime passes seven portfolio cases in each of four modes; six sanitizer jobs
pass all 78 portfolio cases. Existing performance coverage accepts 160 cases.

## Source and problem

The user authorized the complete native-only AAD plan without existing
performance or CI regressions, with later development in new PRs. The
[implementation ledger](../plans/aad-implementation.md) leaves P02 worker
capacity reuse and P03 block policy/extraction outstanding.

The accepted #487 baseline submits one task per path batch and repeats scheduling
for each output block. Its native batch runner constructs a model, RNG, Gaussian
buffer, scenario, private evaluators and output storage per batch. The P02 change
in `dal-public/src/portfolioreplayinternal.cpp` bounds jobs by worker count; the
runner in `dal-cpp/dal/script/portfoliobatch.cpp` retains those buffers per job.
Each original batch still records its prefix, restores each path checkpoint,
reverses each suffix, reverses the prefix and extracts gradients independently.
Existing single-product value-only workers reuse request-local buffers;
see [batching](../../../docs/methodology/script_engine.md#batching-and-thread-pool).

Accepted [portfolio cost evidence](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/47c33a12b142af8aec9532b0c7c1084fd9f064a6/.codex/artifacts/performance/aad-compatible-script-portfolio.md)
contains 130 matched-work cases. Native BS compiled/four-worker weighted calls
cost 268.770 μs for one trade versus 108.330 μs independently; eight compatible
trades cost 697.099 versus 732.113 μs. Distinct owners cost 1501.415 versus
665.168 μs. Full-entry timing does not identify which phase dominates.

## Goals and boundaries

Reduce repeated allocation/setup and unwanted gradient extraction. Preserve
sealed ownership, original meshes, RNG dimensions, absolute paths, private state,
estimators, reductions and detached results. Keep known admission before history
and submission, aggregate runtime budgets and failure recovery.

Start with private implementation changes. Preserve native-only C++/Python/Excel
defaults. Cross-request caches, mutable const evaluators, approximate truncation,
structural sparsity, checkpoint redesign and second order are outside this slice.
An optional public width policy needs an API decision before implementation.

## Inputs and outputs

Inputs remain sealed products/models, resolved groups, original batch ranges,
ordered selections, maximum width and optional tape/scratch allowances. Outputs
retain owning means/objectives/gradients/Jacobians, selected/complete axes, report
factors and frozen provenance/actual diagnostics. Native-empty and passive
zero-column semantics remain distinct. Getters perform no history or valuation.

## Requirements

R01. Construct worker state on its executing thread and bound it to the request
and relevant group/block. Drain tasks before destruction. Active numbers,
checkpoints and recording guards never migrate across threads, including inline
caller execution. No hidden state survives into another request.

R02. Reuse Gaussian, scenario, evaluator/compiled-stack and numeric workspace
capacity across compatible batches. Preserve private state per trade and separate
owners/meshes. Refresh every path-dependent field; capacity reuse is not reuse of
old numerical values, gradients or seeds.

R03. Retain original batch boundaries and absolute RNG offsets. Reposition the
RNG per assigned batch and preserve ordered batch-slot reduction and single
normalization. Initially retain per-batch prefix reversals; combining prefix
seeds across batches requires a separate numerical/reduction proof.

R04. Native model reuse requires parameter re-registration and complete derived
state initialization after reset. Refresh historical aliases, scenario slots,
adjoints, compiled stacks and tail lanes. Unproved paths retain fresh model
construction. Cover all six families before claiming general native reuse.

R05. Bound concurrent state by the admitted worker count. A worker job may handle
multiple original batches, but each original result slot has one writer. Account
for actual task handles, retained capacities and replacement headroom. Recording
guards close on the same thread that constructs them.

R06. Admit known result/task/root/private-history/worker capacities before reads
or submissions. Reuse cannot evade accounting or report allowances as measured
peaks. Handle native-empty, passive execution, short final batches and output tails.

R07. Submission rejection, model/evaluator failure, invalid selected derivatives
and budget exhaustion drain accepted work. The next request behaves like a fresh
request; old owning results remain valid. Preserve group/trade/output error context.

R08. Preserve explicit maximum width and capacity narrowing by default. Eliminate
infeasible widths using known-fit admission; report actual widths/attempts/work.
Only capacity errors may trigger narrowing. Numerical errors must propagate.

R09. Measure widths 1/2/3/8 and supported boundaries after reuse stabilizes.
Include recording, replay, reverse, reduction, extraction, task overhead and peak
capacity in complete request cost. Cover startup and multi-batch throughput.

R10. A performance-driven width policy needs a measured objective, stable default
and API review. New arguments or metadata semantics require C++/Python/Excel
parity and generated exports. Hidden trial valuation/history or additional pricing
paths are prohibited. An explicit maximum remains an upper bound.

R11. Build local-to-selected extraction mappings once. Read/reduce only requested
model/private columns where safe, retaining caller order, shared ownership,
aliases, complete metadata and detached shapes. Selected nonfinite risks fail;
unselected derivatives must not become newly required.

R12. Account for selected buffers before history and guard extent/report-factor
overflow. Preserve zero-column matrices. Complete-axis metadata does not require
materializing every numeric gradient column.

## Executable acceptance

A01. Establish RED before behavior changes using an independent setup/resource
bound plus numerical/work assertions. Tests must not only mirror a new helper's
structure. Existing fresh-batch execution is a reference for reusable workers.

A02. Compare every selected risk with independent frozen single-script calls
for BS, CorrelatedBS, Hybrid, GSR, MultiFactorGSR and GSRSLV. Cover tree/compiled,
one/four workers, widths 1/2/3, shared/distinct owners, meshes and private history.
Keep existing numerical tolerances.

A03. Exercise multiple batches per worker, short final batches, multiple output
blocks, aliases, sparse private vectors, zero tail lanes, signed/zero weights and
reordered selections. Verify original paths and actual scenario/evaluator counts.

A04. Alternate success, rejection and failure on the same pool while changing
groups, shapes, widths, models and finite quotas. Validate retained results after
recycling/destruction. Run lifetime/profiling/combined, ASan/UBSan and TSan modes.

A05. Measure 257 paths and larger multi-batch sizes such as 16,384 and 131,072,
including one trade, compatible groups, mixed meshes, owners and private history.
Retain phase/setup/allocation observations, actual work/bytes and raw durations.
Separate resource correctness from noisy timing.

A06. Preserve nine-target and relevant production gates: two rounds, ten
alternating pairs, minimum, failure only above 4% twice. Reuse accepted timing
only with fresh-relink proof of unchanged binaries/workloads. Retain all failed
or inconclusive runs; repeated steady-state timing cannot replace original requests.

A07. Use focused affected tests per increment. Consolidate full required compilers,
diagnostics, installed consumers, wheels and Windows runtime at the stable head.
Inspect Codacy annotations and paginated review threads twice before merging.

## Delivery sequence and estimate

1. Establish phase/setup observations and one failing reuse test.
2. Reuse private worker capacity while preserving batch reductions and recording
   reset semantics; add lifetime, budget and recovery cases.
3. Complete native initialization coverage across all six families and bindings.
4. Add selected extraction with resource and independent numerical acceptance.
5. Measure widths; add policy only if evidence/API decisions support it. Review
   and fix all final-head checks before the next delivery.

Remaining P02/P03 effort: approximately 4–7 developer days, excluding CI queues.
Re-estimate after phase measurements and native reset proofs. No user clarification
blocks progress. Observer availability, model reset completeness and width-policy
benefit are implementation and measurement questions.
