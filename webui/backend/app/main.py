"""FastAPI application entry point for the DAL portfolio-management web UI."""

from __future__ import annotations

import os

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app import __version__
from app.routers import models, portfolios, products, system, trades
from app.services.store import get_store
from app.services.templates import seed_demo_data


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

    origins = os.environ.get(
        "WEBUI_CORS_ORIGINS",
        "http://localhost:5173,http://127.0.0.1:5173",
    ).split(",")
    app.add_middleware(
        CORSMiddleware,
        allow_origins=[o.strip() for o in origins if o.strip()],
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    app.include_router(system.router)
    app.include_router(products.router)
    app.include_router(models.router)
    app.include_router(trades.router)
    app.include_router(portfolios.router)

    if os.environ.get("WEBUI_SEED_DEMO", "1").strip().lower() not in {"0", "false", "no"}:
        seed_demo_data(get_store())

    return app


app = create_app()
