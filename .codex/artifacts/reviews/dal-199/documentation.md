DAL-199 F2 final documentation handoff, 2026-09-13. Reconciled against clean
integrated tester head `7c56ec532c610f692bfdc7b4864b4c696594a58b`, containing
public dump correction `87edd0acff03abd1b5c08cf783fc4a05e5a274d8` and the
review-approved prerequisite repair `c121fa1d316ea0ebd936c0bb4ae387c4d64e88cc`.
The containing commit is the final documentation handoff for independent
combined-head review. No C++, tests, bindings, generated files, or build
configuration were changed by the doc writer.

## Current Documentation Decision

- Reconciled the complete script methodology and changelog with the updated
  implementation/testing reports and independent F2/prerequisite reviews.
  Inspected the shared public `ProductForDump`, all three wrappers, core
  rendering/partitioning, the public phase/FIX regressions and the tester's
  parse-time clock-change case. Earlier source review covered preparation,
  snapshots, virtual EQ/FX fixing behavior, public valuation, bindings, and
  script examples; those capability boundaries remain accurate.
- `docs/methodology/script_engine.md` describes core preparation from fresh
  original data, one captured date, exact midnight, today policy, structural
  and lookahead validation, all-branch historical demand, canonical
  deduplication and retained uses, and immutable passive values. It preserves
  raw direct/inverse FX semantics and distinguishes global sequence reads
  from final virtual fixing calls. Sequential global capture is non-atomic;
  callers must exclude concurrent writes. Explicit snapshots remain
  authoritative, with no fallback to global history.
- Replaced the obsolete public-debug misclassification wording. Every public
  dump captures D before parsing, partitions a fresh private copy once, and
  refreshes that date for the next dump. JSON/tree retain correct past/future
  phases, resolved metadata, raw branches, and schema `/1` FIX rejection.
  Legacy text stays live-only with unresolved variable indices, no added
  variable-table header, and its empty-live-description error. Dumping calls
  no `PreProcess` and performs no condition folding, fixing lookup, model setup
  or worker submission. Lower-level core dumps still use existing containers; the guide
  explains explicit partitioning for direct callers without implying broken
  public behavior. Corrected the existing core example's claim that parsing
  alone resolves variable indices.
- Execution limits remain explicit: raw FIX raises `PreparationRequired`;
  prepared execution supports only the structurally valid wholly expired
  zero path, with model construction for labels but no Allocate/GeneratePath/
  submission. Every nonexpired prepared product raises
  `UnsupportedExecutionMode`. No future model binding, prepared evaluator,
  historical state/AAD replay, or public-language preparation arguments are
  advertised.
- CHANGELOG decision: retain the qualifying F2 capability entry and describe
  correct public dump compatibility in that entry. The P2 repair restores an
  existing contract, so it does not warrant another changelog entry. Preserve
  the merged prerequisite entry verbatim: direct-core map-wrapper rename,
  nested `vals_t`/typed `Empty()` migration, unchanged global vector type,
  no compatibility alias, and mandatory clean DAL/dependent rebuilds.
- Existing `docs/methodology/index_parsing.md` accurately combines the
  historical adapter link and distinct fixing-container contract;
  `docs/README.md` already lists preparation coverage. Neither needs another
  edit. No methodology document was added, removed, or renamed, so the
  `CLAUDE.md` methodology list needs no change.

## Resolved Findings and Remaining Gates

- The inherited P1 identity collision is repaired in the integrated source.
  The independent prerequisite reviewer approved `c121fa1d`; fresh integrated
  native and actual-header cross-TU ASan evidence now validates the combined
  build. Old conflicting objects remain subject to the published migration
  and clean-rebuild requirement, not a current production-source blocker.
- The P2 public dump correction is implemented and independently tested.
  This handoff supersedes the earlier documentation artifact's open debug
  finding and pre-repair blocker language. Final independent review of the
  complete combined head remains required; this is a documentation decision,
  not that review's approval.
- `testing.md` clearly separates initial evidence from final integrated
  verification and marks P1 resolved/P2 passing. With the orchestrator's
  authorization, refreshed only the implementation report's pending-work
  sentence to reflect completed integration and independent validation,
  linking its disposition to the current testing/documentation handoffs.
  Historical RED/GREEN commands and results remain unchanged.

## Verification

- Independent integrated tester results: native 1625/1625 (including 110
  public and 29 portable Excel cases); focused Adept 95 core plus 13 public
  script cases; focused CoDiPack 94 core plus 13 public script cases. Both
  actual-header include orders compile, and freshly compiled `-O0` cross-TU
  ASan reproductions pass in both link orders. Full commands/configurations
  and logs are in `testing.md` alongside this file. These supersede the
  earlier pre-repair counts; runtime tests were not repeated for this
  documentation-only pass.
- `python3 .github/scripts/check_docs.py`: passed, including local links and
  anchors, Markdown table rules, trailing whitespace, final newlines, and
  documentation metadata/workflow checks.
- `git diff --check` and `git diff --cached --check`: passed. Exact staged
  scope is `docs/methodology/script_engine.md`, `CHANGELOG.md`, this
  artifact, and the authorized status wording in `implementation.md`.
  The prerequisite migration entry is byte-for-byte preserved.
- No outstanding documentation/source mismatch remains. Final independent
  review, publication and remote merge gates belong to the orchestrator. No
  Windows/XLL, Python runtime, XAD runtime, full-suite sanitizer, performance,
  completed F9 matrix, or remote CI acceptance is claimed by this pass. No
  push or platform/GitHub mutation was performed by the doc writer.
