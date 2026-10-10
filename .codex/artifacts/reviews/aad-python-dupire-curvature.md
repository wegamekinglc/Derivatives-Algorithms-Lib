# Python Dupire curvature local review

Verdict: Approve. Local correctness and scoped costs are accepted; current-head
publication gates remain required before merge.

## Findings

No unresolved correctness, ownership or compatibility findings in the reviewed
working-tree change from accepted #528 (`2f1f6f9008b6a0d9e800e718b5eaca5a2d9d4019`).
The full new binding, module/CMake registration, new tests, active specification,
API/critique, documentation and ledger changes were read. Both native libraries
and existing binding function bodies remain unchanged.

The constructor requires copied native matrices and strict numeric sequences;
budgets preserve optional zero. Native planning retains quote-dependent dimension,
finite perturbation, nonzero-row and combined payload validation. All factories
copy Python values before GIL release. Getters return passive copies; no active
number, callback or borrowed request/result state escapes. Method labels retain
the finite-step estimator, full raw axes and separate reported base projection.

The review identified missing direct `<utility>` inclusion before publication;
it is present in the final binding, followed by a rebuilt/installed extension and
strict checks. The draft scope initially described all-zero rows as accepted;
reading the native validator corrected the scope before implementation. The
planner rejects such rows, while zero-row matrices retain base-only semantics.

## Open questions

None required for this bounded Python projection.

## Tests

RED: the existing installed Python package raises `AttributeError` for the absent
public bump-request constructor. GREEN: 61 new cases pass. Installed-package
acceptance passes all 174 selected new/Dupire-risk/Dupire-request cases in 1.85
seconds, including analytic signed Gamma, independently recalibrated mixed
surface/direct products, exact and failing budgets, copied values, frozen history
and dates, repeated calls, GIL release and two calling threads. Four strict
OFF/combined module/binding checks pass. Documentation checks pass for 166 files.

Two scoped entry-cost cases complete eighty paired observations in 3.465331108
measured seconds. Both modes pass analytic checks and preserve fixed-affinity,
one-worker, two-round/ten-pair sampling. Native complete admission is more
expensive than manual first-order composition; this informational comparison
does not establish a regression or speedup. Source/module/library/header hashes
are retained, with no unrelated native timing repeated.

Codacy's three cost-driver assertion findings are repaired with explicit checks
that remain active under optimized Python. The same two affected costs have
eighty refreshed observations (3.568148566 measured seconds); original evidence
is retained. Native and binding objects/tests are unchanged, so no correctness
suite repeats. Maximum cost-function complexity remains six.

The original non-PIC static archive cannot link a Python shared extension; its
failed link is retained. Fresh official PIC core/public builds and standalone
extension installation resolve the configuration boundary. That necessary
library build adds no full-suite or full-performance execution.

## Summary and residual risk

The additive Python surface projects the accepted native financial adapter.
Do not treat local OFF runtime plus combined syntax as remote diagnostic/platform
acceptance. Inspect actual new Python test execution in the extended and Windows
profiles, all required CI/Codacy and complete review bodies before guarded merge.
Excel/rate/other structured surfaces remain separate roadmap work.
