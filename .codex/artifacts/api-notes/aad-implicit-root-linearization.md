# Numeric implicit-root API

Active numeric increment after accepted native coordinate accuracy #500.

Public header: dal/math/optimization/implicitroot.hpp, namespace Dal. The
equation/operator describes a supplied candidate's equation linearization;
the name does not promise a nonlinear solve or certified sensitivity bound.

ImplicitRootEquation_ exposes one const Evaluate(theta,q), returning owning
ImplicitRootEvaluation_: residuals_, parameterJacobian_ and inputJacobian_.
No callback remains in the owning ImplicitRootLinearization_. Its constructor
takes equation, candidate, inputs, explicit accuracy policy and optional relative
pivot tolerance defaulting to 64*epsilon. Required policy arguments precede the
optional numerical tolerance.

ImplicitRootAccuracyPolicy_ owns residualAbsoluteLimits_ (one finite nonnegative
limit per equation) and transposeBackwardErrorLimit_ (finite [0,1]). The retained
accessors Parameters(), Inputs(), Residuals() and Policy() expose captured values
by const reference; ReciprocalJacobianConditionInfinity() reports the physical J
condition separately. No zero-RHS forward report is exposed as root convergence.

Reverse(n-by-m seeds) returns owning ImplicitRootAdjoints_: inputs_ (k-by-m)
and transposeBackwardErrors_ (m). m is an independent seed-column axis, not native
AAD width. k=0 is supported with no invented input row. Native Number inputs,
events, collections and bindings follow in a separate increment.

Example: R(theta,q)=theta^2-q at theta=2,q=4 returns input risk 1/4 for seed1;
the negative branch theta=-2 returns -1/4. Nonzero admitted residuals remain
visible and describe linearization at that candidate. Direct objective terms
are the caller's responsibility.

Wrong dimensions, nonfinite point/evaluation, invalid policies/pivot, root
residual rejection, singular J or unsupported normalized inverse range during
condition measurement reject construction. Uniform scalar scaling preserves
the condition number; reject
an unrepresentable requested transpose solution during reverse.
Invalid seed shape/value, transpose rejection, requested contribution overflow
or capacity failure expose no partial reverse result. Const cache remains usable
after a rejected reverse. Source destruction, copied results and concurrent
const readers follow owning numeric contracts; no native graph is mutated.

Rejected alternatives: treating finite nonlinear iterations as a root map,
reusing weighted underdetermined inverse semantics, hidden convergence scaling,
retaining a callback for reverse, using Gauss-Newton for a complete stationarity
Jacobian, or refactorizing/retaining an explicit inverse for every seed.
