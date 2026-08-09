# DAL-92 Documentation and Agent-Contract Audit

Review date: 2026-08-09.

This artifact controls the independent-review handoff for DAL-92. Retire it after the
review outcome is represented in current documentation and Git history.

## Baseline

- Repository: `wegamekinglc/Derivatives-Algorithms-Lib`.
- Task branch: `agent/dal-doc-writer/4dc6ba07`.
- `git fetch --prune origin master` completed successfully.
- Audited `HEAD` and fetched `origin/master` both resolve to
  `512e3dcfb4cb6dbaa3cf6c4db5a057ae838bef86`.
- Baseline commit date and subject: 2026-08-04,
  `fix: reconcile repository workflows and documentation (#282)`.

## Scope

The baseline inventory was regenerated with `git ls-files`, not copied from an earlier audit.
It contains 96 tracked contract files:

- 84 Markdown files;
- 10 `.codex/agents/*.toml` registrations; and
- 2 `.codex/skills/**/agents/openai.yaml` interfaces.

This audit adds this Markdown artifact, so the final validated scope is 97 files:
85 Markdown, 10 TOML, and 2 YAML.

Baseline Markdown distribution:

- root: 5;
- component READMEs: 6;
- `docs/`: 24;
- `.github/`: 2;
- `.claude/`: 30; and
- `.codex/`: 17.

Final Markdown distribution differs only in `.codex/`, which increases to 18.

## Exhaustive Markdown Inventory

### Root and component documentation — current, except canonical history

```text
AGENTS.md
CHANGELOG.md
CLAUDE.md
CONTRIBUTING.md
README.md
dal-cpp/README.md
dal-excel/README.md
dal-public/README.md
dal-python/README.md
dal-web/README.md
dal-web/backend/README.md
```

`CHANGELOG.md` is canonical history. The other files in this group are current guides or contracts.

### Published documentation — current

```text
docs/README.md
docs/architecture.md
docs/curve-lab.md
docs/installation.md
docs/public-api.md
docs/methodology/_cpp-example-style.md
docs/methodology/aad.md
docs/methodology/black_scholes.md
docs/methodology/dates.md
docs/methodology/dupire.md
docs/methodology/index_parsing.md
docs/methodology/interpolation.md
docs/methodology/log_discount_curve.md
docs/methodology/matrix.md
docs/methodology/pde.md
docs/methodology/quadrature.md
docs/methodology/random.md
docs/methodology/script_engine.md
docs/methodology/underdetermined_search.md
docs/methodology/xccy_calibration.md
docs/methodology/yield_curve.md
docs/methodology/yield_curve_jacobian.md
```

`_cpp-example-style.md` is a maintainer contract. The other methodology pages describe
supported current behavior.

### Experimental documentation — explicitly non-normative

```text
docs/experimental/aad-analytic-jacobian-curve-calibration.md
docs/experimental/replicate-ptirds-single-currency-curve.md
```

### GitHub contracts — current

```text
.github/copilot-instructions.md
.github/pull_request_template.md
```

### Claude contracts — current

```text
.claude/agents/README.md
.claude/agents/dal-api-designer.md
.claude/agents/dal-critic.md
.claude/agents/dal-doc-writer.md
.claude/agents/dal-implementer.md
.claude/agents/dal-orchestrator.md
.claude/agents/dal-performancer.md
.claude/agents/dal-reviewer.md
.claude/agents/dal-simplifier.md
.claude/agents/dal-spec-writer.md
.claude/agents/dal-tester.md
.claude/rules/code-style.md
.claude/rules/dal-web-code-style.md
.claude/rules/dal-web-design.md
.claude/rules/git-commit-pr.md
.claude/rules/unit-test-style.md
.claude/skills/dal-code-style-review/SKILL.md
.claude/skills/dal-commit-and-pr/SKILL.md
.claude/skills/dal-unit-test-skill/SKILL.md
.claude/skills/dal-unit-test-write/SKILL.md
.claude/skills/dal-web-setup/SKILL.md
```

### Claude retained implementation history

```text
.claude/api-notes/joint-aad-gradient.md
.claude/critiques/pde-framework-reimplementation.md
.claude/designs/api-shape-dedup.md
.claude/designs/joint-aad-gradient.md
.claude/specs/dal-web-db-persistence.md
.claude/specs/joint-aad-gradient.md
.claude/specs/multi-curve-simultaneous-example.md
.claude/specs/pde-framework-reimplementation.md
.claude/specs/script-compiled-evaluator-alignment.md
```

These files record implemented work. Their source line citations, retired paths, measured
baselines, unchecked acceptance boxes, and staged commands are retained as historical evidence,
not current instructions. Each file now states that boundary and points to current authority.

### Codex contracts — current

