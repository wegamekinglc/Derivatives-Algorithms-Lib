# DAL-188 Documentation Audit

Status: documentation delivery awaiting independent dal-reviewer acceptance.
Retain this record while DAL-188 is active; retire it after acceptance.

## Baseline and Scope

On 2026-09-07, `git fetch origin master`, `git rev-parse HEAD origin/master`,
and `git ls-remote origin refs/heads/master` agreed on
`0870bc08c97fc5c4fc3db3758aaf0d9e5ffe6d8d` (release dal-python 2026.9.5).
The checkout was clean before edits. Work is isolated on
`fix/DAL-188-documentation-contracts`; the source TOMLs are unchanged.
No C++, generated source, build configuration, or production Python is modified.

The baseline inventory was regenerated from Git and contains 79 Markdown files.
Every file was read by the full-text audit, including historical artifacts;
none was excluded from inventory, link, table, or whitespace checks. Current
path claims, DAL includes, and inline type/function candidates were compared
against tracked source. Changed documentation was additionally reconciled
against source, bindings, tests, or the authoritative agent contract.
This is documentation verification, not a rerun of the numerical test matrix.

| Directory | Baseline Markdown | Delivery Markdown |
|-----------|-------------------|-------------------|
| Root | 5 | 5 |
| `.claude/` | 28 | 28 |
| `.codex/` | 14 | 13 |
| `.github/` | 2 | 2 |
| `dal-cpp/` | 1 | 1 |
| `dal-excel/` | 2 | 2 |
| `dal-public/` | 1 | 1 |
| `dal-python/` | 1 | 1 |
| `docs/` | 25 | 24 |
| Total | 79 | 77 |

The delivery adds a current node-risk methodology and this active audit, and
retires four completed design/plan documents. The full baseline file inventory
appears below. Machine-readable SHA-256/line/path records, roster parity, and
the audit runner are supplied with the issue handoff as an evidence attachment.

## Confirmed Discrepancies and Source Evidence

- Claude role contracts had drifted from all ten canonical TOMLs: the
  orchestrator prohibited reading its required context; implementer required a
  new design approval even for already-approved scope; reviewer required
  unconditional full local builds; performancer described eight regression
  targets and allowed benchmark edits. The TOMLs define read-capable routing,
  approved scope, proportional review verification, and read-only measurement.
  Claude bodies and descriptions now match the TOMLs, with native metadata intact.
- Claude skills duplicated older test/style/publication workflows. They now
  route to the complete shared references. The publication workflow's submodule
  path is corrected to `dal-cpp/externals/`.
- The enum convention incorrectly described a member listing method.
  `dal-cpp/dal/auto/MG_CurveSolveMode_enum.hpp` declares the free function
  `CurveSolveModeListAll()`. Both mirrored style guides now describe it correctly.
- Test authoring used the wrong interpolation namespace and a nonexistent
  archive helper. `dal-cpp/dal/math/interp/interplinear.hpp` declares
  `Interp::NewLinear` returning a unique pointer; the revised handle construction
  and JSON round trip follow `dal-cpp/tests/storage/test_json.cpp`.
  Abstract `Archive::Store_` is not instantiated.
- Core Linux builds also include portable Excel binding tests. The actual
  conditions are in root `CMakeLists.txt` and `dal-excel/CMakeLists.txt`:
  non-Windows, portable inclusion enabled, Excel tests enabled, and Google Test
  available. Setup, architecture, Excel, and mirrored test guidance now distinguish
  portable contract coverage from the Windows XLL.
- `dal-python/src/bindings/script.cpp` returns a string from `Product_Debug`.
  Python conversion errors can raise before native failure-as-data handling;
  `dal-python/src/bindings/curve.cpp` and `dal-python/tests/test_curve_pricing.py`
  enforce list/keyword-only inputs. Product-constant Greek keys depend on the
  named constants in the product, as assembled in `dal-public/src/value.cpp`.
