# Claude-to-Codex Repository Guidance Migration

Status: approved design

Date: 2026-07-24

## Context

The repository already has a partial Codex conversion:

- `AGENTS.md` routes Codex work to project-local skills.
- `.codex/skills/` contains a consolidated DAL role and workflow set.
- `.codex/artifacts/` contains Codex-created specifications, plans, reviews, and performance reports.

The conversion is incomplete. The Claude specialist roles are not registered as native Codex
custom agents, several Codex skills still depend on `.claude/rules/`, and the durable artifacts
under `.claude/{specs,designs,api-notes,critiques}/` do not have Codex-side counterparts.

## Goals

1. Make the complete DAL specialist team directly available as project-scoped Codex custom
   agents.
2. Make Codex guidance self-contained under `.codex/` without duplicating overlapping skill
   triggers.
3. Preserve the durable Claude planning and review artifacts in the matching Codex artifact
   hierarchy.
4. Keep `AGENTS.md` concise and route Codex sessions to the native agents, skills, references,
   and artifacts.
5. Preserve all tracked `.claude/` files unchanged.
6. Validate, publish, and merge the migration through a focused pull request whose exact head
   passes all required checks and review gates.

## Non-Goals

- Removing, renaming, or editing tracked `.claude/` files or `CLAUDE.md`.
- Migrating `.claude/worktrees/`, build products, caches, or other ignored runtime state.
- Translating `.claude/settings.local.json`; it is machine-local, ignored, and contains broad
  command permissions that must not become repository policy.
- Pinning models or reasoning effort for specialist agents.
- Changing C++, Python, Excel, or web application behavior.
- Creating Codex command-approval rules from prose coding conventions. `.codex/rules/*.rules`
  is reserved for executable command policy, not style documentation.

## Approaches Considered

### Literal mirror

Copy every Claude role, skill, rule, and artifact into a same-shaped Codex path.

This is easy to compare but creates multiple skills for the same trigger, retains
Claude-specific tool vocabulary, and makes future guidance drift more likely.

### Agents only

Add native custom-agent TOML files and leave the existing skill and rule arrangement alone.

This is smaller but leaves Codex dependent on Claude rule files and omits durable planning
artifacts.

### Semantic full migration

Register native agents, consolidate equivalent Claude workflows into the existing Codex
skills, move complete guidance into Codex references, and copy durable artifacts into the
existing Codex hierarchy.

This is the selected approach because it is complete without creating ambiguous skill
triggers.

## Architecture

The Codex-side request flow will be:

```text
user request
    -> AGENTS.md routing
    -> dal-agent-team or a named .codex/agents specialist
    -> matching .codex/skills workflow
    -> Codex-owned reference guidance
    -> source/test/docs changes or .codex/artifacts output
```

Each layer has one responsibility:

- `AGENTS.md` defines durable repository routing and work style.
- `.codex/agents/*.toml` registers spawnable specialist identities.
- `.codex/skills/` defines reusable workflows and output contracts.
- skill `references/` contains the complete coding, testing, web, git, and team conventions.
- `.codex/artifacts/` stores specifications, designs, API notes, critiques, reviews, plans, and
  performance reports.

## Native Agent Team

Create one project-scoped custom-agent file for each DAL role:

| Agent | Matching skill | Primary responsibility |
|---|---|---|
| `dal-orchestrator` | `dal-orchestrator` | Route and coordinate end-to-end work |
| `dal-spec-writer` | `dal-spec-writer` | Produce testable requirements |
| `dal-api-designer` | `dal-api-designer` | Design public API and binding surfaces |
| `dal-critic` | `dal-critic` | Adversarially review pre-implementation designs |
| `dal-implementer` | `dal-implementer` | Implement behavior with TDD |
| `dal-tester` | `dal-tester` | Add, run, and repair tests |
| `dal-reviewer` | `dal-reviewer` | Review correctness and merge readiness |
| `dal-performancer` | `dal-performancer` | Measure regressions and benchmark coverage |
| `dal-simplifier` | `dal-simplifier` | Find focused simplification opportunities |
| `dal-doc-writer` | `dal-doc-writer` | Reconcile current-state docs and changelog |

Every `.toml` file will define the required `name`, `description`, and
`developer_instructions` fields. The instructions will require the specialist to read its
matching skill and the applicable DAL shared rules before acting.

Agents will inherit the parent session's model, reasoning effort, sandbox, tools, and
connector availability. No agent will silently broaden permissions. The orchestrator and
team skill will continue to spawn specialists only when the user or applicable repository
guidance explicitly requests delegation or team execution.

No `.codex/config.toml` feature toggle is required because current Codex releases enable
multi-agent support by default and discover project custom agents directly from
`.codex/agents/`.

## Skill And Rule Consolidation

The Claude workflows map into the existing Codex workflows:

| Claude workflow | Codex destination |
|---|---|
| `dal-code-style-review` | `dal-reviewer` plus shared code-style reference |
| `dal-commit-and-pr` | `dal-git-pr` plus git/PR reference |
| `dal-unit-test-skill` | `dal-tester` test-execution path |
| `dal-unit-test-write` | `dal-tester` test-authoring path |
| `dal-web-setup` | `dal-web` start/stop and troubleshooting paths |

The migration will enrich these existing skills instead of adding duplicate skill names.
Descriptions will remain trigger-focused, and detailed conventions will live in reference
files.

