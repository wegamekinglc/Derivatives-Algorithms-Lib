# Selected extraction ownership and admission

Status: active implementation design for [P03](../specs/aad-selected-extraction-block-policy.md).

The resource RED uses two live trades with 1,024 referenced private constants each,
a shared four-input BS owner and 1,048,577 paths in 129 original batches on one worker.
The full-input request supplies the reference capacity. The selected request asks
for three reordered columns and receives a quota smaller by half of the retained
batches' discarded numeric columns. Against reconstructed merged #488 production
it rejects in preflight: required 1,984,448 bytes, quota 1,981,104 bytes. The quota
never depends on the selected request's own peak. Full production sources, object
and executable hashes, commands and the failing log are retained under
`aad-selected-extraction-resource-baseline-03` in the session evidence directory.

The earlier five-batch fixture also rejected after extraction changed because
complete-axis metadata construction exceeded its quota. The revised fixture makes
retained numeric slots dominate that independent metadata floor. Every constant
participates in the historical prefix, because indexing removes unused declarations.
This also keeps forward path work small enough for focused tests. Both earlier
failures remain in the evidence directory.

`PortfolioInputColumns_` records the full source extent and either compact identity
selection or strictly increasing original ordinals. Explicit empty ordinals select
no numeric columns. `PortfolioGradientSelection_` owns model selections by owner
and private selections by original trade position. Complete global selection uses
the full-gradient route without allocating extra mappings, including reordered
requests for the complete axis. `ReplaySelection_` owns subset mappings
for the entire replay, including task draining after failures. Preflight constructs
its own mapping before history under its admission capacity scope. Runtime replay
reconstructs and charges its own mapping; it does not retain allocations attached
to the ended admission scope. The specification makes this two-phase boundary
explicit rather than promising one construction for the entire public call. Batches
retain only numeric arrays and original trade positions; they do not copy mappings.

Derive the full model prefix from sealed complete-axis families. The existing
prepared-axis validation checks those axes against actual models; preflight also
checks their extents against planned models. Keep full source extents for private-axis
offsets and validate core selections against actual model/private extents before
historical initialization. Scatter packed positions via original ordinals and the
complete model prefix into existing ordered public columns.

`UsePackedGradients` protects compact nonempty sources using pre-valuation sizes.
Pack when `(complete inputs - selected inputs) * sizeof(double)` exceeds
`(model owners + trades) * sizeof(PortfolioInputColumns_)`; explicit native empty
selection always packs and complete selection always keeps identity extraction.
This conservative scalar-payload proxy uses no trial valuation and does not claim
minimal capacity for every batch count or block width. Both admission and replay
choose the same layout and charge its actual buffers. Compact requests return
only selected columns although their private batch arrays retain full gradients.

Private batch settings group scratch/tape budgets and the borrowed selection
pointer. A missing pointer preserves the full-gradient core reference route.
Private worker-admission settings group quotas, width, owner and live original
trade positions with the same borrowed mapping. This avoids extending already
long positional argument lists. Public request signatures and defaults are unchanged.

Preflight charges owning mappings, selected batch slots and worker replacement
arrays. Model/evaluator inputs, recording registration and tape estimates remain
complete. A block with no selected private columns still executes its live trades;
a selected risk of a trade with no selected output remains zero without additional
valuation or history. Native empty selection retains native work and matrix rows.

Local acceptance passes 41 core and 47 public affected cases. The resource fixtures
passes tree/compiled without altering its bound. New packed-array shape and extent
rejection/recovery tests preserve native zero-column rows; all six native families
match independent scalar risks across original batches, 1/4 workers and widths
1/2/3. The three-row Jacobian resource fixture retains width two, including its
padded tail and historical prefix alias, while returning only three columns.
Full P03 platform acceptance remains; the measured-width decision appears below.

Four early default-route performance cases retained one failure: the 257-path,
single-trade compiled weighted request regressed by 9.41% and 5.23% in two
confirmation rounds. Raw evidence remains in `aad-selected-extraction-initial-pairs-01`.
The corrective change bypasses subset-map and original-trade admission allocations
for complete selection. Fresh verification uses a new candidate executable;
no unchanged binary is rerun to replace failed evidence.
The revised candidate passes all four initial cases under the unchanged policy.
Round deltas are 4.73%/1.14% (small single trade), 15.29%/3.69% (small shared
portfolio), 4.76%/-2.25% (large width-one Jacobian) and -0.72%/-2.84% (large
passive weighted). None exceeds 4% in both rounds; variability remains visible.
This is partial evidence, not complete performance acceptance or a uniform speedup.
The frozen logs and binaries remain in `aad-selected-extraction-identity-pairs-01`.

