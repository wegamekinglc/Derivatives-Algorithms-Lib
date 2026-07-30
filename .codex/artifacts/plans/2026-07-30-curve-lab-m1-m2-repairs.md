# Curve Lab M1/M2 Review Repairs

## Scope

Close the two DAL-18 Medium findings on the existing DAL-16 integration branch:

1. canonical quote responses use explicit latest-request-wins semantics; and
2. presentation values are rendered by the authoritative backend exact-decimal
   formatter.

No durable financial schema, product family, native pricing behavior, or bond
surface changes are in scope.

## Design

### M1 — request ordering

- Assign a monotonic generation to every canonicalization submission.
- Capture the request family, workspace target token, and authoring-input epoch.
- Ignore success, failure, and completion effects unless the generation and
  captured context still match the current authoring context.
- Keep `CurveLabWorkspace.applyCanonicalQuote` as the final atomic target,
  family, and draft-epoch gate.
- Permit a newer submission while an older request is still in flight.

### M2 — exact presentation rendering

- Add an additive stateless
  `POST /api/curve-lab/quote-renderings` endpoint.
- Its closed request carries only instrument family, canonical raw quote,
  display convention, and display scale.
- Its closed response carries only the exact rendered string.
- The route calls the existing `render_quote` Decimal/round-half-even
  implementation directly and has no repository, queue, gateway, draft, or
  audit dependency.
- The frontend uses a separate latest-request-wins presentation request.
  Convention, scale, rendered string, and rendering errors remain local UI
  state and never enter the workspace document.

## TDD sequence

1. Add production-page deferred canonicalization tests for newer-success then
   older-success, newer-error then older-success, newer-success then
   older-error, duplicate submission, authoring-input edit, family change, and
   same-family row change. Confirm the current implementation fails the
   same-target ordering case.
2. Add backend API tests for exact rendering at scales `0`, `1`, `6`, and `12`,
   positive/negative half-even ties, signed zero, Future price points, closed
   errors, exact string response, and zero side-effect dependencies. Confirm
   the route is absent.
3. Add authoring component and production-page tests that require
   `0.04 / PERCENT / 6` to display `4.000000` without mutating the workspace.
   Confirm RED.
4. Implement request generation/context checks and the minimum additive render
   route/schema/client/UI path.
5. Run focused GREEN tests, then refactor while green.
6. Add the real FastAPI/Vite Playwright display assertion while retaining the
   persisted/fingerprint/build/risk/replay identity proof.
7. Regenerate OpenAPI, check generated artifacts and documentation consistency,
   run the affected frontend/backend suites and required native/public/Python
   regressions, push the same branch, and wait for the current-head CI result.
