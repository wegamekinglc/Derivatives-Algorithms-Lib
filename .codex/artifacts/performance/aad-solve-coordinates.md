# Solve-coordinate scoped performance acceptance

Status: ordinary-path identity and new-API cost observations accepted locally;
exact-head publication gates remain open.

## Scope selected before measurement

The production change adds `linearsolvecoordinates.cpp` and an optional header.
Existing numeric/native solve files, Number/node/tape layouts and call sites are
unchanged. No matrix, portfolio, MC, PDE or scheduled full-target matrix is needed.

Ordinary acceptance reuses #494's immutable evidence after verifying all 167
existing archive members are byte-identical to `d08b9f63`'s runtime. The only
new member is `linearsolvecoordinates.cpp.o`. A fresh ordinary native boundary
link hashes to `42f239b7d96d5bc6d34d9c22d74d6b471d1ac437665233b33b3c6ad2189d22f2`,
identical to the accepted #494 executable. This retains existing performance
acceptance rather than claiming newly measured passes for excluded workloads.

The new numeric API has no previous implementation. Informational comparisons
against the dense numeric API select three useful boundaries, each in complete
and cached-reverse phases:

| Case | Why selected |
|------|--------------|
| Symmetric n=2, m=1 | Small fixed overhead and coupled off-diagonal coordinates |
| Band n=64, below=1, above=2, m=4 | Narrow asymmetric band and multiple RHS |
| Full band n=16, m=2 | Endpoint with p=n*n and no parameter-count reduction |

Complete includes construction, reverse, output reading and destruction. Cached
includes reverse/output reading/destruction but excludes cache construction.
Both retain dense LU. These compare different APIs and are not ordinary-path
regression verdicts or a new target in the scheduled nine-target gate.

## Protocol and evidence

GCC 15.2, C++17, O3/NDEBUG, FP contraction fast, native AAD, Eigen/lifetime/
profiling/native-architecture OFF. CPU affinity 0, DAL_NUM_THREADS=4.
Two rounds of ten alternating process pairs per API, best-of-ten per round.
The same datasets and repetitions are used on each side. Every process checks
all coordinate gradients against dense contributions outside the timed window.
Checksums must agree across all observations.

Session evidence retained outside /tmp:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261008/` contains
`coordinates-baseline/provenance.json`, `coordinates-archive-identity.json`,
the cost source/sampler and the forthcoming `coordinates-cost/` raw outputs,
commands, hashes, sample and result JSON. Final observations and provenance are
in `coordinates-cost-02/`; initial observations remain in `coordinates-cost/`.
The accepted earlier raw observations
remain in `native-performance/`.

## Results

Final runtime `5587958c` retains all 167 existing members; the new object's SHA256
is `e4e70066f2d8fb5609d2ea14ae82fb631fd72e4be46f1d0c9ae1c086cfa055a9`.
Each study contains 40 processes and 240 case observations. All checksums agree.

| Case | Coordinate/dense complete, rounds 1/2 | Coordinate/dense cached, rounds 1/2 |
|------|--------------------------------------|------------------------------------|
| Symmetric n=2, m=1 | 1.223 / 1.242 | 1.074 / 1.083 |
| Band n=64, below=1, above=2, m=4 | 0.907 / 0.899 | 0.605 / 0.603 |
| Full band n=16, m=2 | 1.407 / 1.421 | 1.238 / 1.339 |

The initial full-band cached ratio was 2.077/2.025. Assembly showed repeated
row-boundary and contraction-helper calls. Hoisting validated row ends and
inlining the short contraction reduces that overhead; only the affected six
cost rows are resampled. Final 16/16 coordinate tests and a fresh installed
consumer pass, with unchanged exact budgets and finite rejection checks.

The narrow band benefits from parameter-only contraction. Small/full layouts
retain visible coordinate expansion/traversal cost; this API is not uniformly
faster than direct dense coordinates. Fully independent dense callers can keep
the ordinary operator. No sparse LU or broader speedup claim follows.

## Verdict

Existing ordinary runtime and boundary executable identity: accepted.
New-API informational cost: complete, with the above limits. Final exact-head CI
remains a separate gate. A later test/doc-only commit can reuse this runtime
evidence after confirming no production-source change.
