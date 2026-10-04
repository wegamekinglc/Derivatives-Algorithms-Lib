# DAL-317 Documentation and Agent Contract Audit

Active handoff for doc-writer → reviewer. Keep this artifact through independent
acceptance; retire it after the current-state corrections are accepted.

## Baseline and Scope

- Audit date: 2026-10-04. Remote freshness rechecked at 04:19 UTC.
- `git ls-remote origin refs/heads/master` and `git rev-parse origin/master`
  both resolve to `b5e3caca85bb1a5b1aa3acf7898b387832d443c8`.
- Initial `HEAD` is that same commit and `git status --short` is empty.
- `multica repo checkout` created this worker's isolated branch
  `agent/dal-doc-writer/d0d480912ade` at the handed-off baseline. The orchestrator's
  prepared branch has no changes to carry over.
- `git ls-files '*.md'` regenerated the complete 95-file input inventory.
  This artifact adds one file, bringing the delivery inventory to 96.
- Repository scope is Markdown only. No C++, generated files, TOML, YAML,
  methodology additions/removals/renames, or changelog entries are changed.
  Existing methodology indexes in `docs/README.md` and `CLAUDE.md` remain valid.

| Directory    | Input Markdown files | Delivery Markdown files |
|--------------|----------------------|-------------------------|
| Root         | 5                    | 5                       |
| `.claude`    | 28                   | 28                      |
| `.codex`     | 12                   | 13                      |
| `.github`    | 2                    | 2                       |
| `dal-cpp`    | 1                    | 1                       |
| `dal-excel`  | 3                    | 3                       |
| `dal-public` | 1                    | 1                       |
| `dal-python` | 2                    | 2                       |
| `docs`       | 41                   | 41                      |
| Total        | 95                   | 96                      |

## Corrections and Source Evidence

| Modified file                           | Reason                                                                                                                                                              |
|-----------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `.claude/rules/code-style.md`           | Restore missing example-output, comment-direction, and readability rules from Codex; remove conflicting source-to-doc pointer guidance.                             |
| `.codex/references/code-style.md`       | Restore the shared mirror banner after synchronizing the Claude copy.                                                                                               |
| `.github/copilot-instructions.md`       | Distinguish documentation-only PR gates from the compiler/backend matrix, matching the CI classifier and workflows.                                                 |
| `dal-public/README.md`                  | Correct the product archive writer to v3 and retain v1/v2 reader compatibility.                                                                                     |
| `dal-python/README.md`                  | Document `regression_features` constructor/property and copies, supported rate exercise, historical Libor, and valid EXERCISE-only products; align affected tables. |
| `docs/architecture.md`                  | Correct archive version and name-based equity/rate model observation binding; align the module table.                                                               |
| `docs/excel/script-settings.md`         | Correct archive version, include IR regression features and historical Libor, explain rate-only feature requirements, and remove duplicated SPOT text.              |
| `docs/methodology/index_parsing.md`     | Describe both equity and rate model outputs, consistent with its existing historical Libor support statement.                                                       |
| `docs/methodology/monte-carlo/lsm.md`   | Document model-supported IR regression coordinates and the explicit feature requirement for rate-only GSR exercise.                                                 |
| `docs/methodology/script_engine.md`     | Correct history adapters, archive v3, named rate outputs, C++ field spelling, and missing product field/simulation diagnostic call; align affected tables.          |
| `docs/public-api.md`                    | Correct archive v3, multi-equity/rate observation support, historical Libor, and C++ AAD field spelling.                                                            |
| `docs/python/README.md`                 | Include supported IR features and explicit rate-only GSR exercise selection.                                                                                        |
| `.codex/artifacts/dal-317-doc-audit.md` | Preserve the full input inventory, findings, configuration evidence, verification, and reviewer handoff.                                                            |

The non-trivial findings are grounded in these current implementations:

- `ScriptProductData_::Write` and the v1/v2/v3 readers in
  `dal-cpp/dal/script/event.cpp`; `ScriptArchiveTest.TestIndexVersioning` in
  `dal-public/tests/test_script_archive.cpp` verifies v3 output and feature
  roundtrips. `ScriptContractTest.TestArchiveStaysContractOnlyAfterExplainAndAadRepricing`
  in `dal-public/tests/test_script_contract.cpp` verifies contract-only persistence.
