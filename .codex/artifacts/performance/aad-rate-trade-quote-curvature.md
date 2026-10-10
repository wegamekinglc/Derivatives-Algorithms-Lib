# Rate-trade curvature scoped cost acceptance

Status: scoped acceptance complete. Verdict: no regression.

Baseline is merged #524, `b5b17a0f362daa444deb27406245583d6523cf10`.
Freeze the implementation head and both source/configuration identities before
measurement. Rebuild changed objects and selected binaries; retain hashes and
raw interleaved observations. Use the established two-round paired sampling,
warmup, checksum, single-thread/noise controls and sustained 4% old-caller gate.

## Changed paths and selected cases

- `dal-public/src/ratecurvature.cpp` and its public header: measure a complete
  new single-curve weighted six-family request, and the existing generic
  single-curve complete quote-curvature caller as a baseline/head control.
- The same adapter's fixed/free graph boundary: measure staged basis-only XCCY
  and layered joint-XCCY complete financial requests. Compare new paths with
  equivalent explicit objective capture plus the existing curvature composition;
  report their complete latency without claiming a previously existing adapter.
- `dal-cpp/dal/curve/ratecashflowpricing.cpp` and the focused internal objective
  declaration: measure one affected existing multi-component parameter-Jacobian
  caller as the core baseline/head control. Confirm dependency analysis when the
  final internal-header scope is known.

This is five small cases. The financial functional tests cover all seven trade
families and four calibration families; same-currency joint graph reconstruction
is covered by that acceptance and the core multi-component caller. Do not add a
Cartesian product of portfolio counts, curve families, diagnostics or widths.

## Exclusions and repair rule

No tape primitive, RNG, PDE, Monte Carlo, Dupire or linear-algebra implementation
changes are planned; their benchmark matrices are excluded. Native calibrators
remain unchanged and are included through complete affected financial requests.
Required exact-head CI and scheduled full monitoring remain applicable.

After repair, record which timed paths or executable identities changed before
repeating only those pairs. Add cases only for an observed failure, dependency
change or specific coverage gap, with the reason recorded first. Do not claim
excluded or identity-inapplicable evidence as a new pass.

## Measured evidence

Implementation head: `aa09e4792a4f63cef78a250ce29e917e8888a848`.
The following documentation-only acceptance commit does not change any timed
source or dependency. Baseline is the merge-base recorded above. Isolated
sources/builds are under
`/home/wegamekinglc/.cache/dal-aad-evidence-20261010/rate-trade-quote-curvature/performance/{base,head}-{source,build}`.
Both configure with benchmarks explicitly ON. GCC 15.2.0, C++17, Release
`-O3 -DNDEBUG -ffp-contract=fast`, native AAD, native architecture OFF,
diagnostics OFF and one thread are identical on an i9-13900HX shared host.
The host is considered noisy; no exclusive-measurement claim is made.

Each row uses two rounds of ten interleaved process pairs, alternating first
side. Each process warms up once and times five complete calls. Round and
combined reductions use minima; old callers retain the sustained +4% gate.
All 200 outputs and checksums are retained. The sum of actual timed intervals
is **1.078831179 seconds**, without multiplying by the five calls again.

| Case                        | Reference min (ms / 5 calls) | Head min (ms / 5 calls) | Combined delta | Round 1 | Round 2 | Verdict       |
|-----------------------------|------------------------------|-------------------------|----------------|---------|---------|---------------|
| Six-family weighted request | 2.868891                     | 2.865982                | -0.10%         | -0.10%  | -1.74%  | Informational |
| Staged XCCY request         | 5.651953                     | 5.659694                | +0.14%         | -0.28%  | +0.14%  | Informational |
| Layered joint XCCY request  | 15.137781                    | 15.122638               | -0.10%         | -0.27%  | +0.36%  | Informational |
| Existing generic curvature  | 1.741674                     | 1.732747                | -0.51%         | +1.38%  | -1.50%  | No regression |
| Existing 32-trade Jacobian  | 0.275930                     | 0.276776                | +0.31%         | -0.13%  | +3.53%  | No regression |

The first three references are explicit native objective capture plus the
existing recalibrated curvature driver, linked to the same head libraries.
Both sides construct the base calibration and passive market outside timing;
both include objective capture and complete financial curvature inside timing.
These comparisons show adapter overhead and latency, not a speedup over a
previously available financial adapter. The final two compare the merged
baseline libraries against the implementation libraries. Every pair's checksum
agrees within the declared relative allowance.

Fresh harness binaries retain dependency hashes and executable identities.
The accepted baseline archives retain their immutable identities; one changed
core pricing object and one changed public curvature object are rebuilt and
replace the corresponding members. Both head archive hashes match the final
functional-test archives. A shared test-fixture include repair was compiled
before sampling; it introduces no production behavior change. Functional
verification reruns only its 28 affected public cases and retains unrelated
accepted evidence.

Raw per-process logs, `environment.json`, `results.json` and object/dependency
manifests remain in the evidence directory above. The active
[harness](aad-rate-trade-quote-curvature-cost.cpp) owns the five-case setup.
Existing rate-risk benchmarks cover native trade-Jacobian callers; a future
scheduled workload may incorporate these financial curvature cases. That
coverage advice does not expand this PR's five-case acceptance.

## Codacy preparation refactor

Codacy identified complexity in the shared six-family preparation and harness
dispatcher. Preparation now uses a shared typed visitor with small family
overloads, and the dispatcher delegates to focused financial/generic/Jacobian
runners. This changes test/harness preparation only; production archives remain
identical. The 16 affected public tests and two affected strict probes pass.
Before publication, rebuild all selected harness modes, compare executable
identities against the original evidence and repeat only comparisons whose
identities changed. Retain both evidence directories and record the selection
before any repeat samples; the five-case scope and sampling gates stay fixed.
