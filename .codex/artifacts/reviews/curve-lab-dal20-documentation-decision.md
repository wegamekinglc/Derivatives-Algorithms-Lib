# Curve Lab DAL-20 Documentation Decision

## Baseline

- Integration branch: `feature/DAL-16-curve-lab`
- PR: `#265`
- Final-repair candidate reviewed:
  `0eb35bf79a077c10cc37c80ffb6d990f0675b4b9`
- Prior approved implementation head:
  `1bf5a64e226066b3cd3d4af6411186bd1c2d6199`
- Prior product verification: DAL-23, round 5, passed at that approved head
- Prior code review: DAL-19, round 5, approved at that approved head with no
  High or Medium findings
- Product disposition: DAL-17, `Proceed with caveats`

The final-repair candidate adds family-legal draft migration, recoverable
same-job polling, stable rejection of non-finite numeric input, and the DAL-17
final-disposition evidence. Independent DAL-23 verification and DAL-18 final
review remain downstream gates for the resulting head.

## Published current-state documentation

The supported product contract belongs in:

- `docs/curve-lab.md` for the end-to-end workflow, REST/OpenAPI behavior,
  native/Python surfaces, archives, persistence, restart, limits, matrices,
  compatibility, migration, rollback, and operations;
- `dal-web/README.md` for service ownership, persistence, API orientation,
  tests, and the current screen summary;
- `docs/public-api.md` for the installed C++ and Python rate-pricing surfaces;
- `docs/README.md` for discovery; and
- `CHANGELOG.md` for the already-landed significant native and web capability
  entries.

The July 2026 CHANGELOG entries qualify under the repository policy because
they add a new public typed pricing capability and a significant web workflow.
They are retained and linked to the durable guide. The final-repair changes are
correctness, polling-recovery, validation, and review-evidence closure within
that already-recorded capability: they add no breaking public API, new
methodology or numerical algorithm, distinct significant capability, public
removal, or deprecation. No duplicate or amended CHANGELOG entry is added.

## Historical and control evidence

The following remain under `.codex/artifacts/` and are not presented as the
current user contract:

- product specification `curve-lab-dal-web-v0.5.md`;
- Revision 8 design package and technical design;
- technical-design critique, whose historical `## Verdict: Revise` remains
  unchanged;
- Revision 8 implementation plan; and
- DAL-23/DAL-19 repair plans.

The additive `curve-lab-dal17-final-disposition.md` records the later
`Proceed with caveats` product decision and its durable source mapping. It does
not rewrite the critique or serve as merge authorization.

The three completed repair plans previously under `docs/superpowers/plans/`
were moved byte-for-byte to `.codex/artifacts/plans/`. This resolves the only
Low review finding and keeps delivery narrative out of published docs.

## Verification and review evidence

Source and test reconciliation at the final-repair candidate confirmed:

- the primary family control reconstructs legal draft fields for all seven
  closed instrument families, with exact wire-document unit coverage and a
  seven-family draft-creation browser scenario;
- build, import, and risk admission state is retained before polling, polling
  has no fixed client deadline, and an interrupted build can resume the same
  admitted ID;
- all eight floating-point request fields reject `NaN`, `Infinity`, and
  overflow with stable `422 REQUEST_VALIDATION_FAILED` responses before draft
  or audit persistence; and
- DAL-17 issue state, metadata, and final comment agree with
  `curve-lab-dal17-final-disposition.md`; the earlier critique remains
  byte-for-byte unchanged.

The DAL-20 follow-up executed:

- documentation checks across 40 Markdown files and all 18 checker unit tests;
- all 400 non-native backend tests (2 native tests deselected);
- all 58 frontend unit tests; and
- the frontend production build.

The guarded real-API Playwright suite was not executed locally because the
repository-scoped Chrome runtime was unavailable and its setup download did
not complete. Its seven-family draft-creation scenario was reconciled against
the visual control and request path in source; execution remains part of the
independent DAL-23 gate.

At the prior approved head, DAL-23 reported:

- 1,164 passing CTest cases;
- 375 passing backend tests plus the native integration test;
- 49 passing frontend unit tests and a successful production build;
- 8 passing Playwright scenarios; and
- clean generated-file and OpenAPI drift checks.

DAL-19 then approved that same prior head with no High or Medium findings. Its
only Low finding was the placement of completed repair plans, resolved by the
earlier documentation change.

These counts are integration evidence for the prior approved baseline, not a
claim that the final-repair candidate has the same test totals and not promises
about the size of future test suites. They remain in this artifact rather than
the published guides.
