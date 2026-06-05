# DAL Derivatives Portfolio Management -- Web UI

A web application for managing and pricing derivatives portfolios on top of the
Derivatives Algorithms Library (DAL).

* **Backend** -- FastAPI (Python `>= 3.13`).
* **Frontend** -- React + TypeScript (Vite).
* **DAL access** -- the backend talks to DAL **only** through its Python public
  API (the `dal` package). Every call is funnelled through a single integration
  module, `backend/app/services/dal_gateway.py`.

```
webui/
├── scripts/
│   ├── start.sh             start both services (backend + frontend)
│   └── stop.sh              stop both services (with optional --force)
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

| UI concept       | DAL public API call                                  |
|------------------|------------------------------------------------------|
| Product builder  | `Product_New(dates, events)`, `Product_Debug`        |
| Black-Scholes    | `BSModelData_New(spot, vol, rate, div)`              |
| Dupire           | `DupireModelData_New(spot, rate, repo, ...)`         |
| Evaluation date  | `EvaluationDate_Set` / `EvaluationDate_Get`          |
| Valuation        | `MonteCarlo_Value(product, model, n_paths, ...)`     |

No other module imports `dal` directly -- routers and services depend on
`DalGateway`, satisfying the "calls to DAL only through the Python public API"
requirement.

### Native library vs. development stub

The compiled `dal` package requires a full C++ build with SWIG bindings (see the
repository root `README.md` and `dal-python/`). So that the web app can be
developed and tested without that build, `dal_gateway.py` falls back to
`dal_stub.py` -- a pure-python module that re-implements the **same** public API
surface (closed-form Black-Scholes for European-style payoffs, finite-difference
Greeks). Selection is controlled by environment variables:

| Variable             | Default | Meaning                                                        |
|----------------------|---------|----------------------------------------------------------------|
| `DAL_MODULE`         | `dal`   | Importable module providing the public API.                    |
| `DAL_REQUIRE_NATIVE` | unset   | If truthy, never fall back to the stub -- fail if `dal` is absent. |
| `WEBUI_SEED_DEMO`    | `1`     | Seed a demo portfolio/trade/model/product on startup.          |
| `WEBUI_CORS_ORIGINS` | `http://localhost:5173,http://127.0.0.1:5173` | Comma-separated allowed CORS origins (scheme required). |

To use the real library, build the bindings, install the `dal` package into your
environment, then run the backend normally (with `DAL_REQUIRE_NATIVE=1` to be
strict). The `/api/health` endpoint reports which backend is active.

## Running

### Quick Start (both services)

The easiest way to start and stop the web UI is with the scripts in `webui/scripts/`:

```bash
# Start both services
./webui/scripts/start.sh

# Stop both services
./webui/scripts/stop.sh          # SIGTERM
./webui/scripts/stop.sh --force  # escalate to SIGKILL if needed
```

`start.sh` checks prerequisites (Python ≥ 3.13, uv, node, npm), verifies ports
`8001` (backend) and `5173` (frontend) are free, installs dependencies
(`uv sync` in `webui/backend/`, `npm install` in `webui/frontend/`), launches
both servers in the background, waits for the backend `/api/health` endpoint and
the frontend to become ready, then smoke-tests the vite proxy (`/api` → backend).
PIDs are saved to `webui/{backend,frontend}/.server.pid` and logs to
`.server.log` next to each server.

`stop.sh` kills by PID from those files, verifies each port is actually free,
and falls back to port-based kill if an orphaned child is holding the socket
(for example when the launcher spawns a wrapper process). With `--force` it
escalates to SIGKILL after 5s.

Once running, open **http://localhost:5173** in your browser. The Vite dev
server proxies `/api` requests to the backend automatically (target port is
`8001`, configured in `vite.config.ts`).

### Backend (Python >= 3.13)

Dependencies are managed with [uv](https://docs.astral.sh/uv/). From `webui/backend`:

```bash
cd webui/backend
uv sync                     # create .venv and install runtime + dev deps from uv.lock
uv run uvicorn app.main:app --reload --host 127.0.0.1 --port 8001
```

`uv` provisions a matching Python interpreter automatically (downloading one if
needed) and resolves dependencies from the committed `uv.lock`. API docs are then
available at <http://127.0.0.1:8001/docs>.

> To run against the compiled native `dal` package instead of the dev stub,
> install it into the uv environment (`uv pip install /path/to/dal`) and start
> the server with `DAL_REQUIRE_NATIVE=1`.

### Frontend

```bash
cd webui/frontend
npm install
./node_modules/.bin/vite    # http://localhost:5173 (proxies /api to :8001)
```

The Vite dev server proxies all `/api` requests to the backend URL configured in
`vite.config.ts` (default: `http://127.0.0.1:8001`). If you change the backend
port, update the `proxy.target` in that file and restart the frontend.

> **Note:** Run vite directly rather than `npm run dev` when launching by hand.
> `npm run` wraps the command in a parent process that does not forward SIGTERM,
> which leaves an orphan holding the port on shutdown. The `start.sh` script
> already does this for you.

### Stopping

If you started the services with `start.sh`, stop them with:

```bash
./webui/scripts/stop.sh
```

If you started them manually, press `Ctrl+C` in each terminal, or use the stop
script (it will fall back to port-based kill if no PID files exist).

### Troubleshooting

**Port 8001 already in use.** If the backend fails with `[Errno 98] Address
already in use`, either free the port or run on a different one:

```bash
# Option A — stop any running web UI
./webui/scripts/stop.sh

# Option B — find and kill whatever is using port 8001
fuser -k 8001/tcp

# Option C — use a different port (e.g. 8002)
uv run uvicorn app.main:app --reload --port 8002
```

If you choose Option C, also update the proxy target in
`webui/frontend/vite.config.ts` to match (`http://127.0.0.1:8002`) and restart
the frontend.

**Frontend can't reach the backend.** Make sure the backend starts *before* the
frontend. If you see proxy errors in the browser console, check that the backend
is running and the port in `vite.config.ts` matches.

## API

The backend exposes a REST-ish API under `/api`. Full OpenAPI docs are served at
`/docs` once the backend is running. Highlights beyond the standard CRUD:

| Endpoint | Notes |
|----------|-------|
| `GET /api/health` | Reports which DAL backend is active (native vs. stub). |
| `POST /api/products`, `PUT /api/products/{id}` | Create / partially update a scripted product. |
| `POST /api/products/debug` | Render the DAL `Product_Debug` dump for arbitrary rows. |
| `POST /api/models`, `PUT /api/models/{id}` | Black-Scholes or Dupire model data. |
| `POST /api/trades`, `PUT /api/trades/{id}` | Link a product + model + notional. |
| `POST /api/portfolios/{id}/trades/{tid}` | Add a trade to a portfolio. |
| `POST /api/trades/{id}/value` | Start an **async** single-trade valuation (returns `status: "running"`). |
| `POST /api/portfolios/{id}/value` | Start an **async** portfolio valuation (returns `status: "running"`). |
| `GET /api/valuations/{id}` | Poll until `status` becomes `"completed"` or `"failed"`. |

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
cd webui/backend
uv run pytest               # runs against the in-process DAL stub
```

```bash
cd webui/frontend
npm run build               # type-check + production build
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
