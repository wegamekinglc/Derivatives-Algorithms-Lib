# DAL Documentation Inventory

Reviewed on 2026-08-02. The inventory was regenerated from:

```bash
git ls-files '*.md' '.codex/agents/*.toml' \
  '.codex/skills/**/agents/openai.yaml' | sort
```

The final in-scope inventory contains 102 tracked files: 90 Markdown
documents, 10 Codex agent TOML contracts, and two Codex skill-interface YAML
files.

Status meanings:

- **current** — describes a supported repository, product, or contributor
  surface;
- **current contract** — active agent, skill, review, or automation guidance;
- **active design** — approved neither as implementation nor as current product
  behavior;
- **implemented history** — retained design/specification evidence for behavior
  that is now represented in current source and documentation;
- **historical plan** — retained execution history, not a current user guide;
- **experimental** — explicitly non-normative research or validation material;
- **canonical history** — the maintained release record.

## Repository root (5 Markdown)

- `AGENTS.md` — Purpose: repository-wide agent routing and engineering rules. Status: **current contract**.
- `CHANGELOG.md` — Purpose: released and unreleased change history. Status: **canonical history**.
- `CLAUDE.md` — Purpose: Claude-facing repository map, commands, and delegation guidance. Status: **current contract**.
- `CONTRIBUTING.md` — Purpose: contributor setup, workflow, testing, and pull-request guidance. Status: **current**.
- `README.md` — Purpose: project overview, quick start, component map, and documentation entry points. Status: **current**.

## Component documentation (6 Markdown)

- `dal-cpp/README.md` — Purpose: C++ library build, test, architecture, and usage guide. Status: **current**.
- `dal-excel/README.md` — Purpose: Excel add-in build and runtime guide. Status: **current**.
- `dal-public/README.md` — Purpose: public-header compatibility surface and packaging note. Status: **current**.
- `dal-python/README.md` — Purpose: Python bindings build, install, test, and example guide. Status: **current**.
- `dal-web/README.md` — Purpose: web application setup, architecture, and development guide. Status: **current**.
- `dal-web/backend/README.md` — Purpose: FastAPI backend setup, native-binding requirement, and test guide. Status: **current**.

## Published and retained documentation (28 Markdown)

- `docs/README.md` — Purpose: documentation index and status boundary for published, experimental, and historical material. Status: **current**.
- `docs/architecture.md` — Purpose: repository architecture and component/data-flow overview. Status: **current**.
- `docs/curve-lab.md` — Purpose: current Curve Lab authoring, persistence, pricing, risk, and operations contract. Status: **current**.
- `docs/installation.md` — Purpose: platform prerequisites and component installation instructions. Status: **current**.
- `docs/public-api.md` — Purpose: public C++, Python, and Excel surface guide. Status: **current**.
- `docs/experimental/aad-analytic-jacobian-curve-calibration.md` — Purpose: AAD Jacobian design exploration and validation notes. Status: **experimental**.
- `docs/experimental/replicate-ptirds-single-currency-curve.md` — Purpose: external single-currency curve replication study. Status: **experimental**.
- `docs/methodology/_cpp-example-style.md` — Purpose: maintainer rubric for C++ snippets in methodology pages. Status: **current contract**.
- `docs/methodology/aad.md` — Purpose: automatic adjoint differentiation architecture and backend methodology. Status: **current**.
- `docs/methodology/black_scholes.md` — Purpose: Black/Bachelier pricing formulas and implementation map. Status: **current**.
- `docs/methodology/dates.md` — Purpose: date, calendar, schedule, and day-count conventions. Status: **current**.
- `docs/methodology/dupire.md` — Purpose: Dupire/local-volatility methodology and implementation map. Status: **current**.
- `docs/methodology/index_parsing.md` — Purpose: supported index-name parsing and construction conventions. Status: **current**.
- `docs/methodology/interpolation.md` — Purpose: interpolation schemes, boundaries, and curve usage. Status: **current**.
- `docs/methodology/log_discount_curve.md` — Purpose: log-discount and zero-rate curve parameterizations. Status: **current**.
- `docs/methodology/matrix.md` — Purpose: matrix storage, decomposition, and iterative-solver methodology. Status: **current**.
- `docs/methodology/pde.md` — Purpose: PDE grids, operators, coefficients, and theta rollback. Status: **current**.
- `docs/methodology/quadrature.md` — Purpose: Gauss-Hermite and Simpson quadrature methodology. Status: **current**.
- `docs/methodology/random.md` — Purpose: Sobol, bridge, pseudo-random, and skip semantics. Status: **current**.
- `docs/methodology/script_engine.md` — Purpose: script preprocessing, AST, visitors, evaluators, and compilation. Status: **current**.
- `docs/methodology/underdetermined_search.md` — Purpose: exact/approximate curve-calibration solver methodology. Status: **current**.
- `docs/methodology/xccy_calibration.md` — Purpose: cross-currency pricing, resets, fixing, and calibration contracts. Status: **current**.
- `docs/methodology/yield_curve.md` — Purpose: single- and multi-curve construction and calibration methodology. Status: **current**.
- `docs/methodology/yield_curve_jacobian.md` — Purpose: analytic and bumped curve-Jacobian methodology. Status: **current**.
- `docs/superpowers/plans/2026-07-13-xccy-resettable-mtm-plan-corrections.md` — Purpose: corrections to the XCCY resettable/MTM implementation sequence. Status: **historical plan**.
- `docs/superpowers/plans/2026-07-13-xccy-resettable-mtm.md` — Purpose: XCCY resettable/MTM implementation plan. Status: **historical plan**.
- `docs/superpowers/specs/2026-07-13-xccy-resettable-mtm-benchmark-addendum.md` — Purpose: benchmark addendum for the XCCY work. Status: **implemented history**.
- `docs/superpowers/specs/2026-07-13-xccy-resettable-mtm-design.md` — Purpose: XCCY resettable/MTM design record. Status: **implemented history**.

