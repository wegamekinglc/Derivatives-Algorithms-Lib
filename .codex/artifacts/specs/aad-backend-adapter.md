# AAD backend adapter and executable capabilities

Status: active D03 implementation. Stateless capabilities, channel seed/read
and scoped recording integration are implemented locally. All four backend
scalar contracts pass; generated state, complete builds, consumers, new-head CI
and fresh performance acceptance remain required.
Source baseline: `9b5febcca79643e817999e76e8f5dda44922d618` from draft PR #480.
Controlling scope is the [full AAD plan](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3c2d0bdbdf6532ae9edae507d073f765e7e31f8a/.codex/artifacts/plans/aad-improvement-plan.md).

## Problem and intended behavior

Backend-specific number types, registration, derivative proxies, storage modes
and reverse side effects are spread across the core headers. Scope ownership
provides a common lifecycle, but future risk planning cannot discover which
operations the compiled DAL adapter actually supports. Native vector storage
exists without a common channel seed/read gateway. Upstream Hessian or callback
features are not automatically available through DAL's current first-order aliases.

Introduce a stateless compile-time adapter with executable capability contracts.
Keep existing free APIs, layouts, default scalar execution, error semantics and
backend conventions. Capabilities describe the pinned DAL integration, separately
from mathematical fallbacks in the later request planner.

This increment covers core operations and truthful capability reporting. It does
not implement F02 portfolio/Jacobian requests, F03 reverse events, F04 mixed-mode
types, arbitrary nesting, upstream lifetime metadata or new public binding settings.
Those remain required work in their own plan increments.

## Suggested interface and layers

`BackendCapabilities_` is a small value containing backend identity and independent
flags/limits. `BackendAdapter_` selects a native or upstream-scalar implementation
at compile time. No virtual methods, heap-owned adapter, runtime backend switch or
per-node capability lookup is introduced.

The operation boundary has three parts:

- Shared recording services: activate, independent registration, start recording,
  clear/rewind, mark/restore, gradient clearing and full/suffix/prefix reverse.
- Seed/read services: scalar and indexed-channel set, add and read, plus read-only
  validation of an intended scalar/vector mode and width before planning execution.
- Capability metadata: implemented operations and side effects, diagnostic
  availability versus enabled state, unsupported native events/high-order/nesting.

Existing arithmetic remains in the selected active type. Recording/checkpoint
ownership remains in `RecordingScope_`; the adapter does not expose backend
positions as public risk IDs or make stale values safe on unsupported backends.

## Requirements

R01. Name the compiled backend and declare capabilities independently: scalar
reverse, repeated fixed-graph scalar sweep, interval reverse, prefix accumulation,
vector adjoint storage, maximum native vector width, scoped lifecycle validation,
native-number diagnostic availability/enabled state, independent nesting,
native reverse events and native higher-order mode.

R02. Capability claims are proved by actual adapter operations on the repository's
pinned number/tape types. A skipped unsupported test is not a support claim.
Upstream XAD/Adept/CoDiPack first-order aliases must not advertise native vector
channels, full node-lifetime diagnostics, callbacks or Hessians based on upstream
marketing. Future separately implemented fallbacks must be named separately.

R03. Preserve legacy `Value`, `Adjoint`, `AdjointValue`, `RegisterIndependent`,
`PutOnTape`, recording/reset and mode APIs. Native OFF number/node/tape fields and
layouts remain. Selected arithmetic, propagation kernels and default scalar hot
loops acquire no virtual dispatch, counter registry or new per-node checking.

R04. Indexed seed/read uses the actual native vector adjoint array in vector
mode and scalar storage in scalar mode. Channel zero in vector mode must not
read/write the legacy scalar field. Upstream scalar adapters accept only channel
zero. Reject null native bindings, out-of-range channels and unsupported modes
before incompatible storage access or seed mutation.

