# Native diagnostic solve performance scope

Status: functional and scoped boundary acceptance pass; exact-head CI is open.
Baseline: merged #493, `c3f939302b2066b8b1f120e0bf513ec3b6cb8fc1`.

The only changed production translation unit is
`dal-cpp/dal/math/aad/linearsolve.cpp`. Cache-type extraction shares existing
capture/output/reverse/release logic with the explicit diagnostic operation.
The ordinary public header, native Number/node/tape headers, dispatch/reset
implementation and numeric kernels are unchanged. The new optional public
header has no current production caller outside the owning source/tests.

## Selected acceptance

| Changed path                                    | Selected cases                                                | Reason                                                      |
|-------------------------------------------------|---------------------------------------------------------------|-------------------------------------------------------------|
| Ordinary recorded solve, all three overloads    | n=2, RHS=1, scalar; n=32, RHS=4, width=4; n=2, RHS=4, width=8 | Activity, size, RHS and scalar/vector boundaries            |
| New diagnosed solve with cached reverse         | The same three activity/mode boundaries                       | Report extraction, construction, reverse and owning cleanup |
| Native scalar allocation/reset/reverse helpers  | Verified unchanged source/object provenance                   | No shared native runtime modification                       |
| Numeric LU/transpose/residual/condition kernels | Verified unchanged source/object provenance                   | Accepted #493 arithmetic remains in use                     |

Use identical Release/compiler/flags/dependencies and isolated fresh-linked
executables against immutable libraries. Preserve two rounds of ten alternating
process pairs and best-of-ten reductions; a +4% excess in both rounds requires
investigation. Record same-binary controls for noisy/borderline observations.
These operator costs are boundary evidence, not an extra executable in the
repository's nine-target scheduled regression verdict.

Portfolio, Monte Carlo, PDE, calibration, RNG, interpolation and the full target
matrix are outside this production change. Their accepted evidence is reused
only where unchanged dependencies and executable identity establish applicability;
they are not newly timed passes. Required exact-head CI remains enabled.

The opt-in diagnostic is expected to cost more than ordinary solving because it
computes normalized inverse columns and residuals and copies an owning report.
Report that added capability cost separately from ordinary-path comparisons.
Do not promise that diagnostics are free or automatically enable them.

## Accepted local observations

Measured implementation: `5bfbaee1d69be781a7d095726510f42b48e5dc89`.
GCC 15.2 Release, Eigen/lifetime/profiling/native-architecture OFF,
`DAL_NUM_THREADS=4`, CPU affinity 0. Both sides use matching immutable headers,
verified library provenance and fresh links in isolated build roots. The #493
archive hash matches its accepted final evidence. Of 167 object members, 166
match byte for byte; only `linearsolve.cpp.o` differs. The final compiled native
object also matches the runtime used by the 72-test and installed-consumer runs.

Forty ordinary process runs provide two rounds of ten samples per side;
ten diagnosed runs provide informational added-capability costs. Each process
emits the six preselected complete/cached rows. Checksums agree for every path
and sample. Complete windows include recording, owning output/report extraction,
reverse, recording close and returned container destruction. Cached windows
seed and reverse one retained solve repeatedly. They do not include diagnostic
construction or pretend that those setup costs are free.

| Boundary                         | Round 1 ordinary delta | Round 2 ordinary delta | Diagnosed / ordinary |
|----------------------------------|------------------------|------------------------|----------------------|
| n2, RHS1, both active, complete  | -2.00%                 | -0.60%                 | 1.388                |
| n2, RHS1, both active, cached    | -1.15%                 | -2.82%                 | 0.993                |
| n32, RHS4, active B w4, complete | +3.35%                 | -0.10%                 | 3.189                |
| n32, RHS4, active B w4, cached   | +1.20%                 | +1.83%                 | 0.994                |
| n2, RHS4, active A w8, complete  | +3.50%                 | -3.90%                 | 1.136                |
| n2, RHS4, active A w8, cached    | -1.91%                 | +0.22%                 | 0.968                |

No selected ordinary row exceeds +4% in both independent rounds. Opposite signs
in some small-case rounds show noise; no speedup is claimed. The diagnosed
complete cost is approximately 1.14–3.19 times ordinary in these boundaries;
cached reverse remains comparable because it uses the same numeric pullback.
These observations do not constitute a fresh full benchmark-matrix verdict.

Raw process outputs, all 300 case observations, build commands/hashes,
object-identity proof and reductions remain in session evidence
`native-performance/`. Excluded modules are not newly measured passes.
