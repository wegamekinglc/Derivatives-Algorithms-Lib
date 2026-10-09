# Local strategy review

Verdict: Comment Only — local implementation is ready for publication; exact-head
CI and external reviews remain open.

## Findings

No unresolved local correctness finding. Direct dense planning checks dimensions,
overflow and combined payload before Cartesian metadata, constructs the known
row colors without conflicts, and preserves the generic recovery contract.

The cached financial overload captures current requested structure once before
binding/seeding. It uses fresh numeric recording on a match and the current
rows/axis on dense fallback. It does not catch pricing/input exceptions or return
partial matrices. The strict compressed entry point preserves stale rejection.
Compressed-only budgets can reject a larger dense fallback, as specified.

The new production functions have complexity at most three; new test helpers
have complexity at most seven. Existing pricing formulas and native tape/Number
layouts are unchanged. The native forward capability remains unavailable rather
than being inferred from axis sizes. The dense default performs no analysis or
hidden trial pricing. Published documentation and changelog describe that behavior.

## Tests

Two RED builds reproduce the missing dense factory and cached overload.
GREEN evidence accepts 38 distinct affected tests: 12 numeric planning cases,
13 native execution cases and 13 complete financial execution cases. New cases
cover generic metadata/matrix recovery, empty and rectangular shapes, exact
budgets, oversized extents before allocation, 64-output independent analytic
deposits, fresh reuse, changed terms/rows/axes, fallback budgets and recovery.
The small exhaustive numeric oracle still checks all 512 support patterns.

Eight strict OFF/combined diagnostic checks pass, reusing six unchanged checks
after the final numeric helper placement. Installed-only consumption passes
1/1 with warnings as errors, checking direct numeric recovery, fresh cached
execution and changed-row dense fallback against an independent deposit formula.
The original four-argument empty-settings call compiles after adding the overload.

The final scoped study accepts 760 observations in 8.95 seconds, no sustained
regressions and 177 identical existing archive members. The retained initial
study is superseded by added fallback-path coverage, without repeating unrelated
modules. See the [cost report](../performance/aad-rate-jacobian-strategy.md).

## Open questions and residual risk

No design question remains for this increment. Do not generalize the lightweight
cost data to all cashflow depths or platforms. External review bodies, Codacy,
every thread and final exact-head CI/runtime evidence must be inspected before
merge. This review does not claim those publication gates have already passed.
