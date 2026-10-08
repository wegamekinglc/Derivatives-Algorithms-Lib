# Sampled theta-step local review

Verdict: **Approve for publication; merge awaits exact-head CI/Codacy/review**.
The review covers the complete numeric implementation and active SPEC/API/critique.

## Findings

No open local correctness or style findings. Initial strict warnings for signed
sizes, macro dangling-else and temporary pair diagnostics are repaired without
relaxing warnings. The initial recovery fixture used an incorrect rational value;
its correct independent equation gives 103/105, with focused repair evidence.
External review identifies that ASSERT_DOUBLE_EQ tolerates zero versus the
minimum subnormal. An isolated dropped-value probe reproduces that loophole;
strict ASSERT_EQ rejects it. Both preservation tests and exact zero-seed risks
now use strict equality, with all three affected tests passing. Production
and measured workload bytes stay unchanged, so accepted timing is reused.

A subsequent external review reproduces partial member updates on failed cache
copy assignment. The new cross-layer RED changes the original time-step risk
from -0.27997520337955117 to -2.9555752033795506 after a rejected assignment.
Transactional copy-then-move repairs it. Six directly affected ownership/capacity
cases pass; zero-budget and one-byte-short failures preserve both explicit and
implicit cache solutions/risks, refund all temporary buffers, and permit zero-cost
self-assignment. This repair adds one case, bringing the suite to 28.

## Correctness and design

The generator/stencils match fixed nonuniform finite differences. Endpoint RHS
provenance removes E endpoint rows and routes lambda only to the declared source.
Reverse aggregates all output layers and directly contracts sampled coefficient,
dt/theta risks. Theta zero retains complete theta risk without factor buffers.
Adjacent swaps preserve second-upper fill; transpose reverses combined
elimination/swap order. Strong public consecutive pivots and independent private
matrix references protect this path. Physical accuracy visits at most three
actual entries and includes real transposed boundary neighbors.

The shared residual extraction preserves dense order and passes existing exact
residual and checked-coordinate/transpose tests. Nonzero products/quotients
rounded to zero reject range loss; exact representable subnormal fixtures pass.
Validation precedes buffer capture; explicit theta also validates pivot policy.
Cache copies own independent buffers, move destinations remain valid, and const
reverse owns its results/scratch. Capacity failures refund and preserve reuse.
No provider, mesh or callback derivative is inferred. No dense fallback or
quadratic condition estimator is introduced.

## Tests and evidence

- Missing public header RED, then independent n=3 analytic GREEN.
- 27 new tests pass in the final focused run; seven frozen 80-digit step references
  cover 121 coordinates independently checked at three full-solve differences.
- The subsequent transactional-assignment test passes for theta zero and 0.5;
  the six affected ownership/capacity cases pass after this production repair.
- Complete three-step n=5/two-layer rollback covers 33 shared/state/time/boundary
  coordinates independently checked by 99 complete-rollback differences.
- Seven private matrices/28 forward-transpose requests and seven exact-binary
  2000-digit residual references pass, including a ratio near 8.69e-311.
- 42 direct existing dense diagnostic/accuracy cases pass after extraction.
- 22 strict OFF/combined checks and installed CMake consumption pass. Production
  and test function complexity is at most eight; formatting/Markdown checks pass.
- Six sanitizer matrix filters include every new SampledThetaStepTest case.
  Actual remote execution still requires inspection before merge.
- 172 unchanged old object members and a passive caller preserve identity.
  Twenty actually affected existing caller rows pass two-round performance
  acceptance; twelve new cost rows disclose linear capacities and optional costs.

## Open questions and residual risk

No user clarification is required. Platform/runtime acceptance, exact-head Codacy
annotations and external review remain pending. Numeric finite-range admission
can reject representable final results when intermediate products underflow;
this supported-domain boundary is explicit. Physical accuracy is not a condition,
continuum or sensitivity-error certificate. Native recording/bindings are outside
this increment and follow in a new PR only after numeric merge.
