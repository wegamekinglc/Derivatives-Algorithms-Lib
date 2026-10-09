# Rate quote curvature scoped cost acceptance

Status: scoped acceptance complete; existing-path control shows no regression.

## Selected paths

Only `dal-public/src/ratecurvature.cpp` adds production code. Select the complete
new single-curve (two quotes) and layered same-currency joint (four quotes)
curvature entries, including numeric admission, fresh calibration, objective
reverse, fresh common pullback and owning result assembly. Compare against
matched explicit rebuilt-gradient orchestration using the same candidate replay
primitive; these new-path comparisons are informational. Include one existing
single-curve calibration plus common pullback control linked against immutable
baseline/candidate public archives. Its +4% two-round policy is unchanged.

The source is [aad-rate-quote-curvature-cost.cpp](aad-rate-quote-curvature-cost.cpp).
Compile modes 0/1 select public/manual curvature; mode 2 selects the unchanged
first-order control. Both new sides use one signed full direction, step `2e-4`,
one warmup and five complete timed requests. Two confirmation rounds collect ten
paired process samples per side, alternate first side and reduce by minimum.
Retain all 120 samples and check numerical checksums per pair.

## Isolation and exclusions

Use merge-base `710899adcec70fb185cb4d52d24df6f2a0e77c62` and the immutable
implementation commit, separate detached sources/builds, Release with benchmarks
explicitly enabled, native architecture/diagnostics OFF and one worker. Reuse
the accepted unchanged 183-member core archive and 25 unchanged facade members
only with source/dependency/object/member hash proof; add only the new facade
object. Compile the selected harness in each required mode.

Core tape/Jacobian/solver kernels and old facade paths have identical accepted
objects. Their prior acceptance remains applicable and is not claimed newly
measured. The nine-target gate, large quote/portfolio matrices, unrelated Monte
Carlo paths and additional thread grids are outside this change's local scope.
The layered joint case includes dependency coupling; unlayered joint correctness
is independently tested without a second redundant cost matrix.

Evidence root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261010/rate-quote-curvature/performance`.
Results, environment, commands, source/binary/archive hashes and raw outputs
are retained there. Shared-host noise must be reported honestly; do not
classify a new-path overhead as an old-path regression or infer production
throughput from these small cases.

## Results

Measured implementation: `b04b4fc98cabbf44ca638269dcec6481fca1fe03`.
The original comparison at `471dcb24acd3e8534c023cf073cc0177e10bed7d` is retained.
After Codacy's shape/stencil refactor, only the two affected new entries are
resampled (80 process samples); 40 old-control samples are reused after both
rebuilt old-control executable hashes exactly match the accepted binaries.
GCC 15.2.0, `-O3 -DNDEBUG -ffp-contract=fast`, Intel i9-13900HX under WSL,
one worker, diagnostics/native architecture OFF. Load averages remain 0.31 on
32 logical CPUs; this is a shared host without an exclusive-window claim.
Snapshot construction occurs once before new-path warmup; the timed entry is
the complete `EvaluateRateQuoteCurvature` call. The old control includes its
calibration and retained-provenance construction on both sides.

| Selected case        | Reference min, ms | Candidate min, ms | Combined delta | Round 1 | Round 2 | Verdict               |
|----------------------|-------------------|-------------------|----------------|---------|---------|-----------------------|
| Single curvature     | 1.696242          | 1.667612          | −1.69%         | +0.16%  | −2.50%  | Informational new path |
| Layered joint        | 4.155623          | 4.160269          | +0.11%         | +2.77%  | −2.80%  | Informational new path |
| Existing first order | 0.609514          | 0.623810          | +2.35%         | +1.78%  | +3.59%  | No regression         |

All 120 process samples are retained; all paired numerical checksums agree.
Five timed requests per accepted sample total 1.3698 seconds of work, excluding startup,
warmup, configure and compile time. Both old-control executables have identical
SHA-256 `d810f2efb25f57db1d39b7ddf0b80f46792abaeadbda3ddf4f6c7a7c41e3dcaa`;
the measured movement is shared-host noise within the calibrated threshold.
The 25 old facade members and all core members are unchanged. No nine-target
gate or large parameter/thread grid was repeated.

The initial harness copied joint PWC knots into a single-curve spec, which failed
the existing single-curve maturity-span rule before timing. The corrected
two-quote LOG_DISCOUNT fixture is identifiable and satisfies that rule; sampling
and thresholds were unchanged. This was a harness input repair, not a library
change or a weakened numerical check.

Reproduction: `performance.py` and `performance-repair.py` in the evidence root
record the original and repair builds. `performance-codacy-repair/environment.json`
contains final source/toolchain/host and binary/archive identities;
`performance-codacy-repair/results.json` contains every accepted duration,
checksum and round minimum, including paths to the retained 40 original control
logs. Individual process logs retain raw output in both directories.

Overall verdict: no regression in the scoped existing-path control; new-path
costs are informational. The existing `rate_risk_perf` workload is the natural
place to extend production-sized rate curvature coverage when real trade-list
adapters are added; this small acceptance does not establish production scaling.
