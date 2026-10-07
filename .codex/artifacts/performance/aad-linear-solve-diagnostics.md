# Native diagnostic solve performance scope

Status: functional acceptance is green; affected runtime validation is pending.
Baseline: merged #493, `c3f939302b2066b8b1f120e0bf513ec3b6cb8fc1`.

The only changed production translation unit is
`dal-cpp/dal/math/aad/linearsolve.cpp`. Cache-type extraction shares existing
capture/output/reverse/release logic with the explicit diagnostic operation.
The ordinary public header, native Number/node/tape headers, dispatch/reset
implementation and numeric kernels are unchanged. The new optional public
header has no current production caller outside the owning source/tests.

## Selected acceptance

| Changed path                                   | Selected cases                                             | Reason                                                     |
|------------------------------------------------|------------------------------------------------------------|------------------------------------------------------------|
| Ordinary recorded solve, all three overloads    | n=2, RHS=1, scalar; n=32, RHS=4, width=4; n=2, RHS=4, width=8 | Activity, size, RHS and scalar/vector boundaries            |
| New diagnosed solve with cached reverse        | The same three activity/mode boundaries                     | Report extraction, construction, reverse and owning cleanup |
| Native scalar allocation/reset/reverse helpers | Verified unchanged source/object provenance                 | No shared native runtime modification                      |
| Numeric LU/transpose/residual/condition kernels | Verified unchanged source/object provenance                 | Accepted #493 arithmetic remains in use                     |

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