## CI follow-up and current evidence

First code head `7021ec54` receives one Codacy annotation: `VisitGradients`
complexity 10 against limit 8. The local correction shares the two packed-to-original
ordinal conversions through `OriginalInputOrdinal`; numerical selection is unchanged.
All 46 affected public cases pass after this correction. The fresh executable
passes all 37 default-route paired cases against accepted #488 under the unchanged
policy. Logs remain in `aad-selected-extraction-codacy-default-pairs-01`; sparse
request and measured-width acceptance still remains.

All four Windows modes crash in the two new resource
fixtures while initializing their 1,024-term left-nested sum. A 128 KiB Linux
stack reproduces the crash with exit 139. The revised fixture uses a balanced
sum of the same 1,024 private inputs and passes both tests under that same stack
limit. Quotas, paths, source extents and all numerical assertions are unchanged.
Production parser/interpreter behavior and CI stack settings are unchanged.

The balanced fixture is freshly linked against the frozen #488 production
archives: the weighted request still rejects at 1,984,448/1,981,104 bytes, and the
Jacobian still narrows to widths `{1,1,1}` instead of required `{2,2}`. Both fail
as intended; current production passes both. The new baseline proof and commands
remain in `aad-selected-extraction-balanced-baseline-04`. Actual final-head Codacy
and Windows reruns must confirm these local corrections before merge.

## Performance corrections

The complete initial sparse/empty/width matrix passes 48/50 cases. Two large
four-worker compiled sparse cases fail the frozen two-round rule: weighted
4.99%/5.72%, and width-eight Jacobian 6.43%/5.44%. All samples remain in
`aad-selected-extraction-selected-pairs-02`; the default-route 37/37 result does
not accept these separate sparse workloads.

Moving extraction out of line is rejected: the fresh candidate's first weighted
case still regresses by 7.80%/6.36%. Its binary, source patch and raw samples remain
in `aad-selected-extraction-outlined-selected-pairs-03`. The compiler attributes
on extraction are removed. They do not form part of the implementation.

The next correction reserves the known live-trade count in the owning private
gradient array before harvesting. Eight trades now need one outer-array allocation
instead of repeated growth; the recorded model/path kernel is shared as before.
All 41 core and 46 public affected tests pass. Thirteen freshly linked legacy
executables again match their accepted hashes; proof and commands remain in
`aad-selected-extraction-reserved-link-proof-05`. New complete-request sampling
uses fresh immutable binaries and also records actual prefix/suffix reversals.
It must pass before final-head CI and merge acceptance.

Reservation alone also fails the first large parallel weighted case: 8.83%/11.99%
with ten pairs, and 7.25%/7.08% in a predefined thirty-pair extension. The original
and extended logs remain in `aad-selected-extraction-reserved-selected-pairs-04`
and `aad-selected-extraction-reserved-selected-pairs-30-05`. Same-binary calibration
shows ten-pair minimum differences up to 11.65% on this WSL host; the extension
retains the four-percent, two-round threshold and every initial result. Calibration
does not excuse the confirmed extended regression.

The next candidate gives the shared numerical path loop its own compilation
boundary through a synchronous `NativePathLoop_` functor. It borrows the same
batch, checkpoint, model/evaluator buffers and owning result; harvesting remains
once per original batch. It introduces no graph, seed, RNG or reduction changes.
Local acceptance again passes 41 core and 46 public cases. All thirteen freshly
linked legacy executable hashes remain identical, with proof under
`aad-selected-extraction-path-loop-link-proof-06`. Its predefined thirty-pair
complete-request acceptance rejects its first case at 14.87%/10.67%. Its source,
binary and raw samples remain in `aad-selected-extraction-path-loop-selected-pairs-30-06`.
Both the reservation and path-loop experiments are rolled back; production now
matches published `e425e52d`. Further corrections require actual CPU/hotspot
profiling, not another speculative compilation-boundary change.

Informational CPU histograms at 4,194,304 paths show the existing compiled-event,
recording and expression kernels as the major costs; they do not establish a new
numerical hotspot or replace paired acceptance. Sampling provenance, frozen
published-production archives and raw profiles remain in
`aad-selected-extraction-profile-hotpath-01`.

