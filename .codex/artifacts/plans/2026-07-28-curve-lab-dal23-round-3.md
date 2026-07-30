# Curve Lab DAL-23 Round 3 Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement this plan task-by-task.

**Goal:** Close the seven DAL-23 findings against `65ff3507` without changing the approved Revision 8 product scope.

**Architecture:** Keep Curve Lab jobs asynchronous and immutable, but reserve a bounded shared worker slot before publishing accepted work and persist a 15-minute deadline on every job. Project staged axes from the XCCY pair and native parameter layout, run mandatory central parameter parity for every AAD row, validate the immutable fixing snapshot against native trade-plan demand before queueing, make the visual authoring model own legal topology and dependency selection, and give calibration quote-bump admission a state-independent error precedence.

**Tech Stack:** C++20, pybind11, Python 3.13, FastAPI/Pydantic v2, SQLAlchemy/Alembic, pytest, React 18, TypeScript, Vitest, Playwright, Machinist.

## Global Constraints

- Work only on `feature/DAL-16-curve-lab`, starting at `65ff35075b12739610d8b0bf6958bb6c4b5d9569`.
- Preserve the exact seven-family registry, canonical quote behavior, native-only gateway boundary, immutable archives, Bag keys, closed schemas, Unicode handling, and existing V1 compatibility.
- Add a focused failing test before each production change, confirm the expected RED, implement the minimum change, and rerun GREEN.
- Do not add a changelog entry or open a pull request.

### Task 1: Bounded admission and persisted deadlines

**Files:**
- Add: `dal-web/backend/app/services/curve_lab_jobs.py`
- Modify: `dal-web/backend/app/services/curve_lab_lifecycle.py`
- Modify: `dal-web/backend/app/services/curve_risk.py`
- Modify: `dal-web/backend/app/routers/curve_lab.py`
- Modify: `dal-web/backend/app/schemas/curve_lab.py`
- Modify: `dal-web/backend/app/services/db/models.py`
- Modify: `dal-web/backend/app/services/db/store_db.py`
- Add: `dal-web/backend/migrations/versions/*_add_curve_lab_deadlines.py`
- Modify/Add focused backend tests.

- [x] Add a deterministic stress test that blocks both workers, accepts exactly 100 queued jobs, rejects the next submission with `429` and `Retry-After`, and proves rejection creates no job row.
- [x] Add deadline persistence/restart tests and a worker test proving an expired job becomes `TIMED_OUT` between native calls without cancelling an in-flight native call.
- [x] Implement a reservation-based two-worker/100-queued executor shared by build, import, and risk. Release reservations on persistence/submit failure and worker completion.
- [x] Persist `deadline_at = created_at + 15 minutes` on all three job types, expose it in responses, and check it between native calls.
- [x] Rerun the focused queue/deadline tests GREEN.

### Task 2: Native staged ordering and axes

**Files:**
- Modify: `dal-web/backend/app/services/curve_lab_lifecycle.py`
- Modify: `dal-web/backend/app/services/dal_gateway.py`
- Modify/Add: `dal-web/backend/tests/test_curve_lab_build_modes.py`
- Modify/Add: `dal-web/backend/tests/test_curve_lab_risk_api.py`

- [x] Add a reversed-declaration staged USD/EUR fixture whose semantic keys contain no `domestic`/`foreign` tokens; assert quote and parameter blocks are USD, EUR, basis with native representation-local coordinates.
- [x] Replace string-token/declaration-order inference with one resolved order derived from the XCCY pair, declaration roles/currencies, and native archive parameter layout.
- [x] Persist the resolved component/stage plan and use it for both quote and parameter axes.
- [x] Rerun the focused axis tests GREEN, including Bag-key reorder invariance.

### Task 3: Mandatory AAD parity and accounting

**Files:**
- Modify: `dal-web/backend/app/services/curve_risk.py`
- Modify: `dal-web/backend/app/schemas/curve_lab.py`
- Modify/Add: `dal-web/backend/tests/test_curve_lab_risk_api.py`