- `ValidateIndex`, `ResolveHistory`, `ResolveRegressionFeature`, and `ModelPlan`
  in `dal-cpp/dal/script/preparation.cpp` admit historical EQ/FX/Libor, support
  model-compatible EQ/DF/Libor/Swap regression features, and require explicit
  features for rate-only exercise. `dal-cpp/tests/model/test_gsr.cpp` exercises
  IR parsing, rate observations, and Bermudan bond/swaption exercise.
- `dal-cpp/dal/script/settings.hpp` owns `regressionFeatures_` and `enableAad_`;
  `dal-public/src/value.hpp` owns the diagnostic call defaults.
  `dal-python/src/bindings/script.cpp` accepts a list/tuple or None and returns a
  detached feature list. `dal-excel/src/__scriptsettings.cpp` projects the
  semicolon-separated setting through the same native product settings.
- `dal-cpp/dal/script/preparation.cpp` and `dal-cpp/tests/script/test_exercise_lsmc.cpp`
  accept `EXERCISE` as payoff syntax independently of `PAYS`.
- `.github/scripts/classify_ci_changes.py`, `.github/workflows/cmake-linux.yml`,
  and `.github/workflows/cmake-windows.yml` own documentation-only CI gating.
- `.codex/references/code-style.md`, the existing example programs, and
  `AGENTS.md` ground the shared guidance synchronization. All three rule pairs
  (`code-style`, `git-commit-pr`, `unit-test-style`) now compare byte-identical.

These are corrections to descriptions of shipped behavior, not new public
capabilities or methodology changes; no CHANGELOG entry is warranted.

## Complete File Inventory

Every row was read as a complete UTF-8 file, including code blocks. Checks cover
local links/anchors, table shape, trailing whitespace/final newline, repository
path claims and includes, qualified API names, named inline C++ symbols, and
Python snippet syntax. Runtime/generated paths, illustrative placeholders, and
explicitly historical evidence are classified rather than treated as current
source claims. Domain evidence below supplements these static checks; this
audit does not claim to compile every snippet or revalidate every numerical
result.

Evidence groups: A = agent contracts/routing and shared rules; B = build, CMake,
CI, and package metadata; C = curve calibration/pricing/risk headers, bindings,
examples and tests; S = script/settings/model observation implementations;
N = the named numerical/time/model module and its examples/tests; P = benchmark
inventory/runners and scheduled gates; H = explicitly implemented history.

L/P/Y/I = Markdown links / path claims / Python blocks / named inline symbols.

