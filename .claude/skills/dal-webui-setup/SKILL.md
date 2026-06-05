---
name: dal-webui-setup
description: Start or stop the DAL derivatives portfolio web UI (FastAPI backend + React/Vite frontend). Use when the user says "start the web UI", "run the webui", "stop the web UI", "shut down the webui", "launch the dashboard", or anything about bringing the DAL web UI up or down.
user-invocable: true
---

# DAL Web UI — Start / Stop

Brings up or tears down the two-service web UI that sits on top of the DAL Python public API.

Two scripts handle the actual work:

| Command | What it does |
|---------|--------------|
| `./webui/scripts/start.sh` | Checks prerequisites, starts backend (uvicorn on `:8001`) and frontend (vite on `:5173`), waits for both, runs a smoke test. |
| `./webui/scripts/stop.sh [--force]` | Stops both services (by PID file, falling back to port-based kill). Use `--force` to escalate to SIGKILL. |

## When to use

- **User wants to start the web UI** → run `./webui/scripts/start.sh`
- **User wants to stop the web UI** → run `./webui/scripts/stop.sh`
- **User wants to run tests** → the skill can also invoke the test suites directly (see below).

## Startup flow

When the user asks to start the web UI:

```bash
./webui/scripts/start.sh
```

The script:
1. Verifies prerequisites (python ≥ 3.13, uv, node, npm)
2. Reads the backend port from `webui/frontend/vite.config.ts` (currently `:8001`)
3. Checks that both ports are free
4. Runs `uv sync` in `webui/backend/`
5. Starts uvicorn in the background (PID saved to `webui/backend/.server.pid`)
6. Waits for `/api/health` to respond (up to 20s)
7. Runs `npm install` in `webui/frontend/`
8. Starts vite in the background (PID saved to `webui/frontend/.server.pid`)
9. Waits for `:5173` to respond (up to 30s)
10. Smoke-tests the proxy (frontend → backend)
11. Prints the URLs

Logs go to `webui/backend/.server.log` and `webui/frontend/.server.log`.

## Shutdown flow

When the user asks to stop the web UI:

```bash
./webui/scripts/stop.sh
```

The script:
1. Reads the backend port from `vite.config.ts`
2. Kills the backend by PID (from `.server.pid`), or by port if no PID file
3. Kills the frontend the same way
4. Removes PID files
5. Verifies both ports are free

If a service refuses to stop within 5s, the script warns. Re-run with `--force` to escalate to SIGKILL.

## Running tests

If the user asks to run tests after starting the UI:

```bash
# Backend tests (pytest, runs against the stub by default)
cd webui/backend && uv run pytest

# Frontend type-check + production build
cd webui/frontend && npm run build
```

## Native vs. stub DAL backend

By default the backend uses `dal_stub.py` (pure-Python closed-form Black-Scholes). To use the compiled SWIG bindings:

1. Build `dal-python` per the repo root `README.md`.
2. Install into the uv env: `uv pip install ../dal-python` (from `webui/backend/`).
3. Set `DAL_REQUIRE_NATIVE=1` before starting the backend.

The start script respects environment variables, so you can do:

```bash
DAL_REQUIRE_NATIVE=1 ./webui/scripts/start.sh
```

## Troubleshooting

- **Port already in use** — run `./webui/scripts/stop.sh` first, or manually free the port with `sudo fuser -k <port>/tcp`.
- **Backend fails to start** — check `webui/backend/.server.log`. Common causes: missing dependencies, port conflict, Python version mismatch.
- **Frontend fails to start** — check `webui/frontend/.server.log`. Common causes: port conflict, node_modules out of date (try `rm -rf node_modules && npm install`).
- **Proxy not forwarding** — verify `webui/frontend/vite.config.ts` has the correct `proxy.target` port, and that the backend is actually running.

## Reference

For full details on the web UI architecture, see `webui/README.md`.
