# Claude-to-Codex Migration Review

Date: 2026-07-24

Scope: `e833111229b4ac13bfac6871a03979de40725e08...HEAD` plus the uncommitted Task 6
verification evidence and reviewer-driven fixes.

## Findings

- **Medium - `.codex/skills/dal-git-pr/references/publish-workflow.md:178-182`** -
  The exact-head check-run query omitted pagination. GitHub returns only 30 runs by default;
  recent DAL pull requests had 57-58 check runs, so the workflow could miss required runs while
  claiming complete exact-head verification. The controlling migration plan uses
  `?per_page=100`. Resolution is applied and independently confirmed.
- **Low - `.codex/skills/dal-agent-team/SKILL.md:14-25`,
  `.codex/skills/dal-agent-team/references/team-map.md:5-16`, and
  `.codex/skills/dal-web/references/operations.md:17-20`** - These guidance tables had extra
  cell padding and overlong separator rows, violating the exact-width Markdown rule introduced
  at `.codex/skills/dal-agent-team/references/code-style.md:63-70`. Exact-width repadding is
  applied and independently confirmed.

## Open Questions

None.

## Review Evidence

The independent reviewer reported:

- exactly ten `.codex/agents/*.toml` files, each with exactly `name`, `description`, and
  `developer_instructions`, a same-name skill mapping, and the shared-rules reference;
- all thirteen `dal-*` skill directories valid;
- all thirteen DAL `openai.yaml` metadata files parse successfully;
- 52 direct relative links across 32 changed Markdown files resolve;
- exactly nine tracked Claude durable sources have nine mapped Codex destinations, with
  substantive content preserved through the intentional Codex-path and PDE provenance
  reconciliation;
- no operational `.claude/rules/` or `.claude/skills/` dependency in Codex skills;
- no `.claude/**` or `CLAUDE.md` diff from merge base;
- no whitespace errors across the merge-base diff;
- branch scope limited to `AGENTS.md` and `.codex/**`; and
- GREEN scenario behavior supported by the migrated references and the baseline evidence.

The current Codex manual was checked independently. It confirms project-scoped
`.codex/agents/*.toml` discovery, the required three-field schema, inherited settings when
model and reasoning effort are omitted, and multi-agent support enabled by default. The
reviewer found no overstated custom-agent claim in the migration.

## Resolution

The findings are fixed in the uncommitted working tree:

1. The captured-head query is now
   `gh api "repos/$repo_slug/commits/$head_sha/check-runs?per_page=100"`.
2. The three cited tables preserve their content while using the exact longest-cell-plus-two
   column widths and matching separator dash counts.

Focused assertions confirmed the sole live publish check-run query uses `?per_page=100` and all
three reviewed tables use exact widths and separator counts. The affected static audit also
confirmed:

- all thirteen DAL skills validate and all thirteen `openai.yaml` files parse;
- all ten strict three-field agent TOMLs map to same-name skills;
- all nine durable artifact mappings exist;
- 52 direct local links across 33 changed Markdown files resolve;
- no operational Claude rule/skill dependency remains;
- `.claude/**` and `CLAUDE.md` are unchanged from the merge base;
- all 44 changed paths are limited to `AGENTS.md` and `.codex/**`; and
- `git diff --check e833111229b4ac13bfac6871a03979de40725e08` passes.

## Re-review Confirmation

On 2026-07-24, before the Task 6 commit was created, the same independent reviewer re-read the
uncommitted fixes and durable report. It reported no findings and confirmed:

- the sole live exact-head endpoint at
  `.codex/skills/dal-git-pr/references/publish-workflow.md:181` uses `?per_page=100`;
- exact-width assertions pass for the three corrected guidance tables;
- the three affected skills validate;
- merge-base `git diff --check` passes;
- `.claude/**` and `CLAUDE.md` remain unchanged;
- no changed path falls outside `AGENTS.md` and `.codex/**`; and
- this durable report accurately records both original findings and their fixes.

The reviewer verdict was **APPROVED**.

## Residual Test Risk

No C++ build is required for this guidance-only branch. The full-suite pressure scenario reached
CMake configuration but could not compile or run tests because Git submodules, including
GoogleTest, are uninitialized. The Windows PowerShell launcher was not executed on the Linux
host; its platform, health, log, PID/process-tree, graceful-stop, force-stop, and port-owner
behavior was checked from the Codex web workflow and repository scripts.

## Summary

The migration's structure, mappings, self-containment, artifact reconciliation, exclusions,
routing, metadata, direct links, current Codex custom-agent claims, and both reviewer-driven
fixes passed independent review.

## Verdict

**APPROVED**

## Final Whole-Branch Review Follow-Up

### Important Findings

The final whole-branch review found four additional Important issues after the approved Task 6
review lifecycle above:

1. **Important - `.codex/skills/dal-performancer/SKILL.md`** - The Codex performance workflow
   retained only a shallow six-step summary. It omitted the preserved role's project benchmark
   context, isolated baseline/head builds, exact benchmark enablement, calibrated paired gate,
   current CI reproduction, raw evidence contract, and read-only boundary.