| File                                                          | Lines | SHA-256 prefix | Evidence | L/P/Y/I counts | Result               |
|---------------------------------------------------------------|-------|----------------|----------|----------------|----------------------|
| `.claude/agents/README.md`                                    | 75    | `393f8f48877c` | A        | 5/8/0/0        | Checked              |
| `.claude/agents/dal-api-designer.md`                          | 15    | `9ddd6c7bc0f9` | A        | 0/3/0/0        | Checked              |
| `.claude/agents/dal-critic.md`                                | 15    | `ef5abfc77268` | A        | 0/1/0/0        | Checked              |
| `.claude/agents/dal-doc-writer.md`                            | 15    | `755fdebfc191` | A        | 0/4/0/0        | Checked              |
| `.claude/agents/dal-implementer.md`                           | 15    | `3b9b63b74695` | A        | 0/3/0/0        | Checked              |
| `.claude/agents/dal-orchestrator.md`                          | 21    | `bc34a9ec4116` | A        | 0/2/0/0        | Checked              |
| `.claude/agents/dal-performancer.md`                          | 15    | `c143d513bae6` | A        | 0/2/0/0        | Checked              |
| `.claude/agents/dal-reviewer.md`                              | 15    | `8e35ac3202d4` | A        | 0/2/0/0        | Checked              |
| `.claude/agents/dal-simplifier.md`                            | 15    | `6ad7790f4d90` | A        | 0/2/0/0        | Checked              |
| `.claude/agents/dal-spec-writer.md`                           | 15    | `244d8906116f` | A        | 0/1/0/0        | Checked              |
| `.claude/agents/dal-tester.md`                                | 17    | `ebd890fa6da8` | A        | 0/4/0/0        | Checked              |
| `.claude/api-notes/joint-aad-gradient.md`                     | 365   | `bdaaffc10f36` | H        | 0/19/0/0       | Historical; retained |
| `.claude/critiques/pde-framework-reimplementation.md`         | 218   | `d1a119cdda58` | H        | 0/18/0/0       | Historical; retained |
| `.claude/designs/api-shape-dedup.md`                          | 488   | `2162ead169f1` | H        | 0/56/0/0       | Historical; retained |
| `.claude/designs/joint-aad-gradient.md`                       | 1623  | `ca47b8d3b331` | H        | 0/83/0/0       | Historical; retained |
| `.claude/rules/code-style.md`                                 | 283   | `91ac28d3dbd3` | A        | 10/21/0/16     | Corrected            |
| `.claude/rules/git-commit-pr.md`                              | 55    | `683f313aec5e` | A        | 0/2/0/2        | Checked              |
| `.claude/rules/unit-test-style.md`                            | 53    | `b470e37dc34c` | A        | 0/5/0/7        | Checked              |
| `.claude/skills/dal-code-style-review/SKILL.md`               | 14    | `575bd0323e36` | A        | 1/2/0/0        | Checked              |
| `.claude/skills/dal-commit-and-pr/SKILL.md`                   | 17    | `e9e0b0845ae7` | A        | 3/0/0/0        | Checked              |
| `.claude/skills/dal-unit-test-skill/SKILL.md`                 | 14    | `63ef3db3ce22` | A        | 1/2/0/0        | Checked              |
| `.claude/skills/dal-unit-test-write/SKILL.md`                 | 17    | `c3720a6c7348` | A        | 1/2/0/0        | Checked              |
| `.claude/specs/2026-08-14-dal-web-extraction-design.md`       | 137   | `6aa0ceba0f4a` | H        | 1/24/0/0       | Historical; retained |
| `.claude/specs/2026-08-14-dal-web-extraction-plan.md`         | 1038  | `f889b48518d5` | H        | 1/89/3/0       | Historical; retained |
| `.claude/specs/joint-aad-gradient.md`                         | 873   | `b0113c6a1371` | H        | 0/24/0/0       | Historical; retained |
| `.claude/specs/multi-curve-simultaneous-example.md`           | 830   | `96045456caa8` | H        | 0/35/0/0       | Historical; retained |
| `.claude/specs/pde-framework-reimplementation.md`             | 855   | `1b08a5122886` | H        | 0/46/0/0       | Historical; retained |
| `.claude/specs/script-compiled-evaluator-alignment.md`        | 200   | `43765e0e74e1` | H        | 0/69/0/0       | Historical; retained |
| `.codex/README.md`                                            | 52    | `90fc972b8100` | A        | 1/9/0/0        | Checked              |
| `.codex/artifacts/README.md`                                  | 7     | `c782918b25f1` | A        | 0/0/0/0        | Checked              |
| `.codex/references/benchmark-workflow.md`                     | 269   | `7ef7286f857a` | A        | 10/8/0/1       | Checked              |
| `.codex/references/code-style.md`                             | 283   | `91ac28d3dbd3` | A        | 10/21/0/16     | Corrected            |
| `.codex/references/git-commit-pr.md`                          | 55    | `683f313aec5e` | A        | 0/2/0/2        | Checked              |
| `.codex/references/run-tests.md`                              | 59    | `0e538d5ff094` | A        | 0/0/0/0        | Checked              |
| `.codex/references/style-review.md`                           | 86    | `9bf110c96f65` | A        | 5/5/0/0        | Checked              |
| `.codex/references/unit-test-style.md`                        | 53    | `b470e37dc34c` | A        | 0/5/0/7        | Checked              |
| `.codex/references/write-tests.md`                            | 206   | `780988a61814` | A        | 8/6/0/8        | Checked              |
| `.codex/skills/dal-agent-team/references/shared-rules.md`     | 10    | `04913d1c3482` | A        | 4/4/0/0        | Checked              |
| `.codex/skills/dal-git-pr/SKILL.md`                           | 71    | `d664ca730635` | A        | 2/0/0/0        | Checked              |
| `.codex/skills/dal-git-pr/references/publish-workflow.md`     | 282   | `11337d4e14df` | A        | 10/1/0/0       | Checked              |
| `.github/copilot-instructions.md`                             | 93    | `c9ec93181982` | B        | 8/2/0/0        | Corrected            |
| `.github/pull_request_template.md`                            | 16    | `69353a9445b6` | B        | 0/0/0/0        | Checked              |
| `AGENTS.md`                                                   | 83    | `7f5d7d560acf` | A        | 12/9/0/0       | Checked              |
| `CHANGELOG.md`                                                | 926   | `6b5d9bbd5f7f` | H        | 48/66/0/0      | Historical; retained |
| `CLAUDE.md`                                                   | 198   | `fb26d1870e78` | A        | 36/53/0/0      | Checked              |
| `CONTRIBUTING.md`                                             | 199   | `55ca603e4564` | B        | 3/10/0/0       | Checked              |
| `README.md`                                                   | 221   | `fe0153fa401d` | B/C/S    | 55/3/0/0       | Checked              |
| `dal-cpp/README.md`                                           | 75    | `30dd8e496154` | B/C/S    | 8/1/0/0        | Checked              |
| `dal-excel/README.md`                                         | 193   | `b4bb068b2a3f` | B/C/S    | 9/0/0/0        | Checked              |
| `dal-excel/examples/008.quote_risk.md`                        | 42    | `d3d4fe7e0e9f` | C        | 1/0/0/0        | Checked              |
| `dal-excel/examples/009.generic_joint_quote_risk.md`          | 63    | `ae2cb7842236` | C        | 2/0/0/0        | Checked              |
| `dal-public/README.md`                                        | 118   | `89404fd598b8` | S        | 8/16/0/0       | Corrected            |
| `dal-python/README.md`                                        | 1119  | `03965986565c` | S        | 31/9/12/18     | Corrected            |
| `dal-python/benchmarks/README.md`                             | 590   | `3f4fe3d8b1dd` | P        | 17/6/0/0       | Checked              |
| `docs/README.md`                                              | 44    | `c0f59ce9f4f6` | B/C/S    | 17/0/0/0       | Checked              |
| `docs/architecture.md`                                        | 248   | `f1376b189345` | B/C/S    | 6/9/0/1        | Corrected            |
| `docs/ccy-curves/README.md`                                   | 10    | `1859d9cb961a` | C        | 4/0/0/0        | Checked              |
| `docs/ccy-curves/pricing-calibration.md`                      | 422   | `3956ca1ff82b` | C        | 8/9/0/63       | Checked              |
| `docs/excel/README.md`                                        | 170   | `6d75b6cca41c` | B/C/S    | 7/0/0/0        | Checked              |
| `docs/excel/script-settings.md`                               | 391   | `f8b5a649623e` | S        | 13/5/0/0       | Corrected            |
| `docs/experimental/replicate-ptirds-single-currency-curve.md` | 413   | `524d9faea2bd` | C        | 7/63/0/32      | Checked              |
| `docs/installation.md`                                        | 443   | `c870cdef0c7f` | B        | 5/11/0/1       | Checked              |
| `docs/methodology/README.md`                                  | 33    | `c35c821d1261` | N        | 13/0/0/0       | Checked              |
| `docs/methodology/_cpp-example-style.md`                      | 219   | `d520ab37e1cb` | N        | 1/63/0/14      | Checked              |
| `docs/methodology/aad.md`                                     | 557   | `fa04fde05fca` | N        | 4/18/0/4       | Checked              |
| `docs/methodology/dates.md`                                   | 178   | `5b83997e16bb` | N        | 0/12/0/21      | Checked              |
| `docs/methodology/index_parsing.md`                           | 195   | `fcedab268a33` | S        | 7/18/0/24      | Corrected            |
| `docs/methodology/interpolation.md`                           | 233   | `322b2f1c5fc5` | N        | 7/17/0/26      | Checked              |
| `docs/methodology/matrix.md`                                  | 381   | `53e51644b37c` | N        | 3/12/0/17      | Checked              |
| `docs/methodology/monte-carlo/README.md`                      | 27    | `bfce6b73c762` | N        | 12/0/0/0       | Checked              |
| `docs/methodology/monte-carlo/lsm.md`                         | 408   | `b40da172f0f7` | S        | 10/7/0/9       | Corrected            |
| `docs/methodology/monte-carlo/sampling.md`                    | 409   | `397f59cbcec0` | N        | 5/14/0/22      | Checked              |
| `docs/methodology/monte-carlo/simulation.md`                  | 104   | `7cd48f4a4c34` | N        | 16/1/0/6       | Checked              |
| `docs/methodology/monte-carlo/special-techniques.md`          | 98    | `3ccb3fa828c5` | N        | 9/0/0/8        | Checked              |
| `docs/methodology/pde/README.md`                              | 7     | `805b4fe25aa6` | N        | 3/0/0/0        | Checked              |
| `docs/methodology/pde/framework.md`                           | 364   | `7bfa83af28e5` | N        | 4/11/0/17      | Checked              |
| `docs/methodology/pde/option-pricing.md`                      | 68    | `f000901774eb` | N        | 5/0/0/5        | Checked              |
| `docs/methodology/quadrature.md`                              | 257   | `d6071443ea90` | N        | 2/4/0/5        | Checked              |
| `docs/methodology/script_engine.md`                           | 1754  | `9056d364fcc0` | S        | 68/43/0/39     | Corrected            |
| `docs/methodology/underdetermined_search.md`                  | 459   | `2e4166d60de8` | N        | 7/5/0/13       | Checked              |
| `docs/models/README.md`                                       | 20    | `1062e28adea0` | N        | 7/0/0/0        | Checked              |
| `docs/models/black-scholes.md`                                | 310   | `71b0abf7922c` | N        | 8/13/0/24      | Checked              |
| `docs/models/correlated-bs.md`                                | 111   | `58616e5d534a` | N        | 3/0/0/11       | Checked              |
| `docs/models/dupire.md`                                       | 201   | `7f0558286536` | N        | 8/7/0/10       | Checked              |
| `docs/models/gaussian-short-rate.md`                          | 531   | `307a990a1f6c` | N        | 13/7/5/14      | Checked              |
| `docs/models/hybrid-model.md`                                 | 180   | `4b8e6e7d9f09` | N        | 6/1/0/13       | Checked              |
| `docs/models/local-volatility.md`                             | 101   | `fa9c5cd0b9ee` | N        | 5/0/0/14       | Checked              |
| `docs/public-api.md`                                          | 588   | `4fb420ea2a85` | B/C/S    | 20/3/0/44      | Corrected            |
| `docs/python/README.md`                                       | 265   | `80d3bdec02f2` | S        | 16/0/6/3       | Corrected            |
| `docs/yield-curves/README.md`                                 | 20    | `c2b3ebae35e8` | C        | 9/0/0/0        | Checked              |
| `docs/yield-curves/construction.md`                           | 846   | `9a28c04139bc` | C        | 19/43/0/83     | Checked              |
| `docs/yield-curves/jacobian-risk.md`                          | 673   | `090668ccd65a` | C        | 14/7/0/10      | Checked              |
| `docs/yield-curves/joint-quote-risk.md`                       | 213   | `053a46b44625` | C        | 2/0/0/14       | Checked              |
| `docs/yield-curves/log-discount.md`                           | 247   | `9fb398a28bcd` | C        | 7/7/0/23       | Checked              |
| `docs/yield-curves/node-risk.md`                              | 189   | `3dfabd9b1359` | C        | 6/7/0/13       | Checked              |