## GitHub integration (2 Markdown)

- `.github/copilot-instructions.md` — Purpose: Copilot-specific repository and validation guidance. Status: **current contract**.
- `.github/pull_request_template.md` — Purpose: pull-request evidence and checklist template. Status: **current contract**.

## Claude contracts and artifacts (30 Markdown)

- `.claude/agents/README.md` — Purpose: Claude DAL role roster, stage routing, and artifact flow. Status: **current contract**.
- `.claude/agents/dal-api-designer.md` — Purpose: Claude public-API design role contract. Status: **current contract**.
- `.claude/agents/dal-critic.md` — Purpose: Claude specification-critique role contract. Status: **current contract**.
- `.claude/agents/dal-doc-writer.md` — Purpose: Claude documentation role contract. Status: **current contract**.
- `.claude/agents/dal-implementer.md` — Purpose: Claude implementation role contract. Status: **current contract**.
- `.claude/agents/dal-orchestrator.md` — Purpose: Claude orchestration, routing, and reporting contract. Status: **current contract**.
- `.claude/agents/dal-performancer.md` — Purpose: Claude benchmark and performance-review contract. Status: **current contract**.
- `.claude/agents/dal-reviewer.md` — Purpose: Claude correctness and architecture-review contract. Status: **current contract**.
- `.claude/agents/dal-simplifier.md` — Purpose: Claude behavior-preserving simplification contract. Status: **current contract**.
- `.claude/agents/dal-spec-writer.md` — Purpose: Claude specification-writing contract. Status: **current contract**.
- `.claude/agents/dal-tester.md` — Purpose: Claude test-planning and implementation contract. Status: **current contract**.
- `.claude/api-notes/joint-aad-gradient.md` — Purpose: public API decisions for joint AAD curve calibration. Status: **implemented history**.
- `.claude/critiques/pde-framework-reimplementation.md` — Purpose: critique of the PDE reimplementation specification. Status: **implemented history**.
- `.claude/designs/api-shape-dedup.md` — Purpose: proposal to deduplicate API-shape validation. Status: **active design**.
- `.claude/designs/joint-aad-gradient.md` — Purpose: detailed joint AAD-gradient design. Status: **implemented history**.
- `.claude/rules/code-style.md` — Purpose: Claude C++ and repository code-style rules. Status: **current contract**.
- `.claude/rules/dal-web-code-style.md` — Purpose: Claude backend/frontend web code rules. Status: **current contract**.
- `.claude/rules/dal-web-design.md` — Purpose: Claude DAL web design-system contract. Status: **current contract**.
- `.claude/rules/git-commit-pr.md` — Purpose: Claude commit and pull-request workflow rules. Status: **current contract**.
- `.claude/rules/unit-test-style.md` — Purpose: Claude unit-test style and discovery rules. Status: **current contract**.
- `.claude/skills/dal-code-style-review/SKILL.md` — Purpose: Claude style-review workflow. Status: **current contract**.
- `.claude/skills/dal-commit-and-pr/SKILL.md` — Purpose: Claude commit, push, and PR workflow. Status: **current contract**.
- `.claude/skills/dal-unit-test-skill/SKILL.md` — Purpose: Claude focused-test execution workflow. Status: **current contract**.
- `.claude/skills/dal-unit-test-write/SKILL.md` — Purpose: Claude unit-test authoring workflow. Status: **current contract**.
- `.claude/skills/dal-web-setup/SKILL.md` — Purpose: Claude DAL web setup and validation workflow. Status: **current contract**.
- `.claude/specs/dal-web-db-persistence.md` — Purpose: web persistence specification and migration contract. Status: **implemented history**.
- `.claude/specs/joint-aad-gradient.md` — Purpose: joint AAD-gradient behavioral specification. Status: **implemented history**.
- `.claude/specs/multi-curve-simultaneous-example.md` — Purpose: simultaneous multi-curve example specification. Status: **implemented history**.
- `.claude/specs/pde-framework-reimplementation.md` — Purpose: PDE framework reimplementation specification. Status: **implemented history**.
- `.claude/specs/script-compiled-evaluator-alignment.md` — Purpose: compiled/tree-walk evaluator alignment specification. Status: **implemented history**.

