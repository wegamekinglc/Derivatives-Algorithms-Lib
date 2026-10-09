# Monte Carlo quote curvature scoped cost acceptance

Status: selected before measurement; results pending.

## Scope

Changed paths are the public financial plan/rebuild in
`dal-public/src/dupireriskrequest.cpp` and the new
`dal-public/src/dupirecurvature.cpp`. Select only:

1. Complete flat-surface MC quote curvature, including planning/admission.
2. Complete Merton-surface MC quote curvature, including planning/admission.
3. Existing flat-surface first-order public plan plus MC quote-risk execution,
   comparing the immutable merge-base facade with the candidate facade.

Use six quotes, one signed full direction, step `2e-4`, seventeen common paths,
one worker and tree execution. Each process warms up once and times five whole
requests. Two rounds of ten samples per side alternate pair order. Preserve
all 120 outputs; reduce each round with `min`. The old-path control uses the
4% two-round noise policy. New paths versus matched manual rebuilt-gradient
orchestration are informational, outside the nine-target regression gate.
Both new sides retain the same owning base, point, gradient, directions,
products and execution payload; both include identical preflight/fixing work.

The selected standalone tool is [aad-mc-quote-curvature-cost.cpp](aad-mc-quote-curvature-cost.cpp).
Compile mode 0 is the public curvature call, mode 1 the explicit rebuilt-gradient
reference, and mode 2 the existing first-order request. Link the same mode-2
object against both public archives. Numerical checksums must agree per pair.

## Isolation and exclusions

Baseline is `262492d79d701a3b81acc9e12e4354649aa8054c`, the merge-base.
Separate immutable source snapshots and build directories retain Release
configuration with benchmarks explicitly enabled. Reuse accepted unchanged
objects only after source/dependency/object and archive-member hash checks;
build the affected facade closure and selected harness, without a full rebuild.
The accepted core archive SHA-256 is
`3940e48fbf00b041567f434cc7f8a4ce0976c0bc3a1dbe7a43b14157c2c9719d`.

Tape, linear algebra, PDE, RNG, generic MC and native quote-curvature kernels
are unchanged in that exact core archive. Their earlier accepted timings apply;
they are excluded rather than claimed newly measured. Large path/thread grids,
unrelated rate portfolios and the scheduled nine-target suite are outside this
change's local acceptance. Required exact-head CI remains separate.

Environment, source/binary/archive hashes, exact commands, machine activity,
all raw samples and reduced results are retained under
`/home/wegamekinglc/.cache/dal-aad-evidence-20261010/mc-quote-curvature/performance`.
The shared host is recorded as potentially noisy; unstable minima produce an
inconclusive result and never an invented pass.

## Coverage advisory

Existing `quote_risk_perf` covers native quote pullbacks, and `script_mc_perf`
covers core MC. Neither links the public rebuilt financial request. This small
active tool closes the current increment's acceptance gap. A future durable
case should extend a public financial benchmark when that inventory is expanded;
there is no reason to add all old target/parameter combinations here.