The new audit artifact is separately included in the final Markdown checks.
Hashes identify the exact reviewed file contents; they are not semantic proofs.

## Agent and Skill Registration Evidence

Authority: root `AGENTS.md` makes `.codex/agents/*.toml` the complete shared
role-contract source. Parsed all ten TOMLs, all ten Claude agent frontmatters
and bodies, all five tracked SKILL frontmatters, and
`.codex/skills/dal-git-pr/agents/openai.yaml`.

The Codex/Claude name sets match exactly. Every Claude description equals the
matching TOML description, and every Claude body equals the TOML
`developer_instructions` after removing its synchronization comment and outer
whitespace. Shared role boundaries, artifact paths, delegation and escalation
rules, handoff evidence, and publishing/merge authority therefore match.
The task's pure-documentation route matches `.claude/agents/README.md`.

Read the DAL squad roster with:

```bash
multica squad member list 2b409a72-57c6-447a-a252-a8cff3208dc0 --output json
multica agent get <roster-member-id> --output json
```

All ten members map by name to a TOML. Descriptions match exactly; instructions
match the complete TOML contract after outer-whitespace normalization. Repeated
reads found the entire returned agent records and squad roster unchanged.
**Multica fields synchronized: none; no drift and no update calls.** All
descriptions are below 255 Unicode code points. Runtime, model, thinking level,
service tier, skills, environment, MCP, visibility, concurrency, avatar, roster
membership, role and leader were untouched.

