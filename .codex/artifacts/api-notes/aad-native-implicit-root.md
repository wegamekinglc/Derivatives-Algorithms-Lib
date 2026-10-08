# Native implicit-root API

Active F03 native increment after accepted numeric #501. Public C++ header:
dal/math/aad/implicitroot.hpp, namespace Dal::AAD. Python/Excel projection is
part of later binding work and introduces no change to those surfaces here.

ImplicitRootWithAccuracy(recording, equation, candidate, inputs, policy,
optionalRelativePivotTolerance) returns CheckedImplicitRootResult_ with
Vector_<Number_> parameters_, owning ImplicitRootDiagnostics_ diagnostics_
and the existing SolveAccuracyEvent_ event_. The optional tolerance defaults
to 64*epsilon; all required method/accuracy arguments precede it.

The candidate is passive double data and selects the local branch. Every
Number_ input declares a differentiable slot; aliases accumulate by declared
column. k=0 remains legal. The equation supplies complete same-point residual,
J and K through the accepted numeric contract. It evaluates once at owning
double snapshots; no callback or finite iteration graph remains in the event.
Capture Number bindings before evaluation so mutation of original input
objects does not redirect the dependency represented by that snapshot.

ImplicitRootDiagnostics_ owns residuals_, policy_ and
reciprocalConditionInfinity_. These preserve equation units and captured-point
admission; they do not expose zero construction RHS as convergence evidence.
Return forward values exactly equal to the supplied candidate. Compose direct
objective input dependence through ordinary native expressions.

Reuse ReverseWithSolveAccuracy and its suffix/prefix variants. A root has one
transpose RHS per AAD channel, so the actual report is 1-by-width, including
zero channels. Root, dense and coordinate events share recording/event and
invocation identity. Historical successful reports and forward diagnostics
remain owning data after restoration or recording closure. Native parameter
handles follow the existing live-node/recording lifetime contract.

Example: record q=4, capture theta=2 for theta^2-q=0 with explicit zero
residual/transpose limits, then objective=3*theta+q. Reverse gives q risk 7/4.
The negative branch returns the corresponding signed equation derivative.
An admitted approximate candidate keeps its observed residual and local
linearization; neither residual nor transpose report certifies sensitivity error.

Wrong recording/mode, invalid/stale/foreign input bindings, malformed equation
data, residual/pivot/range failure or capture capacity rejection publish no
returned root output. Reverse accuracy/range failures, nonzero products rounded
to zero and nonfinite accumulated alias/direct risks invalidate the recording
and expose no partial owning collection. Representable subnormals remain
supported through the numeric contraction policy. Refund staged ownership and
preserve detached historical reports on every failed path.

One numeric cache and existing generic owned-event RAII retain method data and
bindings under tape admission; returned outputs, forward diagnostics and
invocation reports use caller admission. Construct the numeric linearization
outside event ownership so callback-managed buffers retain caller accounting,
then deep-copy into tape-owned event storage without a second factorization.
Staged numeric/binding/value buffers count toward caller peak along with
publication overlap and reverse reports/scratch. Revalidate captured slots.
Callback side effects are not rolled back on failure. Verify exact/one-byte-short
limits through actual census. Public API does not expose private LU, private
collector state or new event-account types.

Rejected alternatives: differentiating nonlinear iterations, selecting a root
branch inside the operator, Gauss–Newton for complete stationarity derivatives,
per-slot inferred passivity, duplicated accuracy collectors, n-by-width root
error reports, and borrowed callback/input buffers.