- [x] Replace the zero-bump successful-AAD test with signed central-parity pass, mismatch/fallback, forbidden-fallback, bump-failure isolation, and concurrent-run tests.
- [x] Make `N_param = I_node * 2P` and charge `2PT` price evaluations for every requested node-sensitivity run.
- [x] Build each `+/-` parameter context once, price every admitted trade in it, compare every eligible AAD row using signed absolute/relative tolerances, and persist parity evidence.
- [x] Reuse the same central row for ineligible or parity-failed `ALLOW` fallback; publish no sensitivity value on forbidden parity failure.
- [x] Rerun the focused estimator/parity tests GREEN.

### Task 4: Fixing compatibility admission

**Files:**
- Modify: `dal-python/src/bindings/curve.cpp`
- Modify: `dal-web/backend/app/services/dal_gateway.py`
- Modify: `dal-web/backend/app/services/curve_risk.py`
- Modify: `dal-web/backend/app/schemas/curve_lab.py`
- Modify: `dal-web/frontend/src/api/client.ts`
- Modify/Add focused native, binding, backend, and OpenAPI tests.

- [x] Add a native-plan-backed gateway projection of required fixing index/timestamp/kind/unit per trade.
- [x] Add snapshot units to the closed input contract and test RATE/FX kind and unit mismatches, missing required keys, extra observations, and the `FX`-for-`USD-SOFR` reviewer repro.
- [x] Validate the immutable snapshot against all admitted trade plans before reserving a queue slot or publishing a run row.
- [x] Prove base, parameter bumps, quote bumps, and AAD all receive the same persisted snapshot bytes/hash.
- [x] Rerun focused tests GREEN and regenerate OpenAPI.

### Task 5: Visual-first topology and dependency authoring

**Files:**
- Modify: `dal-web/frontend/src/components/CurveLabWorkspace.tsx`
- Modify: `dal-web/frontend/src/styles.css` only if required for responsive layout.
- Modify/Add: `dal-web/frontend/tests/unit/curve_lab_workspace.test.tsx`
- Modify: `dal-web/frontend/tests/e2e/curve_lab_workspace.spec.ts`

- [x] Add unit tests for declaration add/edit/remove, dependency-version selection, legal topology projection on each mode change, keyboard labels, and narrow viewport behavior.
- [x] Implement controlled declaration CRUD and version dependency controls that update the same canonical draft object used by Advanced JSON.
- [x] Make mode changes create/adjust the minimum legal declaration and instrument topology.
- [x] Change the real Playwright stale/save workflow to edit the visible quote input without opening Advanced JSON.
- [x] Run focused Vitest and guarded real-backend Playwright tests GREEN.

### Task 6: Deterministic calibration 202 follow-up

**Files:**
- Modify: `dal-web/backend/app/services/calibrations.py`
- Modify/Add: `dal-web/backend/tests/test_calibration_api.py`
- Modify/Add restart persistence tests if needed.

- [x] Add a 100-submission/immediate-quote-bump test that observes one stable error code and repeat it across store restart.
- [x] Give statically unavailable effective-inverse requests precedence over mutable worker state so the immediate result is deterministic.
- [x] Rerun focused calibration API/restart tests GREEN.

### Task 7: Generated and full-diff hygiene

**Files:**
- Modify: Revision 8 manifest/spec whitespace.
- Modify: Machinist source/templates responsible for `MG_DiscountPWC*` and `MG_RateInstrumentType*`.
- Regenerate only through the repository generator target.

- [x] Confirm `git diff --check 98f7b659..HEAD` is RED with the seven reported files.
- [x] Remove trailing whitespace/extra EOF blanks at the owning template/source and regenerate.
- [x] Run `dal_check_generated` twice; the second run must write zero files.
- [x] Confirm `git diff --check 98f7b659..HEAD` and current `git diff --check` are both clean.

### Task 8: Full verification and handoff

- [x] Run focused RED/GREEN commands recorded above.
- [x] Run backend Ruff and all backend tests.
- [x] Build native/public/Python targets, run `dal_check_generated`, and run all CTest tests.
- [x] Run all frontend unit tests, production build, all guarded Playwright tests, and the quote-bump boundary checker.
- [x] Run deterministic queue stress and calibration 100-repeat/restart tests once more.
- [x] Review the full diff, commit intentional files, verify a clean worktree, post one concise Multica result comment, and move DAL-22 to `in_review`.
