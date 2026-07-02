# dal-web database persistence

## Goal

Give the dal-web backend real persistent storage backed by a database, with
**SQLite as the default and a switchable backend**. Today every entity lives in
`Store`, a thread-safe in-memory dict that is lost on restart; this spec replaces
that with a database-backed store while keeping the router-facing surface
identical.

## Scope

- Persist **all five entities**: products, models, trades, portfolios, and
  valuation results/history.
- Default backend: **SQLite** (local file). Switchable to any SQLAlchemy-supported
  backend (Postgres, MySQL, ...) by changing one connection URL env var.
- Switchability is **design-time, not CI-proven**: the URL mechanism is real and
  portable, but this work does not stand up a Postgres service or run the suite
  against it.

## Decisions (approved)

| Decision | Choice |
|----------|--------|
| Where DB logic lives | DB-backed `Store` implementing today's public interface (routers untouched) |
| ORM | SQLAlchemy 2.x, **sync** (separate ORM models from Pydantic schemas) |
| Schema management | Alembic migrations, with `create_all()` as the dev/test fast path |
| Backend selection | `DAL_WEB_DB_URL` env var (SQLAlchemy URL), defaults to a local SQLite file; `DAL_WEB_STORE=memory` opts into the legacy in-memory store |
| Column mapping | Hybrid: scalar fields as columns, nested/variable blobs as JSON columns |
| Branch | New branch `feature/dal-web-db-persistence` off `master` |

Rejected: sessions-in-routers (rewrites routers, discards the `Store` seam);
SQLModel (nested schemas don't fit flat tables); async SQLAlchemy (app is
C++-pricing-bound, not DB-bound, and doubles the driver matrix); `create_all()`
-only (no schema evolution).

## Architecture

The current `Store` (`dal-web/backend/app/services/store.py`) is a clean seam:
routers depend only on its public methods, and its docstring states it is meant
to be swapped without touching routers. This design adds a second implementation
behind that seam.

New package `dal-web/backend/app/services/db/`:

- **`session.py`** — builds the `Engine` and a thread-safe `sessionmaker` from
  `DAL_WEB_DB_URL`. Default URL: `sqlite:///<backend>/.data/dalweb.db`
  (`.data/` gitignored). When the URL is SQLite, apply
  `PRAGMA journal_mode=WAL`, `PRAGMA foreign_keys=ON`, and create the engine with
  `check_same_thread=False`. Lazy process singleton.
- **`models.py`** — SQLAlchemy 2.0 declarative ORM models, **separate from the
  Pydantic schemas**, with `to_schema()` / `from_schema()` mappers.
- **`store_db.py`** — `DbStore` implementing the `Store` protocol. **Each method
  opens a short-lived session, commits, and closes**, so no session ever crosses
  an `asyncio.to_thread` boundary. This is what makes it safe with the existing
  worker-thread pricer in `valuation.py`.
- **`migrations/`** — Alembic env + initial migration creating all tables.

`store.py` changes:

- Extract `Store`'s public method surface into a `typing.Protocol`
  (`StoreProtocol`) so both implementations satisfy it.
- **Keep the existing in-memory `Store`** (used for no-DB mode and as a reference).
- `get_store()` returns a `DbStore` by default (using `DAL_WEB_DB_URL`, or the
  default local SQLite file when unset). Set `DAL_WEB_STORE=memory` to get the
  legacy in-memory `Store` (no DB, no SQLAlchemy) — the escape hatch for smoke
  tests or read-only environments.
- `NotFoundError` / `ConflictError` stay in `store.py` where routers import them.

### Concurrency note

`valuation.py`'s async path offloads `_price_trade` (which reads
`store.get_product` / `store.get_model`) to worker threads via
`asyncio.to_thread`, then writes results back with `store.update_valuation` from
the event-loop thread. Per-method short-lived sessions handle this correctly:
each call is self-contained and thread-local. No long-lived session is shared
across threads.

## ORM schema (hybrid mapping)

Scalar fields are columns; nested/variable structures are portable JSON columns
(SQLite JSON1 / Postgres JSONB):

| table | columns |
|-------|---------|
| `product` | `id` PK, `name`, `description`, `template`, `rows` JSON |
| `model` | `id` PK, `name`, `kind`, `params` JSON (holds `bs` or `dupire`) |
| `trade` | `id` PK, `name`, `book`, `counterparty`, `notional`, `quantity`, `product_id` FK→product, `model_id` FK→model, `tags` JSON |
| `portfolio` | `id` PK, `name`, `description` |
| `portfolio_trade` | `portfolio_id` FK, `trade_id` FK, `position` — composite PK (replaces the `trade_ids` list) |
| `valuation` | `id` PK, `target_kind`, `target_id`, `backend`, `is_native`, `config` JSON, `total_pv`, `total_greeks` JSON, `trades` JSON, `created_at`, `status`, `error_message` |

