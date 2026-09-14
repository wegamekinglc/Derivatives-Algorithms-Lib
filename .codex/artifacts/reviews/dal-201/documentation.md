# DAL-201 / F4 documentation decision

DAL-226, 2026-09-14. Documentation is aligned with model-aware prepared AAD
tree valuation. A CHANGELOG entry is required and added for the significant
capability and methodology change: parameter-dependent historical state now
participates in each worker recording. Independent review and coordinator
acceptance remain outstanding; this report is not merge approval.

## Revisions and publication

- Starting published head: `4e964a88ee9d79182dd8a037c1787cf025b5294a`.
- Starting tree: `5904bf9ca38c89d0dc1053c56b8c1c4dc7d05733`.
- Documentation commit: `597301463b3c3c6374c147e3c34cad699fadb6e5`.
- Documentation tree: `63d7432acce3a6da0ccab7ff34a2584e9cc5cded`.
- Independent tester's code: `bbaad1f646eb521eeb84806058c9e4685875d97b`;
  tree `53f38bd7b0b875ced047155f72f8c7608c15320c`.
- Branch: `feature/dal-201-historical-aad-state`.
- Existing draft PR: https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/371.
- Base: `feature/dal-200-eq-observation-slots`; dependency #369.

The starting checkout was clean, and local HEAD, remote branch, and GitHub PR
head matched. Publication adds only this report after the documentation commit.
The attached `documentation-publication.json` records the final published
SHA/tree, remote/GitHub verification, file scope, and one CI snapshot. It is
external to the commit because a report cannot contain its own final SHA.

## Non-trivial discrepancies corrected

- `docs/methodology/script_engine.md` rejected named AAD/fuzzy execution in
  its syntax overview and unsupported-execution section, and restricted all
  nonexpired AAD to products without past events. Those restrictions now
  distinguish supported prepared AAD tree from raw historical AAD, named
  compiled, and prepared AAD compiled execution.
- The same guide described passive preparation without explaining the separate
  active replay. It now documents matching prepared mode/smoothing, retained
  plan versus fresh data-entry valuation, sealed historical doubles, typed
  seed reconstruction before Mark, same-recording path restore, new roots,
  hard historical/fuzzy future conditions, and risk normalization exactly once.
  It gives the 80/SCALE=2 discounted-payment oracle and direct-seed sensitivity.
- `docs/methodology/aad.md` omitted shared historical state and fresh roots in
  its Monte Carlo algorithm. Its batch/mark description and cross-link now
  explain those dependencies and total-path normalization.
- `docs/methodology/index_parsing.md` independently repeated the obsolete
  named AAD/fuzzy rejection. Its valuation summary now agrees with the script
  guide while retaining future-index and public-surface restrictions.

## Ground truth

Read the target methodology documents and changelog, the registered role and
repository conventions, active implementation/testing reports, and current
source. The key contracts were reconciled against:

- `dal-cpp/dal/script/preparation.hpp` and `preparation.cpp`: executable versus
  history-only plans, mode capture, compiled rejection, passive history,
  double replay, IF analysis, and worker historical-state construction.
- `dal-cpp/dal/script/simulation.hpp`: both data/prepared overloads, mode and
  smoothing guards, batch-local active model/evaluator/zero, input registration,
  NewRecording, historical replay, Mark, root propagation, reduction, task drain.
- `dal-cpp/dal/script/visitor/evalstate.hpp`, `evaluator.hpp`, `pastevaluator.hpp`,
  and `fuzzy.hpp`, plus `event.hpp`/`event.cpp`: typed seed precedence, passive
  observation reads, hard past comparisons and settled PAYS, future blending.
- `AAD::PayoffRoot` in `dal-cpp/dal/math/aad/aad.hpp` and the full historical
  replay/observation simulation test files: direct and constant roots, analytic
  and nonlinear risks, 8193-path rebuild oracles, 16385-path recovery,
  threads 1/2/4, repeated inputs, history read barriers, and rejection cases.
- Public `value.hpp`/`value.cpp`, Python script/value bindings, Excel script/value
  wrappers, and the script example: named preparation options are still absent
  from facade valuation; existing example and debug capabilities do not imply
  those options are exposed.

No public contract or implementation change was needed. Published docs contain
current capability descriptions, with no F5-F8 delivery promise or history.
The new dated CHANGELOG entry records the capability; prior dated entries
remain historical evidence. No methodology document was added, removed, or
renamed, so `docs/README.md` and the `CLAUDE.md` methodology list need no change.
The existing implementation and testing reports are untouched.

## Validation

Commands and results are retained in attached `documentation-checks.log`:

```text
python3 .github/scripts/check_docs.py
git diff --check
git diff --cached --name-status
git diff --cached --check
git diff --name-only 4e964a88ee9d79182dd8a037c1787cf025b5294a HEAD
git ls-remote origin refs/heads/feature/dal-201-historical-aad-state
```

The repository checker passed for 66 Markdown files after the four published
doc edits, then for 67 with this active report. It checks local links/anchors,
table structure, whitespace, math macros, stale commands, and repository
documentation contracts. An additional Python inspection confirmed exact
column padding and separator widths in all six tables in the three changed
methodology notes, and final newlines in all changed Markdown files. Both
unstaged and staged whitespace checks passed; only the four documentation
files and this report are in the publication scope.

The first commit attempt found no configured Git identity and created no
commit. Retried with command-local `Codex <codex@openai.com>`, matching prior
commits on this branch; no global Git configuration changed.

No runtime tests were rerun for documentation-only changes. The independent
tester's report on the unchanged code records native 1715/1715, Adept 425/425,
CoDiPack 425/425, and XAD 424/424, with the same 16 F4 cases on each backend.
These are inherited test results, not executions by this documentation role.
Python, Windows XLL, sanitizer, performance, and full alternative-backend
public/Excel coverage remain unclaimed. Local Lizard success is not hosted
Codacy success. CI is captured once after publication, without polling or
waiting; required checks and DAL-227 review remain separate acceptance gates.
DAL-223 performance deferral remains in force.
