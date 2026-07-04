# DAL Web Standards

## Architecture

- `dal-web/backend/`: FastAPI app using the compiled `dal` Python package.
- `dal-web/frontend/`: React, TypeScript, Vite.
- Start/stop scripts live under `dal-web/scripts/`.
- Default frontend URL: `http://localhost:5173`.
- Default backend docs URL: `http://127.0.0.1:8001/docs`.

## Backend Rules

- New request handlers are `async def`.
- Request-path dependencies and helpers should be async.
- Inside async code, offload blocking DAL extension calls with `await asyncio.to_thread(...)`.
- The SQLAlchemy `Store`/`DbStore` seam is deliberately synchronous and may be called directly from handlers.
- Do not rewrite the store to async without an explicit decision.
- External HTTP contract is immutable unless requested: routes, JSON shapes, status codes, and `running -> completed | failed` polling stay stable.
- Use type hints on public signatures.
- Format Python with `ruff format`.

## Persistence

| Variable               | Default                               | Meaning                                      |
|------------------------|---------------------------------------|----------------------------------------------|
| `DAL_WEB_DB_URL`       | local SQLite under backend `.data/`   | SQLAlchemy URL for persistence               |
| `DAL_WEB_STORE`        | unset                                 | `memory` uses the legacy in-memory store     |
| `DAL_WEB_AUTO_MIGRATE` | unset                                 | `1` runs Alembic upgrade on startup          |

## Tests

```bash
(cd dal-web/backend && uv run pytest)
(cd dal-web/frontend && npm run build)
./dal-web/scripts/setup-playwright.sh
(cd dal-web/frontend && npm run test:e2e)
```

## Frontend Design

Use an industrial terminal style inspired by trading desks:

- Dark layered backgrounds.
- Gold/amber accents for active or important controls.
- Green for positive/success, red for negative/error, amber for running/warning.
- Data-dense layouts, restrained decoration, high contrast.
- Top navigation, not sidebar navigation.
- Full-width content; avoid unnecessary `max-width`.
- Spacing follows an 8px grid.
- Border radius is 6px or less for cards and 4px for controls.
- No glow effects, grid backgrounds, purple gradients, bounce, or rotation animations.
- Numbers use JetBrains Mono and tabular numerals.
- Labels are uppercase, small, and bold.

Core palette:

```css
--bg: #0f1419;
--bg-2: #161b22;
--bg-3: #21262d;
--bg-elevated: #2d333b;
--border: #30363d;
--text: #c9d1d9;
--text-dim: #8b949e;
--accent: #d4a017;
--accent-2: #b8860b;
--green: #2ea043;
--red: #da3633;
--amber: #d29922;
```

Styles are centralized in `dal-web/frontend/src/styles.css`.
