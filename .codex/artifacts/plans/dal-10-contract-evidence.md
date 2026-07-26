# DAL-10 Contract Evidence Completion Plan

> **For dal-implementer:** Execute every task with red-green-refactor. The frozen
> spec, API notes, and critique remain read-only.

**Goal:** Close the independent tester Block for draft PR #259 with executable,
production-boundary evidence for every frozen fixture, then obtain raw
Release/native performance artifacts from the repository-marked runner.

**Architecture:** Keep the public C++/Python/REST contracts unchanged. Add a
backend contract-fixture layer that drives real FastAPI routes, the real
SQLAlchemy store, and the production gateway seams. Add typed test-only
dependency instrumentation at existing admission, lifecycle, evidence,
serialization, and Store boundaries so tests observe exact calls, identities,
transactions, and events without duplicating product logic. Add compiled
extension probes for GIL/lifetime behavior and a two-process protocol for
SQLite reconstruction. Performance measurement is a separate strict
Release/native workflow which uploads raw JSON bound to the tested head.

**Tech stack:** C++17/pybind11, Python 3.13, FastAPI/Pydantic v2, SQLAlchemy,
pytest, GitHub Actions.

---

## Task 1: Production API fixture builders

**Files:**

- Add: `dal-web/backend/tests/calibration_contract_fixtures.py`
- Add: `dal-web/backend/tests/test_calibration_acceptance_api.py`
- Modify only if a failing observation proves a product gap:
  `dal-web/backend/app/services/calibrations.py`
- Modify only if a failing observation proves a product gap:
  `dal-web/backend/app/services/dal_gateway.py`

**RED:** Add `FIX-B1` through `FIX-B9`, `FIX-CB1-*`,
`FIX-PWLF-MATRIX-METADATA-200`, `FIX-INPUT-ZERO-PLANNER-PRECEDENCE`, and
`FIX-JOINT-FREE-PARAMETER-LIMIT-200` cases which submit through production
routes. Cover staged/joint POST→poll→completed, exact 99/100/101 and
200/201/202 boundaries, four matrix-flag permutations, serialization
candidate/final/GET counts, submitted/canonical order, seed vectors, and exact
downstream call counts. Run each new test first and record its expected failure.

**GREEN:** Fix only the production boundary demonstrated by each failing case.
Do not replace route observations with helper-only assertions. Re-run the
focused test after each change, then the whole new API file.

## Task 2: Typed recorder and fault matrix

**Files:**

- Add: `dal-web/backend/tests/test_calibration_acceptance_faults.py`
- Modify: `dal-web/backend/app/services/calibrations.py`
- Modify: `dal-web/backend/app/services/dal_gateway.py`
- Modify: `dal-web/backend/app/services/store.py`
- Modify: `dal-web/backend/app/services/db/store_db.py`

**RED:** Add production-path fixtures for `FIX-B5-LOCK-HANDSHAKE`,
`FIX-WORKER-EXECUTION-IDENTITY`,
`FIX-PERSISTED-EXPECTED-IDENTITY-INTEGRITY`,
`FIX-BK06-LOCK-TO-NATIVE-EVIDENCE`, and
`FIX-BK07-FROZEN-INTEGRITY-ERROR-EVIDENCE`. Assert the complete shared event
sequence, poll/health barriers, exact callback/verifier/conversion/timer/native
counts, exception/carrier/bytes object identity, nested mutation/TOCTOU
isolation, Store original-bytes ownership, and every factory/projection/bytes/
commit/fallback fault row.

**GREEN:** Introduce one internal, typed instrumentation object whose default is
the no-op production implementation. Fault injection must occur only through
explicit test dependencies and must not be reachable from HTTP input. Preserve
the continuous gateway lock and project the wire error once after release.
Store commits remain atomic; lifecycle fallback is a separate transaction.

## Task 3: Compiled GIL and lifetime evidence

**Files:**

- Add: `dal-python/tests/test_curve_web_acceptance.py`
- Modify if required: `dal-python/src/bindings/curve.cpp`
- Modify if required: native planner/inspector sources already in PR #259

**RED:** Add runtime barriers for planner, validator, inspector, and solve while
a Python heartbeat advances on another thread. Add handle/GC tests which delete
the calibration result and parent/base handles, force collection, and still
read all four curve representations and recursive bases. Run with the compiled
Release extension.

**GREEN:** Release the GIL only around pure native calls and use pybind lifetime
policies/owned handles required by the failing cases. Keep Python list and
public overload compatibility unchanged.

