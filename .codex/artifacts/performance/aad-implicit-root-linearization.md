# Numeric implicit-root performance acceptance

## Verdict and scope

No existing caller regression: one optional numeric object is added, all 171
accepted archive members remain byte-identical and two fresh old caller links
retain their accepted executable hashes. Reuse the accepted coordinate/checked
solve timing; no new old-caller measurement is justified by this source change.
Four new root rows disclose optional cost and resource usage separately.

Correctness precedes timing. Twenty-two distinct root cases pass, including
independently converged three-step root/stationarity differences, non-symmetric
pivots, complete stationarity derivatives, owning/concurrent reads, inclusive
accuracy, range/recovery and exact/one-byte-short capture/reverse capacities.
An erroneous scalar inverse-range test is preserved and corrected with genuine
normalized inverse overflow; uniformly tiny scalar J remains well conditioned.
No production numerical guard or asserted tolerance is weakened.
The review repair rejects a nonzero contraction product that rounds to zero;
exact minimum subnormal output and subsequent successful requests remain
supported. Its regression has retained RED/GREEN evidence, and only the 18
affected contraction cases are rerun; four unchanged cases reuse prior passes.

## Frozen impact mapping

| Changed path                                                | Actual impact                                                 | Evidence                                              |
|-------------------------------------------------------------|---------------------------------------------------------------|-------------------------------------------------------|
| dal-cpp/dal/math/optimization/implicitroot.cpp              | New optional numeric capture and transpose/input contraction  | Four new rows below                                   |
| dal-cpp/dal/math/optimization/implicitroot.hpp              | New standalone public declaration                             | Strict/direct-header/installed usage                  |
| Existing numeric/native solves, tape, recording, MC and PDE | Existing production objects unchanged                         | 171-member identity and fresh old caller links        |
| .github/workflows/cmake-linux.yml                           | Add only ImplicitRootTest.* to six existing sanitizer filters | Actual exact-head runtime acceptance remains required |

The predeclared new rows are scalar quadratic n=1/k=1/m=1 (10,000 repetitions)
and coupled n=2/k=2/m=3 (5,000 repetitions), each cached reverse and complete
capture+reverse. Direct comparison owns the candidate and evaluates the fixed
analytic transpose formula; it performs less work than residual admission,
condition calculation, checked substitution, report and complete snapshots.
Its ratios are new optional overhead, not an existing regression verdict.
There are no finite nonlinear iterations inside either timing path.

## Environment and provenance

Compiler: `c++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0`. C++17, `-O3 -DNDEBUG -ffp-contract=fast`,
static portable build, Eigen/lifetime/profiling/native architecture OFF, CPU 0,
`DAL_NUM_THREADS=4`; this operator runs serially. Two rounds of ten alternating pairs retain forty process
outputs and 160 row observations. Report best-of-ten for each round; workload,
repetitions, sampling and the existing sustained +4% rule remain unchanged.

Baseline accepted head `2dc6d6eb06da060cdc07d11b61a5d82795465cd3`,
verified merge `f98ef99d6037a1922ef7223d8d23f005292ab53c` and equal accepted
source tree `e29024b96df36bdf3291cba00d590ce7a51eeeaf`.

| Artifact                | SHA-256                                                          |
|-------------------------|------------------------------------------------------------------|
| Baseline archive        | 9784a22ad6a937e9dbb264bcf9acdbdfa609c791085cac452f71d1d3c31a4865 |
| New archive             | 192807efb9ac4e923b55f0aae45bfd2ccba2df0dafffa5ee97b53ea7e3026d03 |
| New implicitroot.cpp.o  | 2aea79cd782c6bbcdb8f22a1224c3b7904c6104a4ee52be919b23d3917180445 |
| Checked workload source | d1c8d9dbb2f2abf5e39b6d4f5bcf9c95a72da8cb7f4e812e5fd0b9f68a8f7c0a |
| Old checked caller      | 655894dffe42e678eaf7ebcf12d14efbb8cb09b1f771c946ad75d032c2a4b11b |
| Old coordinate caller   | 7de2a3e905a1fc396ae0464ae887143890406d908503859bff146427e9ec7208 |
| Direct cost executable  | 25bfe4df8b628c07d4d0b5b55254c10f44b8d464b323dccf7128dd8f22556ecd |
| Checked cost executable | 9095fdefc5a7c03c02005b0f320c54fdd289862e86523f8eef5867b90138b600 |

## Initial new optional costs

The following paired direct/checked measurements describe the initial published
head f8ee503f59ddf8f5093c967791db91084b6701e7. The review repair is measured
separately below; do not combine independent rounds into new direct ratios.

| Case                        | Direct ns, R1/R2 | Checked ns, R1/R2 | Checked/direct, R1/R2 | Retained/capture peak bytes | Result/reverse peak bytes |
|-----------------------------|------------------|-------------------|-----------------------|-----------------------------|---------------------------|
| coupled-n2-k2-m3-cached     | 13.426 / 12.968  | 339.015 / 342.051 | 25.25x / 26.38x       | 192/304                     | 72/112                    |
| coupled-n2-k2-m3-complete   | 21.642 / 21.473  | 585.710 / 602.048 | 27.06x / 28.04x       | 192/304                     | 72/112                    |
| quadratic-n1-k1-m1-cached   | 11.403 / 11.800  | 95.089 / 96.929   | 8.34x / 8.21x         | 76/108                      | 16/40                     |
| quadratic-n1-k1-m1-complete | 18.786 / 18.744  | 281.288 / 288.991 | 14.97x / 15.42x       | 76/108                      | 16/40                     |

