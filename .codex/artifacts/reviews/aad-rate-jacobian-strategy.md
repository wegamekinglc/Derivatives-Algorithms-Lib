# Local strategy review

Verdict: Comment Only — local implementation is ready for publication; exact-head
CI and external reviews remain open.

## Findings

The external operation-label finding is reproduced and repaired. Shared
payload/budget validators now retain the selected planner name; dense financial
admission wraps only planner DAL exceptions to preserve rate request context.
Both label tests fail before the repair and pass afterward, including overflow,
budget rejection, generic structural context and stale-plan dense fallback.

No unresolved local correctness finding. Direct dense planning checks dimensions,
overflow and combined payload before Cartesian metadata, constructs the known
row colors without conflicts, and preserves the generic recovery contract.

The cached financial overload captures current requested structure once before
binding/seeding. It uses fresh numeric recording on a match and the current
rows/axis on dense fallback. It does not catch pricing/input exceptions or return
partial matrices. The strict compressed entry point preserves stale rejection.
Compressed-only budgets can reject a larger dense fallback, as specified.

The eight new/changed production functions have complexity at most three; new test helpers
have complexity at most seven. Existing pricing formulas and native tape/Number
layouts are unchanged. The native forward capability remains unavailable rather
than being inferred from axis sizes. The dense default performs no analysis or
hidden trial pricing. Published documentation and changelog describe that behavior.

## Tests

Two RED builds reproduce the missing dense factory and cached overload.
GREEN evidence accepts 40 distinct affected tests: 13 numeric planning cases,
13 native execution cases and 14 complete financial execution cases. New cases
cover generic metadata/matrix recovery, empty and rectangular shapes, exact
budgets, oversized extents before allocation, 64-output independent analytic
deposits, fresh reuse, changed terms/rows/axes, fallback budgets and recovery.
The small exhaustive numeric oracle still checks all 512 support patterns.

Eight current strict OFF/combined diagnostic checks pass. The review repair
repeats 27 affected numeric/financial cases and reuses thirteen unchanged native
cases. Installed-only consumption passes
1/1 with warnings as errors, checking direct numeric recovery, fresh cached
execution and changed-row dense fallback against an independent deposit formula.
The original four-argument empty-settings call compiles after adding the overload.

The repaired scoped study accepts 760 observations in 9.86 seconds, no sustained
regressions and 177 identical existing archive members. The retained initial
studies are superseded by added fallback-path and operation-label coverage, without repeating unrelated
modules. See the [cost report](../performance/aad-rate-jacobian-strategy.md).

## Open questions and residual risk

No design question remains for this increment. Do not generalize the lightweight
cost data to all cashflow depths or platforms. External review bodies, Codacy,
every thread and final exact-head CI/runtime evidence must be inspected before
merge. This review does not claim those publication gates have already passed.