## Task 4: True two-process persistence and reconstruction

**Files:**

- Add: `dal-web/backend/tests/subprocess_calibration_probe.py`
- Add: `dal-web/backend/tests/test_calibration_acceptance_persistence.py`
- Modify if required: `dal-web/backend/app/services/calibrations.py`
- Modify if required: `dal-web/backend/app/services/db/store_db.py`

**RED:** Process A creates and terminalizes PWC, PWLF, ZERO_RATE, LOG_DISCOUNT,
recursive-base, staged, and joint runs in one SQLite database and exits.
Process B starts from a clean interpreter, opens that database, GETs all
run/curve payloads, reconstructs through the compiled gateway, compares
discount factors, and reports planner/inspector/repair counts. Add payload/hash,
expected-pair, post-solve, factory, projection, commit, and fallback fault
cases; assert atomic rollback and no repair after reopen.

**GREEN:** Persist every reconstruction field and raw integrity pair needed by
the observed failure. Ensure reads copy JSON values and never mutate or repair
stored evidence. Re-run both subprocesses for every representation.

## Task 5: Strict performance report generator and workflow

**Files:**

- Modify: `.github/scripts/generate_web_calibration_perf_reports.py`
- Add: `.github/scripts/run_web_calibration_perf.py`
- Add: `.github/scripts/tests/test_web_calibration_perf_reports.py`
- Modify: `.github/workflows/cmake-linux.yml`
- Update from measured job only:
  `dal-web/backend/performance/fix-perf-30.json`
- Update from measured job only:
  `dal-web/backend/performance/fix-perf-100x100.json`

**RED:** Tests reject skipped, base-bound, empty, short, non-Release, non-native,
or wrong-head reports. Require FIX-PERF-30 3×5 health observations/trial maxima
and FIX-PERF-100X100 seven native, seven serialization, seven materialization
observations plus peak allocation and environment metadata.

**GREEN:** The dedicated workflow uses the repository-marked Release/native
runner, builds the compiled extension, runs one warmup and the fixed trials,
validates thresholds and head binding, and uploads raw JSON with
`actions/upload-artifact`. Generic benchmark jobs remain separate and cannot
satisfy this gate.

## Task 6: Verification and publication

Run focused acceptance files first, then:

```bash
cmake --build build/Release-linux --parallel 2
ctest --test-dir build/Release-linux --output-on-failure
PYTHONPATH=build/Release-linux/dal-python uv run --no-sync pytest dal-python/tests -q
uv run --project dal-web/backend --no-sync pytest dal-web/backend/tests -q
uv run --project dal-web/backend --no-sync ruff check dal-web/backend
cd dal-web/frontend && npm test
cd dal-web/frontend && npm run build
cd dal-web/frontend && DAL_PLAYWRIGHT_TEST_BACKEND=1 npm run test:e2e
cmake --build build/Release-linux --target dal_check_generated --parallel 2
python .github/scripts/check_native_web_gateway.py
git diff --check
```

Review/stage exact scope, commit with an imperative message, push the existing
branch, and keep PR #259 draft. Trigger the dedicated marked-runner workflow
for the final head. Wait for its result because the trigger explicitly makes
final CI and artifacts the deliverable. If the GitHub runner inventory contains
no matching marked runner or the token lacks dispatch/artifact permission,
stop at that external blocker and report the exact API/CLI evidence; never
manufacture timings or convert a skip into pass.

Update the PR body only after the final-head command counts and artifact URLs
exist. Preserve the exact `Closes DAL-10` line. Do not merge, request Copilot,
or change issue status.

## Fixture ownership map

| Fixture group | AC/COR/gates |
|---|---|
| `FIX-B1`…`FIX-B9` | AC-W1/W2/W3/W4; COR-01…09; gates 1…12 |
| `FIX-CB1-*` | AC-W1/W3/W4; COR-10; gates 13, 15, 17 |
| `FIX-PWLF-*`, `FIX-INPUT-ZERO-*` | AC-W3; COR-11/12; gates 16/17 |
| `FIX-WORKER-*`, `FIX-JOINT-*` | AC-W1/W3/W4; COR-13/14; gates 14/18/19 |
| `FIX-PERSISTED-*`, `FIX-BK06-*`, `FIX-BK07-*` | AC-W1/W2/W4; COR-15…17; gates 7/20/21 |
| `FIX-PERF-30`, `FIX-PERF-100X100` | AC-W2/W3/W6; gates 8/11/12 |