- Node-risk plans under published experimental docs contained shipped stages,
  obsolete Deposit-only claims, and claims that Excel bindings did not exist.
  Current methodology is now in `docs/methodology/rate_node_risk.md`, grounded in
  `ratecashflowpricing.hpp`, `ratecashflowpricing.cpp`,
  `ratecashflowpricing_internal.hpp` under `dal-cpp/dal/curve/`, and the C++,
  Python, and Excel contract tests named there.
- Node tensors sum by component key, with no currency axis:
  `AggregationAccumulator_`, `AddGradientSum`, and `AggregateComponentTensor`.
  PV totals and metadata carry actual currencies; `WriteComponentNodeRows` in
  `dal-excel/src/__curvepricing.cpp` leaves tensor-row currency blank.
  The public guide and methodology now state this shape explicitly. Quote risk
  independently accumulates by provenance and actual PV currency.
- The quote-space design and plan still said implementation had not started.
  All three factories, aggregation, public/Python/Excel bindings, examples,
  numerical ladders, and performance coverage now exist. Their current contracts
  are already in `docs/public-api.md` and
  `docs/methodology/yield_curve_jacobian.md`; completed active artifacts are retired
  in accordance with `AGENTS.md`. Git retains the design and delivery history.

No CHANGELOG entry is added: this change documents existing behavior and corrects
guidance; it adds no numerical algorithm, public capability, or breaking API.

## Multica and Platform Differences

The DAL squad roster maps exactly to the ten TOMLs. For every row below,
`description` equals TOML `description`, and `instructions` equals the entire
TOML `developer_instructions` string. No synchronization write was needed.
Descriptions contain 56–77 Unicode code points, below the 255 limit.

| Agent            | Description code points | Fields                           | Action |
|------------------|-------------------------|----------------------------------|--------|
| dal-orchestrator | 56                      | description + instructions equal | none   |
| dal-critic       | 56                      | description + instructions equal | none   |
| dal-implementer  | 59                      | description + instructions equal | none   |
| dal-simplifier   | 77                      | description + instructions equal | none   |
| dal-spec-writer  | 65                      | description + instructions equal | none   |
| dal-performancer | 62                      | description + instructions equal | none   |
| dal-tester       | 68                      | description + instructions equal | none   |
| dal-doc-writer   | 76                      | description + instructions equal | none   |
| dal-reviewer     | 70                      | description + instructions equal | none   |
| dal-api-designer | 64                      | description + instructions equal | none   |

Preserved platform differences:

- Claude uses Markdown/YAML, inherited model selection, colors, and four
  user-invocable skill names; Codex uses TOML and one skill interface YAML.
- Host tools and isolation mechanisms remain native. They do not change shared
  role boundaries, active artifact locations, review authorization, or routing.
- Multica fields map from TOML description/developer instructions. Squad-specific
  routing instructions are a separate platform contract and were not edited.
- Historical Claude artifacts remain explicitly historical. Their missing old
  paths, delivery checkboxes, and four old staged-binary command occurrences are
  preserved as historical evidence, not current execution recommendations.
  The apparent local link in the PDE specification is an inline C++ lambda,
  not a Markdown link; the full audit treats inline code literally.

No runtime, model, reasoning, service tier, skill assignment, environment, MCP,
visibility, concurrency, avatar, squad membership, role, or leader was changed.

## Changed Files

