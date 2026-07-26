# DAL Documentation Review

Review date: 2026-07-26

## Scope and baseline

The review started from freshly fetched `origin/master` at
`54768fdd0c1a551d997475ea9d40e069b146a097` and covered every tracked Markdown
file, every `.codex/agents/*.toml` contract, both
`.codex/skills/**/agents/openai.yaml` interfaces, and the corresponding Claude
agent and skill descriptions. The regenerated starting inventory contained 102
files: 90 Markdown documents, 10 TOML contracts, and two YAML interfaces. The
completed activity plan removed by this review leaves the final inventory at
101 files: 89 Markdown documents, 10 TOML contracts, and two YAML interfaces.
The exhaustive file-by-file classification is in
[DAL documentation inventory](dal-documentation-inventory.md).

Ground truth was established from current public headers, implementations,
bindings, examples, tests, build manifests, dependency locks, CI workflows,
repository-level guidance, and active artifacts. Retained specifications,
designs, critiques, and plans were treated as historical or proposed evidence,
not as shipped behavior. The audit combined semantic source comparison with
full-inventory link, anchor, path, table, whitespace, final-newline,
TOML/YAML/frontmatter, role-set, methodology-index, and Multica roster checks.

## Findings and dispositions

### Medium — a completed plan remained in the active-artifact tree

- **File:** `.codex/artifacts/plans/p1-staged-xccy-sensitivity.md`
- **Evidence:** the plan targets `feature/dal-6-staged-xccy-sensitivity`; PR
  #257 is merged into `master`, and the resulting core, public C++, Python,
  Excel, methodology, public-API, and changelog surfaces are present in the
  current tree.
- **Disposition:** removed the plan. `.codex/README.md` reserves
  `.codex/artifacts/` for work products that still control active work and
  requires completed artifacts to be removed once current documentation and
  Git history preserve the result.

### Medium — Claude feature routing bypassed critique and named an absent role

- **Files:** `.claude/agents/dal-spec-writer.md` and
  `.claude/agents/dal-api-designer.md`
- **Evidence:** the spec contract allowed non-public features to proceed
  directly to `dal-implementer`, and the API contract sent a locked surface
  directly to implementation. Both contracts also assigned work to an
  `architect` role that does not exist in either platform's 10-role roster.
- **Disposition:** route every new feature through `dal-critic` before
  implementation. Public-API work reaches the critic after
  `dal-api-designer`; non-public work reaches it after `dal-spec-writer`.
  Replace `architect` responsibilities with the actual API-design or critique
  gate, depending on context.

### Medium — Claude contracts used stale public paths and namespace spelling

- **Files:** `.claude/agents/dal-spec-writer.md`,
  `.claude/agents/dal-api-designer.md`, `.claude/agents/dal-critic.md`,
  `.claude/agents/dal-implementer.md`, and
  `.claude/agents/dal-doc-writer.md`
- **Evidence:** the contracts referred to a nonexistent `public/` tree, and
  the API designer named a lowercase `dal::` namespace. The current surfaces
  are `dal-public/src/`, `dal-python/src/bindings/`, and `dal-excel/src/`;
  public C++ declarations use `Dal::`.
- **Disposition:** replace the stale path and namespace references with the
  current repository surfaces and `Dal::` spelling.

### No documentation change — exact Krylov residual handling

The latest `master` change makes `CGSolve` and `BCGSolve` return immediately
when the supplied initial guess has an exact component-wise zero residual while
still iterating on representable nonzero residuals whose squared norm
underflows. `docs/methodology/matrix.md` already states the public convergence
criterion and solver roles without claiming a contradictory first-iteration
behavior. This edge-case fix does not add a public API, numerical method, or
significant methodology shift, so neither published prose nor `CHANGELOG.md`
needs another entry.

### No content drift — staged XCCY sensitivity documentation

The staged XCCY additions in `CHANGELOG.md`, the Excel and Python READMEs,
`docs/public-api.md`, and the XCCY/Jacobian methodology pages agree with the
current core and public structs, overloads, binding aliases, Excel selectors,
availability truth table, axis ordering, and solver-scaled effective-inverse
tests. The one-argument defaults remain `ANALYTIC`, forward Jacobian requested,
and effective inverse requested. No corrective edit was required.

