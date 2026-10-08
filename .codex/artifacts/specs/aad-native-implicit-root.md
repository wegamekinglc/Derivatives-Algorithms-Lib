# Native implicit-root recording increment

Active F03 increment on feature/native-implicit-root-accuracy after numeric
#501 merged at 299cc8d02caa408409e66ded93fe4f4a0c8e2a51. Accepted head
a4a43f19a8c76bd908b55536a0328bb3ab25f7e7 and merge trees match; all 36 checks
and all 22 root cases in fourteen profiles passed. Preserve its owning
[numeric contract](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a4a43f19a8c76bd908b55536a0328bb3ab25f7e7/.codex/artifacts/specs/aad-implicit-root-linearization.md).

## Method and public boundary

Public header: dal/math/aad/implicitroot.hpp. One
AAD::ImplicitRootWithAccuracy(recording, equation, passiveCandidate,
Vector_<Number_> equationInputs, explicitPolicy, optionalPivotTolerance)
returns CheckedImplicitRootResult_: Vector_<Number_> parameters_, an owning
ImplicitRootDiagnostics_ and the existing SolveAccuracyEvent_ token.

Inputs typed Number_ declare differentiable slots, consistent with existing
recorded solve activity. Plain double requests use the numeric interface.
The initial native surface needs one input activity; k=0 remains legal.
The candidate selects the local branch and is passive. Forward values equal
the supplied candidate; equation evaluation/accuracy guards remain numeric.
Its complete J/K contract and captured-point approximation interpretation are
unchanged. Direct objective input dependence composes through ordinary nodes.

Retain exactly one numeric ImplicitRootLinearization_ cache in the event.
Copy input Number_ bindings before invoking the equation, snapshot their values
through the live NativeInputSlots_ index and construct the validated numeric
linearization in the caller context. Deep-copy it and the captured bindings
into event-owned storage, revalidating live slots before publication. This
ordering preserves the captured dependency if the callback changes original
input objects. Evaluate once at owning doubles; never retain the callback or
differentiate its finite iterations. Callback-managed allocations/releases
run outside event ownership accounting, including exceptions. Staged numeric,
binding and value buffers obey caller admission; event copies obey tape
admission. Never move foreign allocations into an owned account.
Revalidate recording phase/mode after equation evaluation and before creating
the accuracy event identity or publishing outputs. A callback that finishes
recording must reject, invalidate the invocation and refund staged ownership.

Return owning residuals_, explicit policy_ and reciprocalConditionInfinity_
as ImplicitRootDiagnostics_. These observations use the equation's residual
units; do not expose the private zero construction RHS as root convergence.
Parameter count n and scalar/vector AAD width are independent axes.

## Shared events and report semantics

Reuse the existing owned event, publication/failure guard and collector.
Generalize its MakeOutputs/publication return deduction only as needed for
Vector_<Number_>, and use decltype(auto) for Diagnostics forwarding. Existing
linear payloads still return const LinearSolveDiagnostics_&; the root payload
returns owning ImplicitRootDiagnostics_ by value, copied under caller admission
after event capture and before publication. Preserve matrix specializations
and the forced inline point contribution helper. Add narrowly scoped
internal bridges to create shared accuracy identities, prepare an existing
report and copy one actual error column; preserve current old collector paths.
Avoid duplicate event-account/destructor/RAII implementations and new public
numeric cache accessors.

Keep the payload's output bindings as an n-by-one Matrix_<Number_> so existing
CollectSeeds and ClearOutputs can be reused unchanged. Construct the public
Vector_<Number_> directly and copy its node bindings into that matrix. This
avoids a temporary public matrix and duplicated seed/clear loops. Mark the
existing matrix MakeOutputs helper maybe_unused where a root unit does not
call it; no warning suppression or new allocation is needed. Observe any
shared-object identity change before deciding the old-caller timing scope.

For each AAD channel, collect n root-output seeds into an n-by-one matrix,
call the one numeric root Reverse and scatter its k-by-one input risks into
the captured Number_ slots. Multiple input aliases sum once per declared
column, including direct expression contributions. Clear root outputs after
each successful channel. Zero channels yield exact zero errors and no risk;
range/accuracy/accumulation failure follows recording invalidation semantics.
Reuse the numeric nonzero-product underflow guard; support representable
subnormal products. A skipped zero channel must retain its exact zero report.

ReverseWithSolveAccuracy and full/suffix/prefix variants include root, dense
and coordinate events in one actual reverse invocation. A root transpose
system has one RHS per AAD channel, so its report is 1-by-width, never
n-by-width. Existing event/invocation/mode identities and historical owning
reports are preserved, including omitted-event lookup failure after windows
and checkpoint restore. Ordinary recording Reverse still enforces the policy.