The compact-source correction also fixes an independently demonstrated capacity
regression. A new eight-trade test derives a known-fit quota only from complete
requests, then submits a reordered two-column subset. Published `e425e52d` rejects
at required 43,692 bytes versus quota 43,684. The new compact route passes both
tree/compiled at that independently derived quota. Frozen fixture/object/binary
hashes and RED/GREEN logs remain in `aad-selected-extraction-compact-quota-proof-01`.
All 47 public cases pass and thirteen fresh legacy links retain their accepted
hashes under `aad-selected-extraction-compact-link-proof-07`. Complete thirty-pair
sampling against merged #488 accepts all 50 sparse/empty/width cases. Its default
matrix passes 35/37: small compiled four-worker width-one native Jacobian regresses
4.70%/4.94%, and the passive Jacobian regresses 4.32%/4.36%. Both complete result
sets remain in `aad-selected-extraction-compact-selected-pairs-30-07` and
`aad-selected-extraction-compact-default-pairs-30-06`.

The corrective follow-up restores the original native-subset outer guard before
calling the packing classifier. Passive and complete-axis requests do not invoke
that classifier; only native subsets use the byte/payload proxy. A fresh candidate
must pass the two failed default cases first, then the remaining complete matrix.

The subset-only candidate passes those two priority cases, but its sixth default
case (257-path compiled four-worker full-input weighted request) fails at
9.03%/8.12%. Its retained evidence is
`aad-selected-extraction-subset-only-default-pairs-30-07`. Direct A/A controls for
that same current binary fail at 6.30%/9.05%, while baseline controls differ by
-3.96%/-1.10%, under `aad-selected-extraction-small-aa-30-03`. The short-request
measurement environment is therefore inconclusive; these results do not justify
another speculative production change or establish performance acceptance.

The next measurement freezes 32 separately timed complete requests per process
for compact workloads of at most 1,024 paths, retaining all request durations.
Wide and larger cases retain one request. Process per-request averages are reduced
with the unchanged two-round minimum and 4% rule. Baseline and head are linked to
the identical helper object, and complete A/A controls must pass before the A/B
matrix starts. No request skips preparation, admission, history, replay or result
construction; only the immutable sealed input is shared, as before. Source and
link proof are retained under `aad-selected-extraction-window-link-proof-09`.

The 32-request baseline A/A control differs by 5.07%/-0.15%; the strict calibration
condition prevents the A/B matrix from starting. Retain that control under
`aad-selected-extraction-window-pairs-30-09`. The last local measurement attempt
predefines 256 compact requests per process, with the same helper-object identity,
all raw durations and unaltered two-round 4% acceptance. Baseline and head controls
for the previously unstable small parallel weighted case must each remain within
4% in both rounds before any A/B case runs. Failure stops local sampling and calls
for an independent environment. Proof and measurements use the `-10` suffix.

The final 256-request controls pass: baseline 0.28%/1.29%, head 0.95%/0.21%.
The complete matrix accepts all 81 unique cases, covering 37 default and 50
selected/empty/width cases with six overlaps measured once. Numerical payloads,
work, actual widths and finite budgets remain checked for every pair. See the
[performance report](../performance/aad-selected-extraction.md) for all round
minima, source hashes, measured capacities and retained failure paths. Keep
default maximum width one, caller upper bounds and capacity-only narrowing;
the wide-input measurements do not support a universal automatic increase.
Final production hashes match link proof `-10`, and all thirteen legacy links
remain byte-identical. Local review approves; final publication/CI gates remain.

Copilot review exposes a coverage gap after the compact fallback: the original
six-family scalar fixtures no longer pack. An added packing precondition is RED
(16 discarded bytes versus 120 mapping bytes in the first BS fixture). The
corrected fixture adds 32 live historical private constants per trade, making
discarded storage exceed mapping payload in every family; the full model prefix
comes from the independent passive model's parameter count. Reordered weighted
and Jacobian results still match independent single-script references across
8,193 paths, one/four workers and tree/compiled execution. A separate compact
six-family case preserves the original fixture and asserts the opposite inequality.
Both targeted cases pass in 1.377 seconds. RED/GREEN source/object/binary/log proof
uses `aad-selected-extraction-packed-family-*` in the evidence directory.
No production source/archive or measured executable changes; the accepted
performance matrix is reused by exact hash proof. Local coverage now comprises
41 core and 48 public cases. Final-head platform acceptance must include the
additional compact-family case; completed earlier-head logs cannot substitute.
