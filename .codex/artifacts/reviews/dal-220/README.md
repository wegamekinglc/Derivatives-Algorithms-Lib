# DAL-220 Documentation and Agent Contract Audit

Status: documentation changes delivered for independent dal-reviewer acceptance.
Retain this record while DAL-220 controls active review; retire it after acceptance.
The orchestrator owns reviewer dispatch and keeps DAL-220 `in_progress` during handoff.

## Baseline and Scope

The clean task checkout, `origin/master`, and a live `git ls-remote origin refs/heads/master`
all agreed on `ec8b0072fbf70dab814a543edc625c8e0bf77efa` before edits on 2026-09-13.
A final `git fetch origin master` and remote-reference check confirmed the same baseline.
Changes are isolated on `agent/dal-doc-writer/5651cfcb7db0`; no merge is authorized.

The regenerated baseline inventory contains **92 Markdown files and 11 registrations**:
10 canonical Codex agent TOMLs and one skill interface YAML. Every baseline file receives
full-text structural/extracted-reference checks and an individual disposition below.
Current documentation is reconciled with source, public headers, bindings, examples,
tests, and artifact status. Historical statements remain distinguished from current APIs.
This audit is not a fresh numerical or benchmark acceptance run.

| Directory     | Baseline Markdown | Delivery Markdown |
|---------------|-------------------|-------------------|
| Root          | 5                 | 5                 |
| `.claude/`    | 28                | 28                |
| `.codex/`     | 25                | 13                |
| `.github/`    | 2                 | 2                 |
| `dal-cpp/`    | 1                 | 1                 |
| `dal-excel/`  | 3                 | 3                 |
| `dal-public/` | 1                 | 1                 |
| `dal-python/` | 2                 | 2                 |
| `docs/`       | 25                | 25                |
| Total         | 92                | 80                |

Delivery retires 13 completed active records and adds this review record. All 11
non-Markdown registrations remain unchanged. No methodology page is added or removed;
the index changes expose an existing supported page.

## Corrections and Source Evidence

- `CLAUDE.md`: Index the already-supported generic joint quote-risk methodology; the other methodology entries already match the current directory.
- `README.md`: Include generic joint multi-curve provenance and its worksheet recipe among the supported quote-risk domains.
- `dal-excel/README.md`: Distinguish unchanged v1 single/XCCY fingerprints from generic joint v2; checked against quoteriskprovenance.cpp and the worksheet recipe.
- `dal-public/README.md`: Name the existing generic joint facade in curvespec.hpp and add its quote-risk domain.
- `docs/README.md`: Add the missing existing generic_joint_quote_risk.md entry, including inverse selection, graph propagation and public surfaces.
- `docs/experimental/replicate-ptirds-single-currency-curve.md`: Reconcile Cubic1_ external-range evaluation, the typed PWLF implementation, rounded table comparisons and square-system claims with interpcubic.cpp, ycpwlf.hpp and test_ptirds_curve.cpp. Preserve the reference table and its tolerances; do not claim a fresh rateslib numerical run.
- `docs/methodology/_cpp-example-style.md`: Replace the incomplete fixed scope list with the current index, fix nonexistent THROW_REQUIRE, add existing risk/PDE source mappings and retire duplicated planning corrections.
- `docs/methodology/aad.md`: Identify the repeated BlackTest timing loop as deterministic benchmark repetitions; production Monte Carlo uses simulation.hpp checkpointing. Code loop remains faithful to examples/aad/aad.cpp.
- `docs/methodology/dates.md`: Separate year-field validation from uint16_t representability, qualify unchecked arithmetic, correct Dal::NumericValueOf and the two-month start example, and initialize calendars. Checked date.cpp/date.hpp and syntax-compiled the complete revised excerpt.
- `docs/methodology/dupire.md`: Correct first/second derivative cancellation scaling, flat-tail distribution claim and returned/simulation matrix layouts using model/ivs.hpp and model/dupire.hpp.
- `docs/methodology/interpolation.md`: Document IsInBounds as a query, with endpoint polynomial extension in Cubic1_::operator(); preserve log-DF curve-specific tail policy.
- `docs/methodology/matrix.md`: Distinguish dense containers from Sparse::Square_; correct asymmetric band inequality, CG direction conjugacy and full-column-rank normal-equation condition using squarematrix.hpp, banded.cpp and bcg.cpp.
- `docs/methodology/random.md`: Use Gray-code bits in the Sobol state definition and describe Brownian bridge as an explicit option, matching sobol.cpp Seek/FillUniform and public/script simulation defaults.
- `docs/methodology/underdetermined_search.md`: Limit the weighted pseudoinverse and smoothness preference to local steps/response; link generic joint fixed-subspace selection. No global minimum or unique nonlinear solution is promised by Find.
- `docs/methodology/yield_curve.md`: Correct PWLF knot jumps, local weighted step semantics, simultaneous residual solve, sparse Gradient capture, public generic-joint bindings and passive matrix ownership. Add existing opt-in inverse selection from jointcalibration.cpp/.hpp.
- `docs/methodology/yield_curve_jacobian.md`: Correct shared typed-curve consumers and bumped residual repricing versus full recalibration; retain all numerical thresholds and threshold-review governance verbatim. Checked jointcalibration.cpp, underdetermined.cpp and ratecashflowpricing internals.

