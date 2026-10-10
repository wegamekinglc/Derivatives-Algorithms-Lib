# Segmented Monte Carlo projection critique

Verdict: Proceed with caveats.

## Blocking issues

None in the scoped design. It adds a closed owning financial boundary over
accepted native algorithms and does not require a shared core-header change.

## Significant concerns

- A preparation factory resolves history before later numerical admission.
  Document that boundary explicitly; test that evaluation never re-reads history.
- Historical-only products still retain the four model coordinates. Do not infer
  dimension from Gaussian count or lose constant derivatives through historical
  initialization. Test both historical-only and mixed historical/live contracts.
- Result settings must preserve requested path caps while execution reports the
  minimum bump/path recording limit. Do not silently flatten away optional zero.
- Sampled curvature is a finite-step first-gradient secant. Smooth analytic
  tests need actual-step secants and a separate convergence assertion; hard
  payoff Gamma cannot inherit a smooth second-order accuracy claim.
- Readonly native member bindings can borrow nested state. Return container and
  settings copies and retain the plan in every result, including first order.
- Strict integer conversion must distinguish the 64-bit scramble seed from
  platform `size_t` budgets, especially on Windows builds.

## Minor notes and alternatives

Accept list/tuple points rather than arbitrary iterables. Keep settings keyword
only and reuse shared bump conversion. A generic objective callback or strategy
framework would expand the scope without improving the financial use case.
The native core is unchanged: old native timing should be reused with identity
proof rather than repeated. New Python/C++ boundary timing remains required.

Author questions: none. Review implementation against these caveats before PR.