| Agent              | Multica ID                             | Description characters | Contract SHA-256 prefix | Updated fields |
|--------------------|----------------------------------------|------------------------|-------------------------|----------------|
| `dal-api-designer` | `62641d17-7114-4726-976c-64ee433065dd` | 64                     | `071409df4086`          | None           |
| `dal-critic`       | `5d24a66b-d46b-4e2b-ba87-a273fa37a20b` | 56                     | `99e906e52748`          | None           |
| `dal-doc-writer`   | `92053497-eaee-47b5-9dcf-8e6e302e1348` | 76                     | `4f080b26ff90`          | None           |
| `dal-implementer`  | `2c2adb0a-dba4-4095-844a-d47c9635a518` | 59                     | `28ab5f35f016`          | None           |
| `dal-orchestrator` | `36a667e5-f5b7-460c-83a2-d1575be5f12a` | 56                     | `a53bed64f277`          | None           |
| `dal-performancer` | `6feb9efa-bc37-46f8-bf13-6a2d3d1ce454` | 62                     | `9373bf589fdd`          | None           |
| `dal-reviewer`     | `a541f9c9-b79c-4b31-8ff2-a1178c57a395` | 70                     | `914c6bf02a88`          | None           |
| `dal-simplifier`   | `9fa8445f-7ff7-456d-945a-017c37747348` | 77                     | `e37e9741bdcc`          | None           |
| `dal-spec-writer`  | `0032710b-1be3-40eb-9118-815edd395b53` | 65                     | `5faf1805d293`          | None           |
| `dal-tester`       | `59e8e14e-1f1f-47d7-93a0-e2c2f8f53d0b` | 68                     | `277fb05ce3e4`          | None           |

