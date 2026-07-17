# Master Branch Codebase Review — 2026-07-17

Scope: full repository at `master` @ `dad8a0ef` (clean tree, in sync with `origin/master`).
Method: five parallel read-only reviews — dal-cpp core, API/binding layers (dal-public, dal-python,
dal-excel), dal-web, documentation, build/CI/hygiene — against the conventions in
`.codex/skills/dal-agent-team/references/shared-rules.md`. Findings were verified against the code;
runtime-dependent items are marked *(suspect)*. No files were modified.

---

## 1. Executive Summary

Overall health is **good**. The core library applies its conventions consistently, RAII dominates,
the gtest suite (774 tests, zero `TEST_F`/`EXPECT_`) is strongest exactly where the numerical risk
is (curve calibration, script engine, AAD tape incl. concurrency), the CI is unusually thorough
(16-job compiler × AAD-backend matrix, sanitizers, warning-clean gate, benchmark regression gating,
docs-integrity gate, installed-consumer tests on both OSes), and the published docs verified
accurate against the code. The git tree contains no committed build junk.

The material risks are concentrated and all locally fixable:

1. **One confirmed memory-safety bug** — Excel zero-row `Retval_::ToXloper` heap over-read.
2. **Latent copy/global-state traps in the core** — `SimResults_` self-referential pointers,
   AAD multi-mode globals, unlocked holiday-data readers, `Cholesky_` ctor leak.
3. **Silent wrong prices in the web app** — global evaluation date never restored after valuation.
4. **Agent-facing docs are the stalest docs** — `.github/copilot-instructions.md`, `AGENTS.md`,
   and `shared-rules.md` repeat contracts the repo's own checker already classifies as stale,
   and they sit outside that checker's scope.
5. **CI/script drift** — two benchmarks never run in CI (hand-copied lists), `build_windows.bat`
   is broken on non-Community VS editions and not exercised by CI.

---

## 2. Top Findings (cross-area, by severity)

### Critical
None provably exploitable in current call paths.

### High

| # | Location | Issue |
|---|----------|-------|
| H1 | `dal-excel/src/_excel.cpp:1301-1306` | `Retval_::ToXloper()` allocates with `nRows` but advertises `max(nRows,1)` rows; an empty result (reachable via `=REPOSITORY.FIND("no_match")`) makes Excel read N `OPER_`s from a 0-byte allocation — heap over-read. Fix: allocate/initialize the advertised region. |
| H2 | `dal-cpp/dal/script/simulation.hpp:25-40,305` | `SimResults_::results_` maps to `const double*` into its own `risks_` vector, but copy/move is compiler-generated; copies keep pointers into the *source* and dangle. Currently latent (copies never read via `operator[]`). Fix: store indices, or rebuild pointers in a user-defined copy ctor. |
| H3 | `dal-cpp/dal/math/aad/tape.hpp:33`, `node.hpp:24`, `aad/aad.hpp:21-32` | `Tape_::multi_` / `TapNode_::numAdj_` are process-global statics while the tape is `thread_local`; two recording threads race, and `NumResultsResetterForAAD_` resets to a fixed default instead of restoring the previous value (nesting bug). Fix: make state per-tape; restore previous values. |
| H4 | `dal-cpp/dal/time/holidaydata.cpp:33,52-83` | `AddCenter` mutates global holiday data under a mutex, but all readers access it lock-free — data race (UB) if a center is registered at runtime. Fix: publish immutable `shared_ptr<const>` snapshots via atomic load/store. |
| H5 | `dal-web/backend/app/services/dal_gateway.py:139-141` | Global evaluation date is set before pricing and never restored; a valuation with `evaluation_date=null` silently prices at the previous request's date. Fix: snapshot + restore in `finally` inside the locked section; add regression test. |
| H6 | `dal-web/backend/app/services/valuation.py:267-280` + `main.py:50-88` | Valuations stay `"running"` forever after a server restart (in-process task queue, no reconciliation; routine in dev because `start.sh` uses uvicorn `--reload`). Fix: startup reconciliation marking orphaned runs `failed`. |
| H7 | `dal-web/frontend/src/pages/Trades.tsx:263-270`, `Portfolios.tsx:235-240` | `<ValuationPanel>` rendered without `key`; switching selection shows the previous target's PV/Greeks under the new target's name. Fix: `key={selected.id}`. |
| H8 | `.github/workflows/cmake-linux.yml:403-421`, `cmake-windows.yml:240-258` | CI hard-codes a 17-benchmark list; `threadpool_perf` and `stacks_perf` (of 20 defined) never run in CI. Fix: label benchmarks in CMake and `ctest -L benchmark`. |
| H9 | `build_windows.bat:2` | Hard-coded VS2022 **Community** `VsDevCmd.bat` path; broken on Professional/Enterprise/Build Tools. The workflow already solves this with `vswhere`; port it. |
| H10 | `.github/copilot-instructions.md:17,23-26,34-36` | Materially stale: claims repo-root install (`bin/`, `lib/`, `include/`), `bin/dal_cpp_tests` / `bin/dal_public_tests` paths, and wrong preset defaults (Python/benchmarks are off by default). Contradicts `docs/installation.md` and `CLAUDE.md`. |
| H11 | `dal-cpp/examples/CMakeLists.txt:21,48` | `if(DAL_HAS_EXCEL)` guards the `excelwriter` example, but `DAL_HAS_EXCEL` is defined nowhere — the example is silently never built. |
| H12 | `dal-cpp/CMakeLists.txt:25,55-59` | `DAL_USE_ADEPT_AAD` defaults `ON` (other backends `OFF`) with a mutual-exclusion `FATAL_ERROR`: a bare configure picks Adept instead of the native default, and enabling XAD alone hard-errors. Fix: default all three `OFF`. |

