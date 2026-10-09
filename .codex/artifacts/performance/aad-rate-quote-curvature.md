# Rate quote curvature scoped cost acceptance

Status: scope fixed before measurement; correctness and strict probes pass.

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
will be retained there. Shared-host noise must be reported honestly; do not
classify a new-path overhead as an old-path regression or infer production
throughput from these small cases.
