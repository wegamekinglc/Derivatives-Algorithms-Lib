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

Load and follow [web operations](references/operations.md) for platform dispatch, startup,
health and proxy checks, logs, graceful/forced shutdown, port-owner cleanup, persistence, and
troubleshooting.

## Tests

```bash
(cd dal-web/backend && uv run pytest)
(cd dal-web/frontend && npm run build)
./dal-web/scripts/setup-playwright.sh
(cd dal-web/frontend && npm run test:e2e)
```

Run Playwright when frontend/backend contract or user-facing flows change.

## Backend Rules

Load and follow the complete [backend style reference](references/backend-style.md) before
changing FastAPI request paths, DAL extension calls, persistence, or external HTTP behavior.

## Frontend Style

Load and follow the complete [design system](references/design-system.md) before changing the
React UI or CSS. The style is data-dense, technical, dark, financial, and restrained.

## References

- [Web operations](references/operations.md): start, stop, tests, persistence, and troubleshooting.
- [Backend style](references/backend-style.md): async FastAPI and immutable-input rules.
- [Design system](references/design-system.md): complete financial-terminal UI specifications.
- [Web standards index](references/web-standards.md): concise routing digest for these references.
