# Claude-to-Codex Migration Baseline

Date: 2026-07-24

Commit: cad096d1

| Scenario | Current behavior | Required behavior | Verdict |
|---|---|---|---|
| Native role dispatch | Calls `.claude/agents/dal-spec-writer.md` the native agent definition, then maps to `.codex/skills/dal-spec-writer/SKILL.md`. | Native project dispatch must name `.codex/agents/dal-spec-writer.toml` and the role skill it loads. | RED |
| Full-suite execution | Gives a Linux `build_linux.sh` log/exit-status workflow, but the run-only trigger and complete native fresh-output/Windows workflow are not supplied by the current Codex guidance. | Full-suite requests must trigger a complete fresh-output build/test workflow without adding tests. | RED |
| AAD test authoring | Gives the detailed lifecycle only after following current Codex guidance into `.claude/rules/unit-test-style.md`. | AAD test conventions must be available from Codex guidance without an operational Claude dependency. | RED |
| Web operations | Reconstructs Windows start, health/proxy, logs, graceful/force stop, and listener-kill behavior from repository docs and scripts rather than the Codex web skill. | Codex web operations guidance must supply the platform dispatch, health, log, PID, and force-stop workflow. | RED |
| Style review | Lists C++17/formatting/naming/test rules plus basic Markdown-table, link, whitespace, and documentation-current-state checks. | Scenario 5 omits at least one detailed Markdown or C++ style rule. | RED |
| PR completion | Describes a temporary writable clone, a focused PR, current-head required checks, no unresolved feedback, and a head-SHA-guarded merge. | Scenario 6 omits the writable-clone, review-thread, or exact-head merge gate. | RED |

## Fresh-context evidence

### Native role dispatch

Prompt:

> Use the DAL agent team to delegate a fuzzy feature request to dal-spec-writer. Identify the native project custom-agent file you will target and the role skill it must load.

Observed response:

> I’ll delegate to the DAL spec-writer role for a testable requirements specification.
>
> - Native custom-agent target: `.claude/agents/dal-spec-writer.md`
> - Required role skill: `.codex/skills/dal-spec-writer/SKILL.md`
>
> The delegated prompt will include the fuzzy request, require targeted clarification only where needed, prohibit implementation, and write any durable spec to `.codex/artifacts/specs/<feature-slug>.md`.

Files read: `.codex/skills/dal-agent-team/SKILL.md`, `.codex/skills/dal-agent-team/references/team-map.md`, `.codex/skills/dal-spec-writer/SKILL.md`, `.claude/agents/dal-spec-writer.md`, `.codex/skills/dal-spec-writer/agents/openai.yaml`.

### Full-suite execution

Prompt:

> The user says only "run the full DAL test suite and report whether it passes". Give the exact fresh-output workflow without adding tests.

Observed response:

> `bash ./build_linux.sh > test_output.txt 2>&1`
>
> Report the build/CTest result from the exit code and the tail of `test_output.txt`; do not add or modify tests.

Files read: `.codex/skills/dal-tester/SKILL.md`, `.codex/skills/dal-agent-team/references/shared-rules.md`, `AGENTS.md`, `CLAUDE.md`, `build_linux.sh`.

### AAD test authoring

Prompt:

> Add an AAD Google Test for a scalar gradient. State the required tape lifecycle and DAL test conventions.

Observed response:

> `Clear`, `RegisterIndependent`, `NewRecording`, compute, seed `Adjoint`, `PropagateToStart`, assert `Value`/`AdjointValue`, `Clear`.
>
> The response also supplied the DAL Google Test conventions.

Files read: `.codex/skills/dal-tester/SKILL.md`, `.codex/skills/dal-agent-team/references/shared-rules.md`, `.claude/rules/unit-test-style.md`.

### Web operations

Prompt:

> Start the DAL web UI on Windows, verify it is healthy, and explain how to stop it cleanly if a child process keeps a port open.

Observed response:

> `pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/start.ps1`
>
> Verify the UI and proxy/backend health; inspect logs; use `stop.ps1` first, then `stop.ps1 -Force`, with a listener-kill fallback if a child still owns the port.

The response did not read `.codex/skills/dal-web/SKILL.md`; it reconstructed this from `README.md`, `CLAUDE.md`, web documentation, and the PowerShell scripts.

### Style review

Prompt:

> Style-review changed C++ tests and Markdown guidance. List the DAL-specific code, test, table, and documentation checks.

Observed response:

> - **C++ code:** apply C++17 and `.clang-format` (4 spaces, attached braces, 150-column limit, `T*` binding); verify DAL naming, lowercase filenames and `test_` test filenames, correct header/`namespace Dal` conventions, ownership types (`Handle_` / `unique_ptr`), and `REQUIRE`/`THROW` error handling. Flag prohibited `volatile`, `mutable` members, raw normal-error exceptions, or handwritten DAL enums. If enum markup changed, require regenerated core and Excel `MG_*` files.
>
> - **C++ tests:** keep core tests under `dal-cpp/tests/<module>/test_<name>.cpp`; include `<gtest/gtest.h>` first; use `TEST`, not `TEST_F`; use `ASSERT_*`, not `EXPECT_*`; start test names with `Test`; use suitable deterministic numeric tolerances; use `ASSERT_THROW(..., Dal::Exception_)`; keep setup local to each test; and clear AAD tape state at both start and end of AAD tests.
>
> - **Markdown tables:** inspect every changed table for valid, consistent columns and rendering; check links, trailing whitespace, and a final newline.
>
> - **Documentation:** ensure guidance describes the current library only, with historical context moved to `CHANGELOG.md`; do not cite source line numbers; reuse published examples; update `docs/README.md` and `CLAUDE.md` methodology lists if docs were added, removed, or renamed. Add a changelog entry only for a breaking public API change, new/shifted methodology or numerical algorithm, significant capability, or public-surface removal/deprecation.
>
> - **Review scope:** read full changed files and check correctness, style, tests, documentation/changelog, generated files, security, and compatibility. Report findings first, ordered by severity with file and line references.

Files read: `.codex/skills/dal-reviewer/SKILL.md`, `.codex/skills/dal-doc-writer/SKILL.md`, `.codex/skills/dal-agent-team/references/shared-rules.md`.

### PR completion

Prompt:

> Publish a guidance-only change from a checkout whose .git directory is read-only, then merge only after exact-head CI and review verification.

Observed response:

> I’ll preserve the read-only checkout and publish from a temporary writable clone: verify the guidance-only diff, stage only those files, commit it as a focused docs/chore change, push a feature branch, and open the PR with its targeted validation recorded.
>
> I’ll merge only after the PR’s current head SHA has passing required checks and no unresolved review feedback. Immediately before merging, I’ll re-check that exact SHA and use a head-SHA-guarded merge, so a later push cannot be merged accidentally.

Files read: `.codex/skills/dal-git-pr/SKILL.md`.
