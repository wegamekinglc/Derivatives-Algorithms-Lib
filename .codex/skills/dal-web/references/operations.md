# DAL Web Operations Reference

Brings up or tears down the two-service web UI that sits on top of the DAL Python public API.

## Contents

- [Platform dispatch](#platform-dispatch)
- [Startup flow](#startup-flow)
- [Shutdown flow](#shutdown-flow)
- [Running tests](#running-tests)
- [Compiled DAL backend](#dal-backend-dal-python)
- [Persistence](#persistence)
- [Troubleshooting](#troubleshooting)

Four launcher scripts handle the actual work — pick by platform:

| Platform          | Start                                                                     | Stop (graceful → force)                                                                 |
|-------------------|---------------------------------------------------------------------------|-----------------------------------------------------------------------------------------|
| Linux / macOS     | `./dal-web/scripts/start.sh`                                              | `./dal-web/scripts/stop.sh` → `./dal-web/scripts/stop.sh --force`                       |
| Windows (pwsh 7+) | `pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/start.ps1` | `pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/stop.ps1` → add `-Force` |

`dal-web/scripts/setup-playwright.sh` (one-time frontend e2e browser/runtime setup) has no PowerShell equivalent; run it under bash/git-bash on Windows.

## Platform dispatch

Choose the launcher by platform, not by guess:

- **Windows** — when `pwsh` is on `PATH` (PowerShell 7+) or the session platform is `win32`, use the `.ps1` scripts.
- **Linux / macOS / WSL / git-bash** — otherwise use the `.sh` scripts.

The two families are behaviourally equivalent (same prereqs, ports, health checks, smoke test, PID/log files, exit codes). Two platform differences are worth remembering:

- **Prereq command name:** the bash script checks `python3`; the PowerShell script checks `python`.
- **Log files:** on Linux/macOS each service writes a single merged `.server.log`. On Windows each service writes two files — `.server.log` (stdout) and `.server.log.err` (stderr).
- **Force flag spelling:** `--force` (bash) versus `-Force` (PowerShell). Do not mix them.

## When to use

- **User wants to start the web UI** → run the start script for the current platform.
- **User wants to stop the web UI** → run the stop script for the current platform.
- **User wants to run tests** → the skill can also invoke the test suites directly (see below).

## Startup flow

When the user asks to start the web UI, run the launcher for the current platform:

```bash
./dal-web/scripts/start.sh                                              # Linux/macOS
```
```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/start.ps1  # Windows
```

The script:
1. Verifies prerequisite commands (python, uv, node, npm; `python3` on bash,
   `python` on PowerShell) and enforces Python ≥ 3.13. The committed frontend
   toolchain requires Node.js `^20.19.0` or `>=22.12.0`.
2. Reads the backend port from `dal-web/frontend/vite.config.ts` (currently `:8001`)
3. Checks that both ports are free
4. Runs `uv sync --inexact` in `dal-web/backend/` so the locally installed
   native `dal` package is preserved
5. Starts uvicorn in the background (PID saved to `dal-web/backend/.server.pid`)
6. Waits for `/api/health` to respond (up to 20s)
7. Runs `npm install` in `dal-web/frontend/`
8. Starts vite in the background (PID saved to `dal-web/frontend/.server.pid`)
9. Waits for `:5173` to respond (up to 30s)
10. Smoke-tests the proxy (frontend → backend)
11. Prints the URLs

Logs go to `.server.log` next to each server (plus a separate `.server.log.err` for stderr on Windows).

## Shutdown flow

When the user asks to stop the web UI, run the stopper for the current platform:

```bash
./dal-web/scripts/stop.sh                  # Linux/macOS; add --force to escalate
```
```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/stop.ps1   # Windows; add -Force to escalate
```

The script:
1. Reads the backend port from `vite.config.ts`
2. Kills the backend by PID (from `.server.pid`). On Windows it walks the PID's process tree first so child workers (uvicorn `--reload` worker, node/vite children) are terminated.
3. Kills the frontend the same way
4. Falls back to a port-based kill if a child still holds the socket after the PID kill
5. Removes PID files
6. Verifies both ports are free

If a service refuses to stop within 5s, the script warns. Re-run with `--force` (bash) or `-Force` (PowerShell) to escalate to a hard kill.

## Running tests

If the user asks to run tests after starting the UI:

```bash
# Backend tests (pytest, uses a fake dal module)
(cd dal-web/backend && uv run pytest)

# Frontend type-check + production build
(cd dal-web/frontend && npm run build)

# Frontend e2e smoke tests (Playwright; starts/stops the web UI itself)
./dal-web/scripts/setup-playwright.sh   # one-time browser/runtime setup
(cd dal-web/frontend && npm run test:e2e)
```

## DAL backend (dal-python)

The backend imports the compiled `dal` package (dal-python pybind11 bindings) directly -- there is no pure-Python fallback, so build and install it before running the server:

1. Build `dal-python` per the repo root `README.md`.
2. Install into the uv env: `uv pip install ../../dal-python` (from `dal-web/backend/`).

The start scripts inherit the caller's environment, so once `dal` is importable just run them directly:

```bash
./dal-web/scripts/start.sh
```

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/start.ps1
```

The pytest suite uses a fake `dal` module (see `dal-web/backend/tests/conftest.py`), so `uv run pytest` needs no C++ build.

## Persistence

The backend persists all entities (products, models, trades, portfolios, valuation results) to a SQLAlchemy database behind the `Store` seam. The start scripts pass the caller environment through, so the relevant variables work out of the box:

| Variable               | Default                               | Meaning                                                               |
|------------------------|---------------------------------------|-----------------------------------------------------------------------|
| `DAL_WEB_DB_URL`       | `sqlite:///<backend>/.data/dalweb.db` | SQLAlchemy URL for the DB. Point at Postgres/MySQL to switch.         |
| `DAL_WEB_STORE`        | unset                                 | `memory` bypasses the DB and uses the legacy in-memory store.         |
| `DAL_WEB_AUTO_MIGRATE` | unset                                 | `1` runs `alembic upgrade head` on startup; otherwise `create_all()`. |

Default SQLite file is gitignored under `dal-web/backend/.data/`. To run the Alembic migrations by hand: `cd dal-web/backend && uv run alembic upgrade head`.

## Troubleshooting

- **Port already in use** — run the stop script for your platform first, or manually free the port: `sudo fuser -k <port>/tcp` on Linux/macOS; on Windows, `Get-NetTCPConnection -LocalPort <port> -State Listen` then `Stop-Process -Id <pid> -Force`.
- **Backend fails to start** — check `dal-web/backend/.server.log` (and `.server.log.err` on Windows). Common causes: missing dependencies, port conflict, Python version mismatch.
- **Frontend fails to start** — check `dal-web/frontend/.server.log` (and `.server.log.err` on Windows). Common causes: port conflict, node_modules out of date (try `rm -rf node_modules && npm install`).
- **Proxy not forwarding** — verify `dal-web/frontend/vite.config.ts` has the correct `proxy.target` port, and that the backend is actually running.

## Reference

For full details on the web UI architecture, see `dal-web/README.md`.
