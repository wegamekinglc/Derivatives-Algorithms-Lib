# DAL-202 F5 documentation correction

Documentation now reflects the accepted repair: exact model-aware preparation
retains future branches, while constant arithmetic, live parameter inputs and
prepared fuzzy kernels retain their separate roles. This report supersedes the
earlier documentation report. Parent acceptance and corrected-head DAL-231
independent review remain required; this is not master delivery.

## Revision and scope

- Accepted tester publication: `8cd3d127354de08018fe45a71439b75e3489976c`.
- Accepted tree: `421f0b692a574d3785a78cee938d7f788edb443b`.
- Independently tested commit: `cd33eb794925eea9bc46a202069a3355fb48f994`.
- Tested tree: `3e56d8cfa49de06aad5b1512cec13ca45b8d9b72`.
- Fixed F4 ancestor: `5c954ca2fdded2ca35ad15c7fef44d90a002007d`.
- Branch: `feature/dal-202-compiled-observations`.
- [Draft PR #372](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/372)
  retains base `feature/dal-201-historical-aad-state` and dependencies #371/#369.

Initial clean checkout and GitHub PR head match the accepted publication.
The final comment and attached `publication.json` record resulting SHA/tree,
remote identity and scope without a self-referential commit hash here.

Changed paths:

- `docs/methodology/script_engine.md`
- `docs/README.md`
- `CHANGELOG.md`
- `.codex/artifacts/reviews/dal-202/documentation.md`

Everything outside those four files is byte-identical to the accepted tester
head, including C++, tests, bindings, generated files, executable examples,
configuration and dependency pointers. No methodology file was added, removed
or renamed, so the CLAUDE methodology list needs no change. The docs index
changed because its domain-folding summary also applied too broadly.
The existing AAD recording discussion remains accurate and needs no edit.

## Source reconciliation and decisions

Read the complete affected documents, current implementation/testing reports
and earlier documentation report. Read issue/parent/tester descriptions and
bounded comment history, role/style contracts, and the complete dal-git-pr
workflow. Inspected preparation, constant/domain/compiler/IF processing,
observation reads, simulation mode guards and typed seeds, public/binding
signatures, executable script example and related regression source.

Non-trivial discrepancies corrected:

- Named exact-double execution and pipeline ordering claimed future domain
  analysis and condition pruning. `PreparedScriptBuilder_::Prepare` calls
  `ProcessFuzzyDomains` only when `simulation.enableAad_` is true.
  Exact model-aware preparation skips both `DomainProcessor_` and
  `ConstCondProcessor_`, retaining parsed future branches, including known
  fixing conditions and default-bound SPOT.
- The domain section implied tolerance-based flags were sound exact proofs.
  It now scopes that machinery to enabled legacy preprocessing and the
  prepared fuzzy helper, and explains adjacent-float and small-factor limits.
  Prepared fuzzy comparisons retain continuous runtime kernels and widths;
  legacy discrete-bound behavior remains separately documented.
- Branch retention could be mistaken for disabling all folding. The guide
  distinguishes `ConstProcessor_` metadata and compiler arithmetic from
  domain pruning. Constant comparisons use actual hard double operators;
  the compiler still emits IF control flow and each supplied branch.
  Script parameters remain live `ConstVar` inputs, and conditional assignments
  remain nonconstant for dependency tracking.
- The docs index repeated the broad domain-folding claim. It now identifies
  exact branch retention, constant arithmetic, fuzzy processing and legacy
  folding separately. Named regression references include exact boundary,
  signed-factor and cross-event nested-state oracles.

The pipeline still collects and seals history before optimization, performs
hard historical replay with settled PAYS discarded, tracks historical
dependencies, finalizes IF and constant metadata, and builds requested
bytecode before workers. Tree and compiled execution share one observation
plan and payment mapping. Typed historical seeds are rebuilt locally per AAD
recording. Mode/epsilon guards, live parameters and fuzzy kernels are unchanged.

CHANGELOG decision: **amend the existing 2026-09-14 compiled-observations
capability entry**, adding exact branch retention without tolerance-domain
pruning and distinguishing constant arithmetic. The capability entry qualifies
under the role contract; a second repair/delivery entry would be redundant.
Older capability entries remain historical records. Public methodology contains
current-state explanations, with no issue-delivery narrative.

## Fresh validation and inherited evidence

From the repository root:

```bash
python3 .github/scripts/check_docs.py
python3 ../documentation-evidence/verify_inputs.py
python3 ../documentation-evidence/validate_docs.py
git diff --check
git diff 8cd3d127354de08018fe45a71439b75e3489976c --name-status
git diff --exit-code 8cd3d127354de08018fe45a71439b75e3489976c -- dal-cpp dal-public dal-python dal-excel .github cmake CMakeLists.txt CMakePresets.json
```

Fresh documentation checker: **70 Markdown files pass**. Local links/anchors,
table structure, whitespace, placement and documented command checks pass.
The attached supplemental validator checks final newlines, exact table
padding, unchanged executable code fences, the four-path allowlist and zero
diff outside it. Staged paths and whitespace are checked before commit.
Publication metadata verifies head/tree, fixed F4 ancestry, clean worktree,
unchanged code/configuration, and matching remote branch/PR identity.
The branch-history inventory uses the fixed F4 ancestor because this checkout
has no local master ref; dependency branches are not fetched forward or changed.

Downloaded all three accepted tester attachments through authenticated CLI.
Archive SHA256:
`3695a7707b5974f3013d7dfa0eebbb68d3d3bc5a36b59b243de69461847aedfe`.
All **60** internal entries, **three** tested source hashes and equality of
downloaded/archived/checkout testing.md verify. Inspected inherited full Linux,
backend, legacy, allocation and RED logs; these are not fresh test executions.

No C++ tests were rerun for this documentation-only change. Accepted DAL-229
evidence remains full Linux build/install/CTest **1738/1738**; targeted
native/Adept/CoDiPack **448 each**, XAD **447**; reviewer reproductions **4**,
legacy parity/fuzz **33**, isolated allocation **2**. All eight exact regressions
detect original preparation; the added signed-factor nested-state regression
passes on all four backends. The 27 lifetime combinations and local complexity
4/helper 1 are inherited independent evidence, not remote Codacy acceptance.

## Limits and handoff

Core-only settings remain core-only: no F6-F8 public facade/Python/Excel
valuation settings, default-index archive projection or FIX JSON schema support
is advertised. Explicit BS/Dupire spot binding to one ordinary EQ, historical
EQ/FX rules, unsupported future FX/IR/composite/delivery/multi-asset outputs,
exact midnight/today policy, strict history and non-atomic global capture
limits remain. History-only nonexpired preparation remains nonexecutable.

Allocation probes measure native double C++ requests, not AAD tape allocations
or arbitrary malloc. Observation-read complexity is source-inspected, not
dynamically counted. Compilation failure injection precedes bytecode
construction. No Windows/Python/sanitizer/full alternate public/Excel suite or
performance results are claimed. Retained exact branches have unmeasured cost;
DAL-223 remains deferred, with no experiments or threshold changes.

One post-publication CI snapshot is attached, without watch/sleep/poll. It is
not corrected-head review or merge-gate acceptance. The coordinator accepts
this report, then reopens existing DAL-231 for mandatory independent re-review.
PR stays draft, with no merge/close intent, dependency update or F6 start.
