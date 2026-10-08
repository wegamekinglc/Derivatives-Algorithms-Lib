# Owning implicit-root linearization: numeric increment

Active F03 numeric increment on `feature/implicit-root-linearization`, based on
accepted native coordinate accuracy #500 (`f98ef99d`). Existing nonlinear solver,
effective-inverse, native event and binding contracts remain intact.

## Method and point identity

For a differentiable square equation R(theta,q)=0 with locally invertible
J=R_theta and complete K=R_q, solve J^T lambda=w and return -K^T lambda.
The caller supplies the candidate and selected local branch; this operator
does not solve nonlinear iterations, select a different root or regularize J.
Direct objective dependence on q is added by the caller.

Evaluate one const equation once at owning copies of theta and q. Its owning
evaluation contains residuals, n-by-n parameter Jacobian and n-by-k input
Jacobian. Return all three from the same point. Reject nonfinite values or
incompatible extents. The equation must provide complete derivatives of its
declared method, including residual-Hessian terms for stationarity equations.
The operator validates shape/range/admission; supplied Jacobian correctness
remains a caller obligation, illustrated by independent concrete fixtures.

Retain owning candidate, q, residuals, per-equation accuracy limits, K and one
CheckedLinearSolve_ J cache. Do not retain the callback or an explicit inverse,
form an n-by-n matrix gradient during reverse, or refactorize per seed.
The equation object and original containers may be destroyed after capture.
Concurrent const reverse calls use independent local scratch/results.

## Admission and public result

Require n>0 and k>=0, with supported int/matrix byte extents. Before invoking
the equation, require finite theta/q, exactly n finite nonnegative absolute
residual limits, a finite [0,1] transpose backward-error limit and legal pivot
tolerance. Admission is inclusive: abs(R_i)<=limit_i in each equation's own
units. Zero means exact observed zero. Infinity is never a legal limit.

An admitted nonzero residual produces a linearization at that candidate.
Residual admission alone is not a bound on root or sensitivity error. Preserve
the observed residuals and limits; never present the zero-RHS J factorization
report as root convergence evidence. Expose physical reciprocal Jacobian
condition separately; keep its existing unsupported normalized-inverse-range
policy. Uniformly tiny scalar J is well conditioned; a requested transpose
solution outside the representable range rejects during reverse.

The numeric result is ImplicitRootLinearization_. Reverse accepts n-by-m root
adjoints with m>0 and returns ImplicitRootAdjoints_: k-by-m inputs_ and m
independent transposeBackwardErrors_. Validate all seeds first. k=0 returns a
zero-row/m-column matrix without inventing a risk axis, while still checking
the requested transpose solves. Multiple RHS here are independent root-seed
columns, not an AAD width dimension.

Build J with a one-column zero RHS and use checked RHS-only reverse for each
seed column through the same cache. Never conflate the root residual policy
with transpose componentwise backward error or pivot tolerance. Every returned
risk is finite; unsupported intermediate range rejects rather than clipping.
A failed reverse exposes no partial result and leaves the immutable cache
usable for a subsequent supported request.

All owning numeric containers and reverse scratch participate in active caller
buffer budgets, including overlap peaks. Construction and reverse allocation
failure refund admitted storage. Do not add native tape mutation in this PR.

## Fixed independent acceptance fixtures

1. R=theta^2-q, theta=2/q=4: w=1 gives 1/4. Negative branch theta=-2 gives
   -1/4. theta=1.9 is rejected by a tight residual limit; a permissive limit
   admits its explicit captured-point derivative and unchanged residual.
2. Coupled R=(theta0^2+theta1-q0,theta0+theta1^2-q1), theta=(2,3),q=(7,11):
   w=(1,-2) gives (8,-9)/23. Independent complete root solves at three central
   steps check every q coordinate; J*dtheta+K*dq=0 verifies the direction.
3. Non-symmetric R=(theta0^2+2theta1-q0,3theta0+theta1^2-q1), theta=(2,3),
   q=(10,15): w=(1,-2) gives (2/3,-5/9). theta=(0,3),q=(6,9) requires a
   pivot and gives (-2,1/3). These catch an incorrectly untransposed solve.
4. Stationarity G=2theta(theta^2-q0)+theta-q1 at theta=1,q=(0,3) has G=0,
   J=7,K=(-2,-1), so risks are (2/7,1/7). Its underlying least-squares
   residuals (1,-2) require the full derivative; Gauss-Newton's (2/5,1/5)
   is the wrong map for this contract.
5. Exact rational linear residual/transpose boundaries use inclusive limits
   and nextafter-below rejection. Explicit small legal pivots preserve huge
   finite risks. Converged singular J, nonfinite evaluations/seeds and finite
   overflowing requested contractions reject without corrupting the cache.

First missing-header RED and analytic GREEN precede edge import. Cover owning
copies, original source/equation destruction, repeated/concurrent reverse,
multiple positive/negative/zero seed columns, k=0, malformed dimensions,
invalid policies, exact construction/reverse budgets and one-byte-short refund.
Strict units/direct header in OFF/combined diagnostics ON, CCN <=8, installed
consumer, current-state methodology and qualifying changelog are required.

For n roots/k inputs, the intended owning numeric buffer extent is
(2*n*n+4*n+k+n*k+1)*sizeof(double)+n*sizeof(int). It includes the checked J
cache, candidate/input/residual/limit snapshots and K, with no retained extra J.
The intended capture peak adds (3*n*n+n)*sizeof(double) for original evaluation
J, zero RHS and condition scratch. For m reverse columns, returned capacity is
(k+1)*m*sizeof(double), and intended peak is ((k+1)*m+2*n+1)*sizeof(double).
Verify these claims against actual tracked allocations, including exact and
one-byte-short budgets; refine the design before implementation if ownership
requires another retained buffer, rather than silently weakening assertions.

The new ImplicitRootTest suite is absent from the current six sanitizer filters.
Its production PR must add exactly that affected suite to those existing
filters and verify actual case execution; current PR #500 needs no such change.

## Performance and publication boundary

This increment should add one optional numeric object only. Prove existing
archive-member identity with duplicate basenames and fresh links before reusing
accepted solve/native/tape/MC/PDE evidence. If a shared object changes, expand
only to its actual callers. No full benchmark matrix follows from a new API.

New root costs are informational. Freeze two cases before production edits:
scalar quadratic n=1,k=1,m=1 and coupled n=2,k=2,m=3, each cached/complete.
Compare with direct analytic root linearizations doing less work, disclose
condition/residual/ownership costs, and retain matching root/risk checksums,
resources, source/archive/executable hashes and raw paired samples. Require
actual new cases in all applicable exact-head platform jobs, inspect paginated
CI/Codacy/review, and merge before the subsequent native root increment.
