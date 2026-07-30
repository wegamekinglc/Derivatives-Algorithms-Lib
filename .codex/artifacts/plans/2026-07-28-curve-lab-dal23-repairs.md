# Curve Lab DAL-23 Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Repair the five independently reproduced DAL-23 blockers at `1a636580` without expanding Revision 8 scope.

**Architecture:** Keep the existing V2 lifecycle and native-only gateway boundary, but make build admission resolve immutable dependency snapshots before native work and make the gateway dispatch each approved build mode to the corresponding native calibration surface. Put hostile archive parsing in a dedicated non-DAL preflight module, keep native diagnostics private at the risk response boundary, and drive browser acceptance through the real production routers and canned native gateway instead of mocked HTTP routes.

**Tech Stack:** Python 3.13, FastAPI/Pydantic v2, SQLAlchemy, pybind11 DAL bindings, pytest, React 18, TypeScript, Playwright.

## Global Constraints

- Work only on `feature/DAL-16-curve-lab`, starting at `1a636580dcf007c594b25f1a76f83e01d84bb678`.
- Keep `DEPOSIT,FRA,FUTURE,OIS,IRS,BASIS_SWAP,XCCY` as the exact ordered product registry.
- Preserve canonical decimal RATE/SPREAD quotes and fixed `+0.0001` raw/normalized bumps.
- Keep V1 routes compatible and route every native call through `dal_gateway.py`.
- Do not add bonds, Gamma, Vega, CS01, binary serialization, approval workflow, or a new PR.
- Add one focused failing test before each production change; run it RED, implement minimally, rerun GREEN, then refactor while green.

---

### Task 1: Calibrated build-mode routing and immutable dependency admission

**Files:**
- Modify: `dal-web/backend/app/schemas/curve_lab.py`
- Modify: `dal-web/backend/app/services/store.py`
- Modify: `dal-web/backend/app/services/db/store_db.py`
- Modify: `dal-web/backend/app/services/curve_lab_lifecycle.py`
- Modify: `dal-web/backend/app/services/dal_gateway.py`
- Modify: `dal-web/backend/tests/fake_dal.py`
- Modify: `dal-web/backend/tests/test_curve_lab_lifecycle_api.py`
- Add: `dal-web/backend/tests/test_curve_lab_build_modes.py`
- Modify: `dal-web/backend/openapi/dal-web.openapi.json`

**Interfaces:**
- Produces: closed modes `SINGLE | MULTI_CURVE | STAGED_XCCY | JOINT_XCCY`.
- Produces: `CurveLabDependencyManifestEntryV2(version_id, content_hash, root_kind)`.
- Produces: `StoreProtocol.resolve_curve_lab_versions(version_ids)` as one locked snapshot read.
- Produces: `DalGateway.build_curve_lab_archive(document, dependencies)` with mode-specific native dispatch.

- [ ] **Step 1: Write focused routing and dependency tests**

Add a recording DAL double that rejects passive `DiscountPWC_New` fallback for non-single modes and records calls to `CalibrateMultiCurveBundle`, `CalibrateXccyMarket`, and `CalibrateJointXccyMarket`. Use distinct declaration instruments:

```python
discount_quote = _instrument(DISCOUNT_KEY, maturity="2027-01-15", quote="0.01")
projection_quote = _instrument(PROJECTION_KEY, maturity="2028-01-15", quote="0.02")
curves = gateway._curve_lab_passive_curves(_multi_document(discount_quote, projection_quote))
assert recorded.stage_instruments == {
    DISCOUNT_KEY: [("2027-01-15", 0.01)],
    PROJECTION_KEY: [("2028-01-15", 0.02)],
}
assert set(curves) == {DISCOUNT_KEY, PROJECTION_KEY}
```

Add API tests proving an unknown or archived dependency yields one persisted `FAILED` build with `DEPENDENCY_VERSION_NOT_FOUND` or `DEPENDENCY_VERSION_ARCHIVED`, no native call, and no unresolved ID-only manifest. Add a successful dependency test that asserts ID, immutable content hash, and root kind.

- [ ] **Step 2: Run the focused tests RED**

Run:

```bash
cd dal-web/backend
uv run --no-sync pytest \
  tests/test_curve_lab_build_modes.py \
  tests/test_curve_lab_lifecycle_api.py::test_build_rejects_missing_dependency_before_native_work \
  -q
```

Expected: failures show non-single declarations share one passive PWC and missing dependencies reach `SUCCEEDED`.

- [ ] **Step 3: Implement locked dependency resolution**

Add a store operation that returns all requested version rows under the memory `RLock` or one SQL transaction with `SELECT ... FOR UPDATE`. In `create_build_run`, resolve requested IDs in request order, reject missing/archived/unverifiable rows before gateway work, and persist:

