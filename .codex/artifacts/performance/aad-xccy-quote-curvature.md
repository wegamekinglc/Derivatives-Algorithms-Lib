# Cross-currency quote curvature performance acceptance

Status: active; local acceptance complete. Selection was recorded before measurement.
Correctness, eight strict OFF/combined probes and the installed consumer pass.

## Scope selected from callers

| Changed path / actual caller                                                            | Selected case                                                                               | Boundary and reason                                                                       |
|-----------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------|
| `dal-public/src/ratecurvature.cpp`: staged sealing, replay, refined inverse, provenance | Two-quote staged XCCY curvature, public driver versus explicit rebuild-gradient composition | Fixed currency blocks; one mixed direction; three fresh calibrations per request          |
| `dal-public/src/ratecurvature.cpp`: joint declarations, routes, replay, refined inverse | Ten-quote joint XCCY curvature, public driver versus explicit rebuild-gradient composition  | Five parameter blocks, two quotes each; mixed direction spans domestic, foreign and basis |
| Shared capture, source variant and inverse-product helpers; existing single-curve entry | Existing two-quote single-curve public curvature, accepted #523 versus candidate            | Executes the affected shared code and the complete existing curvature entry               |

The first two cases measure public-driver overhead against the same new replay
provider, including fresh inverse refinement on both sides. They do not claim
an improvement over an old XCCY curvature API: that API did not exist. The third
case is the existing-entry regression control. All timed objectives include
parameter and direct-quote terms. Factory/fixture creation is outside timing;
every timed request still recalibrates base, plus and minus points.

The [harness](aad-xccy-quote-curvature-cost.cpp) reuses the unchanged native
`rate_risk_perf` XCCY materials. Its single-curve control is square LOGDF, avoiding
the rectangular single-curve benchmark fixture. Independent financial correctness
is established separately by 66 multi-step curvature references and 22 gradient
coordinates from the original passive solvers, not by timing checksums.

Exclude unrelated MC/PDE/Dupire, tape/RNG/linear-algebra benchmarks and the full
parameter matrix: none calls the edited public implementation. Broad native
calibration benchmarks do not call the new factories either. Exceptional modes,
historical fixings, graph mutation, legacy route fallback, asymmetric declarations,
non-default tenors and layered/unlayered configurations have scoped correctness
coverage; they do not introduce different timed algorithms requiring a full matrix.
Scheduled full-suite monitoring and required exact-head CI remain applicable.

## Protocol and provenance

- Separate detached baseline and candidate source worktrees and build directories.
  Baseline is #523 merge `de810076b7e3748e17c22a6ee621f7f0190ea551`.
- Release, GCC 15.2, C++17, `-O3 -DNDEBUG -ffp-contract=fast`; native AAD,
  one thread, native architecture flags OFF, diagnostics OFF. Configure both
  worktrees with benchmarks explicitly ON; build only the selected harness.
- Two rounds of ten paired processes per side and case, alternating pair order:
  120 process samples total. Each process performs one warmup and five timed
  complete requests. Compare best-of-ten for each round and best-of-twenty overall.
- Preserve the 4% regression threshold in both rounds for the existing entry.
  New-entry comparisons are informational; check matched checksums in every pair.
- Shared host, no exclusive-window claim. Record CPU/load, commands, source SHAs,
  dependency hashes, archives, retained objects and executable hashes with raw logs.
  Rebuild the affected public object; reuse unchanged core, facade members and
  fixture objects only after proving source/dependency/configuration identity.
- During repair, rerun only affected cases. Documentation-only follow-up retains
  measurements when source/object/archive/executable hashes establish applicability.

## Results

Candidate implementation: `fc42400f38e9210d57959f6f69bb05382101fedc`.
Values below are milliseconds for five complete requests; deltas use unrounded
process timings. Each comparison retains 40 samples across its two rounds.

| Case                                                     | Reference best (ms) | Candidate best (ms) | Overall delta | Round 1 / 2 delta | Verdict                    |
|----------------------------------------------------------|--------------------:|--------------------:|--------------:|-------------------|----------------------------|
| Staged XCCY, explicit composition / public driver        | 6.0279              | 6.0294              | +0.02%        | +0.02% / -0.39%   | Informational new coverage |
| Joint XCCY, explicit composition / public driver         | 13.6402             | 13.5338             | -0.78%        | -0.78% / +0.93%   | Informational new coverage |
| Existing single-curve public curvature, #523 / candidate | 1.6672              | 1.6858              | +1.11%        | +2.43% / +0.38%   | No regression              |

Overall: no regression in the selected existing entry; neither round reaches
the 4% threshold. New-entry driver overhead is within shared-host noise. Every
paired checksum matches. The 120 samples contain 0.889764397 seconds of measured
work, excluding fixture construction, warmup, process startup and compilation.
Host load before/after is approximately 0.36/0.20/0.19.

Raw evidence lives under
`~/.cache/dal-aad-evidence-20261010/xccy-quote-curvature/performance/`:
`environment.json`, `results.json`, `retained-fixture.json`, configuration/link
logs and one raw log per process. The candidate public archive SHA-256 is
`85e0e5c7b3e1e40b3ac45dc7e9f56f5a6bf4707164d80c19f1169e4d10faf85f`;
the unchanged core is
`3940e48fbf00b041567f434cc7f8a4ce0976c0bc3a1dbe7a43b14157c2c9719d`.
Only `ratecurvature.cpp.o` is replaced among 26 public members. Dependency hashes
prove the retained fixture object and unchanged core apply to both worktrees.
Documentation-only follow-up can retain these results with unchanged compiled
source/dependency/archive/harness/executable identity.
