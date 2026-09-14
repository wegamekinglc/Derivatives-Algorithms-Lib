# DAL-202 F5 documentation reconciliation

Model-aware exact and fuzzy preparation both retain parsed future arithmetic
and branches without tolerance-domain processing. The methodology, index and
existing capability entry now reflect that behavior. Parent acceptance and
mandatory DAL-231 independent review of the corrected head remain required.

## Revision and scope

- Accepted tester publication: `3b7b1accc2b09fc2aec282ed623d796ba6add68c`.
- Accepted tree: `d3bd8be932a84563facc25ec4a3ecb96b8759465`.
- Independently tested commit: `1e1e04a7cfdff126db330a2292f38bc3f93954dc`.
- Tested tree: `0cff8c9308c0205d83494d3c86165718aa7fca6a`.
- Fixed F4 ancestor: `5c954ca2fdded2ca35ad15c7fef44d90a002007d`.
- Branch: `feature/dal-202-compiled-observations`.
- [Draft PR #372](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/372)
  retains base `feature/dal-201-historical-aad-state` and dependencies #371/#369.

Initial clean checkout and GitHub head match the accepted publication.
Attached publication metadata and the final issue comment record the resulting
SHA/tree, remote identity and file scope without a self-referential hash here.

Changed paths:

- `docs/methodology/script_engine.md`
- `docs/README.md`
- `CHANGELOG.md`
- `.codex/artifacts/reviews/dal-202/documentation.md`

Everything outside these four documents is unchanged from the accepted tester
head, including production, tests, generated files, bindings, executable
examples, configuration and dependency pointers. No methodology file was added,
removed or renamed; the CLAUDE methodology list therefore needs no change.
The AAD recording discussion remains accurate.

## Source reconciliation and CHANGELOG decision

Read the full target documents and current implementation/testing reports,
the issue/parent/tester context, role/style contracts and complete dal-git-pr
publication workflow. Checked preparation, constant/domain/compiler/IF
processing, parsed comparison defaults, observation reads, typed simulation
state, public/binding signatures, examples and fuzzy arithmetic regressions.

Non-trivial discrepancies corrected:

- The guide and index described prepared fuzzy domain processing through
  `ProcessFuzzyDomains`. That helper and its invocation have been removed.
  `PreparedScriptBuilder_::Prepare` skips both `DomainProcessor_` and
  `ConstCondProcessor_` for exact and fuzzy future programs.
- The domain section attributed initial historical variable domains to active
  model-aware preparation. Historical dependency analysis now feeds only
  constant metadata. Domain and condition-folding pipeline descriptions are
  scoped to legacy `ScriptProduct_::PreProcess` when enabled.
- Tolerance-domain arithmetic is unsuitable for these preparations: adjacent
  floating-point comparisons and tiny factors cannot establish exact branch
  proofs, and interval inversion can misclassify finite signed tiny divisors
  as zero. Parsed arithmetic and branches remain available for evaluation.
- Prepared comparisons use the parsed continuous kernels with default or
  explicit epsilon. Legacy domain processing can still supply discrete bounds.
  Nested fractional state and cross-event/default-width coverage are described
  alongside the independent analytic and unoptimized references.
- The separate `ConstProcessor_` uses ordinary double arithmetic for
  literal/history-only expressions; live `ConstVar` parameters and historical
  parameter dependencies remain active. Conditional assignments stay
  nonconstant. Exact compilation can fold a constant comparison with actual
  hard operators while emitting IF control flow and both supplied branches.

The preparation sequence still performs syntax-wide collection and strict
history prefetch, hard historical replay with PAYS evaluated/discarded,
historical dependency analysis, final IF nesting/affected-variable metadata,
future constant metadata, and requested compilation before worker submission.
Tree and compiled evaluation share observation IDs and payment mappings.
AAD recordings rebuild typed historical seeds locally. Runtime fuzzy kernels,
epsilon behavior, eager booleans and mode guards are unchanged.

CHANGELOG decision: **amend the existing 2026-09-14 Script compiled
observations capability entry** to cover both exact and fuzzy preparation
without tolerance-domain arithmetic. The significant capability already
qualifies for an entry; this reconciliation needs no additional dated repair
or delivery narrative. Older entries remain historical records. The docs index
changes because it explicitly advertised the removed fuzzy domain pass.
Published methodology remains current-state prose.

## Fresh documentation validation

Run from the repository root with the attached evidence scripts:

```bash
python3 .github/scripts/check_docs.py
python3 ../fuzzy-evidence/verify_inputs.py
python3 ../fuzzy-evidence/validate_docs.py
git diff --check
git diff 3b7b1accc2b09fc2aec282ed623d796ba6add68c --name-status
git diff --exit-code 3b7b1accc2b09fc2aec282ed623d796ba6add68c -- dal-cpp dal-public dal-python dal-excel .github cmake CMakeLists.txt CMakePresets.json
```

Fresh docs checker: **70 Markdown files pass**. Supplemental validation checks
local links/anchors, exact table padding, whitespace, final newlines, unchanged
executable code fences, the four-path allowlist and zero diff outside it.
Staged scope and patch whitespace are verified before commit. Publication
metadata verifies SHA/tree, fixed F4 ancestry, clean status and matching
remote/PR head. Branch history is inventoried against the fixed F4 ancestor;
dependency branches are not updated.

## Accepted tester evidence

Downloaded the three accepted DAL-229 attachments via authenticated CLI.
Archive SHA256:
`e848ea64f3c28ffc333f4ace7f6d9dbbe80d6c114d8565bb654b2c6d6757ba6d`.
All **72** internal entries, **seven** tested source hashes and equality of
downloaded/archived/checkout testing.md verify. Read command records, original
RED failures, successful reviewer/allocation logs and backend suite evidence.
These are inherited independent test results, not executions by the doc-writer.

No C++ tests were rerun for this documentation-only change. Accepted DAL-229
results are full native Linux build/install/CTest **1743/1743**; targeted
native/Adept/CoDiPack **453 each**, XAD **452**; all **33** parity/fuzz cases on
every backend; unchanged fuzzy reviewer reproduction **6/6**, original exact
reproduction **4/4**, isolated allocation fixture **2/2**.

The five fuzzy-arithmetic tests cover the existing 25 arithmetic fixtures plus
six signed nested fractional cross-event fixtures with default/explicit widths.
Their analytic PV/SCALE/rate formulas and unoptimized central differences
remain intact; all five detect original preparation, including the new test's
independent RED. The 27 lifetime combinations, 8193 paths, complete risks and
local complexity 4/helper 1 remain inherited evidence.

## Limits and handoff

Named valuation settings remain core-only: F6-F8 public facade/Python/Excel
settings, default-index archive projection and FIX JSON schema support are
unavailable. Explicit BS/Dupire spot binding to one ordinary EQ, historical
EQ/FX rules, unsupported future FX/IR/composite/delivery/multi-asset outputs,
midnight/today policy, strict history and non-atomic global capture limits
remain. History-only nonexpired preparation remains nonexecutable.

Allocation instrumentation measures native double C++ requests, not AAD tape
allocations or arbitrary malloc. Observation indexing complexity is
source-inspected. The compiler-failure seam precedes bytecode construction.
No new Windows/Python/sanitizer/full alternate public/Excel/performance claims
are made. Retained branches' performance is unmeasured. DAL-223 remains
deferred with unchanged gates and thresholds.

One post-publication CI snapshot is attached; it is not remote gate acceptance.
No watch/sleep/poll, dependency update, merge, close intent or F6 start.
The coordinator accepts this report, then reopens existing DAL-231 for
mandatory independent review. DAL-230 returns in_review; DAL-202 owns
acceptance and actual master delivery.