Every risk matches its independent direct fixture within 1e-12 outside timing;
actual residual/report axes and limits are checked. Paired checksums are finite,
nonzero and matching within the frozen floating-point comparison. Resource
counts are identical across all repetitions and agree with exact budget tests.
Direct retained capacity is 8/16 bytes and output/reverse capacity 8/48 bytes
for scalar/coupled cases; the checked results additionally own accuracy reports.

For n roots/k inputs, checked retained numeric capacity is
(2*n*n+4*n+k+n*k+1)*sizeof(double)+n*sizeof(int), and capture peak adds
(3*n*n+n)*sizeof(double). m columns return (k+1)*m*sizeof(double), with
reverse peak ((k+1)*m+2*n+1)*sizeof(double). No dense matrix adjoint is retained
or allocated. Dense condition/factorization work still costs O(n^3); reverse
costs O(m*(n^2+n*k)). These four rows do not establish large-system scaling.

## Measurement fixture correction and retained evidence

The original cost fixture attempted nested caller capacity scopes; the library
correctly rejects that unsupported attachment. No successful sample from that
fixture is used. Preserve its workload, binaries, provenance and failure in
`implicit-root-performance/initial-resource-scope-failure/`. The only repair
closes the capture scope before opening reverse scope while the root remains
alive; numerical work, resource assertions and sampling are unchanged. Only
those two cost executables are rebuilt; production/archive/old-caller proof is
reused unchanged.

Evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261008/`.
`implicit-root-performance/workload-preparation.json` freezes the original scope;
`provenance.json` preserves first build/object/link identity;
`provenance-repair-01.json` retains the scope repair, all compiler commands,
production/archive/workload/binary hashes and accepted merge;
`results.json`, `samples.json` and forty raw JSONL outputs retain all pairs.
`measure-implicit-root.py --sample --repair-resource-scope` verifies immutable
provenance before and after timing. No full benchmark matrix is run.

Platform CI, Codacy and final review remain separate exact-head gates before
merge. Native recording, PDE, sparsity/checkpoints, higher orders and bindings
remain open in the [whole plan](../plans/aad-implementation.md).

## Contraction-underflow review repair

Only implicitroot.cpp.o changes. All 171 legacy members remain byte-identical;
the workload, compiler flags, sampling and regression threshold are unchanged.
Compare the published initial root implementation with the repaired root in
two alternating best-of-ten paired rounds, forty raw outputs and 160 rows.
All four rows pass the existing rule that rejects a regression exceeding +4%
in both rounds. Negative changes are observations, not claimed speedups.
Risks, report checks and retained/peak/output capacity remain identical.

| Case                        | Base ns, R1/R2      | Fixed ns, R1/R2     | Change %, R1/R2 |
|-----------------------------|---------------------|---------------------|-----------------|
| coupled-n2-k2-m3-cached     | 336.514 / 336.177   | 330.712 / 329.488   | -1.72 / -1.99  |
| coupled-n2-k2-m3-complete   | 579.564 / 579.656   | 578.356 / 579.129   | -0.21 / -0.09  |
| quadratic-n1-k1-m1-cached   | 95.202 / 94.457     | 89.850 / 90.607     | -5.62 / -4.08  |
| quadratic-n1-k1-m1-complete | 281.393 / 278.432   | 281.851 / 276.484   | +0.16 / -0.70  |

| Repaired artifact      | SHA-256                                                          |
|------------------------|------------------------------------------------------------------|
| Production source      | a9f63f9c774f1b251308d8de2e41a63953cfecc50ed807621f9a2d04aa9f0072 |
| implicitroot.cpp.o     | 6ad61722ac27794d5de2a8e77ac66f74c1b519e85b0f30d32ebd4d7a39af4a77 |
| Archive                | 9a2a9abff81127422dcf31901503be543c8dabf5127fb778f3e4ecc6f417b398 |
| Checked cost executable | 81d80ee76787b892ff594cbeba657817b37cd2544f1be3f1ea1cb606b5528118 |

Retain `implicit-root-underflow-performance/provenance.json`, `results.json`,
`samples.json`, `sampling.log` and all forty raw JSONL files. The associated
RED, first GREEN and 18-case affected GREEN are retained separately. Strict
compilation of the two changed units in OFF/combined diagnostics and the
reinstalled consumer pass; unchanged units and direct-header bytes reuse their
initial acceptance. No legacy timing or full benchmark matrix is repeated.
Earlier successful repair samples remain in
`implicit-root-underflow-pre-format-performance/`. Final formatting changes
the source/object hashes, so only the four affected rows are refreshed against
the final formatted source and archive. The table above uses that final sample.
