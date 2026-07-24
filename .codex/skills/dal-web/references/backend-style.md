# dal-web Backend Code Style (Python / FastAPI)

This rule governs the FastAPI backend under `dal-web/backend/app/`. It is a sibling
to [the DAL C++ style reference](../../../references/code-style.md) and
[the DAL web design system](design-system.md).

## Async-First Default

- New endpoints MUST be declared `async def`.
- Existing `@router.{get,post,put,delete,...}` handlers in `app/routers/*.py` are
  `async def`; do not add new synchronous `def` handlers.
- Request-path dependencies (`store_dependency`, `gateway_dependency` in
  `app/dependencies.py`) and request-path helpers are `async def`.
- Plain `def` remains correct only for module-level helpers that never touch the
  request path.

## Offload Blocking C++

- Inside any `async def`, wrap blocking DAL-extension calls in
  `await asyncio.to_thread(...)`. This applies in particular to the pybind11
  `gateway.value(...)` Monte Carlo entry point in `app/services/dal_gateway.py`
  and to any other `gateway.*` C++ entry point reached during request handling.
- Reason: these are synchronous calls from the Python caller's perspective, so
  calling them directly blocks the event-loop thread. The valuation and
  evaluation-date bindings release the GIL during native work, but GIL release
  does not make the Python call awaitable or keep the event loop responsive.
- Apply the rule uniformly: offload every DAL-extension call from `async def`,
  including cheap ones like `gateway.get_evaluation_date()`, so the invariant is
  easy to grep for.

## Sync Persistence Layer (Carve-Out)

- The `Store` / `DbStore` seam in `app/services/db/` is synchronous SQLAlchemy
  2.x, not async SQLAlchemy. Current async handlers and services call that seam
  directly; preserve this interface and call pattern for changes whose scope
  does not include persistence architecture.
- Direct store calls are not non-blocking. `DbStore` performs synchronous
  selects and commits, and `DAL_WEB_DB_URL` may select a remote SQLAlchemy
  backend, so database latency blocks the event-loop thread while a call is in
  progress. Keep request-path transactions and queries bounded, and include
  event-loop impact when reviewing a new or expanded store operation.
- If a task changes the persistence or concurrency architecture, choose and
  validate one consistent boundary for the complete store surface; do not mix
  ad hoc per-call offloads or async drivers into the synchronous seam.
- This compatibility carve-out from the uniform C++ `to_thread` rule describes
  the current handler contract, not a claim that database I/O is CPU-only or
  always cheap.

## Naming Honesty

- The `_async` suffix means "coroutine on the async pricing path." It is honest on
  an `async def` that callers `await` (or schedule via `asyncio.create_task`); the
  body need not `await` itself if it delegates scheduling — e.g.
  `value_portfolio_async` / `value_single_trade_async` in `valuation.py`, which
  hand off to `_schedule_pricing(...)` and are awaited by the routers.
- The actual smell the async refactor removed is a **plain `def`** function named
  `*_async` (a sync function lying about being a coroutine). Do not reintroduce
  that regression.
- A function named `*_async` that is neither `await`ed by callers nor schedules a
  task is a real smell: drop the suffix or make it a genuine coroutine.

## No Input Mutation (Functional Style)

- **Functional style is the default.** Prefer pure functions, return new state, and treat inputs as immutable wherever proper. This composes with the async-first model: pure functions are cheap to reason about across `await` boundaries and `asyncio.to_thread(...)` offloads.
- **It is forbidden for a function to mutate its input parameters' state.** Inputs must be treated as immutable; construct and return new state instead. This covers in-place edits of passed-in `dict` / `list` / Pydantic models, reassigning attributes on an argument, and mutable default arguments.
- Why: input mutation creates aliased, hidden state changes that are hard to trace in a concurrent, async codebase, and it breaks the assumption callers rely on when they pass the same object to multiple services or re-use it after the call. The immutability invariant is what makes the rest of the async design safe to read.

## External HTTP Contract Is Immutable

- Async conversion MUST NOT change routes, request/response JSON shapes, HTTP
  status codes, or the `running → completed | failed` polling semantics of the
  existing API.
- `NotFoundError` still maps to HTTP 404 with the same `detail` string.
- Pydantic `response_model` schemas stay unchanged.

## Python Style

- Type hints required on public function signatures.
- 4-space indentation; no tabs.
- `from __future__ import annotations` is not required.
- Format with `ruff format`. Keep imports ordered.
- Files end with a newline.

## References

- C++ style: [code-style.md](../../../references/code-style.md)
- Frontend design: [design-system.md](design-system.md)
- Governs: `dal-web/backend/app/`
