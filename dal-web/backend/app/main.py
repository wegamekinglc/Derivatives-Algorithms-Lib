"""FastAPI application entry point for the DAL portfolio-management web UI."""

from __future__ import annotations

import logging
import os

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app import __version__
from app.routers import models, portfolios, products, system, trades
from app.services.store import get_store, is_memory_mode
from app.services.templates import seed_demo_data

logger = logging.getLogger(__name__)


def _init_database() -> None:
    """Create or migrate the database schema before serving requests.

    ``DAL_WEB_STORE=memory`` skips this entirely -- there is no database to
    initialise. Otherwise the schema is brought up to date either by Alembic
    (``DAL_WEB_AUTO_MIGRATE=1``) or by ``create_all()`` as the zero-friction
    dev / test default.
    """
    if is_memory_mode():
        return
    store = get_store()
    if os.environ.get("DAL_WEB_AUTO_MIGRATE", "").strip().lower() in {"1", "true", "yes"}:
        _run_alembic_upgrade()
        return
    # Dev / test default: ensure tables exist. Idempotent.
    store.create_all()  # type: ignore[attr-defined]


def _run_alembic_upgrade() -> None:
    """Run ``alembic upgrade head`` in-process against the configured DB."""
    from pathlib import Path

    from alembic import command
    from alembic.config import Config

    backend_dir = Path(__file__).resolve().parent.parent
    cfg = Config(str(backend_dir / "alembic.ini"))
    cfg.set_main_option("script_location", str(backend_dir / "migrations"))
    command.upgrade(cfg, "head")


def _reconcile_orphaned_valuations() -> None:
    """Mark in-flight valuations as failed when the server restarts.

    H6 regression: the async task queue lives in-process; a restart orphans
    every ``running`` valuation.  This reconciliation runs once at startup
    so callers do not poll forever.
    """
    if is_memory_mode():
        return
    store = get_store()
    for valuation in store.list_valuations():
        if valuation.status == "running":
            store.update_valuation(
                valuation.id,
                {
                    "status": "failed",
                    "error_message": "Server restarted while pricing",
                },
            )
            logger.info("Reconciled orphaned valuation %s to failed", valuation.id)


def create_app() -> FastAPI:
    app = FastAPI(
        title="DAL Derivatives Portfolio Management",
        version=__version__,
        description=(
            "Web UI backend for managing and pricing derivatives portfolios. "
            "All quantitative work is delegated to the Derivatives Algorithms "
            "Library through its Python public API."
        ),
    )

    raw_origins = os.environ.get(
        "WEBUI_CORS_ORIGINS",
        "http://localhost:5173,http://127.0.0.1:5173",
    )
    origins = [o.strip() for o in raw_origins.split(",") if o.strip()]
    # Never combine wildcard origins with credentials — browsers reject it
    # and it is a CORS misconfiguration.
    allow_credentials = "*" not in origins
    app.add_middleware(
        CORSMiddleware,
        allow_origins=origins,
        allow_credentials=allow_credentials,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    app.include_router(system.router)
    app.include_router(products.router)
    app.include_router(models.router)
    app.include_router(trades.router)
    app.include_router(portfolios.router)

    _init_database()
    _reconcile_orphaned_valuations()

    if os.environ.get("WEBUI_SEED_DEMO", "1").strip().lower() not in {"0", "false", "no"}:
        seed_demo_data(get_store())

    return app


app = create_app()
