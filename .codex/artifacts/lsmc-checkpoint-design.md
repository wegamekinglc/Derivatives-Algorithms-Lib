# LSMC checkpoint admission design (DAL-351 / #570)

This design controls the follow-on production storage work in #571. The baseline
is `e6f57c7237db603256a8d5f99656fb66a639de9a`. The isolated executable in
`dal-cpp/experimental/lsmccheckpoint/` exercises the scheduling contract without
changing `LsmcRows_`, public APIs, training defaults, or persistent replay workers.
The production opt-in and limits are documented in
[training checkpoints](../../docs/methodology/monte-carlo/lsm.md#opt-in-training-checkpoints).

## What an address does and does not restore

`SkipTo(path)` locates a Sobol point. Its identity additionally requires generator
type/version, full simulation dimension, normal precision, Brownian-bridge flag
and timeline, digital-shift key (training/pricing role, seed, replicate), and
absolute path offset. Training uses `[0,N)`, validation `[N,N+V)`, and pricing
starts at `N+V`, with a further `replicate * pricingPaths` offset. Preserve the
existing 32-bit total-range checks and seek each chunk independently.

Normals are inputs to a state transition, not the state itself. A log-Euler model
needs its exact accumulated log spot at the boundary. `log(exp(logSpot))` is not
a bitwise inverse. Local-vol and joint equity/rate models also need every factor,
integration substep, stochastic numeraire accumulator and interpolation state
that influences future arithmetic. Generating only the suffix normals cannot
recover these. Brownian bridges couple dates across the vector: this prototype
still fills the *entire* normal vector on every block visit.

Likewise, SPOT at a boundary cannot recover the script's running sum, count,
maximum, append-only vector, retained fixing or already accumulated receiver.
History can depend on a bumped script constant; each retrained point must replay
that history using the sealed fixing snapshot. Reusing base-point checkpoints
for a different model or script point is invalid.

## Boundary record and ownership

Checkpoint only immediately before an event, after its preceding event has
completed. Numeric and boolean operand stacks must be empty; no `IF`, loop,
compiled stream or payment blend can straddle a checkpoint. Arbitrary instruction
checkpoints would require stack contents, program counters and branch/loop
frames, and are outside this admission.

The immutable run descriptor owns:

- Schema version, scalar representation, build/backend fingerprint, prepared
  program/observation-plan identity, constant and model parameter point,
  evaluation date, history/fixing snapshot, and smoothing/execution mode.
- Random stream identity above; `N`, `V`, pricing count and replicate count;
  fixed path chunk layout, block boundaries, event-to-sample/payment/exercise maps,
  feature schema, dates and discount conventions.

Each `(path, boundary)` record owns:

- A model-specific complete restart state, next integration/sample index and
  required earlier samples. The BS prototype stores the *exact* log accumulator;
  the sample index is implicit in its fixed event boundary. Deterministic
  numeraires and discount slots are rebuilt from the model point. There is no
  production model checkpoint interface yet.
- All live scalar script variables, including cumulative PAYS receiver values;
  vector lengths and values (and any retained observations used later). Immutable
  constants and the initial historical seed stay in the descriptor. The prototype
  conservatively saves every scalar and vector, including dead ones.
- The absolute Sobol address, represented here by `pathOffset + pathSlot`; the
  block ordinal gives the next event. These implicit indexes must remain bound
  to the descriptor. No worker pointer, RNG object or active AAD number is stored.

Block-local row storage owns the original raw selected-receiver payment at each
PAYS event, every feature coordinate, exercise value, conditional eligibility and
any stochastic-numeraire interval ratio. A delayed payment is discounted to its
event exactly as in the current recording sinks. Zeros, terminal payments and
same-event PAYS/EXERCISE order are preserved.

Backward state is independent of the forward checkpoint: retain one holding
value and inclusion mask per training/validation path, and the already fitted
later-date policy. A deferred decision holds pointers into the current rows.
Consume it *before releasing that block*. In the prototype the last pending
decision is applied to the holding value at the boundary; discounting and the
previous event's payment still occur at the next sweep, in the native order.
Never carry a dangling feature/H pointer into the earlier block.

Hard pricing additionally needs first-exercise status/date, receiver strictly
before that event, selected exercise payment, and the survival indicator if
pricing itself is segmented. Training records exercise as a no-op; it must not
save a forward hard exercise decision and use it as a backward policy decision.
Fuzzy pricing instead carries the recursive continuation value and condition/
exercise degrees, not a boolean survival replacement.

## Schedule and deterministic reduction

1. Forward-generate each training and validation path once. Save its boundary
   states and discard its recording rows. Keep the two path offsets distinct.
2. Visit time blocks last to first. Restore each path at the block's left
   boundary, seek its absolute Sobol point, regenerate the block in forward
   event order, and record all original rows for that block.
3. Sweep that block backward. Fit each date over *all* training paths and use
   held-out paths for degree selection as today. Keep the native 8192-path
   chunks and reduction order; a time block must not partition a regression
   into separately fitted path subsets. Multivariate QR/rank fallback retains
   its original design rows for the current date.
4. Resolve the pending boundary decision, retain holding values, and release
   the block rows. Release every training checkpoint before pricing.
5. Use the existing native hard or fuzzy replay and its disjoint pricing ranges.

The prototype stores one time block for the full path population. A further
path tiling scheme needs a multi-pass regression provider, with original
chunk-order reductions and QR data access; it is not implemented here.

## Space and work bounds

Let `L=N+V`, `E` be event count, `D` exercise count, `P` selected PAYS event
count, `F` feature count, `Dc` conditional exercise count, `I` be 1 for a
stochastic numeraire, `B` block length, `K=ceil(E/B)`, and `Cb` the complete
checkpoint bytes per path at boundary `b` including vector payload/metadata.
Ignore common immutable plans/policies in the following storage comparison.

The current training rows plus rollback state occupy

```
M_full = L * (8 * (P + D*(F+1) + I*(E-1)) + Dc + 9) + R + workerScratch
```

`R` is the peak of simultaneously live regression scratch, including scalar
moment/chunk buffers or multivariate QR. In `dal-cpp/dal/script/lsmcmulti.cpp`,
`MakeWorkspace` copies both the eligible targets and every design column.
For `n=eligibleTrainingPaths` and `q=basisColumns` (at most 20), the dynamic
double payload alone is **at least** `8*n*(q+1)`, not an allocation upper bound.
For one native QR workspace, its owned bytes are

```
R_QR_owned = sizeof(RegressionQR::Workspace_<20>)
           + 8 * (targets.capacity() + sum_{j=0}^{19}(columns[j].capacity()))
```

The fixed `sizeof` term includes all 20 vector headers, the target vector
header, row/basis counts, scales, permutation, upper-triangular storage and
projection arrays, including padding. The full `R` also includes other live
regression temporaries (moments, basis powers and coefficient recovery) and
any simultaneously live workspace objects during construction/return. Count
each allocation once by capacity, rather than its logical size, and take the
maximum across construction, factorization, degree/rank fallback and recovery;
do not sum sequential candidate workspaces. Allocator bookkeeping and retained
arena pages are additional process costs, not vector payload.

Validation rows remain live alongside training, but do not add a second
concurrent QR fit in the current serial schedule. The QR target copy is live
alongside the original holding/target rows. Checkpointing removes neither.

For time blocks with row-byte cost `Rb` per path:

```
M_checkpoint = L * sum_b(Cb) + L * max_b(Rb) + 9*L + R + workerScratch
```

The prototype temporarily owns two holding-value vectors while exchanging them
with `BackwardRows_`, so its measured counter uses `17*L` instead of `9*L`.
Object headers and actual container capacities are counted separately in the
reported owned-storage bytes. RSS includes allocator effects, executable/runtime,
RNG tables, path/evaluator workspaces and QR; it need not track payload savings.

With compact scalar state `Cb=O(S+modelState)`, the leading terms are
`O(L*K*S + L*B*(F+payRows+discountRows))`. Optimizing B requires actual state
sizes and per-block row counts. Full append history has `Cb=O(b*B)`; retaining
each boundary can cost `O(L*E^2/B)` doubles and erase the storage benefit. Generic
vectors and retained observations need explicit liveness/deduplication or an
admission rejection, not an assumption of constant-size state.

Let `A` be one full normal-vector generation, `G` one full model path, `Q`
one full script recording pass, `J` native rollback/regression work, and `H`
checkpoint capture/restore/copy work. A model with an exact restart adapter has

```
W_full       = L*(A + G + Q) + J
W_checkpoint = L*((K+1)*A + 2*G + 2*Q) + J + H + bootstrap
```

These are operation bounds, not a runtime factor. The executable counts normal
vectors/coordinates, model integration steps (including workspace bootstrap),
and script events. The BS adapter reuses native coefficient arithmetic and is
checked against native samples bit for bit. Without a restart adapter, the
safe fallback regenerates the *whole model path* per block, replacing `2*G`
with `(K+1)*G`; restarting the script from time zero as well can cost
`O(L*E^2/B)` event work. Neither case supports a universal 2x-forward claim.

Bootstrap also includes historical seed construction. For `Hp` historical
operations, explicit-constant retraining in this prototype can replay that
history in each block workspace, adding `O(K*Hp)` work per fit, versus
`O(workers*Hp)` in native training. A production passive provider can build and
share the sealed seed once per parameter point. Active AAD seeds remain local
to their recording; they cannot use that passive sharing rule. Future-event
counters exclude history replay and state-copy operations; elapsed times include
them.

Mode-specific peaks and work:

- **Hard:** overall peak is `max(trainingPeak, hardReplayWorkers)` plus shared
  immutable data. Pricing is unchanged and needs no population-sized row store.
- **FrozenAAD:** training remains passive double, with the same bounds. Overall
  peak for a fresh process is `max(trainingPeak, workers*(tapePeak + activePath +
  evaluator + perPathRows))`. In a reused process, add previously retained tape
  capacity to the training phase. `RecordingScope_::Close` rewinds the thread's
  tape; its block capacity can outlive that request. Allocator RSS can also
  retain freed row storage. There is no demonstrated reduction of the active
  per-path tape in this task.
- **RetrainedBump:** with `q` model/script inputs and two-sided admissible bumps,
  do one base fit plus `2q` sequential fits and `2q` value-only policy replays,
  in addition to frozen AAD. Multiply the training work bounds by `1+2q`, not
  its peak storage. Keep only the policies needed by the current secant, and
  rebuild model, historical scalar/vector seeds, checkpoints and numeraires at
  every point. A constrained input uses the current one-sided rule and base
  policy reuse. Concurrent bump fits multiply the storage peak and are excluded.

For RetrainedBump specifically, frozen AAD executes before the bumped fits.
Let `Tretained` be the worker tapes' capacity after that replay. The peak bound
must therefore include `trainingFitPeak + Tretained` and
`valueReplayPeak + Tretained`, as well as the base training and active AAD peaks.
Sequential fitting avoids multiplying row/checkpoint storage by q; it does not
make retained tape capacity disappear. The process RSS measurements include this
overlap; the owned-training-storage counter deliberately does not.

## Persistent worker and AAD boundary

Existing workers solve task submission and allocation reuse across *paths*;
they do not checkpoint a model/script at a *time* boundary. Keep that ownership
model and fixed chunk addressing. Adding training checkpoint providers should
not create a new task per path/date or alter ordered replay reductions.

A passive checkpoint is safe for hard training but cannot be restored as an
independent active AAD state: that would lose derivatives through its prefix.
Active numbers, model parameter pointers, historical seed nodes and smoothing
leaves belong to one recording on one worker tape. Extending checkpointing to
fuzzy pricing requires a separate adjoint restart protocol: recompute the
prefix from registered inputs or propagate a segment's boundary adjoints
through its recorded predecessor; seed continuation/survival adjoints exactly
once, then rewind only after all dependent nodes are consumed. This prototype
intentionally uses the complete native per-path recording and harvest protocol.

## Production admission

Admit the schedule as an opt-in candidate only after the measurements and
focused parity checks pass. Keep the current storage as the fallback. The first
production implementation needs a versioned internal model restart adapter,
prepared-state/parameter-point identity checks, event-boundary snapshots,
immutable stream descriptors, and a row provider with the same native reductions.
Reject unsupported model/state combinations before allocating checkpoints.

For #571, budget admission must conservatively bound the complete concurrent
allocation peak before allocation: checkpoint and row capacities/headers,
rollback state, `R` above (including the QR target copy and fixed workspace),
worker/model/RNG scratch, immutable policies and retained AAD tape capacity.
Use checked size arithmetic and known allocator capacity bounds; an unknown
bound or a budget excess requires the existing full-storage fallback or its
existing budget rejection. The owned-training counter excludes QR and tape
and cannot alone admit a production workload. Its published measurements and
RSS observations keep their original scope and provenance; this correction
requires no new process samples or budget-default change.

The prototype is compiled-only, single worker, exact BS, one positive-time
sample per event, three scalar regression features, optional growing vector,
and in-memory recovery within one preparation. It tests held-out selection and
all five named input risks (spot, vol, rate, dividend, K) at an interior point.
It does not qualify local-vol/hybrid states, retained future FIX samples,
stochastic discount ratios, arbitrary program counters, cross-process archives,
one-sided bump constraints, active tape segmentation or parallel restart.
Those need focused admission tests when implemented; no default budget or
public binding change is authorized by this experiment.

The measured admission result is conditional: the 65-event compact-state case
with B=8 reduces owned training storage by 40.60% and process RSS by about
5 MiB. Full growing-vector history increases owned storage by 46.58%; the
single-block and tested short-grid shapes also increase it. Thus #571 can use
this schedule behind explicit model/state and memory admission, while general
default replacement is no-go. Timings remain informational under external
load; see the linked study for all samples, exact provenance and risk evidence.