| File                                                              | Change / reason                                                                         |
|-------------------------------------------------------------------|-----------------------------------------------------------------------------------------|
| `.claude/agents/README.md`                                        | Document shared routing, authorization, artifact ownership, and native wrappers.        |
| `.claude/agents/dal-api-designer.md`                              | Mirror full authoritative TOML contract; preserve name/model/color.                     |
| `.claude/agents/dal-critic.md`                                    | Mirror full authoritative TOML contract; preserve name/model/color.                     |
| `.claude/agents/dal-doc-writer.md`                                | Mirror full authoritative TOML contract; preserve name/model/color.                     |
| `.claude/agents/dal-implementer.md`                               | Mirror full authoritative TOML contract; preserve name/model/color.                     |
| `.claude/agents/dal-orchestrator.md`                              | Mirror full authoritative TOML contract; preserve name/model/color.                     |
| `.claude/agents/dal-performancer.md`                              | Mirror full authoritative TOML contract; preserve name/model/color.                     |
| `.claude/agents/dal-reviewer.md`                                  | Mirror full authoritative TOML contract; preserve name/model/color.                     |
| `.claude/agents/dal-simplifier.md`                                | Mirror full authoritative TOML contract; preserve name/model/color.                     |
| `.claude/agents/dal-spec-writer.md`                               | Mirror full authoritative TOML contract; preserve name/model/color.                     |
| `.claude/agents/dal-tester.md`                                    | Mirror full authoritative TOML contract; preserve name/model/color.                     |
| `.claude/rules/code-style.md`                                     | Correct enum listing to the generated free function; keep mirrors identical.            |
| `.claude/rules/unit-test-style.md`                                | Include portable Excel test binary; keep mirrors identical.                             |
| `.claude/skills/dal-code-style-review/SKILL.md`                   | Keep native skill registration; use the shared complete workflow.                       |
| `.claude/skills/dal-commit-and-pr/SKILL.md`                       | Keep native skill registration; use the shared complete workflow.                       |
| `.claude/skills/dal-unit-test-skill/SKILL.md`                     | Keep native skill registration; use the shared complete workflow.                       |
| `.claude/skills/dal-unit-test-write/SKILL.md`                     | Keep native skill registration; use the shared complete workflow.                       |
| `.codex/README.md`                                                | Document authorized contract synchronization.                                           |
| `.codex/artifacts/designs/quote-space-dv01-design.md`             | Retire completed plan/design; current contracts are in published methodology/API docs.  |
| `.codex/artifacts/plans/quote-space-dv01-implementation-plan.md`  | Retire completed plan/design; current contracts are in published methodology/API docs.  |
| `.codex/references/code-style.md`                                 | Correct enum listing to the generated free function; keep mirrors identical.            |
| `.codex/references/unit-test-style.md`                            | Include portable Excel test binary; keep mirrors identical.                             |
| `.codex/references/write-tests.md`                                | Correct interpolation/archive examples, test targets, and rule drift.                   |
| `.codex/skills/dal-git-pr/references/publish-workflow.md`         | Correct submodule directory in inspection/staging commands.                             |
| `AGENTS.md`                                                       | State TOML authority and preserve platform metadata boundaries.                         |
| `CLAUDE.md`                                                       | Index node-risk methodology; add portable Excel tests and current benchmark reference.  |
| `dal-excel/README.md`                                             | Document portable Excel test target and enablement conditions.                          |
| `dal-excel/examples/008.quote_risk.md`                            | Align worksheet recipe table.                                                           |
| `dal-public/README.md`                                            | Use full public-header paths and align table.                                           |
| `dal-python/README.md`                                            | Correct debug return, conditional Greek keys, exceptions, and table alignment.          |
| `docs/README.md`                                                  | Index current node-risk methodology and remove completed experimental plans.            |
| `docs/architecture.md`                                            | Document portable Excel contracts; align tables.                                        |
| `docs/experimental/aad-node-risk-portfolio-aggregation-design.md` | Retire completed plan/design; current contracts are in published methodology/API docs.  |
| `docs/experimental/aad-node-risk-portfolio-aggregation-plan.md`   | Retire completed plan/design; current contracts are in published methodology/API docs.  |
| `docs/installation.md`                                            | Document portable test options and default inclusion; align tables.                     |
| `docs/methodology/rate_node_risk.md`                              | Document existing native node-risk methodology from source.                             |
| `docs/public-api.md`                                              | Clarify node tensor currency/duplicate-key semantics; align tables.                     |
| `.codex/artifacts/reviews/dal-188/README.md`                      | Active acceptance evidence, scope inventory, discrepancies, and synchronization record. |

## Validation and Limits

- `python3 .github/scripts/check_docs.py`: passed. Its built-in scope covers
  published/Codex/GitHub docs, not all Claude files; the supplemental audit
  covers the complete tracked Markdown inventory.
- `python3 -m unittest discover -s .github/scripts/tests -p test_check_docs.py`:
  34 tests passed.
