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
│   │       ├── dal_stub.py      pure-python dev/CI fallback (same public API)
│   │       ├── store.py         in-memory entity store
│   │       ├── valuation.py     trade/portfolio pricing orchestration
│   │       └── templates.py     product-builder presets + demo seed
│   └── tests/               pytest suite (runs against the stub)
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

### Native library vs. development stub

The compiled `dal` package requires a full C++ build with pybind11 bindings (see the
repository root `README.md` and `dal-python/`). So that the web app can be
developed and tested without that build, `dal_gateway.py` falls back to
`dal_stub.py` -- a pure-python module that re-implements the **same** public API
surface (closed-form Black-Scholes for European-style payoffs, finite-difference
Greeks). Selection is controlled by environment variables:

| Variable             | Default                                       | Meaning                                                            |
|----------------------|-----------------------------------------------|--------------------------------------------------------------------|
| `DAL_MODULE`         | `dal`                                         | Importable module providing the public API.                        |
| `DAL_REQUIRE_NATIVE` | unset                                         | If truthy, never fall back to the stub -- fail if `dal` is absent. |
| `WEBUI_SEED_DEMO`    | `1`                                           | Seed a demo portfolio/trade/model/product on startup.              |
| `WEBUI_CORS_ORIGINS` | `http://localhost:5173,http://127.0.0.1:5173` | Comma-separated allowed CORS origins (scheme required).            |

To use the real library, build the bindings, install the `dal` package into your
environment, then run the backend normally (with `DAL_REQUIRE_NATIVE=1` to be
strict). The `/api/health` endpoint reports which backend is active.

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
ports `8001` (backend) and `5173` (frontend) are free, install dependencies
(`uv sync` in `dal-web/backend/`, `npm install` in `dal-web/frontend/`), launch
both servers in the background, wait for the backend `/api/health` endpoint and
the frontend to become ready, then smoke-test the vite proxy (`/api` → backend).
PIDs are saved to `dal-web/{backend,frontend}/.server.pid`.

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
uv sync                     # create .venv and install runtime + dev deps from uv.lock
uv run python -m uvicorn app.main:app --reload --host 127.0.0.1 --port 8001
```

`uv` provisions a matching Python interpreter automatically (downloading one if
needed) and resolves dependencies from the committed `uv.lock`. API docs are then
available at <http://127.0.0.1:8001/docs>.

> To run against the compiled native `dal` package instead of the dev stub,
> build the `dal-python` bindings (see `dal-python/` and the repository root
> `README.md`) and install the resulting `dal` package into the backend's uv
> environment, e.g. `uv pip install -e ../dal-python` once built, then start
> the server with `DAL_REQUIRE_NATIVE=1` so a missing native build is a hard
> error rather than a silent stub fallback. `/api/health` reports
> `is_native: true` and the resolved `backend` module name once the real
> engine is loaded.

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
uv run python -m uvicorn app.main:app --reload --port 8002
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
| `GET /api/health`                              | Reports which DAL backend is active (native vs. stub).                   |
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

Valuation endpoints now return a pending `ValuationResult` with
`status: "running"` immediately. Pricing runs in a FastAPI background task, and
the result is updated in-place once it completes. The frontend polls
`GET /api/valuations/{id}` at 300ms intervals until the status becomes
`"completed"` or `"failed"`.

### Tests

```bash
cd dal-web/backend
uv run pytest               # runs against the in-process DAL stub
```

```bash
./dal-web/scripts/setup-playwright.sh
cd dal-web/frontend
npm run build               # type-check + production build
npm run test:e2e            # Playwright smoke tests (starts/stops the web UI)
```

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
