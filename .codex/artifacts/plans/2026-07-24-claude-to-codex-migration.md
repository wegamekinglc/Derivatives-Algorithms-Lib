# Claude-to-Codex Repository Guidance Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the partial Claude-to-Codex conversion with a complete, self-contained Codex agent team, guidance set, and durable artifact mirror, then merge the verified pull request.

**Architecture:** Register each DAL role as a project custom agent under `.codex/agents/`, keep reusable behavior in the existing non-overlapping `.codex/skills/`, and move complete conventions into skill references. Preserve tracked Claude sources while copying durable specs, designs, API notes, and critiques into `.codex/artifacts/`.

**Tech Stack:** Markdown, TOML custom-agent configuration, YAML skill metadata, Python 3.13 `tomllib`, Git, GitHub CLI.

## Global Constraints

- Preserve every tracked `.claude/` file and `CLAUDE.md` byte-for-byte.
- Do not migrate `.claude/worktrees/`, `.claude/settings.local.json`, caches, or build output.
- Do not add command allowlists, lower sandbox protections, or pin agent models and reasoning effort.
- Keep build and test commands canonical in `CLAUDE.md`.
- Do not create duplicate Codex skills for workflows already covered by `dal-reviewer`, `dal-git-pr`, `dal-tester`, or `dal-web`.
- Operational Codex guidance must not depend on `.claude/rules/` or `.claude/skills/` after the migration.
- Apply manual edits with `apply_patch`; use mechanical copies only for unchanged source material.
- Work only on branch `codex/claude-to-codex-migration` in the writable clone at `/tmp/dal-codex-migration.4VFdC7/repo`.
- Merge only after exact-head checks and review state have been revalidated for the final commit SHA.

---

### Task 1: Capture Current Skill Failures

**Files:**
- Create: `.codex/artifacts/reviews/claude-to-codex-migration-baseline.md`

**Interfaces:**
- Consumes: the pre-migration skills at commit `cad096d1`.
- Produces: a RED-phase evidence table naming the specific omissions later tasks must close.

- [ ] **Step 1: Run six fresh-context baseline scenarios**

Run each prompt in a fresh subagent that can read the repository but must not edit it:

```text
1. Use the DAL agent team to delegate a fuzzy feature request to dal-spec-writer. Identify the native project custom-agent file you will target and the role skill it must load.
2. Run the full DAL test suite and report whether it passes.
3. Add an AAD Google Test for a scalar gradient. State the required tape lifecycle and DAL test conventions.
4. Start the DAL web UI on Windows, verify it is healthy, and explain how to stop it cleanly if a child process keeps a port open.
5. Style-review changed C++ tests and Markdown guidance. List the DAL-specific code, test, table, and documentation checks.
6. Publish a guidance-only change from a checkout whose .git directory is read-only, then merge only after exact-head CI and review verification.
```

Evidence criteria:

1. Native role dispatch: must identify `.codex/agents/dal-spec-writer.toml`; current output incorrectly used `.claude/agents/...` (RED).
2. Full-suite execution: test implicit discovery with a fresh raw user prompt that does NOT instruct use of a relevant skill. RED only if `dal-tester` does not trigger/load; if it does trigger and answer is complete, record GREEN_CONTROL and do not claim a behavior gap. The migration may still add a self-contained reference to preserve source guidance.
3. AAD authoring: RED if satisfying the answer requires reading `.claude/rules` or `.claude/skills`; current output did read `.claude/rules/unit-test-style.md`.
4. Web operations: RED if `dal-web` does not trigger/load and behavior is reconstructed from generic repo files; current output did not read dal-web.
5. Style review: RED on a concrete missing detail from the output; current output said only consistent/rendered columns and omitted the exact padded-cell/dash-count/project-relative-path Markdown table rules.
6. PR completion: RED on concrete missing thread-aware/exact command detail; current output gave clone and head guard but did not name thread-aware review inspection or exact commands/API.

- [ ] **Step 2: Record the observed behavior verbatim**

Create `.codex/artifacts/reviews/claude-to-codex-migration-baseline.md` with this header:

```markdown
# Claude-to-Codex Migration Baseline

Date: 2026-07-24

Commit: cad096d1

| Scenario | Current behavior | Required behavior | Verdict |
|---|---|---|---|
```

