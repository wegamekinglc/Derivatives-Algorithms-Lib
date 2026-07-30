# Curve Lab Revision 8 Implementation Plan

> **Required execution skill:** use `superpowers:executing-plans` to implement this plan task by task with red/green checkpoints.

**Goal:** Deliver the reviewed Curve Lab foundation on `feature/DAL-16-curve-lab`: exact quote authoring normalization, native JSON/PWC persistence, bytes-first Python archive access, generated HTTP/client contracts, and a DAL-WEB authoring surface that keeps presentation preferences outside durable financial data.

**Architecture:** Keep existing V1 calibration resources compatible. Add one immutable seven-family registry in the backend schema layer and one exact-decimal service that owns parsing, transforms, native-range checks, risk-bump representability, and display projection. Expose that service through a stateless authoring endpoint; all durable V2 instrument DTOs accept canonical `raw_quote` strings only. Repair native archive serialization at its shared boundary, then expose that boundary to Python using exact `bytes` lengths. The frontend derives its authoring family choices from the same committed OpenAPI contract and replaces percent input with the returned canonical bytes before it can become financial state.

**Tech Stack:** C++17, RapidJSON, Machinist, pybind11, Python 3.13, FastAPI/Pydantic v2, Decimal, pytest, React 18, TypeScript, Vitest.

---

## Task 1: Preserve the approved package and pin the contract

**Files:**
- Add: `.codex/artifacts/specs/curve-lab-dal-web-v0.5.md`
- Add: `.codex/artifacts/designs/curve-lab-revision-8/curve-lab-technical-design.md`
- Add: `.codex/artifacts/designs/curve-lab-revision-8/01-curve-builder-dal-web.png`
- Add: `.codex/artifacts/designs/curve-lab-revision-8/MANIFEST.md`
- Add: `.codex/artifacts/designs/curve-lab-revision-8/pricing-risk.mmd`
- Add: `.codex/artifacts/designs/curve-lab-revision-8/versions.mmd`
- Add: `.codex/artifacts/critiques/curve-lab-technical-design-review.md`

1. Reuse the exact approved attachment bytes.
2. Verify every preserved file against the downloaded SHA-256.
3. Confirm the implementation branch remains `feature/DAL-16-curve-lab`.

## Task 2: Exact quote registry and authoring normalization

**Files:**
- Add: `dal-web/backend/app/schemas/curve_lab.py`
- Modify: `dal-web/backend/app/schemas/__init__.py`
- Add: `dal-web/backend/app/services/quote_canonicalization.py`
- Add: `dal-web/backend/tests/test_curve_lab_quote_canonicalization.py`

1. Write failing registry and table-driven lexical/canonicalization tests.
2. Run `uv run pytest tests/test_curve_lab_quote_canonicalization.py -q` and confirm missing-module failures.
3. Add the exact ordered registry `DEPOSIT,FRA,FUTURE,OIS,IRS,BASIS_SWAP,XCCY`, closed request/response DTOs, and stable error envelope.
4. Implement exact integer/scale parsing, `/100`, `1-p/100`, canonical serialization, native-range checks, and distinct fixed-bump conversion.
5. Add display inversion with half-even rounding while keeping display convention/scale out of financial DTOs.
6. Rerun the focused test green and refactor only while green.

## Task 3: Stateless authoring API and committed OpenAPI

**Files:**
- Add: `dal-web/backend/app/routers/curve_lab.py`
- Modify: `dal-web/backend/app/routers/__init__.py`
- Modify: `dal-web/backend/app/main.py`
- Add: `dal-web/backend/tests/test_curve_lab_api.py`
- Add: `dal-web/backend/tests/test_curve_lab_v1_success_registry.py`
- Modify: `dal-web/backend/openapi/dal-web.openapi.json`

1. Write failing API tests for percent/decimal equivalence, Future normalization, override rejection, stable errors, and zero store/native side effects.
2. Add `GET /api/curve-lab/capabilities` and `POST /api/curve-lab/quote-canonicalizations`.
3. Ensure the response is the sole handoff into canonical durable instrument state.
4. Regenerate OpenAPI and assert the exact family order, string quote schema, read-only derived members, and `additionalProperties=false`.
5. Rerun focused API and OpenAPI tests green.

## Task 4: Native canonical JSON and DiscountPWC archive

