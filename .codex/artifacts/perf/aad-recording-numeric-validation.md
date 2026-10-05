# Recording lifecycle: production numeric comparison

Status: the broader D01 A12 numeric comparison passes at native-only publication
`b42f9eb9c12a5987e8a81a869f59b38d8096a1de`. This supplements the
[frozen native-only performance report](aad-native-only.md); it is untimed
functional evidence and does not modify any benchmark or regression policy.

## Identity and coverage

- Original baseline: `b5e3caca85bb1a5b1aa3acf7898b387832d443c8`.
- Separate default-OFF Release archives: `base-build/dal-cpp/libdal_cpp.a`
  and `native-only-off-build/dal-cpp/libdal_cpp.a` under the evidence root.
  Head archive was built from `8816951`; publication C++/build/benchmark
  source equivalence to `b42f9eb` is verified. No archive is rebuilt.
- Standalone probes include each side's existing benchmark fixtures.
  Both fixture source digests match exactly; paths, preprocessing, models,
  smoothing, options and query grids remain the existing workloads.
- GCC 15.2, C++17, O3/Release, matching fast FP contraction. Link-section
  collection discards the unused original benchmark main; the library
  production functions run unchanged. Commands and probe digests are retained.
- Four DAL threads and CPU affinity `4,6,8,10` match prior validation.
- Ten ordinary production rows cover vanilla/barrier tree/compiled,
  passive/AAD with the existing 200K/100K passive and 20K/10K active paths,
  plus both 100K-path passive Bermudan rows. Compare normalized price and
  every named risk, with exact label/order/vector-size agreement.
- All 25 curve workloads: four complete 4,096-query DF vectors; five
  representations crossed with analytic/bumped and diagnostics/solve-only;
  one approximate fit. Compare market/model rates, all residuals, full
  forward Jacobian and effective inverse (or explicit empty dimensions),
  residual norms, fit-mode flag and the complete added monthly DF grid.
- Every numeric cell must be finite and agree at rel/abs tolerance 1e-10.
  Deterministic functional comparison uses one evaluation per side/case;
  it is not statistical timing evidence.
- The three native LSM profiles (BS tree, BS compiled, daily LV compiled) already
  compare every price/risk at the same tolerance in all performance pairs.
  Together these cover the curve/ordinary-AAD/LSM lifecycle migrations.
  Other ordinary benchmark kernels are not claimed as a complete numeric
  cross-side matrix; their source is outside those migrations.

## Results

All 35 cases and 34,028 numeric cells pass.
Largest absolute difference is 1.42109e-14.

| Profile | Case                                                                          | Compared cells | Numeric cells | Max absolute difference | Result |
|---------|-------------------------------------------------------------------------------|----------------|---------------|-------------------------|--------|
| mc      | script engine vanilla double compiled=false (200000 paths x 1 events)         | 11             | 6             | 0                       | pass   |
| mc      | script engine vanilla Number_ compiled=false (20000 paths x 1 events)         | 11             | 6             | 1.42109e-14             | pass   |
| mc      | script engine vanilla double compiled=true (200000 paths x 1 events)          | 11             | 6             | 0                       | pass   |
| mc      | script engine vanilla Number_ compiled=true (20000 paths x 1 events)          | 11             | 6             | 1.42109e-14             | pass   |
| mc      | script engine weekly barrier double compiled=false (100000 paths x 52 events) | 13             | 7             | 0                       | pass   |
| mc      | script engine weekly barrier Number_ compiled=false (10000 paths x 52 events) | 13             | 7             | 7.10543e-15             | pass   |
| mc      | script engine weekly barrier double compiled=true (100000 paths x 52 events)  | 13             | 7             | 0                       | pass   |
| mc      | script engine weekly barrier Number_ compiled=true (10000 paths x 52 events)  | 13             | 7             | 7.10543e-15             | pass   |
| mc      | bermudan double compiled=false                                                | 11             | 6             | 0                       | pass   |
| mc      | bermudan double compiled=true                                                 | 11             | 6             | 0                       | pass   |
| curve   | PWL queries 8                                                                 | 4096           | 4096          | 0                       | pass   |
| curve   | PWL queries 24                                                                | 4096           | 4096          | 0                       | pass   |
| curve   | PWL queries 64                                                                | 4096           | 4096          | 0                       | pass   |
| curve   | PWL queries 256                                                               | 4096           | 4096          | 0                       | pass   |
| curve   | PWC BUMPED SOLVE                                                              | 364            | 364           | 0                       | pass   |
| curve   | PWC BUMPED DIAG                                                               | 916            | 916           | 0                       | pass   |
| curve   | PWC ANALYTIC SOLVE                                                            | 364            | 364           | 0                       | pass   |
| curve   | PWC ANALYTIC DIAG                                                             | 1468           | 1468          | 0                       | pass   |
| curve   | PWL BUMPED SOLVE                                                              | 364            | 364           | 0                       | pass   |
| curve   | PWL BUMPED DIAG                                                               | 1468           | 1468          | 0                       | pass   |
| curve   | PWL ANALYTIC SOLVE                                                            | 364            | 364           | 0                       | pass   |
| curve   | PWL ANALYTIC DIAG                                                             | 2572           | 2572          | 0                       | pass   |
| curve   | LOG_LINEAR BUMPED SOLVE                                                       | 364            | 364           | 0                       | pass   |
| curve   | LOG_LINEAR BUMPED DIAG                                                        | 916            | 916           | 0                       | pass   |
| curve   | LOG_LINEAR ANALYTIC SOLVE                                                     | 364            | 364           | 0                       | pass   |
| curve   | LOG_LINEAR ANALYTIC DIAG                                                      | 1468           | 1468          | 0                       | pass   |
| curve   | LOG_CUBIC_NATURAL BUMPED SOLVE                                                | 364            | 364           | 0                       | pass   |
| curve   | LOG_CUBIC_NATURAL BUMPED DIAG                                                 | 916            | 916           | 0                       | pass   |
| curve   | LOG_CUBIC_NATURAL ANALYTIC SOLVE                                              | 364            | 364           | 0                       | pass   |
| curve   | LOG_CUBIC_NATURAL ANALYTIC DIAG                                               | 1468           | 1468          | 0                       | pass   |
| curve   | MIXED BUMPED SOLVE                                                            | 364            | 364           | 0                       | pass   |
| curve   | MIXED BUMPED DIAG                                                             | 916            | 916           | 0                       | pass   |
| curve   | MIXED ANALYTIC SOLVE                                                          | 364            | 364           | 0                       | pass   |
| curve   | MIXED ANALYTIC DIAG                                                           | 1468           | 1468          | 0                       | pass   |
| curve   | LOG_LINEAR APPROXIMATE                                                        | 364            | 364           | 0                       | pass   |

## Retained evidence and conclusion

Evidence root: `/tmp/dal-aad-evidence/native-numeric-audit`. It contains
`results.json`, `provenance-final.json`, complete baseline/head numeric
outputs, stderr, compiler/link commands, build logs and probe binaries.
The sibling `native-numeric-audit.log` records all four terminal executions.
Archives and standalone helper digests remain unchanged after execution.
Raw evidence is local; these paths are not downloadable PR artifacts.

D01 A12 now has direct affected-workload numeric comparison alongside
the unchanged performance gate, resources/capacity tests and independent
mathematical unit oracles. D01 can be accepted with the native contract
audit and all 28 exact `b42f9eb` CI checks. P01's remaining production
phase/resource/scaling scope and all later development stages remain open.