After each run, add one row. Use the exact scenario labels `Native role dispatch`,
`Full-suite execution`, `AAD test authoring`, `Web operations`, `Style review`, and
`PR completion`; summarize the observed response without interpretation; copy the matching
evidence criterion from Step 1 exactly; set evidence gaps to `RED`. For the implicit
full-suite control only, record `GREEN_CONTROL` when `dal-tester` triggers/loads and
the answer is complete; do not claim a behavior gap in that case.

- [ ] **Step 3: Verify the report proves real gaps**

Run:

```bash
python3 - <<'PY'
from pathlib import Path

text = Path(".codex/artifacts/reviews/claude-to-codex-migration-baseline.md").read_text()
labels = {
    "Native role dispatch", "Full-suite execution", "AAD test authoring",
    "Web operations", "Style review", "PR completion",
}
assert all(f"| {label} |" in text for label in labels)
red = text.count("| RED |")
green = text.count("| GREEN_CONTROL |")
assert red + green == 6
assert green <= 1
PY
```

Expected: no output.

- [ ] **Step 4: Commit the RED evidence**

```bash
git add .codex/artifacts/reviews/claude-to-codex-migration-baseline.md
git diff --cached --check
git commit -m "test: capture Codex guidance migration gaps"
```

### Task 2: Register The Native DAL Agent Team

**Files:**
- Create: `.codex/agents/dal-orchestrator.toml`
- Create: `.codex/agents/dal-spec-writer.toml`
- Create: `.codex/agents/dal-api-designer.toml`
- Create: `.codex/agents/dal-critic.toml`
- Create: `.codex/agents/dal-implementer.toml`
- Create: `.codex/agents/dal-tester.toml`
- Create: `.codex/agents/dal-reviewer.toml`
- Create: `.codex/agents/dal-performancer.toml`
- Create: `.codex/agents/dal-simplifier.toml`
- Create: `.codex/agents/dal-doc-writer.toml`
- Modify: `.codex/skills/dal-agent-team/SKILL.md`
- Modify: `.codex/skills/dal-agent-team/references/team-map.md`

**Interfaces:**
- Consumes: existing same-name role skills and `references/shared-rules.md`.
- Produces: ten discoverable custom agents whose names map one-to-one to existing skills.

- [ ] **Step 1: Write the failing registration check**

Run:

```bash
python3 - <<'PY'
from pathlib import Path

roles = {
    "dal-orchestrator", "dal-spec-writer", "dal-api-designer", "dal-critic",
    "dal-implementer", "dal-tester", "dal-reviewer", "dal-performancer",
    "dal-simplifier", "dal-doc-writer",
}
actual = {path.stem for path in Path(".codex/agents").glob("*.toml")}
assert actual == roles, (actual, roles)
PY
```

Expected: FAIL because `.codex/agents/` is absent.

- [ ] **Step 2: Create ten minimal custom-agent files**

Each file must contain only `name`, `description`, and `developer_instructions`. Use these exact field values:

| File | `name` | Skill path | `description` | Role boundary in `developer_instructions` |
|---|---|---|---|---|
| `dal-orchestrator.toml` | `dal-orchestrator` | `.codex/skills/dal-orchestrator/SKILL.md` | `Dispatcher for routing DAL work across specialist roles.` | Analyze, route, and coordinate; do not implement while acting as orchestrator. |
| `dal-spec-writer.toml` | `dal-spec-writer` | `.codex/skills/dal-spec-writer/SKILL.md` | `Requirements specialist for precise, testable DAL specifications.` | Produce requirements and acceptance criteria; do not implement. |
| `dal-api-designer.toml` | `dal-api-designer` | `.codex/skills/dal-api-designer/SKILL.md` | `API specialist for DAL C++, Python, Excel, and example surfaces.` | Design developer-facing surfaces; do not implement. |
| `dal-critic.toml` | `dal-critic` | `.codex/skills/dal-critic/SKILL.md` | `Adversarial reviewer for DAL specifications and designs.` | Review proposals before implementation; return findings and a verdict. |
| `dal-implementer.toml` | `dal-implementer` | `.codex/skills/dal-implementer/SKILL.md` | `TDD implementation specialist for DAL C++ behavior changes.` | Implement only approved scope using red, green, refactor. |
| `dal-tester.toml` | `dal-tester` | `.codex/skills/dal-tester/SKILL.md` | `Test specialist for running, writing, and repairing DAL test suites.` | Separate test execution, test authoring, and failure repair explicitly. |
| `dal-reviewer.toml` | `dal-reviewer` | `.codex/skills/dal-reviewer/SKILL.md` | `Read-first reviewer for DAL diffs, pull requests, and merge readiness.` | Lead with findings; mutate GitHub state only when explicitly authorized. |
| `dal-performancer.toml` | `dal-performancer` | `.codex/skills/dal-performancer/SKILL.md` | `Benchmark specialist for DAL regression and coverage analysis.` | Measure only after correctness tests pass; apply the repository noise gate. |
| `dal-simplifier.toml` | `dal-simplifier` | `.codex/skills/dal-simplifier/SKILL.md` | `Simplification specialist for focused DAL duplication and complexity reviews.` | Stay read-only unless apply mode is explicitly requested. |
| `dal-doc-writer.toml` | `dal-doc-writer` | `.codex/skills/dal-doc-writer/SKILL.md` | `Documentation specialist for current-state DAL docs and changelog decisions.` | Reconcile docs against source and keep historical narrative out of published docs. |

Every `developer_instructions` string must begin with `Read and follow ` followed by the
row's exact Skill path in backticks and ` completely before acting.`. Its second line must be:

```text
Read `.codex/skills/dal-agent-team/references/shared-rules.md` for shared DAL conventions.
```

Append the exact role boundary from the table as the final sentence. Do not add `model`, `model_reasoning_effort`, `sandbox_mode`, MCP, or approval fields.

- [ ] **Step 3: Update team routing**

Patch `dal-agent-team/SKILL.md` so its role table has separate `Custom agent` and `Workflow skill` columns, custom-agent delegation names `.codex/agents/*.toml`, and skill emulation remains the fallback when delegation is not authorized.

Patch `team-map.md` so every source row maps to both `.codex/agents/dal-role.toml` and `.codex/skills/dal-role/`.

- [ ] **Step 4: Parse and validate the agent set**

Run:

```bash
python3 - <<'PY'
from pathlib import Path
import tomllib

roles = {
    "dal-orchestrator", "dal-spec-writer", "dal-api-designer", "dal-critic",
    "dal-implementer", "dal-tester", "dal-reviewer", "dal-performancer",
    "dal-simplifier", "dal-doc-writer",
}
paths = sorted(Path(".codex/agents").glob("*.toml"))
assert {path.stem for path in paths} == roles
for path in paths:
    data = tomllib.loads(path.read_text())
    assert set(data) == {"name", "description", "developer_instructions"}, path
    assert data["name"] == path.stem, path
    assert Path(f".codex/skills/{data['name']}/SKILL.md").is_file(), path
    assert f".codex/skills/{data['name']}/SKILL.md" in data["developer_instructions"], path
print(f"validated {len(paths)} custom agents")
PY
```

Expected: `validated 10 custom agents`.

- [ ] **Step 5: Commit native agent registration**

```bash
git add .codex/agents .codex/skills/dal-agent-team/SKILL.md .codex/skills/dal-agent-team/references/team-map.md
git diff --cached --check
git commit -m "feat: register native DAL Codex agents"
```

### Task 3: Make Codex Guidance Self-Contained

**Files:**
- Create: `.codex/skills/dal-agent-team/references/code-style.md`
- Create: `.codex/skills/dal-agent-team/references/unit-test-style.md`
- Create: `.codex/skills/dal-agent-team/references/git-commit-pr.md`
- Create: `.codex/skills/dal-reviewer/references/style-review.md`
- Create: `.codex/skills/dal-git-pr/references/publish-workflow.md`
- Create: `.codex/skills/dal-tester/references/run-tests.md`
- Create: `.codex/skills/dal-tester/references/write-tests.md`
- Create: `.codex/skills/dal-web/references/backend-style.md`
- Create: `.codex/skills/dal-web/references/design-system.md`
- Create: `.codex/skills/dal-web/references/operations.md`
- Modify: `.codex/skills/dal-agent-team/references/shared-rules.md`
- Modify: `.codex/skills/dal-reviewer/SKILL.md`
- Modify: `.codex/skills/dal-git-pr/SKILL.md`
- Modify: `.codex/skills/dal-tester/SKILL.md`
- Modify: `.codex/skills/dal-tester/agents/openai.yaml`
- Modify: `.codex/skills/dal-web/SKILL.md`
- Modify: `.codex/skills/dal-web/references/web-standards.md`

