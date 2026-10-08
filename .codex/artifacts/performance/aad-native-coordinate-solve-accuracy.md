# Native coordinate accuracy performance

Status: local scoped acceptance. Exact-head remote completion gates remain open.
Overall local verdict: no regression in the selected existing callers.

## Scope and provenance

Baseline is accepted #499 head `991ca6a45043509f7596f082e046d457b3d30faa`,
merged at `4bf66b4fa7b356cb34a6594626c33826940085cc` with equal trees.
The frozen baseline archive SHA-256 is
`0e2800a6f96de8fb0f69f04b3c5fdfad9464a56e4c9d38896290a7038a42425b`.
The final repaired archive is
`9784a22ad6a937e9dbb264bcf9acdbdfa609c791085cac452f71d1d3c31a4865`.
Source/archive/executable hashes and exact commands pin the uncommitted local
implementation used here; publication must preserve those production bytes.

The initial frozen six-row dense checked scope caught a real sustained
regression: n=32/four RHS/four channels cached reverse was +4.86% and +6.08%.
Both initial archives, executable hashes, all forty outputs and the failed
results remain retained. Disassembly showed one out-of-line AddContribution
call per matrix/RHS element, where the accepted dense scatter inlined that
point contribution. The repair forces this private helper inline without
changing its finite accumulation guard or slot operations.

Because the helper is shared, expand only to its actual callers: six existing
checked rows, six ordinary activity/width rows, two small diagnosed rows and
four packed coordinate rows. Fresh baseline/head linking uses the #499 archive;
older #498 executables are not this increment's baseline. Exactly the two native
solve objects change; archive multisets preserve 169 existing objects, including
all numeric factorization/accuracy, recording, Number/node/tape and MC/PDE/RNG
objects. No full matrix, portfolio or general tape sweep is repeated.

GCC 15.2.0, C++17, portable `-O3 -DNDEBUG -ffp-contract=fast`, static native
archive, Eigen/lifetime/profiling/native-architecture OFF, CPU 0 and
`DAL_NUM_THREADS=4`. Both rounds contain ten alternating process pairs per
family, reduced with best-of-ten minima. Reject only when both independent
rounds exceed the predeclared +4% threshold. WSL2 is a shared host; short timings
have environmental noise. Final repair and optional samples were collected
after local compilations ended, without concurrent agent compilation. This is
scoped acceptance, not a universal speedup guarantee.

## Existing callers

160 processes produce 720 row observations. All eighteen rows pass. Every
baseline/head finite checksum matches; reported resource fields match exactly.
Coordinate complete rows in the unchanged legacy workload report zero resource
fields rather than observing storage; the separate new resource study measures
both phases outside timing.

| Caller / case                                   | R1 base → head (µs)     | R1 change | R2 base → head (µs)     | R2 change |
|-------------------------------------------------|-------------------------|-----------|-------------------------|-----------|
| checked / medium-n32-rhs4-width4-full-cached    | 79.383320 → 78.864283   | -0.65%    | 79.500527 → 78.417153   | -1.36%    |
| checked / medium-n32-rhs4-width4-full-complete  | 136.975332 → 139.104600 | +1.55%    | 137.309115 → 135.911113 | -1.02%    |
| checked / small-n2-rhs1-scalar-full-cached      | 0.284906 → 0.283400     | -0.53%    | 0.284084 → 0.283130     | -0.34%    |
| checked / small-n2-rhs1-scalar-full-complete    | 0.921629 → 0.941094     | +2.11%    | 0.922026 → 0.913592     | -0.91%    |
| checked / tiny-n1-rhs1-scalar-rhs-only-cached   | 0.181443 → 0.177513     | -2.17%    | 0.184282 → 0.178476     | -3.15%    |
| checked / tiny-n1-rhs1-scalar-rhs-only-complete | 0.657224 → 0.671005     | +2.10%    | 0.651982 → 0.662479     | +1.61%    |
| coordinate / band-n64-rhs4-width4-cached        | 63.827570 → 61.229540   | -4.07%    | 62.782775 → 58.486045   | -6.84%    |
| coordinate / band-n64-rhs4-width4-complete      | 129.354950 → 128.892480 | -0.36%    | 132.072885 → 129.127010 | -2.23%    |
| coordinate / symmetric-n2-rhs1-scalar-cached    | 0.182888 → 0.180885     | -1.10%    | 0.182402 → 0.180877     | -0.84%    |
| coordinate / symmetric-n2-rhs1-scalar-complete  | 0.635539 → 0.627462     | -1.27%    | 0.626209 → 0.620641     | -0.89%    |
| diagnosed / n2-rhs1-width0-activity0-cached     | 0.196640 → 0.189282     | -3.74%    | 0.193328 → 0.188672     | -2.41%    |
| diagnosed / n2-rhs1-width0-activity0-complete   | 0.817091 → 0.820919     | +0.47%    | 0.835018 → 0.831146     | -0.46%    |
| ordinary / n2-rhs1-width0-activity0-cached      | 0.195266 → 0.188673     | -3.38%    | 0.194687 → 0.189689     | -2.57%    |
| ordinary / n2-rhs1-width0-activity0-complete    | 0.620934 → 0.610410     | -1.69%    | 0.621030 → 0.615793     | -0.84%    |
| ordinary / n2-rhs4-width8-activity2-cached      | 2.748038 → 2.718915     | -1.06%    | 2.765503 → 2.750726     | -0.53%    |
| ordinary / n2-rhs4-width8-activity2-complete    | 3.299332 → 3.318017     | +0.57%    | 3.327577 → 3.321675     | -0.18%    |
| ordinary / n32-rhs4-width4-activity1-cached     | 23.826107 → 23.553862   | -1.14%    | 24.033397 → 22.919427   | -4.64%    |
| ordinary / n32-rhs4-width4-activity1-complete   | 36.572186 → 35.986595   | -1.60%    | 36.835494 → 36.173792   | -1.80%    |