## Codex contracts and artifacts (31 files)

- `.codex/README.md` — Purpose: Codex DAL role, reference, skill, and artifact map. Status: **current contract**.
- `.codex/agents/dal-api-designer.toml` — Purpose: Codex public-API design role contract. Status: **current contract**.
- `.codex/agents/dal-critic.toml` — Purpose: Codex specification-critique role contract. Status: **current contract**.
- `.codex/agents/dal-doc-writer.toml` — Purpose: Codex documentation role contract. Status: **current contract**.
- `.codex/agents/dal-implementer.toml` — Purpose: Codex implementation role contract. Status: **current contract**.
- `.codex/agents/dal-orchestrator.toml` — Purpose: Codex orchestration, routing, and reporting contract. Status: **current contract**.
- `.codex/agents/dal-performancer.toml` — Purpose: Codex performance-review role contract. Status: **current contract**.
- `.codex/agents/dal-reviewer.toml` — Purpose: Codex correctness and architecture-review contract. Status: **current contract**.
- `.codex/agents/dal-simplifier.toml` — Purpose: Codex behavior-preserving simplification contract. Status: **current contract**.
- `.codex/agents/dal-spec-writer.toml` — Purpose: Codex specification-writing contract. Status: **current contract**.
- `.codex/agents/dal-tester.toml` — Purpose: Codex test-planning and implementation contract. Status: **current contract**.
- `.codex/artifacts/designs/api-shape-dedup.md` — Purpose: proposal to deduplicate API-shape validation. Status: **active design**.
- `.codex/artifacts/reviews/dal-documentation-inventory.md` — Purpose: exhaustive file/status inventory for this review. Status: **current audit**.
- `.codex/artifacts/reviews/dal-documentation-review.md` — Purpose: findings, evidence, dispositions, and validation record for this review. Status: **current audit**.
- `.codex/references/benchmark-workflow.md` — Purpose: current benchmark discovery, smoke, and paired-regression workflow. Status: **current contract**.
- `.codex/references/code-style.md` — Purpose: Codex C++ and repository code-style rules. Status: **current contract**.
- `.codex/references/git-commit-pr.md` — Purpose: Codex commit and pull-request workflow rules. Status: **current contract**.
- `.codex/references/run-tests.md` — Purpose: Codex focused/full C++ test execution workflow. Status: **current contract**.
- `.codex/references/style-review.md` — Purpose: Codex style-review checklist and evidence rules. Status: **current contract**.
- `.codex/references/unit-test-style.md` — Purpose: Codex unit-test style and discovery rules. Status: **current contract**.
- `.codex/references/write-tests.md` — Purpose: Codex unit-test authoring workflow. Status: **current contract**.
- `.codex/skills/dal-agent-team/references/shared-rules.md` — Purpose: compatibility pointer to the canonical repository rules. Status: **current compatibility contract**.
- `.codex/skills/dal-git-pr/SKILL.md` — Purpose: Codex commit, push, and PR skill workflow. Status: **current contract**.
- `.codex/skills/dal-git-pr/agents/openai.yaml` — Purpose: Codex UI metadata and default prompt for the Git/PR skill. Status: **current contract**.
- `.codex/skills/dal-git-pr/references/publish-workflow.md` — Purpose: executable Codex publish workflow and stop conditions. Status: **current contract**.
- `.codex/skills/dal-web/SKILL.md` — Purpose: Codex DAL web task router. Status: **current contract**.
- `.codex/skills/dal-web/agents/openai.yaml` — Purpose: Codex UI metadata and default prompt for the web skill. Status: **current contract**.
- `.codex/skills/dal-web/references/backend-style.md` — Purpose: Codex DAL web backend engineering rules. Status: **current contract**.
- `.codex/skills/dal-web/references/design-system.md` — Purpose: Codex DAL web design-system rules. Status: **current contract**.
- `.codex/skills/dal-web/references/operations.md` — Purpose: Codex DAL web setup and validation workflow. Status: **current contract**.
- `.codex/skills/dal-web/references/web-standards.md` — Purpose: Codex frontend component and accessibility rules. Status: **current contract**.

## Coverage reconciliation

- Final inventory entries: **102**.
- Final files by type: **90 Markdown**, **10 TOML**, **2 YAML**.
- Final files by area: **5 root**, **6 component**, **28 docs**, **2 GitHub**,
  **30 Claude**, **31 Codex**.
- The 10 Claude agent roles and 10 Codex agent roles have the same role-name
  set. Their representations intentionally differ; see the review artifact.
- `docs/README.md` and the `CLAUDE.md` methodology section both reference all
  16 published methodology pages. The root `README.md` intentionally presents
  a shorter 10-page overview rather than another exhaustive index.
