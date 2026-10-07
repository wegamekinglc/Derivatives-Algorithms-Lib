# Solve-coordinate scoped performance acceptance

Status: ordinary-path identity accepted locally; new-API cost observations pending.

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
commands, hashes, sample and result JSON. The accepted earlier raw observations
remain in `native-performance/`.

## Verdict

Existing ordinary runtime and boundary executable identity: accepted.
New-API informational cost: pending. Final exact-head CI remains a separate gate.