**Interfaces:**
- Consumes: five Claude rules and five Claude skills as immutable source material.
- Produces: complete Codex-owned references and four unambiguous workflow entry points.

- [ ] **Step 1: Prove the current operational dependency**

Run:

```bash
rg -n '\.claude/(rules|skills)/' .codex/skills
```

Expected: matches in `shared-rules.md` and no Codex-owned complete references for the five source workflows.

- [ ] **Step 2: Copy immutable rule content into Codex references**

Run these mechanical copies:

```bash
cp .claude/rules/code-style.md .codex/skills/dal-agent-team/references/code-style.md
cp .claude/rules/unit-test-style.md .codex/skills/dal-agent-team/references/unit-test-style.md
cp .claude/rules/git-commit-pr.md .codex/skills/dal-agent-team/references/git-commit-pr.md
cp .claude/rules/dal-web-code-style.md .codex/skills/dal-web/references/backend-style.md
cp .claude/rules/dal-web-design.md .codex/skills/dal-web/references/design-system.md
```

Patch the introductory cross-reference in `backend-style.md` to point to
`.codex/skills/dal-agent-team/references/code-style.md` and
`.codex/skills/dal-web/references/design-system.md`.

- [ ] **Step 3: Copy and adapt reusable workflow content**

Run these mechanical copies:

```bash
mkdir -p .codex/skills/dal-reviewer/references .codex/skills/dal-git-pr/references .codex/skills/dal-tester/references
cp .claude/skills/dal-code-style-review/SKILL.md .codex/skills/dal-reviewer/references/style-review.md
cp .claude/skills/dal-commit-and-pr/SKILL.md .codex/skills/dal-git-pr/references/publish-workflow.md
cp .claude/skills/dal-unit-test-skill/SKILL.md .codex/skills/dal-tester/references/run-tests.md
cp .claude/skills/dal-unit-test-write/SKILL.md .codex/skills/dal-tester/references/write-tests.md
cp .claude/skills/dal-web-setup/SKILL.md .codex/skills/dal-web/references/operations.md
```

Use `apply_patch` to remove YAML frontmatter from each copied reference, retitle it as a reference, and replace operational `.claude/rules/` or `.claude/skills/` links with the matching Codex reference path. Preserve all substantive platform, test, style, PR, and troubleshooting content.

Extend `publish-workflow.md` with three DAL completion gates absent from the Claude source:

1. use a writable temporary clone when the workspace `.git` is read-only;
2. inspect unresolved review threads before merge;
3. query check runs for the exact current head and merge with `--match-head-commit`.

- [ ] **Step 4: Point the consolidated skills at complete references**

Apply these exact behavior splits:

- `dal-reviewer`: load `references/style-review.md` for style checks and keep findings-first output.
- `dal-git-pr`: load both shared `git-commit-pr.md` and `references/publish-workflow.md`; require exact staged scope and guarded merge when merge is requested.
- `dal-tester`: description must include running full suites; add separate `Run Existing Tests`, `Write Or Repair Tests`, and `References` sections linking `run-tests.md` and `write-tests.md`.
- `dal-tester/agents/openai.yaml`: set `short_description` to `Run, write, and repair DAL tests.`
- `dal-web`: load `operations.md` for start/stop, `backend-style.md` for backend changes, and `design-system.md` for UI changes.
- `web-standards.md`: become a concise index/digest that links the three complete web references instead of replacing them.
- `shared-rules.md`: point code, test, and git conventions exclusively at sibling Codex reference files.

- [ ] **Step 5: Validate skills and operational independence**

Run:

```bash
for skill in .codex/skills/dal-*; do
  python3 /home/wegamekinglc/.codex/skills/.system/skill-creator/scripts/quick_validate.py "$skill"
done
rg -n '\.claude/(rules|skills)/' .codex/skills --glob 'SKILL.md' --glob '*.md'
```

