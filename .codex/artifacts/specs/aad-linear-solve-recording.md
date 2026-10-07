# F03 native recorded linear solves

Status: active second F03 increment, based on merged numeric PR #490,
`a08d20e02396ef89629df3edb869f81bca0033f9`.
Source: the controlling detailed AAD plan, F03, C.8 and H.1/H.2, and the user's
native-only, no-regression and sequential-PR requirements.

## Problem and delivery boundary

The accepted numeric operator owns LU and X and computes dense input
contributions. It cannot compose with ordinary native expressions: native Tape
currently traverses only scalar nodes, and native capabilities correctly report
`reverseEvents_ = false`. This increment integrates the solve into a recording,
including scalar/vector sweeps, aliases, independent caching, checkpoint release,
failure invalidation and accounted retained storage.

Keep the full plan intact. Symmetric/banded coordinate maps, residual and
condition diagnostics, implicit calibration/PDE, sparsity, long-path
checkpointing, second order and valuation/binding requests retain separate
acceptance. A pivot policy is not a condition estimate. No integration claim is
made before actual event, numerical and resource acceptance.

## Mathematical and input surface

For AX = B, each adjoint channel independently uses
A-transpose Lambda = W, bar-B += Lambda and bar-A += -Lambda X-transpose.
All RHS columns contribute. Active matrices use independent dense entries;
multiple entries may alias one native slot and their contributions must add.
Passive A or B has no adjoint destination. A passive A uses the transpose solve
without constructing a dense matrix gradient. Numeric parameters and rejection
policies match the accepted operator.

The proposed core `Dal::AAD::LinearSolve` takes an owning-thread
`RecordingScope_*`, A, B and optional relative pivot tolerance. There are three
overloads: both active, passive A/active B, and active A/passive B. It returns a
new `Matrix_<Number_>` of outputs. A fully passive operation remains the numeric
API. No external AAD, implicit activation or nested independent scope is added.

## Numbered requirements

1. Require the supplied default-tape scope to be RECORDING on its owner thread.
   Reject null/wrong-thread/wrong-state use, invalid shapes, non-finite values,
   unsupported numerical range and invalid pivot tolerance with operation context.
2. Snapshot numeric values and input slot bindings before output publication.
   Own the immutable numeric cache and output bindings in the recording; retain
   no reference into caller matrices. Copying/reassigning/destroying caller
   containers does not retarget a previously recorded operation.
3. Record n*m ordinary zero-edge output slots and one event, after all input
   producers and before later consumers. Never record elimination as scalar
   arithmetic or add a virtual dispatch/tag to every native node.
4. Preserve existing scalar and specialized vector propagation loops between
   events. Keep a lazy per-tape event owner and select the empty-event path once
   per interval sweep. Ordinary node storage and allocation remain unchanged.
5. For a half-open scalar window [begin,end), execute events whose output-end
   ordinal obeys begin < boundary <= end, in reverse order. Propagate consumers
   down to the event's canonical end iterator, run its pullback, then continue
   through output slots and earlier producers. Canonicalize block-end positions;
   an event cannot straddle a checkpoint.
6. Collect every output seed in the current scalar/vector mode. Process every
   channel at width one, two, three, four and supported larger widths; unused
   channels remain zero. Exact zero alone may skip computation. Never truncate
   small gradients or skip NaN/Inf as though zero.
7. Compute complete numeric contributions before scattering that channel.
   Sum aliases across A and B, existing scalar contributions, shared solves and
   multiple RHS. Consume event output seeds, retain ordinary independent-input
   accumulation, and keep factors/X for later sweeps. Clearing adjoints clears
   slots, not the numeric cache.
8. Mark snapshots event count along with the existing scalar mark. Suffix
   restoration releases only suffix events/caches before rewinding storage.
   Prefix caches survive repeated suffix replacement. Clear, complete rewind,
   scope close and thread teardown release all events exactly once. Replacing
   checkpoints retains the existing token generation and owner rules.
9. ReverseToStart, ReverseSuffix and ReversePrefix, including their raw native
   interval functions, obey the same event ordering. No special reverse API is
   required to obtain the right mathematical derivative from recorded outputs.
