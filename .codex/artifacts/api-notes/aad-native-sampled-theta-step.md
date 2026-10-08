# Native sampled PDE recording API

Status: active API contract. Numeric #504 is accepted in master at
94799f9e3f349bf89b3a2c58a16bde322d90c9f6. Implement this wrapper in its own PR;
these decisions complement the controlling specification and critique.

## Proposed owning boundary

Use AAD::SampledThetaStepWithAccuracy(scope, numericInputs, activeBindings,
explicitPolicy, optionalPivotTolerance). The existing PDE numeric configuration
supplies passive mesh/provenance and passive field values. A grouped
AAD::SampledThetaStepBindings_ declares active overrides: rates_/drifts_/variances_
Vector_<Number_>, oldValues_/externalValues_ Matrix_<Number_>, and optional<Number_>
dt_/theta_. Empty containers mean that entire field uses the passive values;
nonempty containers bind the complete field. Optional scalar presence means an
active scalar. Do not infer activity or provenance from value equality or aliasing.

Active oldValues_ defines the resulting layer count when present. The overwritten
passive field's dimensions and values are unused; all other resulting fields must
match the resulting n-by-layers request. Active coefficient vectors require n-2
entries, and a declared external matrix requires two rows and that resulting layer
count even when its sides are unused. This permits a fully active state request
without an additional populated passive state matrix.

Active overrides supply their own primal values; they do not have to equal the
passive field that they replace. Validate the resulting complete numeric request
and every declared active binding before publishing output slots. Grid locations
and boundary-source flags remain passive. External active fields retain both
sides and their shape/finite/lifetime checks even if one or both are unused;
unused input risks are zero. A fully passive request uses the numeric operator;
the native call requires at least one declared active field/scalar.

Result is CheckedSampledThetaStepResult_ with Matrix_<Number_> solution_, owning
SampledThetaStepDiagnostics_ (forwardBackwardErrors_, policy_) and the shared
SolveAccuracyEvent_ token. Do not invent a PDE condition report. Forward solution
shape is n-by-layers. At scalar/vector width c, each channel forms one n-by-layers
numeric seed matrix, contracts all layer coefficient risks, and reports actual
transpose errors in layers-by-c. Reuse SolveAccuracyInvocation_ full/suffix/prefix
collection and event identity; report axes follow actual RHS layers and channels.

## Storage and reverse

One native event owns one copied numeric step and only declared active input
bindings plus n-by-layers output bindings. Stage native binding copies and the
numeric construction in caller context, then copy into tape-owned payload without
another factorization. There is no callback/user-owned payload migration issue.
The first wrapper inherits complete-coordinate numeric Reverse admission:
inactive risks are still computed before scattering active slots, and an inactive
contraction range loss still rejects. Document/test this domain explicitly; a
selective numeric reverse requires a separate accepted contract/performance change.
Use the existing generic checked event wrapper and scoped buffer accounting;
change a shared helper only for an actual missing capability.

Reverse clears output seeds after successful contribution. Every active scalar
or cell scatters its matching numeric risk; aliases accumulate all terms, including
across coefficient groups, dt/theta, state/boundary and successive steps. Skip
exact-zero channel seeds only when its defined physical error is exactly zero;
nonfinite seeds/accumulated risk must still fail. A normal scope.Reverse enforces
the policy, even without collecting reports. Shared reports remain owning after
checkpoint restoration/close; output Numbers obey tape lifetime.

Capture failure publishes no output slots and invalidates the recording as in
existing structured events. Reverse failure returns no partial collection and
invalidates reads. Failure/restore/close refunds descriptor, payload, bindings,
cache and scratch storage. Caller report/output overlaps and tape-owned versus
caller-owned charges must be measured, not inferred. Preserve ordinary solve,
coordinate and root payload layout/hot paths.

## Minimum RED and staged acceptance

First independent n=3 analytic numeric fixture wrapped in scalar native recording:
active rate r=0.1, passive old (1,2,3), external (4,5), mu=.2, variance=.4,
dt=.2,theta=.5, explicit policy1e-14. Objective2*solution(1,0)+r.
Numeric rate seed2 gives -572/735; total native rate risk is 163/735.
Physical report is1-by1. Register r before StartRecording, extract before Close.
A missing public header/interface establishes RED before writing the wrapper.

Expand only affected boundaries: all field activities, active override primals,
unused boundaries, scalar/width1/4/8, two layers and serial rollback, independent
expression/complete-solve differences, aliases, repeated seeds, zero/NaN/range,
source mutation/destruction, checkpoints/windows, invalid lifetime/thread/mode,
exact caller/tape peaks and one-byte-short recovery. New cases belong to a named
suite included explicitly in six sanitizer filters.

Performance scope: direct native operator cost for the three accepted PDE shapes,
with numeric versus native capture/reverse disclosed separately. Existing ordinary
structured callers are excluded unless shared-object/header change or fresh links
prove a dependency. Keep accepted source/config hashes and reuse byte-identical
executables; expand only for a concrete new failure or coverage gap.