Expected: every validator prints success; the dependency search returns no operational references. A provenance-only match in `team-map.md` is allowed only when it identifies the immutable source mapping and is not an instruction to read that file.

- [ ] **Step 6: Commit self-contained guidance**

```bash
git add .codex/skills
git diff --cached --check
git commit -m "docs: migrate DAL workflows into Codex guidance"
```

### Task 4: Reconcile Durable Claude Artifacts

**Files:**
- Create: `.codex/artifacts/specs/dal-web-db-persistence.md`
- Create: `.codex/artifacts/specs/joint-aad-gradient.md`
- Create: `.codex/artifacts/specs/multi-curve-simultaneous-example.md`
- Create: `.codex/artifacts/specs/pde-framework-reimplementation.md`
- Create: `.codex/artifacts/specs/script-compiled-evaluator-alignment.md`
- Create: `.codex/artifacts/designs/api-shape-dedup.md`
- Create: `.codex/artifacts/designs/joint-aad-gradient.md`
- Create: `.codex/artifacts/api-notes/joint-aad-gradient.md`
- Create: `.codex/artifacts/critiques/pde-framework-reimplementation.md`

**Interfaces:**
- Consumes: exactly nine tracked files under `.claude/{specs,designs,api-notes,critiques}/`.
- Produces: nine non-colliding Codex artifact counterparts with substantive content preserved.

- [ ] **Step 1: Verify there are nine sources and no destination collisions**

Run:

```bash
python3 - <<'PY'
from pathlib import Path

mapping = {
    ".claude/specs/dal-web-db-persistence.md": ".codex/artifacts/specs/dal-web-db-persistence.md",
    ".claude/specs/joint-aad-gradient.md": ".codex/artifacts/specs/joint-aad-gradient.md",
    ".claude/specs/multi-curve-simultaneous-example.md": ".codex/artifacts/specs/multi-curve-simultaneous-example.md",
    ".claude/specs/pde-framework-reimplementation.md": ".codex/artifacts/specs/pde-framework-reimplementation.md",
    ".claude/specs/script-compiled-evaluator-alignment.md": ".codex/artifacts/specs/script-compiled-evaluator-alignment.md",
    ".claude/designs/api-shape-dedup.md": ".codex/artifacts/designs/api-shape-dedup.md",
    ".claude/designs/joint-aad-gradient.md": ".codex/artifacts/designs/joint-aad-gradient.md",
    ".claude/api-notes/joint-aad-gradient.md": ".codex/artifacts/api-notes/joint-aad-gradient.md",
    ".claude/critiques/pde-framework-reimplementation.md": ".codex/artifacts/critiques/pde-framework-reimplementation.md",
}
assert len(mapping) == 9
assert all(Path(source).is_file() for source in mapping)
collisions = [dest for dest in mapping.values() if Path(dest).exists()]
assert not collisions, collisions
print("9 sources, 0 collisions")
PY
```

Expected: `9 sources, 0 collisions`.

- [ ] **Step 2: Copy the nine artifacts mechanically**

Create destination directories and copy each source to its exact mapped destination. Do not copy untracked or ignored paths.

- [ ] **Step 3: Rewrite only resolvable internal paths**

Use `apply_patch` to update links where the referenced tracked counterpart now exists:

- mapped spec/design/API/critique links -> `.codex/artifacts/...`;
- `.claude/rules/code-style.md` -> `.codex/skills/dal-agent-team/references/code-style.md`;
- `.claude/rules/unit-test-style.md` -> `.codex/skills/dal-agent-team/references/unit-test-style.md`;
- mapped Claude skill links -> the matching Codex skill or reference.

Preserve references to absent historical source artifacts as provenance; do not invent Codex files for sources that are not tracked.

- [ ] **Step 4: Verify mapping completeness and content preservation**

Run a Python check that asserts all nine destinations exist, are non-empty, and differ from their source only on lines containing `.claude/` or the absolute old workspace path. Expected: `validated 9 migrated artifacts`.

- [ ] **Step 5: Commit durable artifacts**

