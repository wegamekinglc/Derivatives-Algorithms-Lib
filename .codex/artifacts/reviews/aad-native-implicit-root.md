# Native implicit-root local review

Verdict: **Comment Only** until exact-head platform CI, Codacy and PR review
finish. No blocking local correctness/design findings remain. Merge this
increment before changing production PDE code.

## Findings

No unresolved local findings. The new operator owns one accepted numeric
linearization and Number input bindings captured before equation evaluation.
Reverse reuses the checked numeric transpose/contraction and existing native
accumulation, output clearing, event ownership and report collector. The
three narrow collector bridges expose no public cache or duplicate lifecycle.
Generic output deduction accepts a vector; decltype(auto) preserves the old
linear diagnostics reference and new owning root value. Old overloads retain
their matrix return types.

## Tests and proof

The missing-header RED and minimum composed-root GREEN are retained in
native-implicit-root-red.log and native-implicit-root-first-green.log.
Subsequent focused batches run only newly added cases: 12 reference/lifecycle
cases, three capacity cases, then six additional boundaries. All 22 pass.
Existing methods are tested once after the shared changes: all 115 cases in
the compiled AADLinearSolveTest suite pass in native-implicit-root-legacy-tests.log.

Independent elementary and Cramer oracles cover positive/negative branches,
nonsymmetric/pivot systems, independent channels and direct terms. Complete
stationarity expects 2/7 and 1/7, distinguishing the residual-Hessian term.
Serial roots match elementary sqrt composition. Inclusive rational transpose
errors retain the actual 2^-55 value across nonzero channels and exact zeros;
a limit one representable step lower rejects ordinary and collected reverse.

Mixed dense/root/coordinate windows preserve order and owning historical
reports through restore/close. Aliases sum declared equation contributions.
The alias-overflow fixture first proves numeric per-column risks are finite,
then shows only native accumulation overflows and invalidates recording.
Underflow, tiny well-conditioned/unrepresentable transpose, stale unoccupied
input and rejected residual paths preserve unpublished output/report semantics.
Null/wrong-phase calls reject before evaluation and preserve a usable scope.

Resource admission tests cover scalar and widths 1/4/8, one/two parameters,
exact tape/caller peaks, one-byte-short failures, refunds and independent
recovery. Initial fixture expectations omitted reverse scratch from caller
peak and mistook tape cleanup headroom for measured occupancy; those failed
logs are retained. Correct expectations follow existing budget contracts;
no production budget logic or threshold changed.

All 20 strict C++17 -Wall -Wextra -Wpedantic -Werror source/direct-header checks
pass in OFF and combined lifetime/profiling ON. Formatting passes for all
affected source/header/test files; 114 functions have maximum CCN six.
The fresh installed find_package(dal-cpp)/DAL::cpp consumer passes composition,
actual accuracy reports, caller ownership and detached observations.

[Scoped performance](../performance/aad-native-implicit-root.md) accepts all
22 existing caller rows with two best-of-ten rounds and sustained +4% gate.
Four new optional costs disclose full native lifecycle overhead. Numeric
root object identity permits accepted evidence reuse. Raw and failed evidence
remain outside the source tree.

## Open questions and residual risk

There are no unresolved API choices. The C++ interface supplies first-order
derivatives of the caller's complete same-point equation and passive branch.
It supplies no nonlinear solver, global branch choice, sensitivity-error
certificate, higher-order mode or Python/Excel wrapper in this increment.
Concurrent const numeric caches remain covered by accepted #501; native
recordings keep owner-thread semantics. Default builds retain raw Number
lifetimes; optional diagnostics add generation/epoch checks.

All six existing sanitizer filters already select AADLinearSolveTest.
Publication must prove actual execution of each of the 22 new cases in four
MSVC, six sanitizer and four extended profiles, not only green job labels.
Exact-head paginated checks/comments/threads/Codacy and review evidence,
repeated final audits and guarded merge/tree verification remain outstanding.
