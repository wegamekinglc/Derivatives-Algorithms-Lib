# F03 dense solve cost and regression coverage

Status: study complete; platform/publication gates remain open.
Measured source: `6fc3a3c4fc6118415bf023d9d0c03485c8fd0b33`, based on merged
P03 `97567d6e7fc395fb3c7b383050edcd7416f68a4e`.

## Verdict and limits

All sixteen informational numeric/scalar comparisons reduce in both rounds.
Complete construction plus reverse reduces 52.96–96.59%; cached reverse reduces
25.26–95.68%. The reference is independently recorded native scalar Gauss-Jordan,
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
Exact environment is retained with the results.

`aad-linear-solve-cost-protocol-01.md` was frozen before sampling:
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
| complete | 2  | 1   | 0.415031     | 0.190836      | -54.02%  | 0.390641     | 0.183766      | -52.96%  |
| complete | 2  | 4   | 0.739836     | 0.222375      | -69.94%  | 0.748492     | 0.228523      | -69.47%  |
| complete | 8  | 1   | 7.754281     | 0.830281      | -89.29%  | 7.618781     | 0.791734      | -89.61%  |
| complete | 8  | 4   | 12.787102    | 1.283125      | -89.97%  | 12.098484    | 1.191555      | -90.15%  |
| complete | 16 | 1   | 55.659773    | 3.050844      | -94.52%  | 51.265813    | 2.869922      | -94.40%  |
| complete | 16 | 4   | 72.836641    | 4.393875      | -93.97%  | 69.050258    | 4.256391      | -93.84%  |
| complete | 32 | 1   | 397.547586   | 13.542586     | -96.59%  | 374.991117   | 12.790211     | -96.59%  |
| complete | 32 | 4   | 472.775609   | 18.459102     | -96.10%  | 434.392117   | 17.019938     | -96.08%  |
| cached   | 2  | 1   | 0.122992     | 0.091930      | -25.26%  | 0.114391     | 0.083023      | -27.42%  |
| cached   | 2  | 4   | 0.220203     | 0.107703      | -51.09%  | 0.208719     | 0.103484      | -50.42%  |
| cached   | 8  | 1   | 2.333305     | 0.445234      | -80.92%  | 2.110727     | 0.388219      | -81.61%  |
| cached   | 8  | 4   | 3.672078     | 0.637188      | -82.65%  | 3.674906     | 0.613773      | -83.30%  |
| cached   | 16 | 1   | 16.620117    | 1.488852      | -91.04%  | 15.430312    | 1.391977      | -90.98%  |
| cached   | 16 | 4   | 23.518812    | 2.195414      | -90.67%  | 20.209609    | 1.991852      | -90.14%  |
| cached   | 32 | 1   | 126.861414   | 5.961898      | -95.30%  | 118.673773   | 5.125164      | -95.68%  |
| cached   | 32 | 4   | 146.108398   | 8.146758      | -94.42%  | 143.090188   | 7.326305      | -94.88%  |

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

`aad-linear-solve-legacy-link-proof-02/provenance.json` records all fourteen
fresh commands and matching accepted SHA-256 hashes. `fresh-0/1` are scalar and
weighted helpers; `fresh-2` through `fresh-10` are the nine formal targets;
`fresh-11/12` are MC/curve-calibration targets; `fresh-13` is the accepted complete
portfolio helper. Production source/archive identities are recorded separately.

Raw process logs, all samples, results, environment and driver/protocol hashes
remain in `aad-linear-solve-cost-pairs-01`. Exact helper build commands and
source/reference/archive/executable hashes are in
`aad-linear-solve-cost-build-proof-01.json`. Source is
`aad-linear-solve-cost-01.cpp`, with the committed test's independent Augment,
ReduceColumn and GaussJordan extracted into
`aad-linear-solve-scalar-reference-01.inc`. Four preliminary smoke runs are
retained separately and do not count as samples.

The follow-up seed-combination test changes no production or measured executable.
Its final publication proof must verify source/archive/executable identities;
accepted timing does not need repeating. The new translation unit also compiles
without Eigen macros/include paths to a byte-identical object, and all nine
focused tests linked to that object pass, under
`aad-linear-solve-eigen-off-01/provenance.json`. Unrelated core dependencies reuse
the accepted build; this does not claim a new full Eigen-OFF workspace rebuild.

Final Windows, six sanitizer and four extended logs must contain all nine affected
cases. Exact final-head CI/Codacy/review gates and a guarded merge remain required.