### Medium (selection; full lists in §3)

- `dal-cpp/dal/math/matrix/cholesky.cpp:44-65` — raw-`new` in ctor followed by `REQUIRE` throw → leak; hold in `unique_ptr` until success.
- `dal-cpp/dal/time/date.cpp:47` — `Date_(y,m,d)` has no range validation; `Date_(2023,2,30)` silently normalizes.
- `dal-cpp/dal/storage/json.cpp:296-316` — `HasParseError()` never checked; `quiet` param ignored.
- `dal-cpp/dal/math/matrix/matrixs.hpp:67-75` — copy-assign `noexcept` lies (terminates on OOM); `(rows+1)*cols` computed in `int`.
- `dal-cpp/dal/utilities/exceptions.hpp:90` — catch-all `XStackInfo_` ctor leaves members uninitialized → UB when formatted; `= delete` it.
- `dal-cpp/dal/model/base.hpp:55-61` (+ interp/PDE/index factories) — pervasive raw owning-pointer returns contradict the repo ownership rule; migrate to `unique_ptr`.
- `dal-web/backend/app/routers/*.py` — synchronous SQLAlchemy I/O on the event loop (native paths are correctly offloaded; DB paths are not). Make handlers `def` or `to_thread`.
- `dal-web/frontend` — failed-run `error_message` never displayed (`Valuations.tsx:86`, `client.ts:83-95`); no polling of in-flight runs (`Valuations.tsx:11-13`); unhandled promise rejections and selection race (`Portfolios.tsx:27-50`); poll loop survives unmount (`ValuationPanel.tsx:47-51`); demo seeding defaults on (`main.py:85`).
- `dal-excel/src/__curvedata.cpp:106-112` — `CURVEBLOCK.NEW.SIMPLE` documents optional `liborBasis` default `"ACT_365F"` but omitted arg → empty string → `DayBasis_("")` throws.
- `dal-excel/src/_excel.hpp:96` — dead `ToHandleVector` template calls `.Empty()` on `Handle_` (only `IsEmpty()` exists); latent compile error.
- `dal-python/src/bindings/curve.cpp:688-699,1002-1006` — four calibration bindings don't release the GIL (MC already does).
- `dal-python/src/bindings/core.cpp:130-144` — `Date_` defines `__eq__` without `__hash__`; equal dates hash differently *(suspect — pybind11 semantics, not executed)*.
- `CLAUDE.md:109,137-139` — stale benchmark inventory (17→19; missing `xccy_perf`, `ycinstrument_perf`); Machinist codegen misattributed to default `build_linux.sh` run (it's `--generate` / `dal_generate` only).
- `dal-cpp/CMakeLists.txt:75` — googletest added without `INSTALL_GTEST OFF` → gtest artifacts pollute the install prefix.
- `build_linux.sh:84-90,108-113` — CMake cache never cleaned (stale `-D` flags persist); clang/llvm-cov coverage path untested and likely broken.
- `dal-web/backend/requirements.txt` — stale (missing `sqlalchemy`/`alembic`); delete or generate from uv.

### Low (selection)

- Core: `mutable` members in calibration caches (`calibration.cpp:128`, `jointcalibration.cpp:125-126`) vs the no-`mutable` rule; `sprintf` past `size()` (`date.cpp:81`); `BlockList_::EmplaceBackMulti` unguarded over-walk; CG zero-RHS NaN (`bcg.cpp:88`); duplicate risk-report axes silently collapse (`report.cpp:59-74`); dead `ParseComposite`/`ParseSuperShot` stubs; size_t→int narrowing sweep candidates.
- Bindings: unknown settings keys silently ignored (`dal/api.py:24-42` + Excel settings parsers); Python wrapper hard-copies C++ builder defaults (`api.py:51-58`); `CollateralType_Libor(tenor)` ignores tenor; facade naming drift `XxxNew` vs `Xxx_New`; `value.hpp` missing `<map>`; "emtpy" typo (`script.hpp:23`); `FromPascalString` fixed 255-byte copy (`_excel.cpp:836`); examples 004–006 `sys.path` hack can bind a stale repo-root `dal/`.
- Web: `ProductDefinition.rows` no `min_length`; `smooth` allows 0; untyped `list_templates` response; Dashboard aggregates failed runs as PV 0; `NaN` from raw `Number()` inputs; health check fetched once; missing input labels (a11y); no auth (document the local-only assumption).
- Build/CI: no `.gitattributes` + lone-CR CMake files; `BUILD_SHARED_LIBS=${UNIX}` default never CI-tested; no CMake sanitizer option; `full-dev` preset hard-codes a `.venv` path; `ctest -C Release` no-op on Ninja; two different `setup-uv` pins; `apt install` without `apt-get update`; `.codacy.yml` excludes nonexistent `doc/**`; missing ignores (`coverage_filtered.info`, `benchmark-results/`, `out/`, `.ruff_cache/`); stale untracked root leftovers (`cmake/` RapidJSON configs, old-layout `bin/*.exe`); five of seven submodules are undocumented personal forks; no issue/PR templates.

---

## 3. Area Reports

### 3.1 dal-cpp core (≈86 k LOC)
**Health: good.** Conventions consistently applied; test suite convention-clean and well-targeted.
Systemic weaknesses: (1) the inherited raw-orphan-pointer factory idiom (leak invitation at every
call site); (2) global mutable state in AAD (`multi_`/`numAdj_`) and holiday data, contradicting the
otherwise solid thread-local design; (3) latent copy/exception-safety traps (`SimResults_`,
`Cholesky_`, `Matrix_` noexcept). Test gaps: `model/`, `risk/`, `indice/` parsers, `currency/`,
math/optimization (only underdetermined covered) are thin; `io/` untested but effectively dead
unless `USE_EXCEL_REPORT` is defined.

### 3.2 Bindings (dal-public / dal-python / dal-excel)
**Architecture fundamentally sound.** dal-public is a genuinely thin, faithful facade; pybind11 is
the most polished layer (lifetime-safe `Handle_` round-trips, correct GIL handling proven by
concurrency tests, defensive matrix validation); dal-excel is structurally clean but holds the one
real memory-safety bug (H1) plus a documented-default-that-isn't and the thinnest test coverage.
The README Python example matches the exported names exactly. Surface drift is modest; the genuine
inconsistencies are the facade's mixed factory naming and Excel's MC function lacking the `compiled`
flag and argument defaults its siblings have.

### 3.3 dal-web (FastAPI + React)
**Good architectural health**: single disciplined native gateway, correct lock/GIL handling around
DAL globals, async offloading for pricing, dual store backends with real persistence tests (40/40
backend tests pass), strict TypeScript with no `any`. Material risks: the evaluation-date leak (H5),
restart-orphaned runs (H6), unkeyed ValuationPanel (H7), unsurfaced failure reasons, and almost no
frontend tests (4 Playwright specs; no unit tests).

### 3.4 Documentation
**Published docs are accurate and recently reconciled** — README, docs/ (20 files), component
READMEs all verified against the code; anchors resolve; no line-number citations; CHANGELOG follows
its own policy; a CI docs-integrity job gates it. The residual risk is concentrated in agent-facing
guidance **outside** that CI gate: `copilot-instructions.md` (H10), `AGENTS.md` and
`shared-rules.md` (`bin/dal_cpp_tests`, `bin/dal_public_tests`, missing `--no-sync` on web pytest,
Machinist path only valid on Windows). Duplication across CLAUDE.md / AGENTS.md /
copilot-instructions.md / CONTRIBUTING.md is the structural cause. Coverage gaps: no conceptual doc
for dates/calendars or index parsing; `MSVC_RUNTIME` undocumented; core-module lists are unlabeled
partial selections.

### 3.5 Build / CI / hygiene
**Well above average.** Modern CMake (target-scoped options, proper install/export validated
end-to-end by installed-consumer tests on both OSes), 16-job matrix, sanitizers, warning-clean,
benchmark regression gating, all actions SHA-pinned. Risks: benchmark list drift (H8); legacy
script rot in `build_windows.bat` (H9; also broken `:set_variable` fallback and missing errorlevel
checks); default-behavior footguns (H11, H12, gtest install pollution); git tree itself is clean of
committed junk — root `bin/`, `lib/`, `include/`, `dal/`, `cmake/` are untracked local leftovers
from the old root-install layout that also make the stale doc paths appear to work.

---

## 4. Prioritized Enhancement Roadmap

**P0 — correctness fixes (small, high value):**
1. Fix `Retval_::ToXloper` zero-row allocation + empty-result regression test (H1).
2. Restore evaluation date in `DalGateway.value()` + regression test (H5).
3. Startup reconciliation for orphaned `running` valuations; surface `error_message` in the UI (H6).
4. Key `ValuationPanel` by selection id (H7).
5. `Date_` range validation (date.cpp:47); JSON `HasParseError` check (json.cpp).
6. `CURVEBLOCK.NEW.SIMPLE` empty→`ACT_365F` default; `ToHandleVector` `.Empty()`→`.IsEmpty()`.

**P1 — remove latent traps / stop CI drift:**
7. `SimResults_` index-based results or user-defined copy ctor (H2).
8. Per-tape AAD multi-mode state + restore-on-scope-exit (H3).
9. Snapshot-publish holiday data (H4).
10. Single-source the CI benchmark list via CTest labels (H8); flip `DAL_USE_ADEPT_AAD` default OFF
    (H12); `INSTALL_GTEST OFF`; submodule-initialized guards; resolve `DAL_HAS_EXCEL` (H11).
11. Rewrite/slim `copilot-instructions.md`; fix paths in `AGENTS.md`/`shared-rules.md`; extend
    `check_docs.py` scope to agent-facing docs (H10).
12. Release the GIL in the four calibration bindings; `py::hash` for `Date_`.

**P2 — structural improvements:**
13. Migrate orphan-pointer factories to `std::unique_ptr` returns (mechanical; call sites already
    `.reset()` or wrap).
14. Exception-safety sweep for new-then-REQUIRE ctors; `= delete` the `XStackInfo_` catch-all.
15. Modernize `build_windows.bat` (vswhere, errorlevel checks) and have CI invoke it; add a
    `DAL_ENABLE_SANITIZERS` CMake option; ccache + `concurrency:` cancellation in workflows.
16. Fill test gaps: core `model/`/`risk/`/`indice/` parsers/optimization solvers; Excel
    registration-table and empty-result tests; frontend vitest + valuation e2e; web tests for
    date-restore, failure path, restart recovery.
17. Docs: consolidate the four agent-facing guides into one source of truth; add PR template;
    conceptual notes for dates/calendars and index parsing; document `MSVC_RUNTIME`.
18. Repo polish: `.gitattributes` + EOL renormalize; `.codacy.yml` `doc/**`→`docs/**`; missing
    gitignore entries; delete stale root leftovers; reject unknown settings keys loudly.

---

## 5. Tests

- Review was static/read-only; no builds were run by the reviewers.
- dal-web backend suite was executed by the reviewer: **40/40 passed**.
- Repo CI state: Linux (16 compiler×backend jobs, ctest, coverage, sanitizers, benchmarks, docs,
  web quality) and Windows (3 MSVC×backend jobs, ctest, excel build, installed-consumer) gates
  exist; local `bin/dal_cpp_tests.exe` lists **774 tests** but is a stale old-layout artifact —
  current builds install test binaries under `build/stage/<preset>/bin/`.
