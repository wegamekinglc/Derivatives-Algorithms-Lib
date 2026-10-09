# Monte Carlo quote curvature scoped cost acceptance

Status: local scoped acceptance complete; no regression found in the selected
existing-path control. New-path comparisons are informational.

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

## Results and reproduction

Measured implementation: `b1427b3ee4306ca63927f18f1db4b49f395422ae`.
GCC 15.2.0, CMake Release, native AAD, diagnostic/native-architecture switches
OFF, `-O3 -DNDEBUG -ffp-contract=fast`, one worker, Intel i9-13900HX under WSL.
Both isolated caches explicitly enable benchmarks. The host also ran an
unrelated compiler; no exclusive-window claim is made. Load averages before
and after sampling were 1.40 and 1.69 on 32 logical CPUs. Alternation and
two-round minima remain required despite the small workload.

| Selected case        | Reference min, ms | Candidate min, ms | Combined delta | Round 1 | Round 2 | Verdict               |
|----------------------|-------------------|-------------------|----------------|---------|---------|-----------------------|
| Flat curvature       | 1.608252          | 1.591666          | −1.03%         | +1.97%  | −1.78%  | Informational new path |
| Merton curvature     | 1.650338          | 1.660695          | +0.63%         | +0.89%  | −0.15%  | Informational new path |
| Existing first order | 0.271806          | 0.271501          | −0.11%         | −0.11%  | +1.09%  | No regression         |

All 120 paired samples are complete and numerical checksums agree. Five timed
requests per sample consume 0.7812 seconds in total, excluding process startup,
warmup, configuration and compilation. No size/thread matrix was repeated.
These numbers establish small-request orchestration cost, not production
throughput or a speedup claim. No nine-target gate was run for unchanged kernels.

`performance/environment.json` retains all compiler/link/configure commands,
immutable source paths, binary/archive hashes and host settings;
`performance/results.json` retains every duration/checksum and round minimum;
`performance/*-round*-*.log` retains every raw process output. The selected tool
was compiled separately in modes 0/1/2, with mode 2's identical object linked
against both archive snapshots. Reproduction on this host is recorded in
`performance.py` at the evidence root; use a fresh evidence directory because
the recorded detached worktrees already exist.

Overall scoped verdict: **no regression**. Coverage outside the selected
existing public path is explicitly excluded; new functionality has no older
equivalent public entry point.

## Coverage advisory

Existing `quote_risk_perf` covers native quote pullbacks, and `script_mc_perf`
covers core MC. Neither links the public rebuilt financial request. This small
active tool closes the current increment's acceptance gap. A future durable
case should extend a public financial benchmark when that inventory is expanded;
there is no reason to add all old target/parameter combinations here.
