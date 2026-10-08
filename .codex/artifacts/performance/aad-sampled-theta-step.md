# Sampled theta-step scoped performance

Status: local acceptance passes; exact-head publication CI/review remain pending.
Baseline is merged #502 at `7ded331fa48a390a8efb2197ee3544c5da233333`,
whose tree equals final accepted `3434a0731c737230dd889837bf0d4ff41b22d2d6`.
Candidate is the frozen `feature/sampled-theta-step-pullback` production content;
its publication SHA is recorded with the evidence after commit. No source edits
occur during capture or timing.

## Scope and identity

The only changed existing production object is
`dal-cpp/dal/math/matrix/linearsolvediagnostics.cpp`, extracting the compensated
physical-row kernel into `physicalrowerrorinternal.hpp` without changing dense
evaluation order. The new files are `dal-cpp/dal/math/pde/sampledthetastep.cpp`
and `sampledtridiagonal.cpp`, with their owning public/internal headers.
All 172 other existing archive members retain exact bytes, including duplicate
member names distinguished by occurrence. Two new members are added. A freshly
linked passive `pde_perf` caller retains its baseline hash, so its accepted
behavior/performance evidence remains applicable without new timing.

Dense diagnosed/checked, checked-coordinate, numeric-root and native-root
callers actually reach the shared residual kernel. These five existing workloads
select twenty cached/complete boundaries. Ordinary unreported native dense and
coordinate solves, MC, portfolio, RNG and all other unrelated executables are
excluded. There is no full benchmark Cartesian matrix. Boundary comparisons use
the repository's calibrated sampling/threshold; they are standalone acceptance
workloads, not a run of the scheduled nine-target gate.

The new optional PDE operator has no previous implementation baseline. Three
shapes, n=3/m=1, n=65/m=2 and n=513/m=2, show theta=0/0.5 and cached/complete
costs. These twelve rows are informational costs, not a legacy speedup claim.

## Environment and method

Both archives use GCC 15.2, C++17 Release, `-O3 -DNDEBUG -ffp-contract=fast`,
static PIC, native AAD with lifetime/profiling OFF. The passive caller retains
its accepted native-architecture flags. The benchmark feature is not needed for
these standalone workload links; no disabled target is presented as executed.
CPU: 13th Gen Intel(R) Core(TM) i9-13900HX; affinity 0; `DAL_NUM_THREADS=4`.
This is a shared host. Base/head alternate first position for each pair.
Two rounds each contain ten process samples per side, reduced by minimum;
a regression requires greater than +4% in both rounds. Raw outputs, complete
sample counts, stable primal/risk checksums and resource observations are checked.
New-interface costs retain twenty head samples. Sampling took 37.97 seconds:
220 processes and 1,040 row observations. No extra confirmation was necessary.

Baseline archive SHA-256:
`02f6a3538170358ab7654d8bfa71ed1e06ce6717ca8d9d11f0fdf972e5064fb2`.
Candidate archive SHA-256:
`6d8171d6e6a441064c81700e140d3b5b8b43c3bfddda9f8407a22f96f8dee8fb`.

## Existing caller results

Nanoseconds per operation are round minima; percentages compare matching rounds.

| Caller / case                                          | Base ns R1 | Head ns R1 | Delta R1 | Delta R2 | Verdict |
|--------------------------------------------------------|------------|------------|----------|----------|---------|
| checked / medium-n32-rhs4-width4-full-cached           | 78693.93   | 78685.63   | -0.01%   | -0.78%   | Pass    |
| checked / medium-n32-rhs4-width4-full-complete         | 135677.67  | 134745.15  | -0.69%   | +2.25%   | Pass    |
| checked / small-n2-rhs1-scalar-full-cached             | 285.37     | 283.29     | -0.73%   | +2.10%   | Pass    |
| checked / small-n2-rhs1-scalar-full-complete           | 944.84     | 932.59     | -1.30%   | +0.35%   | Pass    |
| checked / tiny-n1-rhs1-scalar-rhs-only-cached          | 180.50     | 178.32     | -1.21%   | +1.82%   | Pass    |
| checked / tiny-n1-rhs1-scalar-rhs-only-complete        | 674.39     | 663.06     | -1.68%   | -2.25%   | Pass    |
| diagnosed / n2-rhs1-width0-activity0-cached            | 187.71     | 189.51     | +0.96%   | +0.02%   | Pass    |
| diagnosed / n2-rhs1-width0-activity0-complete          | 816.86     | 826.54     | +1.18%   | +0.64%   | Pass    |
| checked-coordinate / band-n64-rhs4-width4-cached       | 198555.78  | 201861.57  | +1.66%   | -1.14%   | Pass    |
| checked-coordinate / band-n64-rhs4-width4-complete     | 485575.71  | 480196.03  | -1.11%   | +0.43%   | Pass    |
| checked-coordinate / symmetric-n2-rhs1-scalar-cached   | 294.73     | 302.09     | +2.50%   | +0.00%   | Pass    |
| checked-coordinate / symmetric-n2-rhs1-scalar-complete | 991.85     | 985.40     | -0.65%   | -0.16%   | Pass    |
| root-numeric / coupled-n2-k2-width4-cached             | 451.09     | 447.37     | -0.82%   | +1.12%   | Pass    |
| root-numeric / coupled-n2-k2-width4-complete           | 706.27     | 695.70     | -1.50%   | -0.74%   | Pass    |
| root-numeric / quadratic-n1-k1-scalar-cached           | 90.23      | 91.83      | +1.77%   | -1.08%   | Pass    |
| root-numeric / quadratic-n1-k1-scalar-complete         | 275.18     | 277.28     | +0.76%   | +2.80%   | Pass    |
| root-native / coupled-n2-k2-width4-cached              | 985.81     | 988.41     | +0.26%   | +0.56%   | Pass    |
| root-native / coupled-n2-k2-width4-complete            | 37998.41   | 38509.64   | +1.35%   | -0.58%   | Pass    |
| root-native / quadratic-n1-k1-scalar-cached            | 268.86     | 265.24     | -1.34%   | -1.24%   | Pass    |
| root-native / quadratic-n1-k1-scalar-complete          | 37827.43   | 38214.28   | +1.02%   | -0.27%   | Pass    |

