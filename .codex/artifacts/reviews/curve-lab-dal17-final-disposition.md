# Curve Lab DAL-17 Final Disposition

Date: 2026-07-30

Scope: DAL-16 Curve Lab Revision 8

Final design disposition: **Proceed with caveats**

## Evidence lineage

The historical adversarial review remains unchanged at
`.codex/artifacts/critiques/curve-lab-technical-design-review.md`, including its
`Revise` verdict. That artifact records the blocking state of the design when it
was reviewed; it is not overwritten or re-labelled as an approval.

DAL-17 subsequently recorded the final product-owner/coordinator disposition:

- Multica issue: `55c68e22-4191-40f3-b5e8-ad494c14c3b6`
- Final disposition comment: `2a889d11-8884-4511-a7ae-8f952600d033`
- Recorded at: `2026-07-27T16:43:09Z`
- Metadata decision: `proceed_with_caveats_user_core_approved`
- Conclusion: the Revision 8 design may proceed; the remaining authoring
  adapter and presentation-preference items are implementation caveats rather
  than design blockers.

The approved product core remains the closed seven-family Curve Lab workflow:
`DEPOSIT`, `FRA`, `FUTURE`, `OIS`, `IRS`, `BASIS_SWAP`, and `XCCY`. Bonds,
swaptions, approval workflow, and other non-goals remain excluded.

## DAL-17 caveat disposition

| Final caveat | Route/schema closure | Client closure | Test evidence |
|---|---|---|---|
| Percent authoring must be adapted to canonical durable decimals outside the persisted draft | `POST /api/curve-lab/quote-canonicalizations` in `dal-web/backend/app/routers/curve_lab.py`; closed request/response and registry contracts in `dal-web/backend/app/schemas/curve_lab.py`; conversion in `dal-web/backend/app/services/quote_canonicalization.py` | `canonicalizeCurveLabQuote` sends the exact lexeme; `Curves.tsx` connects `CurveLabQuoteAuthoring.tsx` to the explicitly selected target owned by `CurveLabWorkspace.tsx`; only the returned canonical `raw_quote` replaces that row | `test_curve_lab_api.py`, `test_curve_lab_quote_replay.py`, `curve_lab_quote_authoring.test.tsx`, `curves_quote_integration.test.tsx`, `api_client.test.ts`, and the production FastAPI/Vite flow in `curve_lab_workspace.spec.ts` |
| Exact authoring lexemes must reach the server without first becoming JavaScript numbers | `CurveLabQuoteCanonicalizationRequest.input_lexeme` is a string and the response returns canonical plain-decimal strings | `CurveLabQuoteAuthoringRequest` sends the captured input string directly; no binary-float parse is used for durable quote identity | Quote API boundary/replay tests cover percent, decimal, and Future price-point authoring plus restart-stable bytes |
| Display preference is presentation state, not durable financial identity | Durable schemas persist only canonical raw/normalized values and registry-derived axes | `CurveLabQuoteAuthoring.tsx` owns display convention and scale locally; neither is accepted by the workspace application method | Frontend integration and real-browser tests verify preference changes issue no draft mutation, preserve the fingerprint, and leave admitted build evidence non-stale |

## DAL-18 post-disposition correction

The earlier version of this artifact overstated the frontend closure by citing
the canonicalizer component in isolation. At that point `Curves.tsx` rendered
the workspace and authoring component as siblings without supplying
`onCanonicalQuote`; the returned canonical bytes therefore did not reach the
instrument used by create, save, build, risk, or replay.

The corrected verification candidate now has an explicit single-instrument
target and a target epoch owned by `CurveLabWorkspace.tsx`. A successful
canonical response atomically replaces only that target's `raw_quote`.
Unselected targets, failed responses, repeated application, a changed family,
a changed same-family row, and any intervening draft edit cannot partially
write or redirect an old response. The browser regression traverses the
production React page and client, real FastAPI router and persistence services,
build/risk axes, and persisted replay. It compares `4 / PERCENT` with
`0.04 / DECIMAL` using the same stable instrument identity.

The later DAL-18 Medium review found two remaining implementation gaps in that
candidate. Target and draft epochs did not distinguish two concurrent requests
for the same target, and the UI displayed preference labels without projecting
canonical bytes through the approved exact formatter. The repaired candidate
now assigns every canonicalization a monotonic generation and ignores all state
effects from an older generation. It also exposes the existing backend
`render_quote` implementation through a closed, stateless
`POST /api/curve-lab/quote-renderings` route. The response contains only an
exact rendered string; display convention, scale, result, and rendering errors
remain local presentation state.

## Final DAL-18 blocker closure map

| Review blocker | Implementation closure | Focused regression |
|---|---|---|
| Canonicalizer output was displayed but not connected to the durable workspace instrument | `Curves.tsx` wires `CurveLabQuoteAuthoring.tsx` to the target/application boundary in `CurveLabWorkspace.tsx`; the application validates target epoch and family and copies only canonical `raw_quote` | `curves_quote_integration.test.tsx` covers production wiring and all write-isolation edges; `curve_lab_workspace.spec.ts` compares persisted documents, fingerprints, build/risk quote axes, native payload hashes, KRD matrices, and replay |
| Older same-target responses could overwrite a newer quote or error | `CurveLabQuoteAuthoring.tsx` captures request generation, family, target token, and authoring epoch; only the latest matching context may update canonical output, error, submitting state, or invoke the workspace application gate | Production-page deferred tests cover success/success and success/error inverse completion, cancellation, duplicate submission, input changes, family changes, and same-family row changes; Playwright proves `4/PERCENT` followed by `5/PERCENT` remains durable `0.05` when the first response finishes last |
| Display convention and scale were labels rather than an exact inverse projection | Closed rendering request/response schemas and `POST /api/curve-lab/quote-renderings` call the sole backend `render_quote` Decimal/half-even implementation; the frontend keeps a separate latest-render generation | Backend API tests cover scales `0/1/6/12`, signed ties, canonical zero, Future price points, closed errors, and no side-effect dependencies; component and real FastAPI/Vite browser tests assert `0.04/PERCENT/6 → 4.000000` without changing durable identity or stale state |
| Family changes retained terms from the previous family | `migrateCalibrationInstrument` in `CurveLabWorkspace.tsx` replaces currency/pair, raw quote, and the complete family-specific term object for every supported family | `curve_lab_workspace.test.tsx` asserts the exact wire document for all seven families; `tests/e2e/curve_lab_workspace.spec.ts` sends each one through the primary Family control to the real API and requires 201 |
| Client polling abandoned admitted work after roughly ten seconds | `waitForTerminal` has no client total deadline, publishes every admitted/polled state, and build/risk/import each retain the server run ID with an explicit resume action after transport failure | `curve_lab_workspace.test.tsx` proves a build still completes after more than ten seconds and resumes the same admitted ID after a network error |
| Non-finite JSON numbers could reach fingerprint serialization | `CurveLabWireModel` forbids non-finite floats and every Curve Lab solver/instrument float field uses an explicit finite constrained type | `test_curve_lab_lifecycle_api.py` covers all eight float fields with `NaN`, `Infinity`, and `1e999`, asserting 422 and zero draft/audit rows |
| Repository evidence showed only the earlier `Revise` state | This additive disposition preserves that history and records the later authoritative DAL-17 `Proceed with caveats` conclusion with route/schema/client/test traceability | Repository review evidence can now be audited without changing the historical critique |

This disposition records a repaired verification candidate only. It does not
authorize merging PR #265 or claim final review readiness; DAL-23 independent
verification, the documentation/CHANGELOG decision, and DAL-18 final re-review
remain separate gates in that order.
