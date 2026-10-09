# Native Dupire quote-curvature review

Verdict: Comment Only; local implementation/costs accepted, publication pending.

## Findings

No open implementation finding after read-through of the changed API, passive
calibration, complete gradient composition, independent tests and controls.
The strict-warning test-macro dangling-else finding is corrected with explicit
braces; only affected strict probes repeat. No production behavior changed.
PR #520's initial Codacy finding reports mathematical-test cyclomatic complexity
14 against a limit of eight. Extracting the independent value-difference
reference and comparison keeps every assertion, step and tolerance unchanged;
the affected mathematical case and its two strict profiles pass again.
Production objects, installed consumption and accepted costs are retained.
Codex inline `4232923168` identifies two benchmark-labelled aliases that the
scheduled Linux/Windows jobs incorrectly treat as executable names. A focused
registration check reproduces both missing-name contracts. The repaired
`quote_risk_perf` default entry runs the new flat/mixed cases; explicit selection
remains available and only the real executable retains a benchmark CTest label.
Reconfiguration, the name contract, default/flat/mixed executable smoke and five
affected strict profiles pass. The timed request body and production archive
retain their identities, so accepted paired samples are reused.
Copilot full review `5473438407` has zero inline findings but recommends changing
October 10 headers/changelog based on October 9 UTC. The user workspace is
Asia/Shanghai, with supplied date October 10; Git records publication at
`2026-10-10T01:38:50+08:00`. The dates are correct and this body-only finding is
dispositioned with timezone evidence rather than a source/date change.

## Mathematical and lifecycle evidence

Missing recalibration and curvature entries each produce the expected RED.
Twenty-two selected tests pass: ten new quote cases and twelve existing Dupire
snapshot/pullback cases. One separate exhaustive allocation-injection case
passes. It fails each allocation of a complete three-gradient request, checks
width-four mode restoration and immediately verifies a valid recovery.

Direct quote Gamma/cross and signed products match exact polynomial derivatives.
A nonflat surface/quote objective matches independent fresh-calibration value
differences at all six coordinates for all three predeclared outer steps.
The reference uses a fixed `2e-4` coordinate step and tolerance
`0.08 + 0.02 * abs(reference)`, reflecting numerical call-stencil noise rather
than an analytic Hessian precision claim. Observed maximum disagreement is
about `0.00329`. A linear surface objective has nonzero quote curvature and
therefore rejects a frozen-Jacobian implementation.

The initial aligned-knot reference fixture rejects a perturbed convexity domain;
the existing nonaligned quote axes provide the admitted smooth comparison.
Steps and tolerances are unchanged. Invalid-domain requests retain independent
early-admission coverage. Ownership, original-IVS destruction, direct terms,
all-bump preflight, exact numeric/tape budgets, nested recording preservation,
callback phase failures, width-four recovery and concurrent callers pass.

## Integration

Seventeen strict probes pass in ordinary and combined diagnostic/profiling
configurations, including the manual cost reference. The installed-only
`DAL::cpp` CMake consumer passes. The accepted 182-member archive replaces one
Dupire object and adds one curvature object; the other 181 members retain their
bytes. Fresh base/head MC test links are identical, retaining accepted MC and
generic correctness applicability without another runtime matrix.

Six sanitizer filters include the new core suite, and the isolated allocation
target/filter includes its failure-injection suite. Actual execution in all
fourteen CI runtime profiles, every current-head check, Codacy annotation,
complete review body and inline thread must still be inspected before merge.

## Open questions and limits

No user clarification is needed. This delivery accepts smooth sequential native
objectives on raw quote coordinates. Complete MC quote adaptation, rate-provider
rebuilding, report/binding projection, estimator policy and native mixed mode
remain visible planned work. Public docs and changelog state this boundary.

## Evidence

Session directory: `dal-aad-evidence-20261010/dupire-quote-curvature`.
Retain `library.json`, `correctness.xml`, `allocation-final.xml`, `strict.json`,
installed consumer logs and raw mathematical comparisons. Cost evidence and
raw samples establish two informational complete-request cases and one passing
existing Dupire pullback control: 120 processes in 0.2875 seconds. Exact-head
runtime/review/merge proof remains required before publication acceptance.
