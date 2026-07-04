---
name: dal-web
description: Work on the DAL web UI and backend. Use when starting or stopping the FastAPI/React app, changing `dal-web/`, running backend/frontend/e2e tests, applying the industrial terminal UI style, or enforcing async backend rules.
---

# DAL Web

## Overview

Use this skill for `dal-web/` tasks: service startup/shutdown, backend FastAPI changes,
frontend React/Vite work, Playwright smoke tests, persistence configuration, and the
project's financial terminal UI style.

## Start And Stop

Linux, macOS, WSL, or git-bash:

```bash
./dal-web/scripts/start.sh
./dal-web/scripts/stop.sh
./dal-web/scripts/stop.sh --force
```

Windows PowerShell 7:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/start.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/stop.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/stop.ps1 -Force
```

The frontend is normally at `http://localhost:5173`; backend docs are at
`http://127.0.0.1:8001/docs`.

## Tests

```bash
(cd dal-web/backend && uv run pytest)
(cd dal-web/frontend && npm run build)
./dal-web/scripts/setup-playwright.sh
(cd dal-web/frontend && npm run test:e2e)
```

Run Playwright when frontend/backend contract or user-facing flows change.

## Backend Rules

- New request handlers are `async def`.
- Offload blocking DAL extension calls from async paths with `await asyncio.to_thread(...)`.
- The SQLAlchemy `Store`/`DbStore` seam is intentionally synchronous and may be called directly from handlers.
- Do not change route names, JSON shapes, status codes, or `running -> completed | failed` polling semantics unless explicitly requested.

## Frontend Style

Load `references/web-standards.md` before changing UI. The style is data-dense, technical,
dark, financial, and restrained.

## References

- `references/web-standards.md`: design palette, layout, component rules, backend async rules, and persistence variables.
