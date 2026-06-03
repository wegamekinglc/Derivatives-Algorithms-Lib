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
| `WEBUI_CORS_ORIGINS` | `localhost:5173` | Comma-separated allowed CORS origins.                 |

To use the real library, build the bindings, install the `dal` package into your
environment, then run the backend normally (with `DAL_REQUIRE_NATIVE=1` to be
strict). The `/api/health` endpoint reports which backend is active.

## Running

### Backend (Python >= 3.13)

Dependencies are managed with [uv](https://docs.astral.sh/uv/). From `webui/backend`:

```bash
cd webui/backend
uv sync                     # create .venv and install runtime + dev deps from uv.lock
uv run uvicorn app.main:app --reload --port 8000
```

`uv` provisions a matching Python interpreter automatically (downloading one if
needed) and resolves dependencies from the committed `uv.lock`. API docs are then
available at <http://127.0.0.1:8000/docs>.

> To run against the compiled native `dal` package instead of the dev stub,
> install it into the uv environment (`uv pip install /path/to/dal`) and start
> the server with `DAL_REQUIRE_NATIVE=1`.

### Frontend

```bash
cd webui/frontend
npm install
npm run dev                 # http://localhost:5173 (proxies /api to :8000)
```

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

* **Dashboard** -- portfolio counts, latest PV, recent valuation runs.
* **Portfolios** -- group trades into books, add/remove trades, price the book.
* **Trades** -- link a product + model + notional; price a single trade.
* **Product Builder** -- compose DAL scripted products as a schedule of
  (date/label, event) rows; load templates (European, up-and-out call, snowball);
  render the DAL product debug dump.
* **Models** -- create Black-Scholes model data.
* **Valuation Runs** -- reproducible history of every Monte Carlo run with PV and
  Greeks.