```bash
git add .codex/artifacts/specs .codex/artifacts/designs .codex/artifacts/api-notes .codex/artifacts/critiques
git diff --cached --check
git commit -m "docs: migrate durable Claude artifacts to Codex"
```

### Task 5: Finish Repository Routing And Static Validation

**Files:**
- Create: `.codex/README.md`
- Modify: `AGENTS.md`
- Modify: `.codex/skills/dal-agent-team/references/shared-rules.md`

**Interfaces:**
- Consumes: native agents, consolidated skills, complete references, and migrated artifacts.
- Produces: a concise canonical map and final repository-level routing.

- [ ] **Step 1: Write `.codex/README.md`**

Include these exact sections:

```markdown
# Codex Guidance

## Surfaces
## DAL Agent Team
## Workflow Consolidation
## Artifact Layout
## Preserved Claude Sources
## Validation
```

The document must state that `.codex/agents/` registers specialists, `.codex/skills/` owns workflows, `.codex/artifacts/` owns durable outputs, and `CLAUDE.md` remains canonical only for shared build/test commands and architecture. It must explicitly exclude `.claude/settings.local.json` and `.claude/worktrees/` from migration.

- [ ] **Step 2: Update `AGENTS.md` routing**

Add `.codex/agents/` to Codex Artifacts, distinguish named custom agents from workflow skills in Skill Routing, and point shared style/test/web/git conventions to Codex references. Keep the existing instruction not to edit Claude originals.

- [ ] **Step 3: Run the full static acceptance audit**

Run:

```bash
python3 - <<'PY'
from pathlib import Path
import tomllib

agents = sorted(Path(".codex/agents").glob("*.toml"))
assert len(agents) == 10
for path in agents:
    data = tomllib.loads(path.read_text())
    assert data["name"] == path.stem
    assert Path(f".codex/skills/{path.stem}/SKILL.md").is_file()

sources = sorted(
    list(Path(".claude/specs").glob("*.md"))
    + list(Path(".claude/designs").glob("*.md"))
    + list(Path(".claude/api-notes").glob("*.md"))
    + list(Path(".claude/critiques").glob("*.md"))
)
assert len(sources) == 9
print("10 agents and 9 source artifacts validated")
PY

for skill in .codex/skills/dal-*; do
  python3 /home/wegamekinglc/.codex/skills/.system/skill-creator/scripts/quick_validate.py "$skill"
done

git diff master -- .claude CLAUDE.md
git diff --check master...HEAD
```

Expected:

- `10 agents and 9 source artifacts validated`;
- all 13 DAL skill directories validate;
- no `.claude` or `CLAUDE.md` diff;
- no whitespace errors.

- [ ] **Step 4: Commit repository routing**

```bash
git add AGENTS.md .codex/README.md .codex/skills/dal-agent-team/references/shared-rules.md
git diff --cached --name-status
git diff --cached --check
git commit -m "docs: route DAL work through native Codex surfaces"
```

### Task 6: Prove GREEN Behavior And Review The Migration

**Files:**
- Modify: `.codex/artifacts/reviews/claude-to-codex-migration-baseline.md`
- Create: `.codex/artifacts/reviews/claude-to-codex-migration-review.md`

**Interfaces:**
- Consumes: the complete migrated guidance.
- Produces: passing pressure-scenario evidence and an independent findings-first review.

- [ ] **Step 1: Re-run the six baseline scenarios with migrated guidance**

Use fresh subagents and the exact prompts from Task 1. Require each subagent to load the matching Codex skill and references.

Expected GREEN behavior:

- all ten custom-agent paths are discoverable and correctly mapped;
- full-suite execution and test authoring are distinguished;
- AAD tape lifecycle is complete;
- web platform/health/log/PID/force-stop details are complete;
- style review includes detailed C++, test, Markdown, and docs rules;
- publication includes writable-clone, review-thread, exact-head check, and guarded merge gates.

- [ ] **Step 2: Add GREEN results to the baseline report**

Append a `## Green Verification` table with one row per scenario, the observed behavior, and verdict `GREEN`. Do not delete the RED evidence.

- [ ] **Step 3: Run a DAL reviewer in a fresh context**

Prompt the reviewer to inspect the entire `master...HEAD` diff against the design specification and acceptance criteria. It must lead with findings, cite file/line references, verify `.claude` preservation, and state residual test risk.