R05. Set and add seed are distinct operations. Add combines weights for legal
output aliases; set replaces the channel. Reading returns a passive numeric
value. Constant/passive outputs are lifted to supported active roots when the
backend needs one; no hidden parameter registration repairs an unsupported root.

R06. When native lifetime diagnostics are enabled, seed/read first applies
existing owner/epoch/live-slot/generation/mode validation. When disabled, it does
not invent general stale-node detection. Scoped boundary checks stay independent
of this build option. External backends retain their documented scoped limits.

R07. Mode validation checks positive widths, scalar width one and the native
vector limit. External scalar adapters reject vector mode, including a request
to interpret scalar storage as a one-channel vector. Validation is read-only;
native actual selection still uses the established scope-aware mode guard.
Validation or an unsupported-mode error must not rewind a valid graph.

R08. Starting a recording preserves independently registered inputs. Native
`NewRecording` remains its current no-op. Full graph reset, gradient clearing
and suffix restore are different operations. Existing prefix gradient accumulation,
intermediate consumption and caller-controlled clearing remain unchanged.

R09. Report XAD prefix-reverse suffix disposal separately from interval/prefix
support. Callers extract suffix values before a disposal operation. Repeated
fixed-graph sweep claims apply to a graph not already discarded by such an
operation. Capability metadata does not revive a consumed/discarded recording.

R10. Integrate the adapter with existing scoped recording services, rather than
leaving an unused facade. Prefer compile-time aliases to the existing tape
functions where a function pointer is needed, so default calls retain the exact
target instead of introducing extra thunks. Keep controlled failure seams private.
Centralize duplicate common tape declarations without changing definitions.

R11. Existing private lifecycle state enumeration must follow repository
Machinist guidance. Generate its definition/implementation, preserve the six
states, messages, transitions and test seams, and verify generated drift.
This is a separate refactor, with no new lifecycle behavior.

R12. Invalid capability/mode/channel requests fail with operation/backend context.
Precondition failure leaves primal values, seeds, graph position and valid scope
usable. Messages are constructed on failure; no per-scalar successful-path string
or capability-object allocation is added.

R13. Keep the dependency direction `dal-cpp <- dal-public <- bindings`.
The core adapter does not construct Python/Excel objects. Installed consumers
compile against exported backend/diagnostic definitions and matching headers.

R14. Preserve all existing correctness/CI and performance acceptance. Run native
OFF/ON and pinned XAD/CoDiPack/Adept contract tests, relevant complete suites,
installed consumers and current-head CI. Freeze final production/benchmark source
and binaries, then run the unchanged nine-target policy plus affected ordinary
MC/LSM/calibration and resource evidence. Keep every failure and confirmation.

## Shared mathematical and lifecycle oracles

For registered `x=2,y=3`, record `u=x*y` and `v=x*x+y`. Values are `(6,7)`;
Jacobian rows are `(3,2)` and `(4,1)`.

1. Set output seeds `(2,-1)`, reverse: input adjoints `(2,3)`.
2. Clear all gradients; set seeds `(0,3)`, reverse: `(12,3)`, without prior residue.
3. Give one active root `u` two alias weights `2,-1`: added seed one, gradient `(3,2)`.
4. Add an independently supported constant output: requested parameter gradients
   remain unchanged. A direct-input output also returns its identity contribution.
5. Materialize shared intermediates so the repeated-sweep test covers internal
   consumption, rather than only a fused single-root expression.

For native channels, put these two seed vectors in separate channels and a zero
seed in another. Compare each input channel to the corresponding scalar oracle.
Cover widths 1, 3, 5, 10, 16 and a non-specialized width; verify the legacy scalar
field does not serve as channel zero in vector mode. Before retrying after an
invalid channel, prove the previously seeded valid graph still gives its oracle.

Use the existing independent prefix oracle: `p=x*x`, three rebuilt suffix weights
`1,2,3`, accumulated prefix sensitivity six, one prefix sweep gives `dx=24`.
Exercise it through adapter services and scope/checkpoint ownership. For XAD,
save passive suffix values before prefix reverse; do not probe a disposed handle.