## Agent contract and Multica roster reconciliation

The DAL squad contains 10 agent members, and its name set exactly matches the
10 `.codex/agents/*.toml` names. For every member:

- the Multica `description` exactly matches the TOML `description` and is
  between 56 and 77 Unicode characters, below the 255-character limit;
- the Multica `instructions` exactly match `developer_instructions` after the
  TOML block string's single formatting newline is stripped;
- no `description` or `instructions` update was necessary;
- runtime, model, thinking level, service tier, skills, environment, MCP,
  visibility, concurrency, avatar, squad membership, role, and leader fields
  were not changed.

## Intentional Claude/Codex differences

- Both platforms define the same 10 DAL role names and cover specification,
  API design, critique, implementation, testing, review, documentation,
  performance, simplification, and orchestration.
- Claude roles remain verbose Markdown contracts with YAML frontmatter,
  Claude-specific tool declarations, invocation language, and `.claude/`
  artifact paths. Codex roles remain compact TOML contracts with explicit
  delegation authorization, focused references, and `.codex/artifacts/`
  outputs. After correcting the unsupported routing, role-name, path, and
  namespace drift above, these remaining differences are supported platform
  differences rather than missing parity.
- Both route new features through specification, optional API design,
  critique, implementation, testing, review, and documentation. Both keep
  performance and simplification as post-correctness sidecars and require
  reviewer coverage for code changes.
- Claude exposes five user-invocable Markdown skills. Codex exposes two
  reusable skills with `openai.yaml` metadata and seven shared references.
  This packaging difference is intentional.
- `.claude/rules/git-commit-pr.md` and
  `.codex/references/git-commit-pr.md` remain byte-identical, as do the two
  unit-test-style guides. Other pairs retain platform-specific artifact roots
  and invocation behavior.

## Artifact and status decisions

- `.claude/designs/api-shape-dedup.md` and
  `.codex/artifacts/designs/api-shape-dedup.md` remain byte-identical active
  designs awaiting approval; they are not described as shipped behavior.
- Joint-AAD, PDE, web-persistence, simultaneous-multi-curve,
  compiled-evaluator, and XCCY records remain classified as implemented
  history. Their historical commands and proposed paths are not current
  operational guidance.
- The two `docs/experimental/` files remain explicitly non-normative. The four
  `docs/superpowers/` records remain historical design and implementation
  material outside the published documentation index.
- No `CHANGELOG.md` entry was added because this review changes only
  documentation lifecycle state and audit records.

## Remaining decisions and risks

1. `.github/scripts/check_docs.py` still validates a curated 39-file set rather
   than every tracked Markdown document. Expanding CI coverage requires a
   separate automation change outside this documentation-only patch.
2. Retention or relocation of implemented Claude records and
   `docs/superpowers/` history remains a repository-governance decision.
3. The API-shape deduplication design still awaits approval; documentation must
   continue to describe current duplicated validation behavior until it is
   implemented.

## Validation record

- `python3 .github/scripts/check_docs.py` — passed for the repository checker's
  curated 39 Markdown files.
- `python3 -m unittest discover -s .github/scripts/tests -p 'test_check_docs.py' -v`
  — 18 tests passed.
- Targeted Claude contract audit — both specification paths and the API-note
  path reach `dal-critic` before implementation; zero absent `architect`
  role references, standalone `public/` paths, or lowercase `dal::` namespace
  references remain in the five corrected contracts.
- Temporary MarkdownIt/PyYAML/tomllib full-scope audit — 89 Markdown documents,
  365 parsed links or images, 85 exactly aligned tables, 17 valid Markdown
  frontmatter blocks, 10 valid TOML contracts, two valid YAML interfaces, 101
  exact inventory entries, 10 roles per platform, and 16 methodology pages;
  zero link, anchor, table, whitespace, final-newline, parse, set, or index
  errors.
- `multica agent list --output json` plus TOML field comparison — all 10 DAL
  descriptions and normalized instruction contracts matched; zero field
  updates.
- `git diff --check` — passed.
