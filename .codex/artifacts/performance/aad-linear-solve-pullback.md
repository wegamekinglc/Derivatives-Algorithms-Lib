# F03 dense solve cost and regression coverage

Status: corrected study complete; platform/publication gates remain open.
Measured source: `b88e435619d7b28752ab7ba447673a3ee3053fad`, based on merged
P03 `97567d6e7fc395fb3c7b383050edcd7416f68a4e`.
Copilot's early-RHS underflow finding is repaired with per-RHS adaptive late
scaling. Fresh study 02 measures that correction. Initial study 01, measured at
`6fc3a3c4`, remains retained as superseded evidence; it does not accept the current
numeric kernel's cost.

## Verdict and limits

All sixteen informational numeric/scalar comparisons reduce in both rounds.
Complete construction plus reverse reduces 51.74–96.63%; cached reverse reduces
14.92–95.47%. The reference is independently recorded native scalar Gauss-Jordan,
not an optimal existing general LU or integrated calibration/PDE workload.
No default route changes or whole-library speedup claim follow from this study.

Existing regression acceptance is supported separately: fourteen freshly linked
executables are byte-identical to accepted legacy/portfolio executables. They
retain the prior 162 legacy and 81 unique complete-request portfolio comparisons,
including the nine formal targets. This is reuse of measured binary identities;
no new legacy timing is claimed. Only two new opt-in production files differ
from merged P03; no scalar node, simulation, existing solver or public binding
source changes.

## Measurement scope and reproduction

GCC 15.2, C++17 Release, portable architecture; helper flags
`-O3 -DNDEBUG -std=c++17 -fPIE`, linked core flags
`-O3 -DNDEBUG -std=c++17 -fPIC -ffp-contract=fast -O3`.
WSL2, Intel Core i9-13900HX; affinity 0/2/4/6 and DAL_NUM_THREADS=4.
Both kernels execute serially on the caller; this thread environment does not
make the study a parallel workload. Exact environment is retained with the results.

`aad-linear-solve-cost-protocol-02.md` was frozen before sampling:
n = 2/8/16/32, RHS = 1/4, complete/cached, two rounds of ten alternating
numeric/scalar process pairs per case, minimum reduction. Each observation times
128 calls and divides the window by 128. Both sides create owning values and
gradients for complete calls and owning gradients for cached reverse. Validation
and tape scans are outside timing. Results remain owned until the window ends;
this batched observation is not isolated cold-request latency.

Every process constructs the scalar oracle and checks numeric X and all A/B
contributions before timing. Complete numeric calls include factorization,
forward solve, an owning X copy, reverse and temporary solver cleanup. Complete
scalar calls include input registration, graph construction, forward values,
weighted-objective reverse, owning extraction and recording cleanup. Cached
numeric calls retain LU/X; cached scalar calls retain their fixed graph and clear
all adjoints before each reverse. Scalar capacity is warmed/reused; process
startup is outside timing. Neither implementation uses an inverse.

Matrices are row permutations of dense diagonally dominant matrices: diagonal
n+2 and off-diagonal 0.125*((row+3*col)%7-3). Deterministic RHS and signed seeds
are frozen in the helper. All 640 processes exit zero and all 81,920 owning
results satisfy abs/rel 1e-10. No local build/test runs during measurement,
no sample is discarded and no threshold is relaxed. The new-function study is
informational; it does not expand the existing nine-target regression gate.

## All timing comparisons

Microseconds per call, independently reduced with min in each round. Deltas are
numeric/scalar minus one.