Hash columns show the first 12 hexadecimal characters. Contract hashes are SHA-256 of the UTF-8 TOML instruction body with outer
whitespace removed. They provide a compact full-contract comparison.

Preserved platform differences:

- Claude registers Markdown/YAML agents with `model: inherit` and colors;
  Codex uses `name`, `description`, and `developer_instructions` TOML fields.
- Claude has four user-invocable skills. Codex has one `dal-git-pr` skill and
  role-specific references; `dal-agent-team` is a compatibility reference only.
- Multica retains its existing provider/runtime and catalog metadata. The shared
  behavior lives in `instructions`; `description` remains a catalog summary.
- Claude `EnterWorktree` is a host-specific isolation tool, with the same
  preservation and authorization obligations as the other platforms.

## Verification and Classified Exceptions

- `python3 .github/scripts/check_docs.py`: PASS (64 files before the artifact;
  65 with the delivery artifact). This repository check is intentionally
  supplemented because it omits several tracked Markdown surfaces.
- `python3 -m unittest discover -s .github/scripts/tests -p test_check_docs.py`:
  PASS, 32 tests.
- Full-inventory static check: PASS for all 95 input files, 697 Markdown links,
  1,182 referenced-path claims, 680 named inline symbol references, and 26 Python
  code blocks against 1,211 tracked source files. The delivery artifact is also
  checked after creation.
- Registration checks: PASS, ten TOMLs, ten Claude bodies/frontmatters, five
  SKILL frontmatters, one OpenAI registration YAML, ten Multica mappings, all
  summaries within the Unicode limit, and three identical shared-rule pairs.
- `git diff --check` and `git diff --cached --check`: PASS.
- C++ binaries, installed Python valuation examples, Windows XLL/Excel execution,
  and performance runs are outside this documentation-only validation.

The complete path scan classifies 87 absent historical-baseline references,
nine documented local-environment paths, and one submodule-provided include
directory. Historical references remain under existing implemented-history
banners or in CHANGELOG history. Local virtual environments and the RapidJSON
submodule are supplied by documented setup, not committed documentation targets.
No unresolved current source path or include was found.

