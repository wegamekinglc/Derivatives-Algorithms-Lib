# Checked linear-solve accuracy acceptance

Status: local correctness, legacy identity and scoped cost accepted; exact-head
platform, Codacy and remote review gates remain open.

## Scope and provenance

Baseline is accepted #496 merge `3d597db814ca9caee26dc5372b2bf0234b78ffd2`.
Only optional `dal-cpp/dal/math/matrix/linearsolveaccuracy.hpp` and
`linearsolveaccuracy.cpp` are added; no existing numeric/native cache or header
changes. All 168 previous archive members match accepted bytes. Fresh ordinary
and diagnosed callers use current archive bytes and validated frozen include
paths: all 31 actual dependency headers match the current source. Both caller
executables match accepted hashes, so #496 legacy timings are reused.

The first relink with current include paths changed REQUIRE's embedded
`__FILE__` strings. Its failure and path investigation are retained; it was
not treated as executable identity. Only the subsequent equal-header,
equal-path fresh links establish byte identity.

Session evidence is retained outside `/tmp` under
`/home/wegamekinglc/.cache/dal-aad-evidence-20261008/`:
`checked-solve-identity.json`, `checked-solve-identity-path-investigation.json`,
`checked-solve-performance-scope.md` and `checked-solve-performance/`.
The last directory retains every raw process output, samples/results JSON,
compiler commands, environment and source/archive/executable hashes.

| Evidence                  | SHA-256                                                          |
|---------------------------|------------------------------------------------------------------|
| Accepted baseline archive | e28d400e5d7e101f98281331c9c00e6f22f29eb9483429db5a326aa9487402de |
| Current archive           | a3ab6582bd21dbc407891a41cec43e0eb84188596216c07c1d536838d19c4891 |
| Fresh ordinary caller     | 4acda986a85d2e081f56fc757012a2e2928b2791e2a019dc65e930b796d4d6de |
| Fresh diagnosed caller    | 7d0a13c8cb7602ba206ef7c0b5fbeefd41d6ad9856fb24a33146a79a4cd86028 |
| Accuracy implementation   | 871f1b264e5a5f0612632802b8a47887a4b9e30cec2d4196a4b0aa880a81182d |
| Accuracy header           | ad8dbe0049041df788d2e059a26c48cc3b68db37f38e6b381db4074a6da30603 |
| Cost workload             | 1d52a0c0f34d9e218aa695117aa2ff12559d95c7f993648a97a4ad2c388fe295 |
| Cost results              | a71307af1c1eec909846a75b98a3eeb4631b6bc1d200113978cb8b2b9019815b |

## New optional API cost

No existing target covers this wrapper. Three predeclared boundaries select
small asymmetric/full, medium multiple-RHS/RHS-only and tiny physical A/RHS-only
work; each has complete construction+reverse and cached reverse phases.
The baseline bare diagnosed operator performs less work: it has no transpose
snapshot, reverse residual evaluation or policy checks. Ratios disclose the
new optional responsibility; they are not same-workload regression verdicts.

GCC 15.2.0, C++17 Release O3, native AAD, Eigen/lifetime/profiling/native-arch
OFF, DAL_NUM_THREADS=4, affinity CPU 0. The host is shared WSL2; samples can
contain scheduling noise. Two independent rounds of ten alternating process
pairs retain the established best-of-ten reduction: 40 processes total,
six rows per process. All normalized finite risk checksums agree between sides.
All checking/report work is inside timing. Policy limits were fixed at 1e-12
before measurement, independently of timing outcomes.

Times below are microseconds; each round shows its own base/head minima.

| Case                  | Round 1 base/head | Ratio 1 | Round 2 base/head | Ratio 2 |
|-----------------------|-------------------|---------|-------------------|---------|
| n=2/m=1 full complete | 0.2522 / 0.3310   | 1.313   | 0.2501 / 0.3344   | 1.337   |
| n=2/m=1 full cached   | 0.0549 / 0.1166   | 2.124   | 0.0546 / 0.1172   | 2.147   |
| n=32/m=4 RHS complete | 41.7041 / 53.0278 | 1.272   | 41.3898 / 52.5101 | 1.269   |
| n=32/m=4 RHS cached   | 2.4152 / 12.3531  | 5.115   | 2.4382 / 12.3976  | 5.085   |
| n=1 tiny A complete   | 0.1334 / 0.1818   | 1.363   | 0.1337 / 0.1844   | 1.379   |
| n=1 tiny A cached     | 0.0243 / 0.0561   | 2.313   | 0.0246 / 0.0568   | 2.313   |

The tiny-A boundary uses A=1e-150, B=1e150 and W=1: RHS risk is finite while
unused matrix-entry risk overflows. Both sides request RHS-only work.

## Resources and correctness

Capacity results include all buffers; peak accounts for construction scratch
overlap and is not the sum of unrelated phase peaks. Values are base -> checked.

| Case         | Retained bytes  | Complete peak bytes | Reverse output/report bytes |
|--------------|-----------------|---------------------|-----------------------------|
| n=2/m=1 full | 64 -> 96        | 128 -> 160          | 48 -> 56                    |
| n=32/m=4 RHS | 9,376 -> 17,568 | 25,760 -> 33,952    | 1,024 -> 1,056              |
| n=1 tiny A   | 28 -> 36        | 44 -> 52            | 8 -> 16                     |

Additional retained transpose is exactly n*n*sizeof(double), and each returned
report adds m*sizeof(double). A separate exact 4x4/m=2 unit fixture verifies
352-byte retention, 608-byte construction peak, 208-byte full output/report and
80-byte RHS-only output/report. Exact budgets pass, one-byte-short budgets reject,
and construction/reverse/policy failures refund before successful reuse.

Twelve distinct focused cases pass across incremental GREEN logs; only changed
cases were repeated. The missing-header RED and initial test/workload compilation
errors are retained. The independent scalar rational fixture fixes the returned
solution bits and backward error 2^-55 before inclusive/one-ULP-below assertions.
Large finite risks, ownership, concurrency and unused-overflow avoidance pass.

Overall verdict: **no regression in existing paths**, established by immutable
object/header/executable identity and reused accepted evidence. New checks have
the disclosed additional cost. Full benchmark targets/matrices, simulations,
PDE and unrelated callers are outside this change and were not retimed.

Coverage advisory: extend existing matrix informational coverage if durable
benchmark coverage is requested; no additional target or universal speedup
claim is needed for this increment. Native report harvesting remains separate.
