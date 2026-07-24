# DAL Web Standards Index

Use this digest to select the complete Codex-owned DAL web reference:

- Start, stop, health checks, logs, tests, persistence, or troubleshooting:
  [operations.md](operations.md).
- FastAPI handlers, DAL extension calls, async behavior, input immutability, or HTTP contracts:
  [backend-style.md](backend-style.md).
- React UI, CSS, palette, typography, layout, components, animation, or design review:
  [design-system.md](design-system.md).

Core invariants:

- Use the repository launchers selected by platform; verify frontend, backend, and proxy health.
- Keep request paths async, offload blocking DAL extension calls, preserve the intentional
  synchronous store seam, and treat inputs as immutable.
- Preserve routes, JSON shapes, status codes, and `running -> completed | failed` polling unless
  the user requests an API change.
- Keep the UI data-dense and financial: layered dark backgrounds, restrained gold accents,
  monospace tabular numbers, top navigation, full-width content, compact radii, and no glow.
