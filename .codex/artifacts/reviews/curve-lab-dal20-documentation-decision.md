# Curve Lab DAL-20 Documentation Decision

## Baseline

- Integration branch: `feature/DAL-16-curve-lab`
- Approved implementation head:
  `1bf5a64e226066b3cd3d4af6411186bd1c2d6199`
- Product verification: DAL-23, round 5, passed at the approved head
- Code review: DAL-19, round 5, approved with no High or Medium findings

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
They are retained and linked to the durable guide; no duplicate delivery entry
is added.

## Historical and control evidence

The following remain under `.codex/artifacts/` and are not presented as the
current user contract:

- product specification `curve-lab-dal-web-v0.5.md`;
- Revision 8 design package and technical design;
- technical-design critique;
- Revision 8 implementation plan; and
- DAL-23/DAL-19 repair plans.

The three completed repair plans previously under `docs/superpowers/plans/`
were moved byte-for-byte to `.codex/artifacts/plans/`. This resolves the only
Low review finding and keeps delivery narrative out of published docs.

## Verification and review evidence

At the approved head, DAL-23 reported:

- 1,164 passing CTest cases;
- 375 passing backend tests plus the native integration test;
- 49 passing frontend unit tests and a successful production build;
- 8 passing Playwright scenarios; and
- clean generated-file and OpenAPI drift checks.

DAL-19 then approved the same head with no High or Medium findings. Its only
Low finding was the placement of completed repair plans, resolved by this
documentation change.

These counts are integration evidence for the fixed baseline, not promises
about the size of future test suites, so they remain in this artifact rather
than the published guides.
