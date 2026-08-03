# DAL Derivatives Portfolio Management -- Web UI

A web application for managing and pricing derivatives portfolios on top of the
Derivatives Algorithms Library (DAL).

The visual Curve Lab build/version/pricing/risk contract is documented in the
[Curve Lab guide](../docs/curve-lab.md).

* **Backend** -- FastAPI (Python `>= 3.13`).
* **Frontend** -- React + TypeScript (Vite).
* **DAL access** -- the backend talks to DAL **only** through its Python public
  API (the `dal` package). Every call is funnelled through a single integration
  module, `backend/app/services/dal_gateway.py`.

```
dal-web/
├── scripts/
│   ├── start.sh             start both services (backend + frontend) — Linux/macOS
│   ├── start.ps1            Windows/PowerShell 7 equivalent of start.sh
│   ├── stop.sh              stop both services (with optional --force) — Linux/macOS
│   ├── stop.ps1             Windows/PowerShell 7 equivalent of stop.sh (with -Force)
│   └── setup-playwright.sh  one-time browser/runtime setup for the frontend e2e suite
├── backend/                 FastAPI service
│   ├── app/
│   │   ├── main.py          app factory + router wiring
│   │   ├── schemas/         Pydantic request/response models
│   │   ├── routers/         products, models, trades, portfolios, calibrations, Curve Lab, system
│   │   └── services/
│   │       ├── dal_gateway.py   ← the ONLY place that imports the dal public API
│   │       ├── store.py         Store seam: StoreProtocol + in-memory Store + get_store()
│   │       ├── db/              SQLAlchemy 2.x DbStore (session / models / store_db) + migrations
│   │       ├── calibrations.py  asynchronous calibration orchestration + DTO persistence
│   │       ├── curve_lab_*.py   V2 planning, lifecycle, jobs, fixings, and risk
│   │       ├── valuation.py     trade/portfolio pricing orchestration
│   │       └── templates.py     product-builder presets + demo seed
│   └── tests/               pytest suite (fake dal module, no C++ build needed)
└── frontend/                React + Vite SPA
    └── src/
        ├── api/client.ts    typed API client
        ├── components/      valuation, calibration fit/matrix, and quote-risk panels
        └── pages/           Dashboard, portfolio pages, Valuations, Curves, CurveRun
```

## How DAL is used

The backend maps business entities onto DAL's scripted-product / model /
Monte-Carlo public API:

| UI concept      | DAL public API call                                                                |
|-----------------|------------------------------------------------------------------------------------|
| Product builder | `Product_New(dates, events)`, `Product_Debug`                                      |
| Black-Scholes   | `BSModelData_New(spot, vol, rate, div)`                                            |
| Dupire          | `DupireModelData_New(spot, rate, repo, ...)`                                       |
| Evaluation date | `EvaluationDate_Set` / `EvaluationDate_Get`                                        |
| Valuation       | `MonteCarlo_Value(product, model, n_paths, ...)`                                   |
| Curve planning  | `PlanCurveCalibrationKnots`, eligibility and execution-identity inspectors         |
| Calibration     | `CalibrateSingleCurve`, `CalibrateXccyMarket`, `CalibrateJointXccyMarket`          |
| Curve rebuild   | `DiscountPWC_New`, `DiscountPWLF_New`, `DiscountZeroRate_New`, `DiscountLogDF_New` |
| Curve archive   | Private `Storable_` JSON and `Bag_` integration bridge                             |
| Rate pricing    | `PriceRateTrades`, fixing planning, and `RateTradeNodeSensitivities`               |

No other module imports `dal` directly -- routers and services depend on
`DalGateway`, satisfying the "calls to DAL only through the Python public API"
requirement.

### DAL dependency