10. Check event bindings against owner, mode, interval and live storage before
    using them. Event-created bindings cannot outlive restoration/reset.
    Diagnostic ON additionally validates source Number epochs/generations at
    capture. Diagnostic OFF retains the existing raw Number lifetime contract;
    it cannot detect a stale caller Number whose address has already been reused.
    Foreign live slots and missing nodes must still reject in either mode.
11. A reverse/event-construction failure invalidates the graph. Reject subsequent
    event sweeps and documented adjoint reads until reset; do not publish partial
    gradients. Scope failure/cleanup retains its existing exception behavior.
    The next independent scope must recover. Callback code must not record,
    reset or change the mode while reversing; detect structural mutation and
    invalidate rather than continuing with changed boundaries.
12. Cache/descriptor/event-table capacities and reverse numeric scratch are
    explicit resource measurements. Finite tape-capacity scopes account for
    retained event payload before allocation, or reject the operation before
    publication until such admission exists. The increment is not accepted with
    a silent cache-budget bypass. Retained/released charges match actual capacity.
13. Default scopes allocate no event owner. Keep Number and TapNode layout and
    normal AllocateNode hot paths unchanged. Tape is a single-owner graph;
    uniformly reject its unsafe copying/moving as already required by diagnostic
    builds. Existing supported scope and raw scalar APIs remain source-compatible.
14. Enable `reverseEvents_` only with scalar/vector/lifecycle acceptance. Publish
    current-state docs after implementation; retain later unsupported capabilities
    as false. No claim of general user callbacks, arbitrary solver domains,
    structured coordinates or higher-order differentiation follows from this API.

## Executable acceptance

- First RED: a 1x1 active solve with scalar producers A=2q, B=p*p and consumer
  V=3X*X+2p. At q=2,p=3, assert X=2.25, V=21.1875,
  dV/dp=22.25 and dV/dq=-15.1875 at abs 1e-10. Verify consumed output seeds
  and a second negative seed without rerecording.
- Nonsymmetric pivoted 3x3 with two RHS: compare every active coordinate against
  the independent native scalar reference and central differences at
  h=1e-5, h/2 and 2h, abs/rel 2e-8. Include scalar producers/consumers.
- Compose two serial solves and parallel solves sharing inputs; repeat with
  aliased A/B entries and multiple consumers/direct output seeds. Match the
  independent directional identity and retained input accumulations.
- Repeat scalar/vector channels at widths 1,2,3,4 and one larger specialized
  width with distinct positive/negative/zero seeds; compare scalar columns.
- Checkpoints before and after events: suffix/prefix reverse, repeated restore,
  rerecording and replaced-token rejection. Include exact node-block boundaries
  and multiple output blocks. Test-owned cache counters observe exact releases.
- Caller container mutation/destruction, missing/foreign slots, non-finite seeds,
  numerical/contribution overflow, allocation/admission failure and deliberate
  reverse mutation exercise invalidation and a succeeding independent scope.
- Retain/release/budget measurements match actual cache/vector capacities, with
  finite budgets at/under the required payload and no partial output publication.
- Select affected local filters during implementation. At the stable head,
  inspect actual OFF/ON/vector/profiling Windows and sanitizer logs, all applicable
  CI/Codacy/review, two complete audits and SHA-guarded publication.
- Compare against accepted #490 for existing complete-request/native workloads
  under the unchanged two-round paired 4% policy. Select the smallest affected
  caller/boundary set under the project-wide 2026-10-07 scope rule; a shared tape
  change does not require every target or parameter combination. Fresh affected
  binaries need measurements; reuse accepted evidence only after verifying
  immutable provenance and executable identity.
  Measure complete active solves and repeated reverse, owning result extraction,
  cache cleanup and peak storage against the scalar reference. Retain failures
  and noise; a noisy run does not prove acceptance.

## Open decisions and implementation order

No user choice blocks development. Deliver one PR with an initial analytic RED,
event/solve GREEN, alias/vector/checkpoint/failure/resource increments, local
review, stable performance and exact-head CI acceptance. The implementation may
start with one path while growing under tests; it cannot mark recording
integration complete before every requirement above is verified.