No CHANGELOG entry is needed: these changes correct descriptions of existing behavior,
not the implemented algorithms, public capabilities, compatibility contracts or thresholds.
C++, generated files, tests, build settings, bindings, agent contracts and skill registrations
have no repository delta.

## Retirement and Continuing Contracts

Read-only `multica issue get` checks confirm DAL-188, DAL-193, DAL-198 and DAL-199 are
`done`. Their records still describe pending delivery/review gates, so they no longer
belong in the active artifact directory under `AGENTS.md`. The evidence attachment
contains the status snapshot, each retired file's original bytes and SHA-256, and this
mapping; Git preserves the complete original history.

- DAL-188: continuing quote-risk threshold governance stays in
  `docs/methodology/yield_curve_jacobian.md`, including all seven triggers, frozen
  baselines, raw bucket evidence, conservative upward rounding and critic approval.
- DAL-193: consumed graph/base propagation, exact builtin eligibility, C1/C3 custom-input
  compatibility restrictions, failure ordering and isolated preparation remain in
  `docs/methodology/generic_joint_quote_risk.md` and CHANGELOG.
- DAL-198/DAL-199: grammar, debug schema rejection, historical preparation, snapshot
  authority, public execution restrictions and remaining future execution limits remain
  in `docs/methodology/script_engine.md`. Fixing container identities and clean-rebuild
  migration remain in `docs/methodology/index_parsing.md` and CHANGELOG.

Retired files:

- `.codex/artifacts/api-notes/dal-199-fixhistory-identity.md`
- `.codex/artifacts/reviews/dal-188/README.md`
- `.codex/artifacts/reviews/dal-188/p2-remediation.md`
- `.codex/artifacts/reviews/dal-193/implementation.md`
- `.codex/artifacts/reviews/dal-198/documentation.md`
- `.codex/artifacts/reviews/dal-198/implementation.md`
- `.codex/artifacts/reviews/dal-198/testing.md`
- `.codex/artifacts/reviews/dal-199-fixhistory/documentation.md`
- `.codex/artifacts/reviews/dal-199-fixhistory/implementation.md`
- `.codex/artifacts/reviews/dal-199-fixhistory/testing.md`
- `.codex/artifacts/reviews/dal-199/documentation.md`
- `.codex/artifacts/reviews/dal-199/implementation.md`
- `.codex/artifacts/reviews/dal-199/testing.md`

The ten legacy Claude specification/design/critique/API records remain explicitly
labelled implemented history. Their old paths, checkboxes and commands describe past
baselines; they are not current recommendations or new authorization.

## Agent Synchronization and Preserved Platform Differences

