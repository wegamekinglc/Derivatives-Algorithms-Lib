# Structural Jacobian numeric caller cost

Local result: no regression on existing paths established by immutable identity.
Two new numeric caller costs are informational; publication gates remain open.
This report does not claim production VJP savings or complete P04 acceptance.

## Scope and frozen provenance

Baseline is accepted #506, merged as
`bef4d87a752da7d0459eeece44f8c8799480539e`. The isolated accepted archive retains
all 176 baseline members. The head archive adds exactly one cold member,
`structuraljacobian.cpp.o`. Existing production sources/headers, core build inputs
and all 176 object files retain their bytes. Seven freshly linked accepted
solve/root/PDE/plain callers match their accepted executable hashes.

Reuse the immutable
[financial caller report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/8493c3c11e75d68dfc8f12eaf4c30a981a3014ce/.codex/artifacts/performance/aad-pde-financial-acceptance.md)
and its original prior-caller proof. Zero old timing rows repeat. New APIs are
opt-in and no existing tape, dense calibration or weighted-risk caller changes.
All ten new named cases, six strict checks and installed consumption pass before
timing; all 34 new functions have complexity <= 8.

Select only two new constructor-plus-recovery boundaries: the original D.4
3-output/4-input independent matrix with two colors, and a 64-by-64 tridiagonal
linear map with three deterministic colors. No graph/size/mode/financial matrix
belongs to this numeric-only change. Future native and provider work requires its
own affected complete-request acceptance.

## Environment and boundary

Linux WSL2, Intel i9-13900HX, GCC 15.2, C++17 `-O3 -DNDEBUG`,
`-ffp-contract=fast`, diagnostics OFF, CPU 0 affinity, `DAL_NUM_THREADS=4`,
and no workers. Twenty processes provide two rounds of ten observations per row.
Each observation contains 1,024 requests and one unmeasured warmup per row.

The numeric boundary includes fresh owning plan construction, normalization/
coloring, complete recovery, every coordinate's independent dense validation,
checksum validation and local plan/result release. Descriptors and independent
color gradients are prepared outside. No native graph recording, seed execution
or reverse sweep is represented by these timings. The matrix budget includes
directions and result; it excludes plan metadata and other caller/process memory.

| Inputs / outputs | Colors | Numeric payload bytes | Round 1 minimum (ms) | Round 2 minimum (ms) |
|------------------|--------|-----------------------|----------------------|----------------------|
| 4 / 3            | 2      | 160                   | 0.000298             | 0.000332             |
| 64 / 64          | 3      | 34304                 | 0.013242             | 0.013822             |

All forty observations retain exact full reference matrices, checksums, color
counts and numeric payloads. Sampling wall time is 0.3613 seconds. No source
edit, compilation or test overlaps pure timing. These are new-entry costs with
no historical pair or speedup/regression threshold; ordinary no-regression
acceptance follows the byte-identical existing paths.

## Evidence and publication

Session evidence retains `structural-jacobian-performance/baseline-objects.json`,
`identity-proof.json`, `identity-build.log`, raw process JSONL/stderr files,
`results.json`, the cost source/executable, strict/functional/installed logs and
earlier failures. Identity binds all old members, seven fresh callers and the
three new C++ source/header hashes. Cost evidence binds the actual archive,
executable, source, command, environment, reduction and every observation.

Require exact-head CI/Codacy and full review-body inspection, actual execution
of all new cases in fourteen profiles, repeated merge gates and tested/merged
tree equality before accepting this increment. Native execution, fresh financial
proof/identity, dynamic fallback and full-cost strategy selection remain open.
