DAL-199 F2 documentation handoff, 2026-09-13. This record controls the pending
independent review. Reconciled against tester head
`fef17a0af1e048d2863c74f74114da4cf039a20d` and production handoff
`feb5b6fc1d6042dde801f2cc0fd66385add0493f`. The containing commit is the
documentation handoff; no C++, tests, bindings, generated files, or build
configuration were changed in this role.

## Documentation Decision

- Read the complete target documents, active issue description and
  implementation/testing evidence; inspected preparation/settings/plan,
  event partitioning and simulation guards, snapshot projection and virtual
  index resolution, public script/value headers and implementations,
  Python/Excel script/value bindings, and C++/Python script-debug examples.
- `docs/methodology/script_engine.md` incorrectly limited FIX to parsing and
  said event partitioning occurred during parsing. It now describes fresh
  original-data preparation, one captured evaluation date, exact midnight,
  today policy, lookahead/structural validation, all-branch historical demand,
  canonical deduplication and retained uses, and immutable passive values.
  The snapshot section distinguishes global sequence reads from final virtual
  fixings, preserves raw direct/inverse FX semantics, and states that global
  capture is non-atomic and requires callers to exclude concurrent writes.
  Explicit snapshots never fall back to global history.
- The execution boundary explicitly retains raw `PreparationRequired`, the
  wholly expired zero path (including model construction for risk labels),
  and nonexpired prepared `UnsupportedExecutionMode`. It does not advertise
  future model binding, prepared evaluators, historical state/AAD replay, or
  preparation arguments on the public C++ facade or Python/Excel bindings.
- `docs/methodology/index_parsing.md` links the core historical adapter
  contract while retaining distinct parse, preparation, and execution limits.
  `docs/README.md` lists the existing note's preparation coverage. No
  methodology document was added, removed, or renamed, so the `CLAUDE.md`
  methodology list needs no change.
- CHANGELOG decision: add a qualifying September 13 entry. Immutable core
  historical preparation is a significant capability, and deferring event
  partitioning changes observable core/debug behavior. The entry names the
  execution limits and sequential capture constraint. The September 12 syntax
  entry's stale present-tense no-preparation claim is made historical.

## Open Findings and Delivery Boundaries

- Public debug compatibility remains open for independent reviewer judgment.
  `dal-public/src/script.hpp:32` (`ProductForDump`) reparses and indexes but
  never partitions. `DebugScriptProductJson` at line 39 and
  `DebugScriptProductTree` at line 45 use that copy. Meanwhile
  `dal-cpp/dal/script/event.cpp:27` retains every parsed event in `eventDates_`,
  and `DebugJson` at line 238 / `DebugTree` at line 267 label that container
  `future`. Thus an event before the global evaluation date is labeled
  future through those public dumps, unlike the previous parse-time split.
  Published methodology describes the actual container/partition behavior
  and explicitly says the wrappers' labels do not classify against the
  global date. This is disclosure, not acceptance of misleading phase labels;
  the orchestrator has been notified and must route the compatibility finding
  to the independent reviewer. A repair would require a fresh docs decision.
- The inherited conflicting map/vector `Dal::FixHistory_` definitions remain
  a prerequisite P1 blocker at this head. The orchestrator owns a separate
  repair worktree and its API decision. This F2 documentation change neither
  documents an unimplemented repair nor claims its tests pass.
- Independent tester evidence is pre-repair: native 1617/1617, focused Adept
  92/92 and CoDiPack 91/91. Full commands, configurations, and limits are in
  `testing.md` alongside this file. Documentation inspection adds no runtime,
  benchmark, Windows/XLL, Python, sanitizer, or remote CI evidence.
- PR 367 remains draft until prerequisite repair/integration, proportional
  retesting, final documentation reconciliation, and independent review.
  No platform/GitHub mutations or push were performed by this role.

## Validation

- `python3 .github/scripts/check_docs.py`: passed, including local links and
  anchors, Markdown table rules, trailing whitespace, final newlines, and
  documentation metadata/workflow checks.
- `git diff --check` and `git diff --cached --check`: passed. Exact staged
  scope is the four published Markdown files named above plus this artifact.
- Reviewed the full scoped diff against source. Runtime tests were not
  repeated for this documentation-only commit. The orchestrator records its
  exact SHA and supplies the complete resulting head to the reviewer.
