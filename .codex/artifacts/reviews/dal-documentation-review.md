# DAL Documentation Review

Review date: 2026-08-02

## Scope and baseline

The review started from a clean dedicated branch whose `HEAD` and freshly
resolved `origin/master` were both
`b60c07491cd01a9e429bfc87c21bab6aced40118` (`fix: prevent Report notice
lifetime misuse (DAL-57) (#273)`, committed 2026-08-02 06:36:04 +08:00).
The regenerated starting inventory contained 117 in-scope tracked files: 105
Markdown documents, 10 `.codex/agents/*.toml` contracts, and two
`.codex/skills/**/agents/openai.yaml` interfaces.

Every tracked Markdown file was opened and parsed. Semantic reconciliation used
current public headers, source, bindings, examples, tests, build manifests,
OpenAPI, CI, repository contracts, and active artifacts. Retained plans,
specifications, designs, and critiques were treated as proposed or historical
evidence unless current source and tests proved delivery. The completed
artifact cleanup leaves 102 in-scope files: 90 Markdown documents, 10 TOML
contracts, and two YAML interfaces. The exact file-by-file classification is in
[DAL documentation inventory](dal-documentation-inventory.md).

## Findings and dispositions

### Medium — completed Codex work remained marked as active

`.codex/README.md` reserves `.codex/artifacts/` for work products that still
control active work. Sixty tracked files instead preserved completed DAL-10,
DAL-33, and Curve Lab delivery state, including statements that PRs were open,
unmerged, or awaiting gates that are now complete.

Current `origin/master` proves the relevant outcomes:

- DAL-10 is represented by merged PR #259 (`f8381ffc`) and the current
  calibration workbench source, tests, and `dal-web/README.md`.
- DAL-33 is represented by merged PR #262 (`8d34e79e`), its matrix tests,
  `docs/methodology/matrix.md`, and `CHANGELOG.md`.
- Curve Lab is represented by merged PR #265 (`b089b8c9`), the current backend,
  frontend, OpenAPI, tests, `docs/curve-lab.md`, `dal-web/README.md`,
  `docs/public-api.md`, and `CHANGELOG.md`.

The following completed artifact groups were removed; Git history preserves all
of their delivery and review evidence:

- DAL-33: `.codex/artifacts/DAL-33-codacy-complexity-before.json`,
  `.codex/artifacts/DAL-33-implementer-v2.md`,
  `.codex/artifacts/DAL-33-performance-v2/paired/results.json`, its
  `summary.md`, and all 40
  `krylov_perf/{01..20}-{base,head}.txt` samples.
- Curve Lab design/review:
  `.codex/artifacts/critiques/curve-lab-technical-design-review.md`, all five
  tracked files under
  `.codex/artifacts/designs/curve-lab-revision-8/`,
  `.codex/artifacts/reviews/curve-lab-dal17-final-disposition.md`,
  `.codex/artifacts/reviews/curve-lab-dal20-documentation-decision.md`, and
  `.codex/artifacts/specs/curve-lab-dal-web-v0.5.md`.
- Curve Lab execution: all six plans named
  `.codex/artifacts/plans/2026-07-28-curve-lab-dal23-repairs.md`,
  `2026-07-28-curve-lab-dal23-round-3.md`,
  `2026-07-29-curve-lab-dal19-round5.md`,
  `2026-07-30-curve-lab-canonical-quote-wiring.md`,
  `2026-07-30-curve-lab-m1-m2-repairs.md`, and
  `curve-lab-revision-8-implementation.md`.
- DAL-10: `.codex/artifacts/plans/dal-10-contract-evidence.md`.

The active `designs/api-shape-dedup.md` remains because it is design-only and
still awaits approval. This inventory and review remain as the active handoff
for independent documentation review.

### Low — current Curve Lab tables were not exactly aligned

Ten pipe tables added or extended with Curve Lab content had valid column
counts but did not satisfy the repository rule that every column use its exact
maximum content width. The tables were mechanically aligned without changing
their words, links, API claims, or ordering:

- `dal-web/README.md` — four tables covering DAL calls, persisted calibration
  entities, endpoint highlights, and Curve Lab operations;
- `docs/curve-lab.md` — five tables covering topology, endpoints, archive
  roots, matrix semantics, and admission limits;
- `docs/public-api.md` — the Python workflow table.

### No content change — source changes after the Curve Lab documentation pass

The remaining source changes after the latest Curve Lab documentation commits
do not require published prose or changelog updates:

- PR #270 freezes admitted risk inputs, deduplicates runtime paths, offloads
  native admission work, and adds review coverage while explicitly preserving
  public and OpenAPI contracts, restart behavior, deadlines, and ordering.
- PR #269 already updated `docs/public-api.md` for the closed deposit node
  sensitivity failure contract.
- PR #266 initializes internal Black-Scholes clone state without changing the
  public surface.
- The BCG workspace-boundary target and PR #273 Report notice lifetime fix are
  internal test/correctness changes with unchanged methodology and APIs.

No new methodology page, numerical algorithm, public removal, deprecation,
breaking API, or significant capability is introduced by this maintenance
patch. `CHANGELOG.md`, `docs/README.md`, and the `CLAUDE.md` methodology list
therefore require no edit.

## Modified-file disposition

- `.codex/artifacts/reviews/dal-documentation-inventory.md` — refreshes the
  exhaustive inventory, adds `docs/curve-lab.md`, and records final counts.
- `.codex/artifacts/reviews/dal-documentation-review.md` — records this audit,
  discrepancies, lifecycle decisions, agent reconciliation, and verification.
- `dal-web/README.md` — aligns four current pipe tables only.
- `docs/curve-lab.md` — aligns five current pipe tables only.
- `docs/public-api.md` — aligns one current pipe-table row only.
- The 60 files enumerated under the completed-artifact finding were removed
  because their work is merged and no longer controls an active gate.

## Agent contract and Multica roster reconciliation

The DAL squad contains 10 agent members, and its name set exactly matches the
10 `.codex/agents/*.toml` names. For every member:

- Multica `description` exactly matches the TOML `description`;
- descriptions are 56–77 Unicode code points, below the 255-code-point limit;
- Multica `instructions` exactly match normalized TOML
  `developer_instructions`;
- no Multica `description` or `instructions` update is necessary.

No repository agent TOML changed. Runtime, model, thinking level, service tier,
skills, environment, MCP, visibility, concurrency, avatar, squad membership,
role, and leader fields were read but not modified.

## Intentional Claude/Codex differences

- Both platforms retain the same 10 role names and cover specification, API
  design, critique, implementation, testing, review, documentation,
  performance, simplification, and orchestration.
- Claude keeps verbose Markdown contracts, Claude-specific tool/invocation
  language, `.claude/` artifact paths, and its worktree conventions. Codex keeps
  compact TOML contracts, explicit delegation authorization, focused shared
  references, and conditional `.codex/artifacts/` outputs.
- Both route new work through specification, optional API design, critique,
  implementation, testing, review, and documentation; performance and
  simplification remain post-correctness sidecars. Both require reviewer
  coverage for code changes.
- Claude's five user-facing skill documents and Codex's two reusable skills
  plus shared references are different platform packaging, not missing role
  coverage.

No unsupported role, routing, responsibility, permission, escalation, handoff,
or artifact-path drift remains. Implemented `.claude/**` records and historical
`docs/superpowers/**` material remain deliberately classified as retained
history outside the Codex active-artifact lifecycle.

## Validation record

- `python3 .github/scripts/check_docs.py` — passed for the repository checker's
  40 published and top-level agent-facing Markdown files.
- `python3 -m unittest discover -s .github/scripts/tests -p 'test_check_docs.py' -v`
  — passed all 18 tests.
- Full-scope Markdown/TOML/YAML audit — 90 Markdown documents (24,379 lines),
  376 links/images, 93 pipe tables, 10 valid TOML contracts, two valid YAML
  interfaces, 17 valid frontmatter blocks, 10 roles per platform, and 16
  indexed methodology pages; zero local-link, anchor, pipe-table, whitespace,
  final-newline, parse, role-set, or index errors.
- Multica DAL roster comparison — 10 squad agents mapped one-to-one to the 10
  TOML contracts; zero `description` or `instructions` differences.
- `git diff --check` — passed.

No C++ build or test suite was run: this patch changes documentation formatting,
active-artifact lifecycle state, and audit records only, and the doc-writer role
does not edit or validate C++ behavior.

## Remaining decisions and risks

1. `.github/scripts/check_docs.py` covers 40 of the final 90 Markdown files; the
   full-inventory structural audit remains an explicit maintenance step rather
   than a CI gate.
2. Retention or relocation of implemented Claude records and
   `docs/superpowers/` history remains a repository-governance decision.
3. `.codex/artifacts/designs/api-shape-dedup.md` still awaits approval and must
   continue to be treated as a proposal, not shipped behavior.
