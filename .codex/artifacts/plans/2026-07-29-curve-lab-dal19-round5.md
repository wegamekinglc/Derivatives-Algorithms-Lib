# Curve Lab DAL-19 Round-5 Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close DAL-19's six review findings at `97a71c18` without changing the approved Curve Lab product scope.

**Architecture:** Preserve the queued build record as the sole worker snapshot, serialize SQLite publication predicates with `BEGIN IMMEDIATE`, and enforce decoded-string validity once at the native RapidJSON document boundary. Keep PostgreSQL row locking, the synchronous store seam, native archive bytes, HTTP shapes, and all existing Curve Lab behavior unchanged.

**Tech Stack:** C++17, RapidJSON, pybind11, Python 3.13, FastAPI, SQLAlchemy 2, SQLite/PostgreSQL, React/Vite/Vitest, Google Test.

## Global Constraints

- Revision 8 and its approved hashes remain controlling; restore approved bytes rather than reapprove artifacts.
- Apply strict red-green-refactor and do not weaken existing tests.
- Do not add bond, Gamma, Vega, CS01, binary serialization, approval workflow, or unrelated refactoring.
- Do not create a PR; deliver one clean candidate commit for DAL-23 and DAL-19.
- Curve Lab endpoints are `async def`; synchronous persistence remains behind the existing store seam.
- New C++ tests use `ASSERT_*`, never `EXPECT_*`.

---

### Task 1: Immutable queued build snapshots

**Files:**
- Modify: `dal-web/backend/tests/test_curve_lab_lifecycle_api.py`
- Modify: `dal-web/backend/app/services/curve_lab_lifecycle.py`

**Interfaces:**
- Consumes: the canonical queued build record persisted by `create_build_run`.
- Produces: one persisted queued-record read in `_execute_build_run` and terminal records derived only from that snapshot.

- [x] Add a gateway barrier regression that queues revision 1, updates the draft to revision 2 while native work is blocked, releases the old worker, and asserts the run still records revision 1, fingerprint 1, request 1, and a later terminal timestamp.
- [x] Run the focused test and confirm it fails because the worker rereads revision 2 and reuses `created_at`.
- [x] Load the queued snapshot once in the worker, remove the current-draft read, derive failure/timeout/success records from the queued identity, and stamp `_now()` only when the terminal state is written.
- [x] Re-run the concurrency regression and the nearby lifecycle tests.

### Task 2: SQLite publication transaction

**Files:**
- Modify: `dal-web/backend/tests/test_curve_lab_lifecycle_api.py`
- Modify: `dal-web/backend/app/services/db/store_db.py`

**Interfaces:**
- Consumes: `DbStore.publish_curve_lab_version` and its existing draft/run/dependency predicates.
- Produces: a SQLite write transaction acquired before the first predicate read; PostgreSQL retains `SELECT ... FOR UPDATE`.

- [x] Add a deterministic two-store barrier regression in which a draft update owns the SQLite write order before old-revision publication; assert the publisher returns a revision conflict and inserts no version.
- [x] Run the focused test and confirm the deferred transaction selects before the prior draft update commits.
- [x] Execute `BEGIN IMMEDIATE` as the first SQLite publication statement, before selecting draft, run, idempotency, or dependency rows.
- [x] Re-run the race plus existing concurrent publication and dependency-archive tests.

### Task 3: Native/Python decoded JSON string safety

**Files:**
- Modify: `dal-cpp/tests/storage/test_json.cpp`
- Modify: `dal-python/tests/test_curve_web_contract.py`
- Modify: `dal-cpp/dal/storage/json.cpp`

**Interfaces:**
- Consumes: the length-aware RapidJSON document produced by `JSON::ReadString`.
- Produces: one recursive decoded-document validation step before `XDocView_` or any archive reader.

- [x] Add C++ regressions for escaped U+0000 in keys and values, invalid/lone surrogate input, and a valid supplementary Unicode read-write-read round trip with stable error codes.
- [x] Add the same escaped-NUL and supplementary round-trip contract through compiled `_StorableFromJson` / `_StorableToJson`.
- [x] Run the focused native/Python tests and confirm escaped U+0000 is accepted on read.
- [x] Recursively validate every decoded object key and string value with explicit lengths; map invalid Unicode parse failures to a stable archive-string error.
- [x] Rebuild and re-run both focused suites.

### Task 4: Repository and DAL-WEB standards

**Files:**
- Modify: `dal-web/backend/tests/test_curve_lab_api.py`
- Modify: `dal-web/backend/app/routers/curve_lab.py`
- Modify: `dal-web/frontend/src/styles.css`
- Modify: `dal-cpp/tests/curve/test_ratecashflowpricing.cpp`

**Interfaces:**
- Consumes: registered Curve Lab FastAPI routes and rendered Curve Lab workspace styles.
- Produces: coroutine endpoints and solid, shadow-free Curve Lab declarations.

- [x] Add a route contract that asserts every `/api/curve-lab` endpoint is a coroutine, then run it RED.
- [x] Convert all new Curve Lab handlers to `async def` without changing HTTP contracts and re-run GREEN.
- [x] Add a focused stylesheet regression for no Curve Lab gradient or box shadow, run RED, remove only the two offending declarations, and run GREEN.
- [x] Replace the two review-identified `EXPECT_EQ` assertions with `ASSERT_EQ`.

### Task 5: Approved provenance and public history

**Files:**
- Modify: `.codex/artifacts/specs/curve-lab-dal-web-v0.5.md`
- Modify: `.codex/artifacts/designs/curve-lab-revision-8/MANIFEST.md`
- Modify: `CHANGELOG.md`

**Interfaces:**
- Consumes: hashes recorded in the approved manifest and critique.
- Produces: byte-identical approved spec/manifest plus current public capability history.

- [x] Record the current hash mismatch as RED.
- [x] Restore the six approved Markdown line-ending spaces and verify the spec, manifest, design, Mermaid, critique, and PNG hashes match the critique/manifest.
- [x] Add concise 2026-07 changelog entries for native seven-family pricing/archive/AAD surfaces and the immutable Curve Lab DAL-WEB workflow.

### Task 6: Full verification and candidate

**Files:**
- Verify all files changed by Tasks 1–5.

- [x] Run backend Ruff and full pytest.
- [x] Regenerate OpenAPI and prove the committed snapshot has no drift.
- [x] Build native/public/Python, run `dal_check_generated`, full CTest, and full Python pytest.
- [x] Run frontend Vitest, production build, and real-router Playwright.
- [x] Run quote-boundary, artifact-hash, generated-drift, mutable-path `git diff --check`, and working-tree checks.
- [x] Review the complete diff against the six findings, commit one candidate, then repeat the drift/worktree checks.