The DAL roster maps by name and ID to all ten canonical TOMLs. Claude names,
descriptions and complete shared behavior bodies agree with those TOMLs; the three
shared rule mirrors are byte-identical. Fifteen YAML frontmatters and the skill
interface YAML parse successfully. Codex descriptions contain 56–77 Unicode code points.

For every agent below, both Multica `description` and complete `instructions` match
before and after the audit. **No agent fields were written.** A hash comparison of
non-text configuration fields, excluding volatile metadata and secret-bearing fields,
also agrees before/after; roster membership/roles are byte-equivalent JSON values.
The attachment records complete allowed text fields, hashes, IDs and lengths.

- `dal-api-designer`: 64 description code points; both fields unchanged.
- `dal-critic`: 56 description code points; both fields unchanged.
- `dal-doc-writer`: 76 description code points; both fields unchanged.
- `dal-implementer`: 59 description code points; both fields unchanged.
- `dal-orchestrator`: 56 description code points; both fields unchanged.
- `dal-performancer`: 62 description code points; both fields unchanged.
- `dal-reviewer`: 70 description code points; both fields unchanged.
- `dal-simplifier`: 77 description code points; both fields unchanged.
- `dal-spec-writer`: 65 description code points; both fields unchanged.
- `dal-tester`: 68 description code points; both fields unchanged.

Preserved native differences: Claude Markdown/YAML registration with inherited models,
colors and four user-invocable skill wrappers; Codex TOMLs, one Git-PR interface YAML
and its compatibility shim; Multica squad routing and host configuration. Runtime,
model, thinking level, service tier, skills, environment, MCP, visibility, concurrency,
avatar, squad membership, roles and leader are untouched.

## Verification and Limits

- `python3 .github/scripts/check_docs.py`: passes. Its built-in selection is narrower
  than the issue scope; it is not described as an all-tracked-files check.
- Attached `audit_docs.py <checkout>`: passes for all 92 baseline Markdown files and
  11 registrations, with per-file line counts, hashes, links/anchors, extracted paths,
  DAL includes, type candidates, frontmatter, final newlines and explicit exceptions.
  Source/header placeholders, component-relative test paths, created virtualenv paths,
  historical references and retired records are classified rather than silently accepted
  as existing current files. Inline C++ lambda syntax is not treated as a Markdown link.
- The changed date example compiles with `c++ -std=c++17 -I dal-cpp -fsyntax-only` after
  wrapping its statements in `main`; the exact excerpt and compiler result are attached.
- Python benchmark `--coverage` runs without a DAL import and agrees with the 21-target
  native inventory and documented coverage gaps. Its parser options were checked against source.
- `git diff --check` and `git diff --cached --check`: pass; the staged scope is Markdown only.
- No fresh full C++/Python/XLL, numerical, AAD-backend, sanitizer or timing result is
  claimed. Current APIs and example contracts were checked against local source/tests;
  every published example was not executed. The external rateslib reference returned a
  verification page, so its live content was not independently revalidated; the unchanged
  reference values were checked against the shipped PTIRDS test arrays.

The final handoff supplies the commit/PR and one nonblocking CI snapshot. CI completion
and independent acceptance are not claimed here. There are no implementation blockers;
the only external-source limitation is recorded above.

## Complete Baseline File Inventory

The attachment adds extracted references/type candidates and before/after SHA-256 to
this individual file list. Corrected-file reasons and retired-file ownership are above;
unchanged current files passed structural/reference and relevant source-contract checks.