The backend imports the compiled `dal` package (the dal-python pybind11 bindings;
see `dal-python/` and the repository root `README.md`) directly -- it is the sole
pricing engine, with no pure-Python fallback. Build and install `dal-python` into
the backend's uv environment before running the server. The canonical
staged-prefix command is in [the installation guide](../docs/installation.md#web-ui).

The pytest suite registers a minimal fake `dal` module (see `tests/conftest.py`)
so the FastAPI wiring can be exercised without a C++ build; production imports
the real `dal`.

Runtime configuration:

| Variable               | Default                                       | Meaning                                                                                                     |
|------------------------|-----------------------------------------------|-------------------------------------------------------------------------------------------------------------|
| `WEBUI_SEED_DEMO`      | `1`                                           | Seed a demo portfolio/trade/model/product on startup.                                                       |
| `WEBUI_CORS_ORIGINS`   | `http://localhost:5173,http://127.0.0.1:5173` | Comma-separated allowed CORS origins (scheme required).                                                     |
| `DAL_WEB_DB_URL`       | `sqlite:///<backend>/.data/dalweb.db`         | SQLAlchemy URL for the persistence layer.                                                                   |
| `DAL_WEB_STORE`        | unset                                         | Set to `memory` to bypass the DB and use the legacy in-memory store.                                        |
| `DAL_WEB_AUTO_MIGRATE` | unset                                         | Set to `1` to bring the schema up to date via `alembic upgrade head` on startup (otherwise `create_all()`). |
| `DAL_NUM_THREADS`      | hardware concurrency                          | Positive cap for DAL's lazy native thread pool; set before the backend imports `dal`.                       |

## Persistence

The five existing entity types (products, models, trades, portfolios, and
valuation results) and the three curve-calibration entity types are persisted
by a SQLAlchemy 2.x **sync** store (`app/services/db/store_db.py`) that
implements the same `StoreProtocol` the routers depend on:

| Calibration entity                | Persisted state                                                                                  |
|-----------------------------------|--------------------------------------------------------------------------------------------------|
| `CalibrationRun`                  | Versioned request, normalized solver/options, lifecycle, execution evidence, results, and errors |
| `CurveDefinition`                 | Versioned reconstruction data and base-curve ID; each base curve is an independent persisted row |
| `CalibrationInstrumentDefinition` | Normalized instrument payload plus its input and canonical calibration ordering                  |

Curve rows store complete reconstruction data for
`PIECEWISE_CONSTANT_FWD`, `PIECEWISE_LINEAR_FWD`, `ZERO_RATE`, and
`LOG_DISCOUNT`: anchor/node dates, day count, representation-specific
parameters, actual log-DF scheme where applicable, and `base_curve_id`. Each
base curve is stored as an independent row; `GET /api/curves/{id}` and completed
run reads recursively expand the linked rows into the response DTO. No
process-local C++ handle is stored. A completed run can therefore be read in a
fresh backend process, and its curves can be rebuilt through `DalGateway` using
only database DTOs.

The default backend is a local SQLite file under `dal-web/backend/.data/`
(gitignored); point `DAL_WEB_DB_URL` at any SQLAlchemy URL to switch backends,
e.g. `postgresql+psycopg://host/db`. SQLite connections get WAL journaling and
foreign-key enforcement enabled automatically.

Schema management defaults to `create_all()` on startup (idempotent, zero
friction). Set `DAL_WEB_AUTO_MIGRATE=1` to apply Alembic migrations instead
(`alembic upgrade head`); the migration set lives in `dal-web/backend/migrations/`
and can be run directly with `uv run alembic upgrade head` from the backend
directory.

For ephemeral or read-only environments where no database is wanted, set
`DAL_WEB_STORE=memory` to fall back to the original in-memory store -- no file,
no SQLAlchemy. Everything then lives in process memory, so a backend restart
loses all entities and in-flight work. With the database store, entities and
terminal valuation/calibration results survive a restart. At startup, every
calibration still `"running"` in any of the `queued`, `solving`,
`serializing`, or `persisting` phases is reconciled to `"failed"` with error
code `SERVER_RESTARTED`; completed and already-failed runs remain unchanged.
This mirrors valuation recovery, where an orphaned `"running"` valuation
becomes `"failed"` because its in-process task cannot resume.

### Curve Lab persistence and restart behavior

Curve Lab adds durable drafts, build/import/risk runs, immutable versions,
audit events, fixing snapshots, and matrix blobs. A successful build stores the
exact canonical native archive and its hash; a single component has REST root
kind `DISCOUNT_CURVE`, while a multi-root native `Bag_` has REST root kind
`CURVE_SET`. Archiving changes only a version's visibility.

Build, import, and risk work share two workers and a bounded queue of 100
waiting jobs. Submission fails with `429` before persistence when that capacity
is full. Every job has a 15-minute soft deadline checked between native calls.
On database-backed startup, orphaned in-flight rows are reconciled to
`FAILED` with `SERVER_RESTARTED`, or to `TIMED_OUT` when the persisted deadline
has elapsed. Terminal rows and versions remain readable. In-memory mode loses
all Curve Lab state on restart.

See [Curve Lab persistence, restart, and rollback](../docs/curve-lab.md#persistence-restart-and-rollback)
for migration and destructive downgrade guidance.

## Running

### Quick Start (both services)

The easiest way to start and stop the web UI is with the scripts in `dal-web/scripts/`.
Use the `.sh` scripts on Linux/macOS and the `.ps1` scripts on Windows (PowerShell 7+):

```bash
# Start both services — Linux/macOS
./dal-web/scripts/start.sh

# Stop both services — Linux/macOS
./dal-web/scripts/stop.sh          # SIGTERM
./dal-web/scripts/stop.sh --force  # escalate to SIGKILL if needed
```

```powershell
# Start both services — Windows (PowerShell 7+)
pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/start.ps1

# Stop both services — Windows
pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/stop.ps1           # graceful
pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/stop.ps1 -Force    # escalate to force kill
```

Both launchers check their platform prerequisites, including Python ≥ 3.13,
uv, node, and npm. The bash launcher also needs `curl`, `grep`, `nohup`, and
either `ss` or `lsof`; its stopper needs `lsof`. They verify ports `8001`
(backend) and `5173` (frontend) are free, synchronize backend dependencies with
`uv sync --inexact`, and run the native-DAL preflight with
`uv run --no-sync python -m app.native_runtime`. They then install frontend
dependencies, launch Uvicorn with `uv run --no-sync` and Vite in the background,
wait for both services, and smoke-test the proxy (`/api` → backend). `--inexact`
and `--no-sync` preserve the locally installed DAL package. PIDs are saved to
`dal-web/{backend,frontend}/.server.pid`.

Log files differ by platform. On Linux/macOS both streams are merged into a
single `.server.log` next to each server. On Windows each service writes two
files: `.server.log` (stdout) and `.server.log.err` (stderr).

The bash script checks `python3`; the PowerShell script checks `python`.

`stop.sh` / `stop.ps1` kill by PID from those files, verify each port is
actually free, and fall back to port-based kill if an orphaned child is holding
the socket. The Windows stopper additionally walks the recorded PID's process
tree so child workers (the uvicorn `--reload` worker, vite/node children) are
terminated before the port-based fallback. With `--force` (bash) or `-Force`
(PowerShell) the stopper escalates to a hard kill after a 5s grace period.

Once running, open **<http://localhost:5173>** in your browser. The Vite dev
server proxies `/api` requests to the backend automatically (target port is
`8001`, configured in `vite.config.ts`).

### Backend (Python >= 3.13)

Dependencies are managed with [uv](https://docs.astral.sh/uv/). From `dal-web/backend`:

```bash
cd dal-web/backend
uv sync --inexact
uv pip install ../../dal-python "--config-settings=cmake.define.DAL_INSTALL_PREFIX=/absolute/path/to/build/stage/<platform-preset>"
uv run --no-sync python -m app.native_runtime
uv run --no-sync python -m uvicorn app.main:app --reload --host 127.0.0.1 --port 8001
```

`uv` provisions a matching Python interpreter automatically (downloading one if
needed) and resolves dependencies from the committed `uv.lock`. API docs are then
available at <http://127.0.0.1:8001/docs>.

Replace `<platform-preset>` with the stage produced by the active build, such as
`Release-linux` or `Release-windows`. The quoted command works in POSIX shells
and PowerShell.

> The backend requires the compiled `dal` package. If the preflight fails, install
> the package against the staged DAL prefix as shown in the
> [installation guide](../docs/installation.md#install-the-native-package-into-the-backend-environment).

### Frontend

```bash
cd dal-web/frontend
npm install
./node_modules/.bin/vite    # http://localhost:5173 (proxies /api to :8001)
```

The Vite dev server proxies all `/api` requests to the backend URL configured in
`vite.config.ts` (default: `http://127.0.0.1:8001`). If you change the backend
port, update the `proxy.target` in that file and restart the frontend.

> **Note:** Run vite directly rather than `npm run dev` when launching by hand.
> `npm run` wraps the command in a parent process that does not forward SIGTERM,
> which leaves an orphan holding the port on shutdown. Both `start.sh` and
> `start.ps1` already do this for you.

### Stopping

If you started the services with a start script, stop them with the matching
stop script:

```bash
./dal-web/scripts/stop.sh                       # Linux/macOS
```
```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/stop.ps1   # Windows
```

If you started them manually, press `Ctrl+C` in each terminal, or use the stop
script (it will fall back to port-based kill if no PID files exist).

### Troubleshooting

**Port 8001 already in use.** If the backend fails with `[Errno 98] Address
already in use` (Linux/macOS) or reports the port is bound (Windows), either
free the port or run on a different one:

```bash
# Option A — stop any running web UI
./dal-web/scripts/stop.sh

# Option B — find and kill whatever is using port 8001
fuser -k 8001/tcp

# Option C — use a different port (e.g. 8002)
uv run --no-sync python -m uvicorn app.main:app --reload --port 8002
```

On Windows the equivalents are:

```powershell
# Option A — stop any running web UI
pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/stop.ps1

# Option B — find what owns the port, then kill it
Get-NetTCPConnection -LocalPort 8001 -State Listen
Stop-Process -Id <pid from above> -Force
```

If you choose Option C, also update the proxy target in
`dal-web/frontend/vite.config.ts` to match (`http://127.0.0.1:8002`) and restart
the frontend.

**Frontend can't reach the backend.** Make sure the backend starts *before* the
frontend. If you see proxy errors in the browser console, check that the backend
is running and the port in `vite.config.ts` matches.

## API

The backend exposes a REST-ish API under `/api`. Full OpenAPI docs are served at
`/docs` once the backend is running. Highlights beyond the standard CRUD:

| Endpoint                                       | Notes                                                                                        |
|------------------------------------------------|----------------------------------------------------------------------------------------------|
| `GET /api/health`                              | Reports the active DAL backend (`dal`).                                                      |
| `POST /api/products`, `PUT /api/products/{id}` | Create / partially update a scripted product.                                                |
| `POST /api/products/debug`                     | Render the DAL `Product_Debug` dump for arbitrary rows.                                      |
| `POST /api/models`, `PUT /api/models/{id}`     | Black-Scholes or Dupire model data.                                                          |
| `POST /api/trades`, `PUT /api/trades/{id}`     | Link a product + model + notional.                                                           |
| `POST /api/portfolios/{id}/trades/{tid}`       | Add a trade to a portfolio.                                                                  |
| `POST /api/trades/{id}/value`                  | Start an **async** single-trade valuation (returns `status: "running"`).                     |
| `POST /api/portfolios/{id}/value`              | Start an **async** portfolio valuation (returns `status: "running"`).                        |
| `GET /api/valuations/{id}`                     | Poll until `status` becomes `"completed"` or `"failed"`.                                     |
| `POST /api/calibrations/single`                | Submit a versioned single discount/projection curve calibration; returns `202` + `Location`. |
| `POST /api/calibrations/xccy/staged`           | Submit staged XCCY basis calibration over persisted domestic/foreign curve blocks.           |
| `POST /api/calibrations/xccy/joint`            | Submit one joint domestic, foreign, and basis calibration.                                   |
| `GET /api/calibrations/{id}`                   | Read the persisted run and optionally request a quote-bump preview.                          |
| `GET /api/curves/{id}`                         | Read a versioned, recursively reconstructible persisted curve DTO.                           |

### Curve Lab V2 API

The `/api/curve-lab` surface is additive to the calibration endpoints above:

| Resource            | Operations                                                                                     |
|---------------------|------------------------------------------------------------------------------------------------|
| Capabilities/quotes | Read the closed V2 contract, canonicalize quote lexemes, and render exact presentation strings |
| Drafts/build runs   | Create or compare-and-swap a draft, snapshot a build, and poll its immutable lifecycle         |
| Versions/imports    | Publish, clone, archive, export native JSON/manifest, and preflight/reconstruct imports        |
| Fixings/risk        | Persist immutable snapshots; run typed PV/DV01/KRD; fetch provenance-rich sensitivity matrices |

Quote canonicalization is stateless: the endpoint accepts the exact input
lexeme as a string and returns a canonical `raw_quote`. The production
workspace applies that value only to the explicitly selected instrument. Input
convention and lexeme remain local authoring inputs, so `4/PERCENT` and
`0.04/DECIMAL` for the same instrument produce the same financial identity,
quote/risk axes, and replay. A monotonic request generation permits concurrent
same-target submissions but allows only the latest matching request to update
the quote, canonical output, error, or submitting state. Older success,
failure, and cancellation responses are ignored; target, family, draft, or
authoring-input edits invalidate the pending request.

`POST /api/curve-lab/quote-renderings` has a closed, additive request containing
`instrument_type`, canonical string `canonical_raw_quote`,
`display_convention`, and integer `display_scale` from 0 through 12. It returns
only string `rendered_quote`. The backend uses `Decimal` with round-half-to-even,
so `0.04/PERCENT/6` renders as `4.000000`; tests also cover scales `0/1/12`,
positive and negative ties, normalized negative zero, and Future price points.
Family/convention mismatches, invalid scale, and non-canonical input use the
structured Curve Lab `422` envelope. Rendering has an independent
latest-request-wins generation. Its convention, scale, result, and error remain
presentation-only: they do not mutate the draft or fingerprint, alter
build/risk axes or replay identity, or mark a build stale.

Build, import, and risk submissions return `202 Accepted` with a persisted
record ID; poll the corresponding run/job endpoint constructed from that ID.
Publishing a successful non-stale build returns an immutable version. Draft
updates require `If-Match`; a stale revision or conflicting publication
returns `409`. Non-finite floating-point input returns a structured `422`
response with code `REQUEST_VALIDATION_FAILED` before draft or audit
persistence. Queue exhaustion returns `429` with `Retry-After`. The full
endpoint inventory, JSON example, native/Python entry points, archive limits,
matrix units, and compatibility contract are in the
[Curve Lab guide](../docs/curve-lab.md).

### Async curve calibration

All three calibration POST endpoints return a persisted run with
`status: "running"` and a `Location` header immediately. Planning and native
solves are offloaded with `asyncio.to_thread`, and the frontend polls
`GET /api/calibrations/{id}` while the run progresses through `queued`,
`solving`, `serializing`, and `persisting`.

The completed response reports the actual `ANALYTIC` or `BUMPED` Jacobian mode,
solver status and residual metrics, per-instrument market/model rates and
residuals, persisted curve DTOs, named parameter/residual ranges, and XCCY FX
forwards. Request options independently control materialized forward Jacobian
and effective-inverse values; matrix metadata remains present when values were
not requested or are unavailable for the selected mode. Materialized matrices
are limited to `100 × 100`, and every serialized calibration/curve response is
limited to 1 MiB.

For a completed run with an available effective inverse, supply both
`quote_bump_index` and `quote_bump_size` to
`GET /api/calibrations/{id}`. The backend returns the parameter preview using
`delta_x = effective_inverse * delta_quote / residual_tolerance`; the frontend
does not duplicate that calculation. Validation and analytical-eligibility
errors use structured HTTP 422 responses with a declaration/instrument
`location` where applicable.

### Delete guards

Deleting a product or model that is still referenced by any trade returns
`409 Conflict` with a detail message naming the referencing trade. Delete the
trade first (which cascades out of any portfolios) and then the product/model.

### Async valuation

Valuation endpoints return a pending `ValuationResult` with
`status: "running"` immediately. Pricing runs as an `asyncio` task that
offloads the blocking C++ pricing call to a worker thread via
`asyncio.to_thread`. The Python binding releases the GIL around the pure native
Monte Carlo call, so the event loop and unrelated Python work remain responsive.
The public valuation configuration calls the pseudo-random choice `pseudo`; the
gateway maps it to DAL's `mrg32` generator before entering the native binding.
DAL itself holds a re-entrant valuation/mutation barrier for the native pricing
interval: evaluation-date setters wait, while getters can read the stable date
through the store mutex. Separately, `DalGateway` holds a Python orchestration
lock across request-level date mutation, product/model construction, and
valuation. Pricing dispatch therefore remains serialized within a backend
process. The result is updated in-place once it completes, and the frontend polls
`GET /api/valuations/{id}` at 300ms intervals until the status becomes
`"completed"` or `"failed"`.

A settled valuation distinguishes two failure layers. A per-trade pricing
failure is contained per trade: the valuation still reaches `"completed"`, the
failing trade's row carries an `error` and contributes zero PV, and the
remaining trades price and aggregate normally -- one bad trade cannot abort a
portfolio. A task-level failure (anything outside an individual trade's
pricing) flips the whole valuation to `"failed"` with `error_message` set,
zeroed PV, empty Greeks, and no trade rows.

Pricing tasks live inside the backend process, so a valuation still `"running"`
when the server stops cannot resume. On startup, such orphaned rows are
reconciled to `"failed"` with `error_message: "Server restarted while
pricing"`; completed and already-failed rows are left untouched.

### Tests

```bash
cd dal-web/backend
uv run --no-sync pytest     # uses a fake dal module (no C++ build needed)
```

```bash
./dal-web/scripts/setup-playwright.sh
cd dal-web/frontend
npm run build               # type-check + production build
npm test                    # vitest unit tests (jsdom; no browser or backend needed)
npm run test:e2e            # Playwright smoke tests (starts/stops the web UI)
```

The vitest unit suite under `frontend/tests/unit/` covers the API client, the
valuation panel, model-form parsing, formatting helpers, and Curve Lab visual
and API state. It runs in jsdom against mocked API responses, needs neither a
browser nor a backend, and also runs in the web-quality CI job.

The default Playwright command uses the native-only application startup path.
CI uses an explicit canned DAL test double while retaining the real FastAPI
routers and Vite frontend:

```bash
DAL_PLAYWRIGHT_TEST_BACKEND=1 npm run test:e2e
```

The test-backend entry point refuses to start without that flag and reports
`backend=canned-dal`, `is_native=false` from the health endpoint. It is a
browser integration fixture, not a development or production fallback. Specs
that require the canned backend skip themselves when the flag is absent.

## Screens

* **Dashboard** -- portfolio counts, latest PV, recent valuation runs (with
  per-run status indicator: running / completed / failed).
* **Portfolios** -- group trades into books, add/remove trades, price the book.
  Delete buttons require confirmation; portfolios cascade-delete their trades
  from the book.
* **Trades** -- link a product + model + notional; price a single trade. Trades
  are updated in place via `PUT /api/trades/{id}`.
* **Product Builder** -- compose DAL scripted products as a schedule of
  (date/label, event) rows; load templates (European, up-and-out call, snowball);
  render the DAL product debug dump. Saved products can be **loaded** back into
  the editor for iteration.
* **Models** -- create Black-Scholes or Dupire model data (vol surface entered
  as a whitespace-separated matrix).
* **Valuation Runs** -- reproducible history of every Monte Carlo run with PV,
  Greeks, and per-run status.
* **Curve Lab** -- use visual editors for single, multi-curve, staged XCCY, or
  joint XCCY builds across all seven supported rate families. Build and import
  runs are asynchronous; successful runs publish immutable native versions.
  Selecting another instrument family reconstructs a legal family-specific
  draft template instead of retaining stale terms. Each instrument is an
  explicit canonical-quote target; canonicalization changes only that target's
  stored `raw_quote`, while display preferences remain local. The workspace
  displays each admitted build, import, or risk ID immediately, polls without a
  fixed client deadline, and can resume the same ID after a transport error
  during the current workspace session.
  The same workspace clones, archives, imports, and exports versions, captures
  immutable fixing snapshots, and runs typed PV/DV01/KRD with explicit axes,
  units, provenance, partial pricing failures, and sensitivity matrices.
  Advanced JSON edits the same validated V2 document as the visual controls.