The repository link regex, applied indiscriminately to all 95 files, treats
a `[=]` capture followed by `(double)` in an inline C++ lambda in the historical
PDE specification as a link to `double`. Inline code is not Markdown link syntax; excluding inline
code from link recognition removes that false positive. One Python snippet in
the historical web-extraction plan is an indented function-body excerpt; it is
retained as historical evidence. The remaining 25 Python blocks parse as Python.
Convention examples such as `EnumName_`, its generated headers, and
`<SuiteName>` are templates. `bD_` and `delta_` in the Jacobian derivation are
local explanatory names, not promises of exported API members.

## Reproduce the Full Markdown and Registration Checks

Run from the repository root with Python 3.11+ and PyYAML. These checks use only
local source and authenticated, read-only Multica CLI queries.

The replay checks current equality. Unchanged whole-record and roster evidence
above comes from comparing the initial and final reads during this audit.

```bash
python3 - <<'PY'
"""Full tracked-Markdown and DAL registration audit; read-only repository checks."""
import ast
import hashlib
import importlib.util
import json
import re
import subprocess
import sys
import tomllib
from pathlib import Path

import yaml

ROOT = Path.cwd()
sys.path.insert(0, str(ROOT / ".github/scripts"))
spec = importlib.util.spec_from_file_location("docs_checks", ROOT / ".github/scripts/check_docs.py")
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)

def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()

def cli(*args):
    result = subprocess.run(["multica", *args, "--output", "json"], capture_output=True, text=True, check=True)
    return json.loads(result.stdout)

def frontmatter(path):
    text = path.read_text()
    assert text.startswith("---\n"), path
    _, metadata, body = text.split("---", 2)
    data = yaml.safe_load(metadata)
    assert isinstance(data, dict)
    if path.name == "SKILL.md":
        assert data["name"] == path.parent.name
    assert isinstance(data["description"], str) and data["description"]
    return data, re.sub(r"<!--.*?-->", "", body, flags=re.S).strip()

tracked = git("ls-files").splitlines()
markdown = [p for p in tracked if p.endswith(".md")]
paths = tuple(ROOT / p for p in markdown)
source_paths = [p for p in tracked if p.endswith((".cpp", ".hpp", ".inc", ".py")) and "/externals/" not in p]
source = "\n".join((ROOT / p).read_text(errors="replace") for p in source_paths)
source_words = set(re.findall(r"\b\w+\b", source))
submodules = [line.split("\t", 1)[1] for line in git("ls-files", "--stage").splitlines() if line.startswith("160000 ")]
errors = []
rows = []
path_exceptions = []
link_cache = {}
inline = re.compile(r"(`+).*?\1")
historical_fragments = []
placeholders = {"EnumName_", "Is_", "EXPECT_", "ASSERT_", "ComponentKey_", "serialNumber_", "test_", "bD_", "delta_"}
for relative, path in zip(markdown, paths):
    raw = path.read_bytes()
    text = raw.decode("utf-8")
    historical = "Artifact status: implemented history" in text or relative == "CHANGELOG.md"
    assert raw.endswith(b"\n"), relative
    claims = checks.agent_referenced_paths(text)
    for line_number, token in claims:
        if (ROOT / token).exists() or (path.parent / token).exists():
            continue
        if historical:
            reason = "historical baseline"
        elif token in checks.AGENT_ALLOWED_MISSING or "/.venv" in token:
            reason = "local environment created by documented build"
        elif any(token == p or token.startswith(p + "/") for p in submodules):
            reason = "git submodule initialized by documented setup"
        else:
            errors.append(f"{relative}:{line_number}: missing path {token}")
            continue
        path_exceptions.append({"document": relative, "line": line_number, "path": token, "reason": reason})
    for token in re.findall(r"#include\s*[<\"]((?:dal/|dal-public/)[^>\"\n]+)[>\"]", text):
        if any(c in token for c in "<>{}*") or "MG_EnumName_enum." in token:
            continue
        target = ROOT / ("dal-cpp/" + token if token.startswith("dal/") else token)
        if not target.exists() and not historical:
            errors.append(f"{relative}: missing include {token}")
    if not historical:
        for name in re.findall(r"\b(?:Dal::|dal\.)([A-Za-z_]\w*)", text):
            if name not in source and name not in {"ifc", "mgl"}:
                errors.append(f"{relative}: undefined qualified API {name}")
    links = 0
    for line_number, line in checks.without_fenced_code(text.splitlines()):
        # Inline code is not Markdown: e.g. a C++ lambda [=](double).
        for match in checks.LINK_RE.finditer(inline.sub("", line)):
            links += 1
            checks.check_link(path, line_number, match.group(1), link_cache, errors)
    python_count = 0
    for block in re.finditer(r"^```python\s*\n(.*?)^```", text, flags=re.M | re.S):
        python_count += 1
        try:
            ast.parse(block.group(1))
        except SyntaxError as error:
            if not historical:
                errors.append(f"{relative}: Python syntax: {error}")
            else:
                historical_fragments.append(relative)
    symbols = set()
    if not historical:
        for match in checks.INLINE_CODE_RE.finditer(text):
            symbols.update(re.findall(r"\b[A-Za-z_]\w+_\b", match.group(1)))
        missing = symbols - source_words - placeholders
        if missing:
            errors.append(f"{relative}: undefined named symbols: {sorted(missing)}")
    rows.append({"path": relative, "lines": len(text.splitlines()), "sha256": hashlib.sha256(raw).hexdigest(),
                 "historical": historical, "links": links, "path_claims": len(claims),
                 "python_blocks": python_count, "symbols": len(symbols)})
