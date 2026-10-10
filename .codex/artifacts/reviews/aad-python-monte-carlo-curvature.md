# Owning segmented Monte Carlo boundary review

## Findings

No unresolved local findings. Review found that `BlackScholesMonteCarloPlan_`
initially copied its observation vector on every by-value plan projection.
It now shares a const vector alongside the sealed kernel and copied contract,
so native plan copies have constant cost. Python getters still return detached
containers. A pre-existing partial aggregate initializer in the touched public
consumer also failed the strict probe; default construction preserves its
MODEL default and clears that warning.

## Open questions

None for the financial projection. LSMC and Excel remain separate deliveries.
CI/platform execution and current-head remote review are pending publication.

## Tests

- RED: the baseline lacks the public header and Python preparation factory.
- Six new C++ cases pass: closed preparation, independent time-zero/linear
  financial oracles, mixed HVP and metadata, historical constant replay without
  fixing reads, pre-worker errors/recovery, empty exact budget and submission-
  time mutation of plan/point/settings.
- 54 new Python cases pass, including independent finite-path analytic value,
  every gradient and actual-step HVP, separate Hessian convergence, sampling,
  tail batches, exact/short/zero budgets, historical-only/time-zero replay,
  strict conversion, GC/copy/deepcopy, GIL and two independent calling threads.
- Six selected existing shared-bump, compiled/tree direct curvature, scalar-risk
  and rate-curvature callers pass. No local full native/Python matrix is run.
- Fourteen OFF/combined strict probes pass for affected translation units and
  the retained baseline bridge; formatting and documentation are checked.
- The changed installed consumer passes in the actual staged package, alongside
  the two existing installed consumers (3/3). The installed public script API
  consumer also passes its old/global-date and new mean/curvature contracts.
- Changed public/Python targets are rebuilt. Accepted core identities and all
  26 old public objects match; old binding computation identity is retained,
  with diagnostic-path-only differences disclosed in the cost report.
- The shared-metadata refactor repeats only the two affected costs; final 80
  samples cover both new complete Python boundaries. Unequal native bridge
  contracts keep them informational, with every sample independently checked.

## Summary

The closed preparation retains immutable contract, valuation, known observations
and compiled native kernel. Evaluation copies numeric requests before GIL
release and delegates native admission/replay/reduction. Results retain their
plan and expose owning passive data. Existing signatures, numerical methods,
sampling defaults and higher-order capability flags are unchanged.

Verdict: Comment Only, pending exact-head CI/Codacy, complete remote review,
actual platform runtime checks, two final audits and guarded merge.
