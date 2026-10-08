# Checked coordinate accuracy performance

Status: local scoped acceptance. Remote exact-head gates remain open.
The [specification](../specs/aad-coordinate-solve-accuracy.md) fixes the numerical
scope; the new optional cost study is separate from existing-caller regression.

## Scope and provenance

Baseline is merged #498 (`e30858c8151e21a116c8e065056b3b91425577b8`), whose
production tree equals accepted `45ba16db`. Archive-member hash multisets account
for duplicate object basenames. 169 existing objects match baseline bytes;
only `linearsolvecoordinates.cpp.o` changes, with one added
`linearsolvecoordinateaccuracy.cpp.o`. Native event, dense numeric, recording,
Number/node/tape, MC, PDE and binding objects remain identical.

The frozen scope selects two actual native coordinate callers: symmetric n=2,
one RHS/scalar; banded n=64, four RHS/width four. Complete and cached phases
give four rows. Fresh baseline linking reproduces the accepted old caller hash.
The changed head executable requires timing; unchanged families reuse accepted
evidence. No full benchmark or local test matrix is repeated.

GCC 15.2.0, C++17, `-O3 -DNDEBUG -ffp-contract=fast`, portable ISA, Eigen/lifetime/
profiling OFF, `DAL_NUM_THREADS=4`, CPU 0. Each family has two rounds of ten
alternating process pairs, best-of-ten per round. The existing gate rejects a
change above +4% in both rounds. Raw process outputs, commands, source/archive/
executable hashes and matching finite checksums are retained. This is scoped
changed-caller evidence, not a rerun of every scheduled benchmark target.

## Existing native callers

Forty processes supply 160 row observations. All four rows pass the declared
sustained-regression rule; every baseline/head checksum matches.

| Caller/phase           | Round 1 baseline → head (µs) | Change | Round 2 baseline → head (µs) | Change |
|------------------------|------------------------------|--------|------------------------------|--------|
| Symmetric n=2 cached   | 0.219638 → 0.215404          | -1.93% | 0.213632 → 0.211181          | -1.15% |
| Symmetric n=2 complete | 0.704094 → 0.729966          | +3.67% | 0.719904 → 0.709255          | -1.48% |
| Banded n=64 cached     | 72.024580 → 71.132210        | -1.24% | 68.693030 → 74.234020        | +8.07% |
| Banded n=64 complete   | 151.014100 → 146.730885      | -2.84% | 148.239590 → 148.144715      | -0.06% |

The cached band row exceeds +4% in one round and improves in the other. It
passes the predeclared two-round rule; the fluctuation is retained, not hidden
or replaced by additional full runs. Shared-host short timings do not establish
a universal speedup or certify every unmeasured workload.

## Added optional numeric work

The comparison uses ordinary and checked numeric coordinate caches linked to
the same head archive. The ordinary API performs less work: no condition report,
physical transpose retention or compensated residual policy. These four new
cost rows are informational and are not subject to a zero-cost/+4% replacement
claim. Forty processes supply 160 row observations, with equal finite risk
checksums and valid checked error axes/limits.

| Numeric caller/phase   | Round 1 ordinary → checked (µs) | Ratio | Round 2 ordinary → checked (µs) | Ratio |
|------------------------|---------------------------------|-------|---------------------------------|-------|
| Symmetric n=2 cached   | 0.050860 → 0.128648             | 2.53  | 0.057233 → 0.136660             | 2.39  |
| Symmetric n=2 complete | 0.141917 → 0.412195             | 2.90  | 0.156154 → 0.411573             | 2.64  |
| Banded n=64 cached     | 12.023420 → 61.496005           | 5.11  | 11.513810 → 65.690295           | 5.71  |
| Banded n=64 complete   | 77.765950 → 375.005605          | 4.82  | 77.723705 → 379.365140          | 4.88  |

Complete numeric phases include construction, reverse and destruction. Cached
phases reuse the cache and include result allocation/destruction. Fixture/seed
setup, oracle comparison, report validation and resource observation occur
outside the clock. Native and numeric cached phases have different scope and
their absolute times are not interchangeable.

| Shape            | Retained ordinary → checked (bytes) | Construction peak | Full reverse capacity/peak |
|------------------|-------------------------------------|-------------------|----------------------------|
| n=2, p=3, m=1    | 56 → 96                             | 88 → 192          | 40 → 48                    |
| n=64, p=252, m=4 | 35072 → 67872                       | 67840 → 166176    | 4064 → 4096                |

Checked retention adds one physical n-by-n transpose plus m forward errors;
reverse adds m transpose errors. Construction includes the physical expansion
and condition scratch. Packed reverse allocates p risks, not n² risks. Dense
factorization/condition/residual costs remain explicit; this is not sparse or
tridiagonal solver evidence. Existing interfaces retain their original work.

## Reproducible evidence

Session evidence root: `dal-aad-evidence-20261008`.

- `next-coordinate-accuracy-performance-scope.md` freezes scope before timing.
- `coordinate-accuracy-performance/base.json` identifies the immutable archive.
- `coordinate-accuracy-performance/provenance.json` retains object identities,
  exact commands, workload/source/archive/caller hashes.
- `sample-coordinate-accuracy-existing.py` and
  `coordinate-accuracy-performance/{results,samples}.json` retain the gate and
  all forty raw process outputs.
- `coordinate-accuracy-cost.cpp`, `sample-coordinate-accuracy-cost.py` and
  `coordinate-accuracy-cost/{provenance,results,samples}.json` retain optional
  cost commands, hashes, resources and forty raw process outputs.
- `coordinate-accuracy-first-green.log`, `coordinate-accuracy-edges-green.log`
  and `coordinate-accuracy-affected.log` prove 13 new plus 44 affected old cases.

The installed consumer and strict OFF/combined ON checks pass. Exact-head
CI/Codacy/review and actual fourteen-profile new-case execution remain open.
