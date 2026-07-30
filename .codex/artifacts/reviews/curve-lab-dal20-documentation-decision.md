# Curve Lab DAL-20 Documentation Decision

## Baseline

- Integration branch: `feature/DAL-16-curve-lab`
- PR: `#265`
- Complete implementation head reviewed:
  `8ec625e20087964f28d5f3770c8fb3e68a082a56`
- Superseded documentation head:
  `57aa3f3db4a267e56269fbfe350557eae3fa7b5f`
- Independent final-head verification: DAL-23,
  `verification_head=8ec625e20087964f28d5f3770c8fb3e68a082a56`
- Product disposition: DAL-17, `Proceed with caveats`

The final implementation delta adds two related presentation-boundary repairs.
Canonicalization now has explicit same-target latest-request-wins semantics.
Exact display strings come from a new additive, stateless rendering endpoint
that calls the backend `Decimal`/round-half-even formatter. The display path
does not change the durable draft or financial identity.

## Published current-state documentation

`docs/curve-lab.md` and `dal-web/README.md` describe the complete production
contract:

- every canonicalization submission receives a monotonic generation, and only
  the latest matching generation, family, target token, and draft epoch may
  update the canonical output, error, submitting state, or workspace quote;
- older success, failure, and cancellation responses are ignored, while
  target, family, draft, and authoring-input changes invalidate pending work;
- `POST /api/curve-lab/quote-renderings` accepts a closed request containing
  instrument family, canonical plain-decimal string, family-compatible display
  convention, and integer scale from 0 through 12;
- the closed response contains only exact string `rendered_quote`;
- the backend uses `Decimal` and round-half-to-even, including scale, signed
  tie, normalized-zero, and Future price-point behavior;
- rendering uses an independent latest-request-wins generation; and
- display convention, scale, rendered string, and rendering error are
  presentation-only and do not enter the draft, fingerprint, build/risk axes,
  replay identity, or stale state.

The rendering route is additive and has no store, gateway, queue, draft, or
audit dependency. Existing durable schemas, installed C++/Python surfaces,
native archive format, and V1 calibration behavior are unchanged. No
methodology document was added, removed, or renamed, so `docs/README.md`,
`CLAUDE.md`, and `docs/public-api.md` require no update.

## CHANGELOG decision

`CHANGELOG.md` is updated by amending its existing July 2026 Curve Lab entry.
The new stateless REST surface and server-authoritative exact display are
user-visible parts of the already qualifying significant Curve Lab capability.
The amendment records the complete capability without adding a duplicate
delivery-history bullet or a new dated section.

Latest-request-wins ordering is a correctness repair and would not qualify as
a separate entry by itself. This delta introduces no breaking public API, new
methodology or numerical algorithm, significant methodology shift, public
removal, or deprecation.

## Source, OpenAPI, and test reconciliation

The implementation at `8ec625e2` maps to the documentation as follows:

- `CurveLabQuoteAuthoring.tsx` permits a newer same-target submission while an
  older request is active. It gates success, failure, completion, and workspace
  application on the current generation, family, and target context.
- `CurveLabWorkspace.tsx` remains the final atomic target, family, and draft
  epoch gate and copies only canonical `raw_quote` into the selected row.
- Rendering has a separate request generation. Only its latest response or
  error is displayed; no rendering state is passed to the workspace.
- `CurveLabQuoteRenderingRequest` and
  `CurveLabQuoteRenderingResponse` are closed schemas. The committed OpenAPI
  snapshot exposes the additive `POST /api/curve-lab/quote-renderings` path,
  string `canonical_raw_quote`, integer `display_scale`, string
  `rendered_quote`, and the structured `422` response.
- `render_authoring_quote` calls `render_quote` directly. The service validates
  the family/convention pair, canonical stored bytes, and scale `0..12`, then
  renders with backend `Decimal` and `ROUND_HALF_EVEN` and removes a negative
  sign from zero.
- Backend API tests cover `0.04/PERCENT/6 -> 4.000000`, scales `0/1/6/12`,
  positive and negative half-even ties, normalized negative zero, Future price
  points, exact string response, closed errors, and absence of persistence or
  queue dependencies.
- Frontend component and production-page tests cover canonical and rendering
  request ordering, including both success/error completion orders,
  cancellation, duplicate submission, authoring edits, family/row changes, and
  presentation-only state.
- The real FastAPI/Vite browser scenario proves that a later `5/PERCENT`
  response keeps durable `raw_quote=0.05` when an earlier `4/PERCENT` response
  finishes last. It also proves exact display and unchanged draft,
  fingerprint, build/risk axes, native payload identity, replay, matrices, and
  stale state across display changes.

The full guide remains consistent with earlier final repairs: all seven family
selections reconstruct legal drafts; admitted build, import, and risk IDs
remain available for same-job polling recovery; and non-finite numeric input
returns stable `422 REQUEST_VALIDATION_FAILED` responses before draft or audit
persistence.

## Independent verification evidence

DAL-23 independently verified exact head `8ec625e2` without changing production
code or tests and reported:

- 1,209 passing native CTest cases, including `dal_python_pytest`;
- 415 passing backend tests, with 2 native-marked tests deselected, plus the
  focused native integration checks;
- 73 passing frontend tests and a successful production build;
- 11 passing real FastAPI/Vite Playwright scenarios with no skips;
- clean OpenAPI regeneration, generated-file checks, and diff hygiene;
- documentation checks across 40 Markdown files and 37 pytest checks; and
- all 58 GitHub checks passing on the implementation head.

These are DAL-23's independently reported results, not claims that DAL-20
reran the complete native/backend/browser matrix.

DAL-20 locally reran the documentation and affected contract checks after the
documentation update:

- documentation integrity across 40 Markdown files;
- 37 documentation-checker pytest cases;
- 36 focused rendering/canonicalization API tests;
- 32 focused frontend client, authoring-component, and production-page tests;
- OpenAPI generation with no snapshot drift; and
- `git diff --check`.

All passed. The focused backend run retained the existing Starlette deprecation
warning recorded below.

## Historical disposition and residual risks

The historical technical-design critique retains its original
`## Verdict: Revise`. The additive
`curve-lab-dal17-final-disposition.md` records the later
`Proceed with caveats`, the prior production-wiring overstatement, and the
current source/test closure. It does not authorize merging or replace DAL-18
final review.

The following verified boundaries remain explicit:

- live PostgreSQL publication locking was not exercised; evidence is limited
  to SQLite concurrency behavior and PostgreSQL SQL-path compilation/review;
- the real-browser suite uses a guarded deterministic test backend, while
  production pricing is covered separately by native/public/Python tests;
- `npm ci` still reports 2 moderate dependency findings; and
- existing Starlette/Alembic deprecation warnings remain.

PR #265 remains open, draft, and unmerged. DAL-18 final re-review remains the
next gate.