```python
{
    "version_id": version["id"],
    "content_hash": version["native_payload_hash"],
    "root_kind": version["root_kind"],
}
```

Persist dependency admission failures as immutable failed-build evidence with a stable sanitized error.

- [ ] **Step 4: Implement native mode dispatch**

Split `_curve_lab_passive_curves` into one dispatcher and focused helpers:

```python
if mode == "SINGLE":
    return self._curve_lab_single_curves(document, dependency_curves)
if mode == "MULTI_CURVE":
    return self._curve_lab_multi_curves(document, dependency_curves)
if mode == "STAGED_XCCY":
    return self._curve_lab_staged_xccy_curves(document, dependency_curves)
if mode == "JOINT_XCCY":
    return self._curve_lab_joint_xccy_curves(document, dependency_curves)
raise ValueError(f"unsupported Curve Lab build mode {mode!r}")
```

For every declaration, select only included instruments whose `terms.component_key` equals that declaration. Sequential multi-curve builds a `MultiCurveCalibrationSpec_` and calls `CalibrateMultiCurveBundle`; staged XCCY builds domestic/foreign blocks then calls `CalibrateXccyMarket`; joint XCCY builds the native joint declaration/spec and calls `CalibrateJointXccyMarket`. Preserve component keys when extracting result curves into the archive Bag.

- [ ] **Step 5: Run focused tests GREEN and regenerate OpenAPI**

Run the RED command again, then:

```bash
cd dal-web/backend
uv run --no-sync pytest tests/test_curve_lab_lifecycle_api.py tests/test_dal_gateway.py -q
uv run --no-sync python scripts/generate_openapi.py
```

- [ ] **Step 6: Commit**

```bash
git add dal-web/backend/app dal-web/backend/tests dal-web/backend/openapi/dal-web.openapi.json
git commit -m "fix(curve-lab): calibrate build modes and dependencies"
```

### Task 2: Recursive archive preflight and import limits

**Files:**
- Add: `dal-web/backend/app/services/archive_preflight.py`
- Modify: `dal-web/backend/app/services/curve_lab_lifecycle.py`
- Modify: `dal-web/backend/app/routers/curve_lab.py`
- Add: `dal-web/backend/tests/test_curve_lab_archive_preflight.py`
- Modify: `dal-web/backend/tests/test_curve_lab_lifecycle_api.py`

**Interfaces:**
- Produces: `ArchivePreflightLimits`, `ArchivePreflightResult`, `ArchivePreflightError`.
- Produces: `preflight_archive(payload, content_encoding=None)` returning exact expanded bytes, parsed root tag, and byte/value/reference evidence.

- [ ] **Step 1: Write table-driven adversarial tests**

Cover 10 MiB exact/one-above wire bytes, gzip expansion at 50 MiB/one-above, malformed JSON, trailing non-whitespace, NUL, invalid UTF-8, duplicate object keys, depth 64/65, value count, string length, numeric-array length, archive object/reference limits, duplicate tags, dangling references, reference cycles, unknown fields/tags, and wrong handle positions. Every rejection asserts its exact code and a native-reader counter of zero.

- [ ] **Step 2: Run tests RED**

Run:

```bash
cd dal-web/backend
uv run --no-sync pytest tests/test_curve_lab_archive_preflight.py -q
```

Expected: module-not-found first, then current lifecycle collapses parser failures to `IMPORT_ROOT_TYPE_FORBIDDEN`.

- [ ] **Step 3: Implement bounded byte and JSON admission**

Enforce in order: wire cap, bounded gzip expansion, expanded cap, empty/UTF-8/NUL, duplicate-aware single-document JSON parse, trailing-byte detection, iterative depth/value/string/numeric-array traversal, archive object/reference graph collection, duplicate/dangling/cycle checks, and the approved root/child field-position allowlist. Do not call DAL or create a visible version in this module.

- [ ] **Step 4: Integrate preflight and persist classified failures**

Pass `Content-Encoding` from the router. On any `ArchivePreflightError`, persist one failed import job with `phase=PREFLIGHT`, exact compressed/expanded lengths available at failure, stable error code/path/offset, and no version. Pass the exact expanded bytes—not the compressed request—to `gateway.import_curve_lab_archive`.

- [ ] **Step 5: Run focused tests GREEN**

Run:

```bash
cd dal-web/backend
uv run --no-sync pytest \
  tests/test_curve_lab_archive_preflight.py \
  tests/test_curve_lab_lifecycle_api.py -q
```

- [ ] **Step 6: Commit**

```bash
git add dal-web/backend/app/services/archive_preflight.py \
  dal-web/backend/app/services/curve_lab_lifecycle.py \
  dal-web/backend/app/routers/curve_lab.py \
  dal-web/backend/tests
git commit -m "fix(curve-lab): enforce archive import preflight"
```

