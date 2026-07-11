# DAL Derivatives Portfolio Management -- Web UI

A web application for managing and pricing derivatives portfolios on top of the
Derivatives Algorithms Library (DAL).

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
│   │   ├── routers/         products, models, trades, portfolios, system
│   │   └── services/
│   │       ├── dal_gateway.py   ← the ONLY place that imports the dal public API
│   │       ├── store.py         Store seam: StoreProtocol + in-memory Store + get_store()
│   │       ├── db/              SQLAlchemy 2.x DbStore (session / models / store_db) + migrations
│   │       ├── valuation.py     trade/portfolio pricing orchestration
│   │       └── templates.py     product-builder presets + demo seed
│   └── tests/               pytest suite (fake dal module, no C++ build needed)
└── frontend/                React + Vite SPA
    └── src/
        ├── api/client.ts    typed API client
        ├── components/      ValuationPanel
        └── pages/           Dashboard, Portfolios, Trades, ProductBuilder, Models, Valuations
```

## How DAL is used

The backend maps business entities onto DAL's scripted-product / model /
Monte-Carlo public API:

| UI concept      | DAL public API call                              |
|-----------------|--------------------------------------------------|
| Product builder | `Product_New(dates, events)`, `Product_Debug`    |
| Black-Scholes   | `BSModelData_New(spot, vol, rate, div)`          |
| Dupire          | `DupireModelData_New(spot, rate, repo, ...)`     |
| Evaluation date | `EvaluationDate_Set` / `EvaluationDate_Get`      |
| Valuation       | `MonteCarlo_Value(product, model, n_paths, ...)` |

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

All five entities (products, models, trades, portfolios, valuation results) are
persisted by a SQLAlchemy 2.x **sync** store (`app/services/db/store_db.py`)
that implements the same `StoreProtocol` the routers depend on. The default
backend is a local SQLite file under `dal-web/backend/.data/` (gitignored);
point `DAL_WEB_DB_URL` at any SQLAlchemy URL to switch backends, e.g.
`postgresql+psycopg://host/db`. SQLite connections get WAL journaling and
foreign-key enforcement enabled automatically.

Schema management defaults to `create_all()` on startup (idempotent, zero
friction). Set `DAL_WEB_AUTO_MIGRATE=1` to apply Alembic migrations instead
(`alembic upgrade head`); the migration set lives in `dal-web/backend/migrations/`
and can be run directly with `uv run alembic upgrade head` from the backend
directory.

For ephemeral or read-only environments where no database is wanted, set
`DAL_WEB_STORE=memory` to fall back to the original in-memory store -- no file,
no SQLAlchemy.

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

Both launchers check prerequisites (Python ≥ 3.13, uv, node, npm, curl), verify
ports `8001` (backend) and `5173` (frontend) are free, synchronize backend
dependencies with `uv sync --inexact`, and run the native-DAL preflight with
`uv run --no-sync python -m app.native_runtime`. They then install frontend
dependencies, launch Uvicorn with `uv run --no-sync` and Vite in the background,
wait for both services, and smoke-test the proxy (`/api` → backend). `--inexact`
and `--no-sync` preserve the locally installed DAL package. PIDs are saved to
`dal-web/{backend,frontend}/.server.pid`.

Log files differ by platform. On Linux/macOS both streams are merged into a
single `.server.log` next to each server. On Windows each service writes two
files: `.server.log` (stdout) and `.server.log.err` (stderr).

Note the one prerequisite-name difference: the bash script checks `python3`,
the PowerShell script checks `python`.

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

| Endpoint                                       | Notes                                                                    |
|------------------------------------------------|--------------------------------------------------------------------------|
| `GET /api/health`                              | Reports the active DAL backend (`dal`).                                  |
| `POST /api/products`, `PUT /api/products/{id}` | Create / partially update a scripted product.                            |
| `POST /api/products/debug`                     | Render the DAL `Product_Debug` dump for arbitrary rows.                  |
| `POST /api/models`, `PUT /api/models/{id}`     | Black-Scholes or Dupire model data.                                      |
| `POST /api/trades`, `PUT /api/trades/{id}`     | Link a product + model + notional.                                       |
| `POST /api/portfolios/{id}/trades/{tid}`       | Add a trade to a portfolio.                                              |
| `POST /api/trades/{id}/value`                  | Start an **async** single-trade valuation (returns `status: "running"`). |
| `POST /api/portfolios/{id}/value`              | Start an **async** portfolio valuation (returns `status: "running"`).    |
| `GET /api/valuations/{id}`                     | Poll until `status` becomes `"completed"` or `"failed"`.                 |

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

### Tests

```bash
cd dal-web/backend
uv run --no-sync pytest     # uses a fake dal module (no C++ build needed)
```

```bash
./dal-web/scripts/setup-playwright.sh
cd dal-web/frontend
npm run build               # type-check + production build
npm run test:e2e            # Playwright smoke tests (starts/stops the web UI)
```

The default Playwright command uses the native-only application startup path.
CI uses an explicit canned DAL test double while retaining the real FastAPI
routers and Vite frontend:

```bash
DAL_PLAYWRIGHT_TEST_BACKEND=1 npm run test:e2e
```

The test-backend entry point refuses to start without that flag and reports
`backend=canned-dal`, `is_native=false` from the health endpoint. It is a
browser integration fixture, not a development or production fallback.

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
