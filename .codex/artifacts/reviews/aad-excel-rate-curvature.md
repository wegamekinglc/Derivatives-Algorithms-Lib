# Excel rate curvature implementation review

Verdict: Comment Only; scoped local correctness and paired costs accepted,
Windows runtime/publication gates pending.

## Findings

No actionable production finding remains in the local review. The design review
identified null Excel output handles and the existing rate axis's absent value
field. The fixing getter now returns records with explicit snapshot presence;
quote values come from Point rather than an invented metadata field.

## Reviewed contract

Nineteen worksheet functions reuse native sealed snapshots, the closed native
trade objective, common immutable bump requests and the existing quote-axis
formatter. Four retained result types dispatch through one templated factory;
the final specification is solved anew with default options. Unsupported handles
and missing retained specifications reject through explicit/native admission.
No old fitted parameters, option overrides, numerical algorithm or Python
behavior changes.

Settings own finite signed weights and optional copied historical observations.
Absent and explicit empty history remain distinct. Dynamic trade rows identify
wrong/null types and preserve duplicates. Native admission still validates
zero-weight rows, trade-count agreement and actual PV currency. Full raw quote
gradient/product units remain unchanged. Empty directions preserve 0-by-Q shape.
Passive getters return copies and can run inside caller recordings; financial
entries retain native nested-call rejection and atomic output replacement.

Numeric and supported caller-thread recording caps remain separate. Native
execution fields are projected directly, including peak capacity and cleanup
reserve; no RSS claim is made. Archive serialization rejects explicitly.

## Local evidence

Twelve new typed cases pass, including an independent analytic deposit value,
gradient and signed finite-step Gamma; four factory routes and full axis labels;
copied weights/history; duplicate trades; detached getters; zero directions;
exact numeric and failed/recovered recording caps; wrong/null/kind/NUL inputs;
zero-weight semantic admission; bumped solve failure and caller graph/seed
preservation. Initial missing-snapshot and missing-portfolio interface RED
logs are retained outside the source checkout.

Ten strict OFF/combined probes cover the new source, typed/raw/registration
tests and the complete financial cost bridge. The actual repository CMake
binding-boundary check passes. Official generation produces 19 wrapper/help
pairs and its drift check passes. New functions have complexity at most eight;
documentation/link and patch checks pass.

## Open questions and residual risk

No API question remains. Three authored Windows raw tests cover all 19 exports,
signed analytical spills, existing worksheet construction, strict numeric/NUL
and handle admission, integer normalization, empty shapes, explicit empty
history and separate zero caps. A registration test checks exact names, argument
order/types and help. Actual Windows execution is not yet accepted locally.
Two selected complete rate requests retain 80 calibrated process observations
in 8.458444867 measured seconds, with a shortest batch of 95.489899 ms.
Independent analytical checks pass before/after each batch; all object,
dependency and executable hashes are retained. Unequal owning costs remain
informational. Exact-head CI/Codacy/review bodies and both final audits remain
required before guarded merge and tree verification.