| Baseline file                                                   | Lines | Disposition       |
|-----------------------------------------------------------------|-------|-------------------|
| `.claude/agents/README.md`                                      | 75    | unchanged         |
| `.claude/agents/dal-api-designer.md`                            | 15    | unchanged         |
| `.claude/agents/dal-critic.md`                                  | 15    | unchanged         |
| `.claude/agents/dal-doc-writer.md`                              | 15    | unchanged         |
| `.claude/agents/dal-implementer.md`                             | 15    | unchanged         |
| `.claude/agents/dal-orchestrator.md`                            | 21    | unchanged         |
| `.claude/agents/dal-performancer.md`                            | 15    | unchanged         |
| `.claude/agents/dal-reviewer.md`                                | 15    | unchanged         |
| `.claude/agents/dal-simplifier.md`                              | 15    | unchanged         |
| `.claude/agents/dal-spec-writer.md`                             | 15    | unchanged         |
| `.claude/agents/dal-tester.md`                                  | 17    | unchanged         |
| `.claude/api-notes/joint-aad-gradient.md`                       | 365   | history preserved |
| `.claude/critiques/pde-framework-reimplementation.md`           | 218   | history preserved |
| `.claude/designs/api-shape-dedup.md`                            | 488   | history preserved |
| `.claude/designs/joint-aad-gradient.md`                         | 1623  | history preserved |
| `.claude/rules/code-style.md`                                   | 258   | unchanged         |
| `.claude/rules/git-commit-pr.md`                                | 55    | unchanged         |
| `.claude/rules/unit-test-style.md`                              | 53    | unchanged         |
| `.claude/skills/dal-code-style-review/SKILL.md`                 | 14    | unchanged         |
| `.claude/skills/dal-commit-and-pr/SKILL.md`                     | 17    | unchanged         |
| `.claude/skills/dal-unit-test-skill/SKILL.md`                   | 14    | unchanged         |
| `.claude/skills/dal-unit-test-write/SKILL.md`                   | 17    | unchanged         |
| `.claude/specs/2026-08-14-dal-web-extraction-design.md`         | 137   | history preserved |
| `.claude/specs/2026-08-14-dal-web-extraction-plan.md`           | 1038  | history preserved |
| `.claude/specs/joint-aad-gradient.md`                           | 873   | history preserved |
| `.claude/specs/multi-curve-simultaneous-example.md`             | 830   | history preserved |
| `.claude/specs/pde-framework-reimplementation.md`               | 855   | history preserved |
| `.claude/specs/script-compiled-evaluator-alignment.md`          | 200   | history preserved |
| `.codex/README.md`                                              | 52    | unchanged         |
| `.codex/artifacts/README.md`                                    | 7     | unchanged         |
| `.codex/artifacts/api-notes/dal-199-fixhistory-identity.md`     | 176   | retired           |
| `.codex/artifacts/reviews/dal-188/README.md`                    | 294   | retired           |
| `.codex/artifacts/reviews/dal-188/p2-remediation.md`            | 92    | retired           |
| `.codex/artifacts/reviews/dal-193/implementation.md`            | 251   | retired           |
| `.codex/artifacts/reviews/dal-198/documentation.md`             | 88    | retired           |
| `.codex/artifacts/reviews/dal-198/implementation.md`            | 185   | retired           |
| `.codex/artifacts/reviews/dal-198/testing.md`                   | 218   | retired           |
| `.codex/artifacts/reviews/dal-199-fixhistory/documentation.md`  | 52    | retired           |
| `.codex/artifacts/reviews/dal-199-fixhistory/implementation.md` | 94    | retired           |
| `.codex/artifacts/reviews/dal-199-fixhistory/testing.md`        | 119   | retired           |
| `.codex/artifacts/reviews/dal-199/documentation.md`             | 96    | retired           |
| `.codex/artifacts/reviews/dal-199/implementation.md`            | 158   | retired           |
| `.codex/artifacts/reviews/dal-199/testing.md`                   | 321   | retired           |
| `.codex/references/benchmark-workflow.md`                       | 268   | unchanged         |
| `.codex/references/code-style.md`                               | 258   | unchanged         |
| `.codex/references/git-commit-pr.md`                            | 55    | unchanged         |
| `.codex/references/run-tests.md`                                | 59    | unchanged         |
| `.codex/references/style-review.md`                             | 86    | unchanged         |
| `.codex/references/unit-test-style.md`                          | 53    | unchanged         |
| `.codex/references/write-tests.md`                              | 206   | unchanged         |
| `.codex/skills/dal-agent-team/references/shared-rules.md`       | 10    | unchanged         |
| `.codex/skills/dal-git-pr/SKILL.md`                             | 71    | unchanged         |
| `.codex/skills/dal-git-pr/references/publish-workflow.md`       | 282   | unchanged         |
| `.github/copilot-instructions.md`                               | 91    | unchanged         |
| `.github/pull_request_template.md`                              | 16    | unchanged         |
| `AGENTS.md`                                                     | 79    | unchanged         |
| `CHANGELOG.md`                                                  | 474   | history preserved |
| `CLAUDE.md`                                                     | 197   | corrected         |
| `CONTRIBUTING.md`                                               | 191   | unchanged         |
| `README.md`                                                     | 209   | corrected         |
| `dal-cpp/README.md`                                             | 75    | unchanged         |
| `dal-excel/README.md`                                           | 160   | corrected         |
| `dal-excel/examples/008.quote_risk.md`                          | 42    | unchanged         |
| `dal-excel/examples/009.generic_joint_quote_risk.md`            | 63    | unchanged         |
| `dal-public/README.md`                                          | 76    | corrected         |
| `dal-python/README.md`                                          | 820   | unchanged         |
| `dal-python/benchmarks/README.md`                               | 439   | unchanged         |
| `docs/README.md`                                                | 174   | corrected         |
| `docs/architecture.md`                                          | 216   | unchanged         |
| `docs/experimental/aad-analytic-jacobian-curve-calibration.md`  | 11    | unchanged         |
| `docs/experimental/replicate-ptirds-single-currency-curve.md`   | 419   | corrected         |
| `docs/installation.md`                                          | 371   | unchanged         |
| `docs/methodology/_cpp-example-style.md`                        | 220   | corrected         |
| `docs/methodology/aad.md`                                       | 505   | corrected         |
| `docs/methodology/black_scholes.md`                             | 298   | unchanged         |
| `docs/methodology/dates.md`                                     | 145   | corrected         |
| `docs/methodology/dupire.md`                                    | 228   | corrected         |
| `docs/methodology/generic_joint_quote_risk.md`                  | 213   | unchanged         |
| `docs/methodology/index_parsing.md`                             | 176   | unchanged         |
| `docs/methodology/interpolation.md`                             | 225   | corrected         |
| `docs/methodology/log_discount_curve.md`                        | 247   | unchanged         |
| `docs/methodology/matrix.md`                                    | 348   | corrected         |
| `docs/methodology/pde.md`                                       | 348   | unchanged         |
| `docs/methodology/quadrature.md`                                | 253   | unchanged         |
| `docs/methodology/random.md`                                    | 368   | corrected         |
| `docs/methodology/rate_node_risk.md`                            | 189   | unchanged         |
| `docs/methodology/script_engine.md`                             | 819   | unchanged         |
| `docs/methodology/underdetermined_search.md`                    | 455   | corrected         |
| `docs/methodology/xccy_calibration.md`                          | 419   | unchanged         |
| `docs/methodology/yield_curve.md`                               | 832   | corrected         |
| `docs/methodology/yield_curve_jacobian.md`                      | 659   | corrected         |
| `docs/public-api.md`                                            | 783   | unchanged         |
| `.codex/agents/dal-api-designer.toml`                           | 9     | unchanged         |
| `.codex/agents/dal-critic.toml`                                 | 9     | unchanged         |
| `.codex/agents/dal-doc-writer.toml`                             | 9     | unchanged         |
| `.codex/agents/dal-implementer.toml`                            | 9     | unchanged         |
| `.codex/agents/dal-orchestrator.toml`                           | 15    | unchanged         |
| `.codex/agents/dal-performancer.toml`                           | 9     | unchanged         |
| `.codex/agents/dal-reviewer.toml`                               | 9     | unchanged         |
| `.codex/agents/dal-simplifier.toml`                             | 9     | unchanged         |
| `.codex/agents/dal-spec-writer.toml`                            | 9     | unchanged         |
| `.codex/agents/dal-tester.toml`                                 | 11    | unchanged         |
| `.codex/skills/dal-git-pr/agents/openai.yaml`                   | 4     | unchanged         |

Added delivery artifact: this review record. The machine-readable evidence is attached to DAL-220.