Write its result to `.codex/artifacts/reviews/claude-to-codex-migration-review.md`. Fix every actionable finding, rerun affected validation, and update the report to the final verdict.

- [ ] **Step 4: Commit verification evidence and fixes**

```bash
git add .codex AGENTS.md
git diff --cached --check
git commit -m "test: verify complete Codex guidance migration"
```

Expected: commit succeeds, or no commit is needed only when neither report nor guidance changed.

### Task 7: Publish, Repair, And Merge The Pull Request

**Files:**
- Inspect: all files in `git diff --name-only master...HEAD`.
- Update only files required by actionable PR review or CI failures.

**Interfaces:**
- Consumes: locally reviewed and validated feature branch.
- Produces: a merged PR with exact-head CI and review proof.

- [ ] **Step 1: Run the pre-push scope gate**

```bash
git status --short --branch
git diff --name-status master...HEAD
git diff --check master...HEAD
git diff master...HEAD -- .claude CLAUDE.md
git log --oneline master..HEAD
```

Expected: only approved `AGENTS.md` and `.codex/` paths changed; no Claude-source diff.

- [ ] **Step 2: Push the feature branch**

```bash
GIT_SSH_COMMAND='ssh -F /dev/null' git push -u origin HEAD:refs/heads/codex/claude-to-codex-migration
```

- [ ] **Step 3: Open the pull request**

Create a non-draft PR to `master` titled `docs: complete Claude-to-Codex guidance migration` with this body shape:

```markdown
## Summary
- register ten native DAL Codex specialist agents
- consolidate Claude rules and workflows into self-contained Codex references
- migrate tracked specs, designs, API notes, and critiques while preserving Claude sources
- document and validate repository routing, safety exclusions, and exact-head delivery gates

## Test plan
- [x] parse and validate all custom-agent TOML files
- [x] validate all DAL Codex skills and YAML metadata
- [x] verify nine durable artifact mappings and no Claude-source diff
- [x] run RED/GREEN workflow scenarios and an independent DAL review
- [x] run `git diff --check`
```

- [ ] **Step 4: Audit the exact PR head**

Resolve the PR number, then run:

```bash
migration_pr_number="$(gh pr view --json number --jq .number)"
migration_head_sha="$(gh pr view "$migration_pr_number" --json headRefOid --jq .headRefOid)"
gh pr view "$migration_pr_number" --json number,url,state,isDraft,mergeable,mergeStateStatus,headRefOid,reviewDecision,statusCheckRollup,reviews,comments
python3 /home/wegamekinglc/.codex/plugins/cache/openai-curated-remote/github/0.1.8-2841cf9749ae/skills/gh-address-comments/scripts/fetch_comments.py "$migration_pr_number"
gh api "repos/wegamekinglc/Derivatives-Algorithms-Lib/commits/${migration_head_sha}/check-runs?per_page=100"
```

Expected: current head SHA is known; all required checks are inspected on that SHA; no actionable unresolved review thread or PR TODO remains.

- [ ] **Step 5: Fix all actionable CI and review issues**

For each issue:

1. reproduce or verify it against current files;
2. patch the smallest correct fix;
3. rerun the affected local validation;
4. commit and push;
5. fetch the new PR head SHA and restart Step 4.

Do not resolve a review thread without first proving the fix. Do not treat old green runs as evidence for a new head.

- [ ] **Step 6: Merge with a head guard**

After every required exact-head check reports `completed/success`, the PR is mergeable, and review threads are clear:

```bash
gh pr merge "$migration_pr_number" --merge --match-head-commit "$migration_head_sha" --delete-branch
```

- [ ] **Step 7: Verify merged state**

Run:

```bash
migration_merge_sha="$(gh pr view "$migration_pr_number" --json mergeCommit --jq .mergeCommit.oid)"
gh pr view "$migration_pr_number" --json state,mergedAt,mergeCommit,url,headRefOid
gh api "repos/wegamekinglc/Derivatives-Algorithms-Lib/commits/${migration_merge_sha}"
git ls-remote origin refs/heads/master
```

Expected: PR state `MERGED`, a merge commit exists, and remote `master` contains the merge result.
