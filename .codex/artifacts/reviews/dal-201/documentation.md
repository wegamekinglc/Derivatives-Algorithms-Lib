# DAL-201 payoff-root documentation correction

DAL-226, 2026-09-15. The two payoff-root review comments are addressed in
the local documentation candidate. Native terminal-node reuse and the
registered-zero fallback now agree with the source. This replaces the prior
documentation handoff; independent review and coordinator publication remain
outstanding.

## Revisions and scope

- Starting published commit: `12353bb773ad16de37405f6e37a312867c918acd`.
- Starting tree: `466d55a4ed7e215ca79877679173022aceee47e8`.
- Local branch: `fix/dal-226-payoff-root-docs`.
- Product branch: `feature/dal-201-historical-aad-state`, base `master`.
- Existing [PR #371](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/371)
  was OPEN and non-draft when read; GitHub head matched the clean starting checkout.
- The attached `documentation-local.json` records the final local commit/tree,
  starting commit/tree, three changed-file SHA256 hashes, patch hash, and
  reconstruction result. It is outside the commit to avoid self-referential hashes.
- `documentation.patch` is the complete portable diff from the starting commit:
  only the two methodology guides and this role's report. No push, PR-head
  mutation, review-thread resolution, or merge is part of this handoff.

## Review-thread mapping and discrepancies

- `PRRT_kwDOBtahP86iL2IX`,
  [AAD algorithm comment](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/371#discussion_r4007127736):
  `docs/methodology/aad.md` described a fresh root on every path. The algorithm
  now creates or reuses a path-local root; the following paragraph states both
  native reuse conditions and the registered-zero fallback on other cases and
  alternative backends.
- `PRRT_kwDOBtahP86iL2I-`,
  [script lifetime comment](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/371#discussion_r4007127792):
  `docs/methodology/script_engine.md` unconditionally described adding zero.
  The lifetime section now requires a nonempty post-mark range and an exactly
  terminal payoff for native reuse; otherwise it records `payoff + activeZero`.
  Adept, XAD, and CoDiPack always use that addition. The later value/AAD
  algorithm repeated the fresh-root claim and now agrees with this section.

Both guides retain historical seed-adjoint accumulation, passive-constant and
empty-suffix safety, hard historical/fuzzy future behavior, and normalization
once by total paths. A nonterminal post-mark payoff also takes the fallback;
being recorded after the mark alone does not qualify it for reuse.

## Ground truth and documentation decisions

Read both target guides in full, the prior documentation report, role and Git
contracts, current `dal-cpp/dal/math/aad/aad.hpp` and
`dal-cpp/dal/script/simulation.hpp`, and both relevant test files in full:
`dal-cpp/tests/math/aad/test_payoff_root.cpp` and
`dal-cpp/tests/script/test_past_replay.cpp`. Reconciled the root optimization
section of the active implementation report and the historical test inventory.
Read the two GitHub review comments directly and the current parent handoff.

`PayoffRoot` checks `End() != Mark()` and payoff/terminal adjoint identity on
native; all other cases return `payoff + activeZero`. `EvaluateAADBatch`
registers the zero through `InitModel4ParallelAAD`, calls the helper after
evaluation, seeds the returned root, and propagates to the mark. Batch-end
propagation and risk reduction retain their existing accumulation semantics.
The native root test asserts no extra node over 257 paths; historical tests
cover direct seeds, constants, empty suffixes and batch boundaries. These were
read as source evidence, not rerun or newly certified by this documentation task.

**CHANGELOG: no change.** This is a correction of implementation wording,
with no new capability, algorithm, public API, deprecation, or significant
methodology shift. The existing dated historical-AAD entry remains a delivery
record; its fresh-root statement for direct seeds and constants still describes
the fallback. No guide was added, removed, or renamed, so `docs/README.md` and
the `CLAUDE.md` methodology list need no update. Other role artifacts and all
product, test, binding, build, benchmark, and diagnostic files are unchanged.

## Validation and delivery limits

Attached `documentation-checks.log` records commands, outputs, and exit codes;
the attached `validate-documentation.py` reproduces the additional inspection.

- `python3 .github/scripts/check_docs.py`: 55 Markdown files pass, including
  local links/anchors, table structure, whitespace, and documentation contracts.
- Additional inspection: all five target-guide tables have exact column padding
  and separator widths; all three changed Markdown files have final newlines
  and no trailing whitespace. No unconditional fresh-root wording remains in
  the target guides.
- `git diff --check`, staged scope/whitespace checks, and final diff scope pass.
- Portable patch application to a temporary Git index initialized from the
  starting commit reproduces the delivered local tree; the working tree is clean.

No product backend tests or benchmarks were rerun for these documentation-only
changes. No new CI was triggered or awaited, and no performance acceptance is
claimed. The parent handoff's hosted paired 87/90 failure and performance
thread `PRRT_kwDOBtahP86iL2H4` remain outside this scope; DAL-223 remains deferred.
The separate hosted diagnostic candidate is untouched. The coordinator must
arrange independent documentation review and publication; this local correction
does not complete or approve the F4 merge.