## New optional costs and capacity

Times are the minimum of all twenty samples. These counts exclude fixed object
fields and allocator overhead, and describe owned buffer capacity rather than RSS.
The input request is prepared before attaching each measuring budget.

| PDE case                       | Minimum ns | Cache bytes | Capture peak | Result bytes | Reverse peak |
|--------------------------------|------------|-------------|--------------|--------------|--------------|
| n3-layers1-theta0-cached       | 182.35     | 128         | 128          | 72           | 96           |
| n3-layers1-theta0-complete     | 283.51     | 128         | 128          | 72           | 96           |
| n3-layers1-theta0.5-cached     | 225.73     | 200         | 224          | 72           | 96           |
| n3-layers1-theta0.5-complete   | 571.42     | 200         | 224          | 72           | 96           |
| n513-layers2-theta0-cached     | 70786.70   | 53224       | 53224        | 20520        | 28728        |
| n513-layers2-theta0-complete   | 94195.11   | 53224       | 53224        | 20520        | 28728        |
| n513-layers2-theta0.5-cached   | 108433.34  | 71656       | 79864        | 20520        | 28728        |
| n513-layers2-theta0.5-complete | 207537.78  | 71656       | 79864        | 20520        | 28728        |
| n65-layers2-theta0-cached      | 9177.30    | 6632        | 6632         | 2600         | 3640         |
| n65-layers2-theta0-complete    | 11171.17   | 6632        | 6632         | 2600         | 3640         |
| n65-layers2-theta0.5-cached    | 13704.38   | 8936        | 9976         | 2600         | 3640         |
| n65-layers2-theta0.5-complete  | 27278.70   | 8936        | 9976         | 2600         | 3640         |

For n locations/m layers, the factor-free cache owns
`[9*(n-2)+2*n*m+m]*sizeof(double)`. Nonzero theta adds
`(4*n-4)*sizeof(double)+(n-1)*sizeof(int)` and capture peak adds another
`n*m*sizeof(double)` RHS/result overlap. The owning reverse result is
`[n*m+3*m+3*(n-2)]*sizeof(double)`; reverse peak adds
`n*m*sizeof(double)` lambda scratch. Exact-limit, one-byte-short and refund
unit tests agree with these formulae. Adjacent-pivot factors and bounded row
visits establish linear work; the three sizes support that audit with timing.
The smallest optional workload exposes allocation/validation overhead.

## Evidence and verdict

Retained under `/home/wegamekinglc/.cache/dal-aad-evidence-20261008`:
`pde-sampled-step-performance/{baseline-provenance,provenance,samples,results}.json`,
all raw JSONL outputs, build commands, archive/member hashes, workload/dependency
hashes, installed consumer logs, `pde-final-tests.log`, `pde-final-dense-tests.log`
and `pde-sampled-step-strict.{log,json}`. Initial failing numerical fixture,
strict warnings and missing-interface RED remain distinct repair evidence.

Overall: **no regression in the selected affected callers**. All twenty pass
the unchanged sustained +4% policy; all twelve optional rows have consistent
primal/risk/error/resource observations. This does not certify unrelated paths
or all continuum PDE sensitivities. Existing `pde_perf` covers passive rollback;
a future native-step increment should add only its actual recording/sweep costs.
The test-only review repair strengthens subnormal/exact-zero assertions. It
changes no production or workload bytes; the recorded archive/executable and
dependency identities preserve this acceptance without repeating measurements.

The subsequent transactional copy-assignment repair changes the new PDE object
and header. Freshly linking every previously selected legacy caller and passive
PDE caller produces byte-identical executables, so zero legacy timing rows are
repeated. Only the twelve optional PDE rows are sampled again: twenty processes,
240 observations and 1.91 seconds of timing. Primal/risk/error/resource checksums
remain identical. The original table above records the initial optional binary;
the repaired binary's minimum costs are listed below. Cache construction and
reverse capacities stay unchanged. Copy assignment separately stages one entire
copied cache before replacement, and self-assignment allocates nothing.

| Repaired PDE case              | Minimum ns |
|--------------------------------|------------|
| n3-layers1-theta0-cached       | 184.47     |
| n3-layers1-theta0-complete     | 292.42     |
| n3-layers1-theta0.5-cached     | 223.23     |
| n3-layers1-theta0.5-complete   | 575.03     |
| n65-layers2-theta0-cached      | 8833.36    |
| n65-layers2-theta0-complete    | 11354.40   |
| n65-layers2-theta0.5-cached    | 13189.11   |
| n65-layers2-theta0.5-complete  | 26924.46   |
| n513-layers2-theta0-cached     | 76802.57   |
| n513-layers2-theta0-complete   | 94894.24   |
| n513-layers2-theta0.5-cached   | 111176.58  |
| n513-layers2-theta0.5-complete | 209543.66  |

Evidence: `pde-copy-assignment-{red,green,edges}.log` and
`pde-sampled-step-performance/copy-assignment-repair/` retain the original
archive/binary/provenance, fresh-link commands, six strict checks and all repaired
optional samples. Final platform execution requires all 28 suite cases.