**Files:**
- Modify: `dal-cpp/dal/storage/json.hpp`
- Modify: `dal-cpp/dal/storage/json.cpp`
- Modify: `dal-cpp/dal/curve/ycimp.cpp`
- Modify: `dal-cpp/dal/curve/ycconst.cpp`
- Modify: generated `dal-cpp/dal/auto/MG_DiscountPWC_v1_*.inc`
- Modify: `dal-cpp/tests/storage/test_json.cpp`
- Add: `dal-cpp/tests/curve/test_ycconst_archive.cpp`

1. Add failing tests for pointer-plus-length parsing, trailing bytes/NUL, escaped strings, negative zero/17-digit doubles, and PWC/base round trips.
2. Run focused CTest filters and confirm expected failures.
3. Add `JSONReadOptions_`, exact range parsing, UTF-8/NUL/size validation, centralized string escaping, and finite `to_chars(max_digits10)` output.
4. Add the Machinist `DiscountPWC_v1` schema, regenerate, implement passive-double reader/writer validation, and preserve recursive bases.
5. Rerun focused native tests green and run `dal_check_generated`.

## Task 5: Python hierarchy and bytes-first archive bridge

**Files:**
- Modify: `dal-python/src/bindings/global.cpp`
- Modify: `dal-python/src/bindings/curve.cpp`
- Modify: `dal-python/tests/test_curve_web_contract.py`

1. Add failing hierarchy/archive tests for `Storable_`, `YCComponent_`, `DiscountCurve_`, `YieldCurve_`, `CurveBlock_`, `Bag_`, PWC, exact bytes, embedded NUL, and recursive base round trips.
2. Extend the existing `Storable_` binding once; register base-before-derived classes with compile-time hierarchy guards.
3. Implement `_StorableToJson`, `_StorableFromJson`, `_BagNew`, and `_BagContents` with exact `PyBytes_AsStringAndSize` lengths and scoped GIL release.
4. Rerun the focused Python contract tests green.

## Task 6: Frontend canonical authoring path

**Files:**
- Modify: `dal-web/frontend/src/api/client.ts`
- Add: `dal-web/frontend/src/curves/curveLabRegistry.ts`
- Add: `dal-web/frontend/src/components/CurveLabQuoteAuthoring.tsx`
- Modify: `dal-web/frontend/src/pages/Curves.tsx`
- Modify: `dal-web/frontend/src/styles.css`
- Add: `dal-web/frontend/tests/unit/curve_lab_quote_authoring.test.tsx`
- Modify: `dal-web/frontend/tests/unit/api_client.test.ts`

1. Write failing component/client tests for exact family order, compatible conventions, percent/decimal replacement with canonical bytes, Future price details, keyboard interaction, and no binary-number financial conversion.
2. Add typed client methods and a generated registry projection checked against OpenAPI.
3. Add the accessible visual authoring mode beside Advanced JSON, using the approved terminal design language and semantic CSS only.
4. Keep display preference local; only adapter results may update financial quote state.
5. Rerun frontend unit tests and production build green under Node 20.20.2.

## Task 7: Replay/100x regression and compatibility verification

**Files:**
- Add: `dal-web/backend/tests/test_curve_lab_quote_replay.py`
- Modify: `.github/scripts/check_frontend_quote_bump_boundary.py`

1. Add a failing restart-style fixture that compares `4/PERCENT` with `0.04/DECIMAL` across canonical DTO bytes, fingerprint input, quote-axis evidence, and `+0.0001` bump conversion.
2. Add the minimum shared evidence serializer needed by authoring/build callers; do not add persistence or queues to the stateless adapter itself.
3. Make the fixture green and assert `0.01` is rejected as the 100-times regression.
4. Run full backend, frontend, native, and Python suites.

## Task 8: Final scope review and handoff commit

1. Read every changed source/test/artifact and compare it with Revision 8, the approved caveat, repository style, and non-goals.
2. Confirm V1 calibration schemas/routes/snapshots remain compatible and no Gamma/Vega/CS01, binary archive, approval workflow, or unrelated refactor entered the diff.
3. Run `git diff --check`, generated-file validation, full native CTest, full Python/backend pytest, frontend Vitest, and frontend build.
4. Commit intentional files on `feature/DAL-16-curve-lab`.
5. Report exact commits, changed-file scope, RED/GREEN commands, full verification, compatibility notes, and any independently verifiable residual risk.