```text
.codex/README.md
.codex/artifacts/README.md
.codex/references/benchmark-workflow.md
.codex/references/code-style.md
.codex/references/git-commit-pr.md
.codex/references/run-tests.md
.codex/references/style-review.md
.codex/references/unit-test-style.md
.codex/references/write-tests.md
.codex/skills/dal-agent-team/references/shared-rules.md
.codex/skills/dal-git-pr/SKILL.md
.codex/skills/dal-git-pr/references/publish-workflow.md
.codex/skills/dal-web/SKILL.md
.codex/skills/dal-web/references/backend-style.md
.codex/skills/dal-web/references/design-system.md
.codex/skills/dal-web/references/operations.md
.codex/skills/dal-web/references/web-standards.md
```

This file, `.codex/artifacts/DAL-92/doc-writer/audit.md`, is the one active review artifact.

## Findings and Corrections

### Current benchmark acceptance language

`CONTRIBUTING.md` said the paired regression gate reduces each confirmation round with
the median. The executable gate in `.github/scripts/check_benchmark_regressions.py` and
the canonical `.codex/references/benchmark-workflow.md` both reduce with the best-of-N
minimum. The contributor guide now states the implemented minimum rule.

### Historical artifact status

Nine retained Claude artifacts described completed work without a durable current/history
boundary. The API-shape design and script-evaluator specification also explicitly claimed that
implementation was still pending after the implementation had shipped. Each retained artifact
now identifies itself as implemented history and points to the current methodology, source, or
application guide. Historical body content was not rewritten as if it were current evidence.

### Active artifact indexes

`.codex/README.md` and `.codex/artifacts/README.md` previously said that no artifact controlled
active work. They now identify this DAL-92 handoff while independent review is pending.

## Agent and Platform Reconciliation

- The DAL squad contains exactly the same 10 names as `.codex/agents/*.toml` and
  `.claude/agents/dal-*.md`.
- Every Multica DAL agent `description` and `instructions` value exactly matches the
  authoritative TOML `description` and `developer_instructions` after whitespace normalization.
- Every authoritative description is within the 255-Unicode-code-point limit.
- No Multica agent field was changed.
- Claude and Codex preserve the same role coverage and stage order. Claude keeps verbose
  frontmatter, `EnterWorktree` and tool vocabulary, and `.claude/{specs,api-notes,critiques}`
  artifact roots. Codex keeps compact TOML contracts, explicit delegation authorization,
  repository-runtime branch handling, and `.codex/artifacts/` roots. These are justified
  platform differences rather than drift.
- The Claude orchestrator's restricted tool surface and report-driven handoff remain
  Claude-specific. Multica squad routing and status authority remain workspace-specific.
- Both surfaces require explicit authorization before side-effecting review, publication,
  or merge actions. DAL-92 authorizes a draft PR but explicitly forbids merge.

## Changelog Decision

No `CHANGELOG.md` entry is warranted. The patch corrects documentation and artifact status;
it introduces no breaking public API, numerical method, significant capability, methodology
shift, removal, or deprecation.

## Modified Files and Reasons

- `CONTRIBUTING.md` — match the benchmark gate's best-of-N minimum reduction.
- Nine files under `.claude/{api-notes,critiques,designs,specs}/` — mark shipped work as
  implemented history and identify current authority.
- `.codex/README.md` and `.codex/artifacts/README.md` — index the active DAL-92 review artifact.
- `.codex/artifacts/DAL-92/doc-writer/audit.md` — preserve exhaustive scope, evidence,
  decisions, checks, and handoff risk for independent review.

No methodology document was added, removed, or renamed, so `docs/README.md` and the
`CLAUDE.md` methodology list require no edit.

## Validation Record

- `python3 .github/scripts/check_docs.py` — passed for 55 curated Markdown files.
- `python3 -m unittest discover -s .github/scripts/tests -p 'test_check_docs.py' -v`
  — 24 tests passed.
- Full tracked-Markdown link, anchor, table, math-macro, whitespace, and final-newline audit
  — 85 files passed.
- TOML, skill YAML, and `SKILL.md` frontmatter parsing — 10 TOML, 2 YAML, and 7 skill
  frontmatters passed.
- Methodology-index reconciliation — both indexes exactly cover all 16 normative pages.
- Claude/Codex/Multica agent-set and exact Multica text reconciliation — 10 roles passed.
- Current-document path audit — no unexpected missing path; 18 runtime-created or illustrative
  path occurrences were classified explicitly.
- `git diff --check` — passed.

## Preserved Differences, Risks, and Blockers

- Retained implementation-history files intentionally keep historical source line citations,
  paths later retired from the tree, and pre-implementation commands. Their new status blocks
  those records from being mistaken for current usage guidance.
- Runtime-created paths such as `.server.pid`, `.server.log`, and `dal-python/.venv/`, plus
  illustrative placeholders in agent examples, are intentionally absent from Git.
- No C++ build or numerical test suite is required for this Markdown-only patch. Repository
  documentation checks and parser/static consistency checks are the acceptance surface.
- No blocker remains for independent review. The draft PR must not be merged by this stage.