`portfolio_trade` uses `ON DELETE CASCADE`. `valuation` keeps history even if its
target trade/portfolio is deleted (no FK to target) — valuation results are an
audit trail.

## Backend switching

- Env var `DAL_WEB_DB_URL` holds a SQLAlchemy URL. Default:
  `sqlite:///<repo>/dal-web/backend/.data/dalweb.db`.
- `DAL_WEB_DB_URL=postgresql+psycopg://host/db ./dal-web/scripts/start.sh`
  switches backend. The start scripts already pass the caller environment through.
- For ephemeral/test runs: `sqlite:///:memory:` or a temp file.
- `DAL_WEB_STORE=memory` bypasses the DB entirely and uses the legacy in-memory
  store (no file, no SQLAlchemy).

## Error handling and integrity

- `DbStore` raises the same `NotFoundError` / `ConflictError` so routers'
  `try/except` blocks are untouched.
- Map SQLAlchemy `IntegrityError` (e.g. add_trade pointing at a missing
  product/model) to `NotFoundError`; keep the explicit delete-guard checks for
  referenced product/model since they produce clearer messages than a raw FK error.
- Portfolio→trade membership changes go through `portfolio_trade`; removing a
  trade cascades its membership rows.

## Seeding

`seed_demo_data` becomes idempotent: only seeds when the products/portfolios
tables are empty. Still gated by the existing `WEBUI_SEED_DEMO` env flag.

## Testing

- `conftest.py`: the `store` fixture backs each test with a fresh temp-file
  SQLite DB so persistence tests exercise the real `DbStore` path (no fake store
  for persistence). Gateway/valuation tests keep their existing fake `dal`.
- Existing API tests pass unchanged because the routers and the `Store` interface
  are unchanged.
- New tests:
  - data survives a session close/reopen (the core persistence guarantee);
  - delete a product still referenced by a trade → `ConflictError`;
  - JSON-column round-trip (nested `rows`, model params, valuation `trades`);
  - store selection: `DAL_WEB_STORE=memory` returns the legacy in-memory store;
    otherwise `DbStore` (default SQLite file, or `DAL_WEB_DB_URL` when set).

Run with `(cd dal-web/backend && uv run pytest)`.

## Dependencies added

`dal-web/backend/pyproject.toml`: `sqlalchemy>=2.0`, `alembic`. Sync drivers only
(`sqlite3` is stdlib; Postgres/MySQL drivers are the caller's responsibility when
they switch the URL).

## Startup

`main.create_app()`: build the engine, run `create_all()` (dev default,
zero-friction) or `alembic upgrade head` (when migrations are the chosen path),
then the conditional idempotent seed.

## Known edge (pre-existing, now visible)

If the server restarts while an async valuation is in flight, its row is left at
`status="running"` forever (today the in-memory store simply loses it).
Mitigation, if desired later: on startup, mark stale `running` rows as `failed`.
Out of scope for this change unless explicitly requested.

## File plan

New:

- `dal-web/backend/app/services/db/__init__.py`
- `dal-web/backend/app/services/db/session.py`
- `dal-web/backend/app/services/db/models.py`
- `dal-web/backend/app/services/db/store_db.py`
- `dal-web/backend/app/services/db/migrations/` (Alembic env + initial revision)
- `dal-web/backend/alembic.ini`

Modified:

- `dal-web/backend/app/services/store.py` — extract `StoreProtocol`, keep in-memory `Store`, route `get_store()`.
- `dal-web/backend/app/main.py` — init DB on startup, idempotent seed.
- `dal-web/backend/tests/conftest.py` — DB-backed `store` fixture.
- `dal-web/backend/pyproject.toml` — add sqlalchemy, alembic.
- `dal-web/.gitignore` (or repo `.gitignore`) — ignore `.data/`.
- `dal-web/README.md`, `.claude/skills/dal-web-setup/SKILL.md`, `CLAUDE.md` — document the DB and `DAL_WEB_DB_URL`.

## Acceptance criteria

1. With no env vars set, the backend persists to the default local SQLite file.
   `DAL_WEB_STORE=memory` runs the legacy in-memory store instead.
2. With `DAL_WEB_DB_URL=sqlite:///<file>`, all five entity types survive a
   backend restart.
3. All existing `dal-web/backend` tests pass unchanged.
4. New persistence tests pass (restart-survival, referential-integrity,
   JSON round-trip, store selection).
5. `DAL_WEB_DB_URL` pointing at a non-SQLite URL selects the corresponding
   SQLAlchemy dialect without code changes.
6. Switching to SQLite uses WAL + FK pragmas automatically.

## Out of scope

- Postgres/MySQL CI or docker-compose.
- Auth / multi-tenancy.
- Stale-`running`-valuation cleanup on startup (noted above).
- Changing the async valuation execution model.