2. **Important - `.codex/skills/dal-git-pr/references/publish-workflow.md`** - The review-thread
   GraphQL query stopped at 100 threads and the final merge block did not repeat the full thread
   and exact-head check audit. A PR with more than 100 threads or a late review/check change
   could therefore pass incomplete merge guidance.
3. **Important - `.codex/skills/dal-git-pr/references/publish-workflow.md`** - The first
   pagination fix used `gh api --slurp`, but the installed GitHub CLI 2.46.0 does not provide
   that flag. The documented audit helpers therefore failed before querying any review thread or
   check run.
4. **Important - `.codex/skills/dal-git-pr/references/publish-workflow.md`** - Initial and final
   audit/PR-state captures and their downstream parsers were not fail-closed. A failed command
   could leave an empty or truncated file and allow the documented sequence to continue toward
   merge.

### Uncommitted Fixes

The current uncommitted working tree:

- replaces the shallow performance steps with a concise entry point plus
  `dal-performancer/references/benchmark-workflow.md`, a self-contained workflow with an early
  contents index, inline eight-benchmark module map, merge-base selection, isolated detached
  head/baseline worktrees and build directories, explicit Release configure commands, build-tree
  executable paths, paired/interleaved best-of-N measurement, current 2x10/4% CI reproduction,
  environment/raw-sample/verdict reporting, coverage advice, and no-edit/no-merge boundaries;
- removes every operational dependency on absent Claude performance artifacts while pointing
  only to current repository files;
- defines a cursor-paginated `gh api graphql --paginate` thread audit with `$endCursor`,
  `reviewThreads(first:100, after:$endCursor)`, and
  `pageInfo { hasNextPage endCursor }`; and
- runs both the paginated thread audit and paginated
  `commits/<head_sha>/check-runs?per_page=100` audit initially and immediately before merge,
  revalidating the same head and PR review/merge/check status between gates and requiring a full
  restart on head change before `--match-head-commit`; and
- uses only GitHub CLI 2.46.0-compatible `--paginate` and `--jq` flags, emits review threads and
  check runs as one JSON object per line, consumes those JSONL files without assuming an
  enclosing array, and treats empty JSONL inputs as valid input requiring policy adjudication
  rather than a parse failure; and
- guards every initial/final thread, check, and PR-state capture, every downstream `jq` parse or
  filter, and the final exact-head assertion with `|| exit 1`, so command failure, truncated
  output, malformed JSON, or parse failure cannot reach guarded merge.

### Focused Validation

Focused assertions confirmed:

- the gate allowlist matches the eight names in
  `.github/scripts/check_benchmark_regressions.py`;
- both isolated configure commands contain `--preset=Release-linux`, explicit `-S` and `-B`,
  and `-DDAL_CPP_BUILD_BENCHMARKS=ON`;
- only build-tree benchmark paths are executable guidance;
- merge-base, paired/interleaved N >= 10, best-of-N minimum, two rounds of ten, and the strict
  4% threshold are present and match the current script and Linux workflow;
- the initial and final review-thread calls are cursor-paginated, the initial and final
  exact-head check calls use REST pagination with `per_page=100`, and the final block repeats
  head, `reviewDecision`, mergeability/status, and rollup checks before guarded merge;
- installed `gh 2.46.0` help contains `--paginate` and `--jq` but not `--slurp`;
- the exact read-only helper query shapes ran successfully against DAL PR #250 at head
  `5556dc9eec55b50c4481e540fd2de45e24141812`, yielding four valid review-thread JSONL objects
  and 57 valid check-run JSONL objects with exit status zero;
- the documented per-object `jq` filters accepted those JSONL files, accepted empty JSONL inputs
  without false failure, and an extracted representative helper script passed `bash -n`;
- a representative extracted gate passed `bash -n`; injected thread-audit, check-audit,
  PR-state-wrapper, and malformed-JSON failures each exited nonzero without reaching a merge
  sentinel, while successful empty JSONL and live PR-shaped JSONL both completed;
- all thirteen DAL skills validate and all thirteen `openai.yaml` files parse;
- 43 direct local links across the DAL skill trees resolve;
- the changed guidance introduces no Markdown pipe table requiring new width adjudication;
- all tracked `.claude/**` files and `CLAUDE.md` match their pre-edit hashes;
- working-tree scope is limited to the two affected skills, the new performance reference, and
  this durable review; and
- both working-tree and merge-base `git diff --check` pass.

No C++ build was run because these are guidance-only changes.

### Verdict

**APPROVED**

The final reviewer re-read the complete four-path follow-up diff and reported no findings.
GitHub CLI 2.46.0 compatibility is confirmed: the guidance uses supported `--paginate` and
`--jq` flags, contains no `--slurp`, and the exact read-only query shapes returned four review
threads and 57 check runs from PR #250 as valid JSONL. The fail-closed re-review also confirmed
that every initial and final capture, parser, filter, and exact-head assertion is guarded;
injected audit, wrapper, and malformed-JSON failures all stopped before the merge sentinel,
while legitimate empty and live-shaped JSONL completed normally.

**Ready to merge: Yes**