### Task 3: Partial-pricing diagnostic sanitization

**Files:**
- Modify: `dal-web/backend/app/services/curve_risk.py`
- Modify: `dal-web/backend/tests/test_curve_lab_risk_api.py`

**Interfaces:**
- Produces: `_native_pricing_error()` returning a stable public message that never includes native exception text, paths, source lines, or C++ function names.

- [ ] **Step 1: Add a failing partial-success leak regression**

Make the native double return one successful trade and one failure whose error contains `/absolute/path/ratecashflowpricing.cpp:101`, a C++ signature, and a secret marker. Assert the success row remains, the failure row remains, and none of the native text occurs anywhere in response bytes.

- [ ] **Step 2: Run RED**

Run:

```bash
cd dal-web/backend
uv run --no-sync pytest \
  tests/test_curve_lab_risk_api.py::test_base_pricing_partial_failure_sanitizes_native_diagnostics \
  -q
```

- [ ] **Step 3: Implement the minimum boundary fix**

Replace direct `str(native["error"])` publication with a stable public message such as `Native trade pricing failed.` Keep missing-fixing and dependency-key arrays as the actionable per-trade evidence.

- [ ] **Step 4: Run GREEN and commit**

```bash
cd dal-web/backend
uv run --no-sync pytest tests/test_curve_lab_risk_api.py -q
git add dal-web/backend/app/services/curve_risk.py dal-web/backend/tests/test_curve_lab_risk_api.py
git commit -m "fix(curve-lab): sanitize native pricing failures"
```

### Task 4: Real Playwright lifecycle acceptance

**Files:**
- Modify: `dal-web/frontend/tests/e2e/curve_lab_workspace.spec.ts`
- Modify: `dal-web/frontend/src/components/CurveLabWorkspace.tsx` only if the real flow exposes an accessibility or state defect

**Interfaces:**
- Consumes: production Curve Lab API client and routers through `DAL_PLAYWRIGHT_TEST_BACKEND=1`.
- Produces: browser assertions for stale invalidation, save/load, valid and malformed import, Clone, Export, and Archive actions.

- [ ] **Step 1: Replace HTTP-route mocks with real backend actions**

Skip only when `DAL_PLAYWRIGHT_TEST_BACKEND != 1`. Create/build/publish through the UI, edit the real JSON textarea and save to observe `rebuild required`, assert publish disabled until rebuild, export with Playwright's download event, import the downloaded bytes, submit malformed bytes and assert the classified API error, click Clone and verify a new draft, then click Archive and verify the version disappears.

- [ ] **Step 2: Run RED against the real test backend**

Run:

```bash
cd dal-web/frontend
DAL_PLAYWRIGHT_TEST_BACKEND=1 npm run test:e2e -- tests/e2e/curve_lab_workspace.spec.ts
```

Expected: current mock-only test does not exercise the required production actions; any surfaced UI defect is recorded before modification.

- [ ] **Step 3: Make only required UI corrections and rerun GREEN**

Keep all API calls in `client.ts`; do not add financial logic to React. Rerun the focused Playwright file, then all e2e tests with the real canned backend.

- [ ] **Step 4: Commit**

```bash
git add dal-web/frontend/tests/e2e/curve_lab_workspace.spec.ts dal-web/frontend/src/components/CurveLabWorkspace.tsx
git commit -m "test(curve-lab): exercise durable browser lifecycle"
```

### Task 5: Fresh full verification and handoff

**Files:**
- Modify only generated OpenAPI if fresh generation requires it.

- [ ] **Step 1: Review the diff against all five DAL-23 findings**

Confirm the non-single passive fallback is unreachable, dependency failures make no native call, every preflight failure makes no native-reader call, public risk bytes contain no native diagnostic, and Playwright uses no Curve Lab route mocks.

- [ ] **Step 2: Run backend verification**

```bash
cd dal-web/backend
uv run --no-sync ruff check app tests
uv run --no-sync pytest -q
```

- [ ] **Step 3: Run native/public/Python verification**

```bash
cmake --build build/Release-linux -j2
cmake --build build/Release-linux --target dal_check_generated -j2
ctest --test-dir build/Release-linux --output-on-failure -j2
```

- [ ] **Step 4: Run frontend and boundary verification**

```bash
export PATH=/home/wegamekinglc/.nvm/versions/node/v20.20.2/bin:$PATH
cd dal-web/frontend
npm test
npm run build
DAL_PLAYWRIGHT_TEST_BACKEND=1 npm run test:e2e
cd ../..
python3 .github/scripts/check_frontend_quote_bump_boundary.py
```

- [ ] **Step 5: Verify repository state and commit any generated closure**

```bash
git diff --check
git status --short
git log --oneline 1a636580..HEAD
git diff --name-only 1a636580..HEAD
```

The final working tree must be clean. Do not open a PR.
