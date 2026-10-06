# PR #480 concentrated review and repair

Verdict: Merged; final review-repair head `38eaceac` passes all 35 checks.
Repeated exact-head/master/policy/status/review audits pass, including Codacy
zero issues and annotations and both resolved threads. The guarded squash merge
is `079c9d520db17dde7017b329aff0ddc4c6a22afb`; its tree equals the accepted head.
The registration repair at `64bf01a6` passes all 36 checks, including all four
actual Windows configurations and the unchanged registration assertion.
No further local correctness or compatibility finding remains from the reviewed
native, calibration/request, binding, build/export and documentation boundaries.

## Findings

Resolved locally: `dal-excel/src/_excel.cpp` previously copied only explicit
input help. Machinist's three new multi-output getters append a `format`
argument/type/name but omit its help. All four real Windows configurations at
`7a46b93219055d7a591727d0860e80e4a1106e1a` fail the unchanged global registration
count assertion, first at `xl_DupireScriptRiskSettings_Get_Configuration`.
Native request, raw-guard and new registration-contract tests otherwise pass.
The repair completes only a single missing trailing format-help item in the
stored production registration. Explicit help, other missing help, argument
names/types and generated files remain unchanged. No assertion is relaxed.

Resolved performance acceptance: the complete current-head default-OFF MC
confirmation passes 44/44 original cases. Every earlier failed/inconclusive
capture remains. See the [complete final rows](../performance/aad-production-final-confirmation.md).
The accepted curve, nine-target and affected-entry gates remain separate.

Resolved locally after the automatic review at `64bf01a6`: the standard-header
group must precede DAL includes in `dal-cpp/dal/benchmarks/aad.hpp` and
`dal-cpp/dal/math/aad/blocklist.hpp`. Restore that ordering with separated
groups so clang-format preserves it. Only includes move; no declaration,
expression, layout, recording or benchmark workload changes. Keep the accepted
mathematical and performance evidence and require the final publication's own CI.

Resolved locally from the automatic review overview: the native-header fixture
`tests/native-aad/header/main.cpp` needs the three-line Created-by file header.
Add that comment without changing its include or executable body. This overview
nit was not represented by either review thread; both the overview and all
paginated threads are included in the final review audit.

## Review scope and F01 requirement reconciliation

The PR base is `2fc748efe64bcbf6e8ad822967988a8a1e3bcb7a`; remote master still
matches it at intake. Review PR metadata, all branch commit subjects and changed
module inventory. Reconcile the accepted component reviews with the current
native/math/lifecycle and public request sources, bindings, native-only
configuration/export/CI changes and published guides. Preserve independent
oracles and inspect the exact current publication rather than an unrelated branch.

| Boundary                  | Evidence and remaining gate                                                                                                                                 |
|---------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------|
| C01–C05/D00–D03           | Accepted propagation/recording/lifetime/native-only component reviews and exact-head CI; removal/migration/export paths remain native only                  |
| Scalar D04                | Accepted typed axes, passive ownership/provenance, numeric budgets and C++/Python/Excel compatibility                                                       |
| Frozen Dupire/Hybrid      | Accepted full snapshot identity, contracted replay, complete node inputs and original common-path/recalibration oracles                                     |
| Common calibration        | Accepted four native curve providers/both inverse modes, captured coordinates, inverse scaling, actual PV currency groups and native legacy mappings        |
| Common/automatic requests | C++ plans seal source/configuration and preflight complete payload/mandatory inputs; selections and reporting preserve raw native estimator/contributions   |
| Direct dependencies       | Explicit typed external or constant bindings are exclusive; source/value/ordinal validation and fixed-surface partial addition remain native and single-use |
| Python                    | Strict copied input/GIL boundaries and detached owning outputs; common/automatic installed parity and final exact-head CI accepted                          |
| Excel                     | 24 functions/48 generated files, strict raw guards and passive owning copies; global format-help repair and all four Windows configurations accepted        |
| P01/performance           | Resources/scaling complete, final MC 44/44 and curve 25/25 pass; unchanged-policy affected-entry gates retained                                             |
| Final delivery            | Repeated paginated threads/checks/status, policy/review and master/head guards accepted; squash merge and identical tree verified                           |

Methodology remains frozen-calibration scalar AAD; portfolio VJP, blocks,
workspace reuse, structured operators, sparsity/checkpointing and second order
belong to later PRs. No full-plan completion is inferred from this PR.

## Open questions

None. The authorized F01-first delivery boundary is complete. Later stages
retain their own specifications, PRs and final acceptance.

## Tests

Evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- `aad-pr-fix-msvc-{default,lifetime,profiling,combined}-failed-01.log` retains
  each actual failure. Every configuration fails the same unchanged registration
  test; no compile/link or native request failure is hidden by that classification.
- `aad-pr-fix-registration-focused-01.log` passes two portable tests. Coverage
  includes all three implicit format signatures, preserving explicit custom help,
  ordinary one-input functions, empty signatures and other missing-help cases.
- `aad-pr-fix-registration-msvc-01.json` passes runtime-registration and test
  translation units in both OFF/combined modes. This is actual MSVC syntax,
  not a claim of corrected Windows XLL runtime acceptance.
- `aad-pr-fix-registration-ccn-01.log` passes CCN-eight. Only a cold registration
  constructor changes; native/MC and measured old Excel/Python paths do not.
  Reuse their verified frozen inputs instead of repeating full local suites.
- `aad-pr-fix-production-mc-confirmation-01/` passes 44/44 with 600 processes;
  `aad-pr-fix-production-mc-after-01.json` verifies 1,240 unchanged hashes and
  bitwise LSM PV/risk. Complete original oracles/thresholds remain.
- `aad-pr-fix-dependency-reconciliation-01.json` verifies all five dependency
  pins against the original baseline; `aad-pr-fix-docs-01.log` passes all 135
  Markdown files.
- `aad-pr-fix-review-threads-03.jsonl` has no unresolved threads at `7a46b932`;
  checks-05/06 retain its actual Windows failures. `aad-pr-fix-master-01.txt`
  matches the PR base. A corrected head requires a new complete check audit.

## Summary

Feature, platform, production performance and final delivery acceptance are
complete. `aad-pr-480-completion-head38-{initial,final}-01/validated.json`
retains repeated checks/review/policy audits. `aad-pr-480-after-merge-01.json`
and `aad-pr-480-merged-tree-01.json` verify the actual merge and accepted tree.
GitHub Actions runner acquisition failures and the lost Windows host remain
retained; failed-only retries preserve successful same-head work. Continue F02
in a new PR; the full plan remains unfinished.
