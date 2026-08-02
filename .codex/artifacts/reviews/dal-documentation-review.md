# DAL Documentation Review

Review date: 2026-08-02

## Scope and baseline

The final P5 refresh started from a clean dedicated branch whose `HEAD` and
freshly resolved `origin/master` were both
`0215d9e78417102cab47578689dc670b7e275e62` (`DAL-64: harden Krylov numerics
and validation (#275)`). The regenerated in-scope inventory contains 102
tracked files: 90 Markdown documents, 10 `.codex/agents/*.toml` contracts,
and two `.codex/skills/**/agents/openai.yaml` interfaces. Repository-wide
tracked totals are 90 Markdown, 12 TOML, and nine YAML/YML files.

The earlier exhaustive pass opened and parsed every tracked Markdown file. The
final P5 refresh re-ran the inventory and reconciled its changed claims against
current public headers, source, bindings, examples, tests, build manifests,
OpenAPI, CI, repository contracts, and active artifacts. Retained plans,
specifications, designs, and critiques were treated as proposed or historical
evidence unless current source and tests proved delivery. The completed
artifact cleanup still leaves the same 102 in-scope files at the final merged
tree. The exact file-by-file classification and reproducible count commands
are in the [DAL documentation inventory](dal-documentation-inventory.md).

`.github/scripts/check_docs.py` dynamically selects the root and component
guides, the published and retained `docs/` tree, and four agent-facing guides.
That selection currently contains 40 Markdown files.

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

### Medium — DAL-55 changelog disposition conflicted with precedent

The earlier audit treated PR #266 as an internal correction that did not need a
changelog entry. The accepted P5 API decision instead records
`DAL55_CHANGELOG=yes`. Immutable merge commit `417845e5` initializes
`BlackScholes_::todayOnTimeLine_` and `defLine_` and adds a poisoned-storage
pre-allocation clone regression. The existing `Matrix_` entry records the
analogous removal of latent uninitialized state without a signature change.

`CHANGELOG.md` now records the Black-Scholes correction in the same current
format and states that public constructor and binding signatures are unchanged.
It does not claim a new methodology or public API.

### No published-prose change — other final merged packages

The remaining source changes after the latest Curve Lab documentation commits
do not require published prose or changelog updates:

- PR #270 freezes admitted risk inputs, deduplicates runtime paths, offloads
  native admission work, and adds review coverage while explicitly preserving
  public and OpenAPI contracts, restart behavior, deadlines, and ordering.
- PR #269 already updated `docs/public-api.md` for the closed deposit node
  sensitivity failure contract.
- The BCG workspace-boundary target, PR #273 Report notice lifetime fix, and
  PR #275 test-harness/numerics hardening do not add a public API or methodology;
  the solver behavior in PR #275 is already covered by `CHANGELOG.md` and
  `docs/methodology/matrix.md`.

No new methodology page, numerical algorithm, public removal, deprecation,
breaking API, or significant capability is introduced by the remaining
packages. `docs/README.md` and the `CLAUDE.md` methodology list therefore
require no edit.

## P5 modified-file disposition

- `CHANGELOG.md` — corrects the seven-family citations and records the
  source-backed DAL-55 latent-state fix.
- `.codex/artifacts/reviews/dal-documentation-inventory.md` — binds the
  exhaustive inventory and repository-wide tracked counts to the final tree.
- `.codex/artifacts/reviews/dal-documentation-review.md` — records this audit,
  the checker selection, DAL-55 discrepancy, final counts, and verification.
- `.codex/skills/dal-web/references/operations.md` and
  `.claude/skills/dal-web-setup/SKILL.md` — add the authoritative Bash and
  PowerShell staged-prefix installation commands while preserving their
  platform-specific packaging.

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

- `python3 .github/scripts/check_docs.py` — passed for the 40 Markdown files
  dynamically selected from root/component guides, `docs/`, and the four
  agent-facing guides.
- `python3 -m unittest discover -s .github/scripts/tests -p 'test_check_docs.py' -v`
  — passed all 18 tests.
- Repository-wide tracked counts — 90 Markdown, 12 TOML, and nine YAML/YML
  files. The audit's narrower contract/interface inventory remains 90 Markdown,
  10 Codex agent TOML, and two Codex skill-interface YAML files (102 total).
- In-scope inventory reconciliation — all 102 command-selected paths are
  classified once; the area totals remain 5 root, 6 component, 28 docs,
  2 GitHub, 30 Claude, and 31 Codex files.
- Multica DAL roster comparison — 10 squad agents mapped one-to-one to the 10
  TOML contracts; zero `description` or `instructions` differences.
- `git diff --check` — passed.

No C++ build or test suite was run: this patch changes changelog, documentation,
setup-instruction, and audit content only, and the doc-writer role does not edit
or validate C++ behavior.

## Remaining decisions and risks

1. `.github/scripts/check_docs.py` dynamically selects 40 of the final 90
   Markdown files; the full-inventory structural audit remains an explicit
   maintenance step rather than a CI gate.
2. Retention or relocation of implemented Claude records and
   `docs/superpowers/` history remains a repository-governance decision.
3. `.codex/artifacts/designs/api-shape-dedup.md` still awaits approval and must
   continue to be treated as a proposal, not shipped behavior.