- `python3 audit_docs.py <checkout>` (attached runner): all tracked Markdown
  links/anchors, table structure, whitespace, final newlines, reference paths,
  DAL includes, current command checks, and inline symbol candidates passed.
  Literal placeholders, formulas, runtime environments, and explicitly historical
  paths are classified separately in the attached inventory, not silently treated
  as existing current files. Repository GitHub links to master are also resolved
  against this checkout; arbitrary external websites are not availability-tested.
- Parsed ten TOMLs, fifteen YAML frontmatters, and one skill interface YAML;
  ten Claude descriptions/bodies equal the TOMLs; all three rule mirrors are
  byte-identical; roster mapping and both allowed text fields match for all ten agents.
- `git diff --check` and `git diff --cached --check`: passed.
- C++ builds, Machinist generation, numerical/performance suites, and executing
  every published example are outside this prose-only verification. No such
  test result is claimed. Independent dal-reviewer acceptance remains pending;
  the handoff reports one nonblocking CI snapshot and does not claim merge readiness.

## Complete Baseline Markdown Inventory

Every baseline file below received full-text structural checks and extracted
reference/symbol inspection. Historical claims remain classified by their explicit
status. New delivery files are listed in Changed Files above.

| Baseline file                                                     | Lines | Disposition        |
|-------------------------------------------------------------------|-------|--------------------|
| `.claude/agents/README.md`                                        | 112   | corrected          |
| `.claude/agents/dal-api-designer.md`                              | 180   | corrected          |
| `.claude/agents/dal-critic.md`                                    | 190   | corrected          |
| `.claude/agents/dal-doc-writer.md`                                | 211   | corrected          |
| `.claude/agents/dal-implementer.md`                               | 235   | corrected          |
| `.claude/agents/dal-orchestrator.md`                              | 184   | corrected          |
| `.claude/agents/dal-performancer.md`                              | 202   | corrected          |
| `.claude/agents/dal-reviewer.md`                                  | 256   | corrected          |
| `.claude/agents/dal-simplifier.md`                                | 230   | corrected          |
| `.claude/agents/dal-spec-writer.md`                               | 144   | corrected          |
| `.claude/agents/dal-tester.md`                                    | 200   | corrected          |
| `.claude/api-notes/joint-aad-gradient.md`                         | 365   | history preserved  |
| `.claude/critiques/pde-framework-reimplementation.md`             | 218   | history preserved  |
| `.claude/designs/api-shape-dedup.md`                              | 488   | history preserved  |
| `.claude/designs/joint-aad-gradient.md`                           | 1623  | history preserved  |
| `.claude/rules/code-style.md`                                     | 258   | corrected          |
| `.claude/rules/git-commit-pr.md`                                  | 55    | checked; unchanged |
| `.claude/rules/unit-test-style.md`                                | 51    | corrected          |
| `.claude/skills/dal-code-style-review/SKILL.md`                   | 84    | corrected          |
| `.claude/skills/dal-commit-and-pr/SKILL.md`                       | 130   | corrected          |
| `.claude/skills/dal-unit-test-skill/SKILL.md`                     | 65    | corrected          |
| `.claude/skills/dal-unit-test-write/SKILL.md`                     | 194   | corrected          |
| `.claude/specs/2026-08-14-dal-web-extraction-design.md`           | 137   | history preserved  |
| `.claude/specs/2026-08-14-dal-web-extraction-plan.md`             | 1038  | history preserved  |
| `.claude/specs/joint-aad-gradient.md`                             | 873   | history preserved  |
| `.claude/specs/multi-curve-simultaneous-example.md`               | 830   | history preserved  |
| `.claude/specs/pde-framework-reimplementation.md`                 | 855   | history preserved  |
| `.claude/specs/script-compiled-evaluator-alignment.md`            | 200   | history preserved  |
| `.codex/README.md`                                                | 49    | corrected          |
| `.codex/artifacts/README.md`                                      | 7     | checked; unchanged |
| `.codex/artifacts/designs/quote-space-dv01-design.md`             | 437   | retired            |
| `.codex/artifacts/plans/quote-space-dv01-implementation-plan.md`  | 532   | retired            |
| `.codex/references/benchmark-workflow.md`                         | 268   | checked; unchanged |
| `.codex/references/code-style.md`                                 | 258   | corrected          |
| `.codex/references/git-commit-pr.md`                              | 55    | checked; unchanged |
| `.codex/references/run-tests.md`                                  | 59    | checked; unchanged |
| `.codex/references/style-review.md`                               | 86    | checked; unchanged |
| `.codex/references/unit-test-style.md`                            | 51    | corrected          |
| `.codex/references/write-tests.md`                                | 195   | corrected          |
| `.codex/skills/dal-agent-team/references/shared-rules.md`         | 10    | checked; unchanged |
| `.codex/skills/dal-git-pr/SKILL.md`                               | 71    | checked; unchanged |
| `.codex/skills/dal-git-pr/references/publish-workflow.md`         | 282   | corrected          |
| `.github/copilot-instructions.md`                                 | 91    | checked; unchanged |
| `.github/pull_request_template.md`                                | 16    | checked; unchanged |
| `AGENTS.md`                                                       | 76    | corrected          |
| `CHANGELOG.md`                                                    | 408   | history preserved  |
| `CLAUDE.md`                                                       | 192   | corrected          |
| `CONTRIBUTING.md`                                                 | 191   | checked; unchanged |
| `README.md`                                                       | 209   | checked; unchanged |
| `dal-cpp/README.md`                                               | 75    | checked; unchanged |
| `dal-excel/README.md`                                             | 147   | corrected          |
| `dal-excel/examples/008.quote_risk.md`                            | 41    | corrected          |
| `dal-public/README.md`                                            | 76    | corrected          |
| `dal-python/README.md`                                            | 755   | corrected          |
| `docs/README.md`                                                  | 174   | corrected          |
| `docs/architecture.md`                                            | 213   | corrected          |
| `docs/experimental/aad-analytic-jacobian-curve-calibration.md`    | 11    | checked; unchanged |
| `docs/experimental/aad-node-risk-portfolio-aggregation-design.md` | 289   | retired            |
| `docs/experimental/aad-node-risk-portfolio-aggregation-plan.md`   | 262   | retired            |
| `docs/experimental/replicate-ptirds-single-currency-curve.md`     | 419   | checked; unchanged |
| `docs/installation.md`                                            | 363   | corrected          |
| `docs/methodology/_cpp-example-style.md`                          | 220   | checked; unchanged |
| `docs/methodology/aad.md`                                         | 505   | checked; unchanged |
| `docs/methodology/black_scholes.md`                               | 298   | checked; unchanged |
| `docs/methodology/dates.md`                                       | 140   | checked; unchanged |
| `docs/methodology/dupire.md`                                      | 228   | checked; unchanged |
| `docs/methodology/index_parsing.md`                               | 133   | checked; unchanged |
| `docs/methodology/interpolation.md`                               | 225   | checked; unchanged |
| `docs/methodology/log_discount_curve.md`                          | 247   | checked; unchanged |
| `docs/methodology/matrix.md`                                      | 348   | checked; unchanged |
| `docs/methodology/pde.md`                                         | 348   | checked; unchanged |
| `docs/methodology/quadrature.md`                                  | 253   | checked; unchanged |
| `docs/methodology/random.md`                                      | 368   | checked; unchanged |
| `docs/methodology/script_engine.md`                               | 616   | checked; unchanged |
| `docs/methodology/underdetermined_search.md`                      | 455   | checked; unchanged |
| `docs/methodology/xccy_calibration.md`                            | 419   | checked; unchanged |
| `docs/methodology/yield_curve.md`                                 | 832   | checked; unchanged |
| `docs/methodology/yield_curve_jacobian.md`                        | 618   | checked; unchanged |
| `docs/public-api.md`                                              | 752   | corrected          |

## Handoff

Return to dal-orchestrator to arrange dal-reviewer independent acceptance.
DAL-188 remains in progress during that handoff. There are no implementation
blockers or user questions. No PR merge is authorized or performed.
