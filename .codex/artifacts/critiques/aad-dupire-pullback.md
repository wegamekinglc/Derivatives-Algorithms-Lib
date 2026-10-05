# F01 design critique

Verdict: Proceed with caveats.

## Blocking issues

None for the explicitly fixed-grid, deterministic-carry scalar VJP boundary.
Do not claim full F01 completion before Hybrid, language and curve integration.

## Significant concerns

- An IVS reference cannot provide durable provenance. Freezing every base
  stencil/ATM sample resolves lifetime and mutation risk; replay must reject
  missing samples rather than call the original object as a fallback.
- Fixed band selection and the 1e-4 stencil are part of the differentiated
  function. Central quote differences must preserve those choices. Report
  finite-difference cancellation instead of widening tolerances after failures.
- Boundary copies share active nodes; overwrite seeding loses contributions.
  Use additive seeds and independently test a boundary-weighted objective.
- Same-length matrices and matching labels are insufficient identities. Include
  complete axes, quotes, base samples, settings and surface values. Reject
  missing selected Hybrid surface risks rather than substituting zero.
- Hybrid data preserves input component order, but runtime components sort by
  name before parameter construction. Derive offsets from that typed ordering,
  validate the reconstructed complete axis, and test unsorted input explicitly.
- Domain checks must identify invalid discrete curvature/local variance without
  silently regularizing. Keep those checks off the existing unrequested path.
- Public price/risk means require no additional path normalization at this
  numeric calibration boundary. Direct quotes must be added exactly once.

## Minor notes and alternatives

A numeric frozen sampler is more general than adding clone support to existing
IVS subclasses. A cached full calibration Jacobian may later be useful for many
outputs, but is not justified before measuring scalar pullback cost. Names are
for display; full content and typed coordinates establish compatibility.

## Required review evidence

The ARM wheel failure and local contracted-arithmetic RED control the
[replay correction](../api-notes/aad-dupire-replay-rounding.md). A scalar call
primal must be independently recomputed from frozen inputs, bounded against its
active expression, and retain the existing derivative. The original 1e-12
surface check and every oracle tolerance remain. Inspect shared formula and
boundary copies carefully; do not introduce a final-output value override.

Language projection must not cast away const ownership of the frozen surface.
Return detached geometry/matrices/surface, and retain the GIL during custom-IVS
sampling. The IVS override must validate its numeric return rather than accept
bool/enum conversion to volatility. The public BS convenience must delegate to
the existing core and preserve spot/rate/dividend exactly. An exception in a
Python callback must not enter a recording or poison later requests.

Retain the predeclared multi-step calibration and complete-chain oracles, raw
failed steps, flat parallel-vol reference, rectangular/reordered grids, direct
terms, lifetime/alias tests and success/failure/success recovery. Reconcile
combined diagnostics, sanitizers, binding parity and final-head CI independently
from the still-inconclusive P01 production-performance gate.

For Excel, review the actual generated signatures/defaults and DLL exports,
not only portable functions. Original cell text must be checked for NUL even
when a generic converter truncates it. Integer normalization belongs to the
new entry points. A model factory must copy frozen carry and surface; requiring
a dummy BS model leaves the Merton worksheet workflow incomplete. Worker
isolation must configure and restore the XLL runtime as well as the independent
native oracle runtime. Existing string conversion returns a plain String_, so
an optional string default must not generate an optional-value method call.