Diagnostics ON additionally tests seed/read on discarded/full-reset, suffix and
foreign-thread numbers. OFF tests make no stale-access safety claim. Mode errors
and external unsupported vector requests are checked before changing an existing
scalar graph.

## Acceptance mapping

| ID  | Required evidence                                                                                       |
|-----|---------------------------------------------------------------------------------------------------------|
| A01 | Four-backend scalar values, weighted VJPs, alias addition and repeated materialized-intermediate sweeps.  |
| A02 | Native indexed channels match independent scalar results; zero channels and legacy scalar field isolate.|
| A03 | Unsupported mode/channel validation preserves the valid graph on all applicable backends.              |
| A04 | Diagnostic ON seed/read rejects stale/foreign activity before unsafe access under ASan/UBSan.           |
| A05 | Three-suffix prefix accumulation and backend disposal behavior obey the actual adapter contract.        |
| A06 | Scope lifecycle, failure recovery, ordinary/LSM task draining and legacy callers retain behavior.        |
| A07 | Generated lifecycle enumeration has no drift; states/messages and the installed headers remain valid.   |
| A08 | Capabilities distinguish available/enabled diagnostics and advertised support matches executed tests.   |
| A09 | Fresh complete applicable builds/bindings/consumers and every exact-head required CI check pass.         |
| A10 | Fresh unchanged formal and affected production pairing passes; layouts/default costs retain guarantees. |

## Implementation order and evidence boundaries

1. Establish compile/runtime RED contracts for the missing adapter and native
   channel gateway. Add scalar/alias/repeated-sweep and unsupported-mode cases.
2. Implement stateless capabilities and seed/read specializations; validate
   native OFF/ON and upstream scalar paths before migrating recording services.
3. Integrate compile-time tape operations and centralize declarations, then run
   the existing lifecycle/production recovery suites.
4. Generate the internal lifecycle enum in its own refactor and retain the
   generated outputs; run drift and state tests.
5. Complete backend/consumer/CI and isolated performance acceptance before
   checking off D03. Do not let a capabilities struct alone count as completion.

The D02 publication head is retained unchanged while this work proceeds in its
independent source. Local D03 commits are not publication-head CI evidence until
they are packaged and that exact new head is verified.

## Current local evidence

The missing adapter header first fails compilation in
`backend-contract-red.log`. The initial scalar-node/vector-mode channel
access independently fails UBSan with null-reference binding in
`backend-vector-storage-red.log`; the channel gateway now rejects missing
vector storage before access. The corrected focused OFF executable passes nine
backend cases under ASan/UBSan, including restored scalar graph correctness.
These logs retain both failures, rather than relabeling them as passing checks.

Shared six-case backend contracts pass on pinned XAD, CoDiPack and Adept;
native additionally covers widths 1/3/5/7/10/16, unbound numbers, missing vector
storage and diagnostic stale/foreign handles. Native diagnostic ON integration
passes 32 backend/recording/state cases; CoDiPack integration passes 22. These
standalone checks compile the changed recording implementation and applicable
tape implementation. Supporting symbols/GTest use existing verified archives;
they are focused evidence, not complete new-source CTest or installed consumers.

The initial standalone warning builds retain failures for an omitted explicit
exception include on CoDiPack and existing unused-parameter/unused-local test
warnings. The header now includes its exception dependency. New backend-only
tests pass warnings-as-errors with the repository's unused-parameter allowance;
existing recording coverage runs with the normal test flags. No existing test
or CI warning policy was changed to obtain a pass.

D02 publication head `9b5febcca79643e817999e76e8f5dda44922d618` now passes
all 49 exact-head checks, including complete diagnostics ON/OFF, diagnostic
ASan/UBSan, Windows diagnostics and consumers. Its checked PR head is unchanged.
This validates D02 publication and does not validate unpublished D03 source.
