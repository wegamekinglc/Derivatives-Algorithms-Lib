# Curve Lab Canonical Quote Wiring Repair Plan

**Goal:** Close the production-page gap between the stateless quote
canonicalizer and the selected durable Curve Lab instrument without moving
financial state into `Curves.tsx`.

**Architecture:** `CurveLabWorkspace` remains the sole owner of the visual and
Advanced JSON draft. It exposes a narrow imperative method that accepts one
server-returned `CurveLabCanonicalQuote`, rechecks the current selected row and
family, and replaces only that row's `raw_quote` in one immutable state update.
`Curves.tsx` owns only the component wiring and selected-family presentation
signal. `CurveLabQuoteAuthoring` remains responsible for non-durable input and
presentation preferences.

## Task 1: Prove the production call-graph gap

**Files:**

- Add: `dal-web/frontend/tests/unit/curves_quote_integration.test.tsx`

1. Render the real `Curves` page with mocked HTTP client methods.
2. Select a workspace quote target, canonicalize `4 / PERCENT`, and create a
   draft.
3. Assert the created durable document contains the returned `0.04` bytes.
4. Run the focused test and retain the expected failure that no target/wiring
   exists.

## Task 2: Add the smallest atomic workspace bridge

**Files:**

- Modify: `dal-web/frontend/src/components/CurveLabWorkspace.tsx`
- Modify: `dal-web/frontend/src/components/CurveLabQuoteAuthoring.tsx`
- Modify: `dal-web/frontend/src/pages/Curves.tsx`

1. Add an explicit single-row canonical quote target control, initially
   unselected.
2. Expose `applyCanonicalQuote`, which rejects no target, an invalid target,
   and family mismatch before changing state.
3. Replace only the selected instrument's `raw_quote`; never copy response
   coordinate/unit/bump members or authoring preferences into the draft.
4. Keep the canonicalizer family synchronized with the selected row and reject
   an old response that returns after the target family changes.
5. Retain server-issued instrument identity in the editable document after
   create/save so equivalent authoring can be compared byte-for-byte.
6. Rerun the focused production-page test green.

## Task 3: Lock edge behavior

**Files:**

- Modify: `dal-web/frontend/tests/unit/curves_quote_integration.test.tsx`
- Modify: `dal-web/frontend/tests/unit/curve_lab_quote_authoring.test.tsx`
- Modify: `dal-web/frontend/tests/unit/curve_lab_workspace.test.tsx`

1. Add one failing case at a time for no selected target, backend failure,
   repeated application, two-row isolation, and a delayed response after a
   family switch.
2. Make each case green without weakening the production-page assertion.
3. Prove convention/presentation changes alone issue no draft mutation and do
   not change the workspace financial bytes.

## Task 4: Prove the real browser and persistence path

**Files:**

- Modify: `dal-web/frontend/tests/e2e/curve_lab_workspace.spec.ts`

1. Traverse Vite, the production React page, production client, real FastAPI
   routers, the exact-decimal service, and the in-memory persistence store.
2. Author the same stable instrument as `4 / PERCENT` and
   `0.04 / DECIMAL`.
3. Compare persisted document and fingerprint, build quote axis, published
   native identity, risk quote axis/results/matrix, and GET replay evidence.
4. Change presentation preference without applying new financial bytes and
   assert the original build remains non-stale.

## Task 5: Correct review evidence and verify delivery

**Files:**

- Modify: `.codex/artifacts/reviews/curve-lab-dal17-final-disposition.md`

1. Preserve the historical critique and its `Revise` verdict.
2. Replace the incorrect frontend-closure claim with the actual production
   route, workspace application boundary, and tests.
3. Run focused and full frontend unit/build/e2e checks, affected backend
   API/persistence checks, OpenAPI/generated/docs consistency, native/public/
   Python regressions required by repository CI, and diff hygiene.
4. Commit and push only to `feature/DAL-16-curve-lab`.
5. Wait for every PR #265 check on the new head and report exact evidence
   without merging or claiming final review readiness.