Complete Codex-owned references will cover:

- C++ and repository code style.
- Google Test conventions and DAL-specific test patterns.
- Git branch, commit, pull-request, and submodule conventions.
- `dal-web` backend style and async boundaries.
- `dal-web` frontend design system and review checklist.
- Team routing, artifact ownership, handoff, and safety rules.

After migration, operational Codex instructions must not require reading `.claude/rules/` or
`.claude/skills/`. Provenance documents may name the Claude sources only to explain the
migration.

## Durable Artifact Migration

Copy tracked durable artifacts without deleting their sources:

| Claude source | Codex destination |
|---|---|
| `.claude/specs/*.md` | `.codex/artifacts/specs/*.md` |
| `.claude/designs/*.md` | `.codex/artifacts/designs/*.md` |
| `.claude/api-notes/*.md` | `.codex/artifacts/api-notes/*.md` |
| `.claude/critiques/*.md` | `.codex/artifacts/critiques/*.md` |

Existing Codex artifacts take precedence if a destination name already exists. A collision
must be reconciled intentionally; it must not be overwritten mechanically.

Copied artifacts will preserve their substantive content. Internal links and path references
will be rewritten only where necessary to point at their Codex counterparts or current
repository paths. Historical status remains in these internal artifacts; the
current-state-only rule continues to apply to published `docs/`.

## Repository Guidance

Update `AGENTS.md`, the DAL team skill, the team map, and shared references so that:

- Native agent TOML files are the Codex specialist registration surface.
- Skills remain the role workflows and reusable task entry points.
- Codex artifacts use `.codex/artifacts/`.
- Claude guidance remains preserved source material, not an operational dependency.
- Build and test commands remain canonical in `CLAUDE.md`, as already established by the
  repository.

Add a concise `.codex/README.md` migration map so maintainers can identify canonical Codex
surfaces and explicit exclusions without reverse-engineering the directory tree.

## Safety And Failure Handling

- Begin and end with a working-tree scope check.
- Never delete or edit tracked `.claude/` content.
- Never migrate ignored worktrees, caches, build output, or local permissions.
- Do not add destructive command allowlists or lower sandbox/approval protections.
- Do not pin an agent model that could become stale or unavailable.
- Fail validation if a custom-agent file is invalid TOML, omits a required field, names a
  missing skill, or points operational guidance back to Claude-only rules or skills.
- Fail artifact migration on an unresolved destination collision.
- Keep publication isolated from unrelated changes and verify the staged path list before
  every commit.

## Verification

### Skill behavior

Use documentation TDD for each changed workflow:

1. Run representative pressure scenarios against the current skill and record the missing or
   incorrect behavior.
2. Make the smallest skill/reference change that closes the observed gap.
3. Re-run the same scenarios and confirm the Codex workflow produces the required behavior.
4. Check for new loopholes or trigger ambiguity.

Scenarios must cover role selection, native-agent handoff, test running versus test writing,
web start/stop, style review, and PR packaging.

### Static validation

- Parse every `.codex/agents/*.toml` file with Python `tomllib`.
- Assert every custom agent has `name`, `description`, and `developer_instructions`.
- Assert every role references an existing matching skill.
- Run the available Codex skill validator over every changed skill.
- Validate YAML metadata files.
- Check all migrated relative Markdown links.
- Confirm every tracked source artifact has a mapped Codex destination.
- Search Codex operational guidance for stale `.claude/rules/` and `.claude/skills/`
  dependencies.
- Confirm `.claude/` has no diff.
- Run `git diff --check`.

No C++ build is required unless CI or an unexpected repository validation demonstrates that
guidance files participate in the build.

### Independent review

Run the DAL reviewer over the final diff. Findings must be fixed or explicitly proven
non-actionable before publication.

## Publication And Merge

The workspace `.git` directory is read-only, so publication will use a writable temporary
clone:

1. Verify the workspace diff contains only the approved migration.
2. Create a branch from the current `origin/master`.
3. Copy the verified migration snapshot into the clone and confirm file hashes match.
4. Stage only intended files, run `git diff --cached --name-status` and
   `git diff --cached --check`, then commit.
5. Push the branch and open a pull request with the migration scope and verification evidence.
6. Inspect review threads, PR comments, mergeability, and checks on the exact current head.
7. Fix all actionable review and CI failures, push the fixes, and restart the exact-head audit.
8. Merge only with a head-SHA match guard after all required checks are successful and no
   unresolved blocking review thread or PR TODO remains.
9. Verify the merged PR state and the resulting `master` commit.

## Acceptance Criteria

- Ten DAL roles exist as valid project-scoped Codex custom agents.
- Every native agent maps to and loads the correct project Codex skill.
- Existing overlapping Claude skills are represented by enriched, non-duplicated Codex
  workflows.
- Complete Codex-owned coding, testing, git, web, and team references exist.
- All tracked Claude specs, designs, API notes, and critiques have reconciled Codex
  counterparts.
- `AGENTS.md` and the team guidance route Codex work to native Codex surfaces.
- Codex operational guidance has no dependency on `.claude/rules/` or `.claude/skills/`.
- Tracked `.claude/` files and `CLAUDE.md` remain unchanged.
- Ignored `.claude/worktrees/` and `.claude/settings.local.json` are not migrated.
- Static validation, skill behavior tests, and independent review pass.
- The pull request is merged only after exact-head CI and review verification.