Allocate a returned invocation report before measured owned scratch, so caller
capacity includes overlap. Numeric cache, input/output bindings and event
ownership use tape admission; returned parameter vectors, diagnostics and
reports use caller admission. Include the staged numeric cache/input bindings,
capture input-value temporary and
input/output Node_ bindings in exact resource proofs. Failed admission refunds
all staged ownership and publishes no root output or partial collection.

Resource hypotheses to test independently: numeric retained storage plus
(n+k)*sizeof(Number_) for owned input/output bindings and a fixed event object;
public outputs/diagnostics retain n*(sizeof(Number_)+2*sizeof(double)). The
numeric temporary capture, k-double value snapshot and k-Number binding copy
contribute to caller peak admission. The live-node range index uses tape
admission. The index is temporary and depends
on occupied node blocks, so do not hide it in a shape-only constant. For a
nonzero reverse channel, reuse the accepted numeric peak and add the n-double
collected seed, giving (k+3*n+2)*sizeof(double) owned scratch. With a caller
budget active, the same physical reverse scratch is also admitted to that
budget; its peak includes returned outputs/diagnostics, invocation reports and
this scratch overlap, staged numeric construction and the staging/publication
overlap. For n=k=1/2, staged capture peaks are numeric 108/304 bytes plus
n*(sizeof(Number_)+sizeof(double)); on this host these are 132/352 bytes.
Caller peak is the maximum of capture, staging/publication and reverse overlap.
Reports remain caller-owned. Callback-owned persistent buffers stay caller
owned even if callback execution throws. Tape cleanup is reserved
admission headroom and is excluded from reported occupied/peak capacity.
Verify n=1/k=1 and n=2/k=2 storage differences, all widths and zero channels,
then exact/one-byte-short boundaries; refine any hypothesis contradicted by
actual census rather than weakening a resource assertion.

## Focused independent acceptance

Start with missing-header RED and a composed quadratic root: theta=2/q=4,
objective 3*theta+q gives q risk 7/4. Then cover:

- Independent native elementary/Cramer root derivatives and three-step complete
  positive/negative/non-symmetric/pivot/stationarity root solves; complete
  stationarity J distinguishes residual-Hessian terms.
- Scalar, vector widths 1/4/8, independent channels and zero columns; report
  shape 1-by-width and actual inclusive/nonzero/below-limit transpose error.
- Multiple Number_ input aliases and direct terms, repeated/serial roots and
  ordinary expressions. No inferred per-slot passive semantics are introduced.
- Candidate/input/equation destruction and source mutation, including callback
  mutation after dependency capture; one equation evaluation and no reverse
  callback. Validate foreign/stale live slots through existing capture indexing.
- Mixed root/dense/coordinate report collections, full/suffix/prefix order,
  checkpoint restoration, detached prior reports, recording/mode identity and
  failed-event lookup. Current private collector state is shared, not duplicated.
- Root residual/pivot/singularity/range rejection before publication; ordinary
  reverse accuracy enforcement, partial accumulation/alias overflow and failed
  recording adjoint reads; new supported recording/report remains usable.
- Tape/caller report/scratch resource census, exact and one-byte-short limits,
  refund on failed construction/collection and input/output node occupancy.
- Strict/direct header OFF and combined diagnostics ON, CCN <=8, formatting,
  installed consumer, current methodology/changelog and actual execution in all
  fourteen applicable platform profiles. Existing AADLinearSolveTest suite can
  hold native root cases without broadening sanitizer selection.

## Performance mapping

Freeze accepted #501 source/tree/archive and matching caller binaries first.
Expected production additions are one AAD root unit plus a new header/internal
collector bridge. GNU archive duplicate implicitroot.cpp.o basenames must be
matched by name/hash multiplicity, not ordinal position alone.

If generic return deduction and the bridge preserve old objects/executables,
reuse their accepted timing. If actual native solve units change, select only
their actual ordinary/diagnosed/dense-checked/coordinate callers. The prepared
18-row helper scope plus four now-existing checked-coordinate rows supplies
22 relevant rows; no MC/PDE/full matrix follows. Numeric root object/callers
should retain accepted bytes; expand to its four rows only if those change.
Keep the accepted sustained +4% paired gate and noise handling unchanged.

New native root costs are informational: quadratic n=1/k=1/one output scalar
and coupled n=2/k=2/two outputs width4, each cached and complete. Compare with
accepted numeric root requests performing less work and independently verify
all primal/risk/report checksums, actual resources and source/binary hashes.
Merge only after repeated paginated exact-head CI/Codacy/review audits and
guarded head/tree verification, before the PDE implementation increment.