The previously failed row is now -0.65%/-1.36%. The gate, workloads and
repetition counts were not relaxed to pass. Repair evidence is separate from
the first failed implementation.

## New optional work costs

Four informational rows compare ordinary and checked native coordinate calls
linked to the same repaired archive. The ordinary API performs less work;
physical accuracy checks, condition diagnostics, retained transpose and owning
invocation reports are explicitly optional. Forty processes produce 160 rows
with equal risk checksums, valid actual report axes/limits and stable resources.

| New caller / phase                | R1 ordinary → checked (µs) | Ratio | R2 ordinary → checked (µs) | Ratio |
|-----------------------------------|----------------------------|-------|----------------------------|-------|
| band-n64-rhs4-width4-cached       | 61.420195 → 206.842300     | 3.37  | 59.831040 → 204.548340     | 3.42  |
| band-n64-rhs4-width4-complete     | 130.470335 → 503.608655    | 3.86  | 130.613165 → 498.026185    | 3.81  |
| symmetric-n2-rhs1-scalar-cached   | 0.186936 → 0.303251        | 1.62  | 0.183640 → 0.299168        | 1.63  |
| symmetric-n2-rhs1-scalar-complete | 0.627106 → 1.008076        | 1.61  | 0.627333 → 0.971722        | 1.55  |

Cached timing includes seed writes, sweep, report allocation/destruction for
checked calls and one risk read. Complete timing also includes registration,
recording, capture and teardown. Fixture setup, independent risk/primal
comparisons, report validation and resource observation stay outside timing.
The workload's mathematical reference is the already accepted numeric coordinate
pullback; independent Cramer/finite-difference correctness has separate tests.
These ratios disclose new work, not existing-caller regressions.

| Shape                    | Nodes | Retained (bytes) | Scratch (bytes) | Caller retained (bytes) | Caller peak (bytes) |
|--------------------------|-------|------------------|-----------------|-------------------------|---------------------|
| band-n64-rhs4-width4     | 764   | 47736 → 80640    | 6112 → 6144     | 4096 → 4328             | 10208 → 10472       |
| symmetric-n2-rhs1-scalar | 7     | 608 → 752        | 56 → 64         | 32 → 120                | 88 → 184            |

Resource values are measured outside both phases under the same caller budget.
Both APIs create the same input/output nodes. Checked retention adds the
physical transpose, forward errors and checked descriptor state; reverse adds
per-RHS errors and caller-owned reports. Packed parameter bindings and risks
remain O(p), with no n-squared active expansion or dense matrix gradient.
Factorization, condition and residual operations remain dense; this is not
sparse/tridiagonal/PDE complexity acceptance.

## Reproducible evidence

Session root: `dal-aad-evidence-20261008`.

- `next-native-coordinate-accuracy-performance-scope.md` freezes initial scope.
- `native-coordinate-accuracy-performance/{provenance,results,samples}.json`
  and forty raw outputs preserve the initial failed gate.
- `native-coordinate-accuracy-repair-impact.md` records the justified expansion.
- `repair-native-coordinate-accuracy.py` and
  `native-coordinate-accuracy-performance/repair-01/{provenance,results,samples}.json`
  retain commands, object identities and all 160 final raw process outputs.
- `native-coordinate-checked-cost.cpp`, `sample-native-coordinate-checked-cost.py`
  and `native-coordinate-accuracy-performance/optional/{provenance,results,samples}.json`
  retain new optional costs, exact resources and forty raw process outputs.
- `native-coordinate-accuracy-first-green.log`,
  `native-coordinate-accuracy-edges-green.log` and repair correctness logs cover
  eighteen new, twenty-five affected dense and eight ordinary/packed boundaries.
- `native-coordinate-accuracy-repair-01-strict.log`, quality JSON and refreshed
  installed-consumer log confirm both diagnostic profiles, CCN <=8 and 1/1 usage.

Exact-head CI, Codacy/review and actual eighteen-case execution in fourteen
platform configurations remain required before merge.