| Path     | n  | RHS | Scalar R1 us | Numeric R1 us | R1 delta | Scalar R2 us | Numeric R2 us | R2 delta |
|----------|----|-----|--------------|---------------|----------|--------------|---------------|----------|
| complete | 2  | 1   | 0.377578     | 0.164703      | -56.38%  | 0.377422     | 0.170148      | -54.92%  |
| complete | 2  | 4   | 0.695680     | 0.335742      | -51.74%  | 0.696562     | 0.334977      | -51.91%  |
| complete | 8  | 1   | 7.214148     | 0.761602      | -89.44%  | 7.178094     | 0.753820      | -89.50%  |
| complete | 8  | 4   | 11.439195    | 1.332398      | -88.35%  | 11.333109    | 1.311398      | -88.43%  |
| complete | 16 | 1   | 50.851328    | 2.771492      | -94.55%  | 50.053453    | 2.734000      | -94.54%  |
| complete | 16 | 4   | 68.001578    | 4.286211      | -93.70%  | 64.359602    | 4.073516      | -93.67%  |
| complete | 32 | 1   | 361.356367   | 12.165086     | -96.63%  | 353.219367   | 11.901164     | -96.63%  |
| complete | 32 | 4   | 418.191789   | 16.576789     | -96.04%  | 415.684242   | 16.441523     | -96.04%  |
| cached   | 2  | 1   | 0.111570     | 0.081062      | -27.34%  | 0.111023     | 0.081883      | -26.25%  |
| cached   | 2  | 4   | 0.198195     | 0.168617      | -14.92%  | 0.200742     | 0.164367      | -18.12%  |
| cached   | 8  | 1   | 2.029273     | 0.375008      | -81.52%  | 2.015383     | 0.390617      | -80.62%  |
| cached   | 8  | 4   | 3.266805     | 0.674398      | -79.36%  | 3.311250     | 0.680023      | -79.46%  |
| cached   | 16 | 1   | 14.529969    | 1.338680      | -90.79%  | 14.595445    | 1.309836      | -91.03%  |
| cached   | 16 | 4   | 20.681438    | 2.082492      | -89.93%  | 20.353898    | 2.014664      | -90.10%  |
| cached   | 32 | 1   | 112.764922   | 5.168227      | -95.42%  | 114.199133   | 5.177273      | -95.47%  |
| cached   | 32 | 4   | 133.955406   | 7.046383      | -94.74%  | 134.233039   | 7.073930      | -94.73%  |

## Storage accounting

All figures are bytes of element payload. Numeric retained payload is
8*(n*n+n*m)+sizeof(int)*n for LU, X and permutations; returned gradient payload
is 8*(n*n+n*m). The transpose solve buffer becomes the returned RHS gradient,
so there is no separate numeric element scratch beyond the result. Complete
calls also copy 8*n*m bytes of values. Container/object metadata, allocator
bookkeeping and caller inputs are excluded: these figures are not total allocation
or RSS. The harness separately retains 128 returned results per window.

Scalar tape logical payload/capacity come from `MeasureTape` outside timing.
Retained active input/output value containers are separately recorded in raw JSON;
the table does not claim that tape alone is total scalar memory. Both sides have
the same numeric gradient payload. Capacity includes unused cached blocks.

| n  | RHS | Numeric retained | Gradient payload | Tape logical | Tape capacity | Tape nodes | Tape edges |
|----|-----|------------------|------------------|--------------|---------------|------------|------------|
| 2  | 1   | 56               | 48               | 1224         | 1966080       | 19         | 29         |
| 2  | 4   | 104              | 96               | 2856         | 1966080       | 43         | 71         |
| 8  | 1   | 608              | 576              | 33768        | 1966080       | 433        | 1028       |
| 8  | 4   | 800              | 768              | 52968        | 1966080       | 673        | 1628       |
| 16 | 1   | 2240             | 2176             | 223656       | 1966080       | 2721       | 7176       |
| 16 | 4   | 2624             | 2560             | 295848       | 1966080       | 3585       | 9528       |
| 32 | 1   | 8576             | 8448             | 1612584      | 2621440       | 19009      | 53264      |
| 32 | 4   | 9344             | 9216             | 1892136      | 2621440       | 22273      | 62576      |

## Legacy identities and retained evidence

`aad-linear-solve-legacy-link-proof-03/provenance.json` records all fourteen
fresh commands and matching accepted SHA-256 hashes. `fresh-0/1` are scalar and
weighted helpers; `fresh-2` through `fresh-10` are the nine formal targets;
`fresh-11/12` are MC/curve-calibration targets; `fresh-13` is the accepted complete
portfolio helper. Production source/archive identities are recorded separately.

Raw process logs, all samples, results, environment and driver/protocol hashes
remain in `aad-linear-solve-cost-pairs-02`. Exact helper build commands and
source/reference/archive/executable hashes are in
`aad-linear-solve-cost-build-proof-02.json`. Source is
`aad-linear-solve-cost-01.cpp`, with the committed test's independent Augment,
ReduceColumn and GaussJordan extracted into
`aad-linear-solve-scalar-reference-01.inc`. Four preliminary smoke runs are
retained separately and do not count as samples.

Final publication must verify these frozen source/archive/executable identities;
documentation-only changes do not require repeating accepted timing. The new translation unit also compiles
without Eigen macros/include paths to a byte-identical object, and all ten
focused tests linked to that object pass, under
`aad-linear-solve-eigen-off-02/provenance.json`. Unrelated core dependencies reuse
the accepted build; this does not claim a new full Eigen-OFF workspace rebuild.

Final Windows, six sanitizer and four extended logs must contain all ten affected
cases. Exact final-head CI/Codacy/review gates and a guarded merge remain required.