checks.check_tables(paths, errors)
checks.check_whitespace(paths, errors)

tomls = sorted((ROOT / ".codex/agents").glob("*.toml"))
contracts = {p.stem: tomllib.loads(p.read_text()) for p in tomls}
assert len(contracts) == 10
claude = sorted(p for p in (ROOT / ".claude/agents").glob("*.md") if p.name != "README.md")
assert {p.stem for p in claude} == set(contracts)
for path in claude:
    data, body = frontmatter(path)
    expected = contracts[path.stem]
    assert data["name"] == expected["name"] == path.stem
    assert data["description"] == expected["description"]
    assert body == expected["developer_instructions"].strip()
    assert data["model"] == "inherit" and data["color"]
skills = sorted(p for p in paths if p.name == "SKILL.md")
for path in skills:
    frontmatter(path)
openai = sorted((ROOT / ".codex/skills").glob("**/agents/openai.yaml"))
assert len(openai) == 1
for path in openai:
    interface = yaml.safe_load(path.read_text())["interface"]
    assert all(isinstance(interface[k], str) and interface[k] for k in ("display_name", "short_description", "default_prompt"))
for name in ("code-style", "git-commit-pr", "unit-test-style"):
    assert (ROOT / f".claude/rules/{name}.md").read_bytes() == (ROOT / f".codex/references/{name}.md").read_bytes()

roster = cli("squad", "member", "list", "2b409a72-57c6-447a-a252-a8cff3208dc0")
agent_rows = []
for member in roster:
    assert member["member_type"] == "agent"
    agent = cli("agent", "get", member["member_id"])
    expected = contracts[agent["name"]]
    assert len(expected["description"]) <= 255
    assert agent["description"] == expected["description"]
    assert agent["instructions"].strip() == expected["developer_instructions"].strip()
    agent_rows.append({"name": agent["name"], "id": agent["id"], "description_chars": len(expected["description"]),
                       "contract_sha256": hashlib.sha256(expected["developer_instructions"].strip().encode()).hexdigest(),
                       "fields_updated": []})
assert {a["name"] for a in agent_rows} == set(contracts)
assert not errors, "\n".join(errors)
print(f"PASS: {len(rows)} Markdown files read in full; {sum(r['links'] for r in rows)} links; "
      f"{sum(r['python_blocks'] for r in rows)} Python blocks ({len(historical_fragments)} historical fragment); "
      f"{len(tomls)} TOMLs; {len(skills)} SKILL frontmatters; {len(openai)} openai.yaml; "
      f"{len(claude)} Claude agents; {len(agent_rows)} matching Multica agents; {len(source_paths)} source files.")
PY
```

## Delivery and Reviewer Handoff

Open the documentation PR against `master` with DAL-317 in its title; the final
Issue reply supplies the PR URL and immutable commit. Do not merge in this run.
The orchestrator owns parent status and independently dispatches dal-reviewer.
No new blockers or open questions. The reviewer should confirm the corrected
history/model/feature boundaries against source and repeat the contract checks.
