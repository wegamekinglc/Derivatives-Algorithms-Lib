"""Playwright-only FastAPI entry point backed by the canned DAL test double.

Importing this module is guarded by ``DAL_PLAYWRIGHT_TEST_BACKEND=1`` so it
cannot silently become a development or production fallback.  The regular
``app.main`` entry point remains native-only.
"""

from __future__ import annotations

import os
import sys

from fastapi import FastAPI

from tests.fake_dal import build_fake_dal


def _build_app() -> FastAPI:
    if os.environ.get("DAL_PLAYWRIGHT_TEST_BACKEND") != "1":
        raise RuntimeError("The canned Playwright backend requires DAL_PLAYWRIGHT_TEST_BACKEND=1")

    # Force process-local, empty state regardless of the caller's environment.
    os.environ["DAL_WEB_STORE"] = "memory"
    os.environ["WEBUI_SEED_DEMO"] = "0"
    sys.modules["dal"] = build_fake_dal()

    # Import only after the fake module is installed.  These are the production
    # app and routers; only the gateway dependency is replaced below.
    from app.dependencies import gateway_dependency
    from app.main import app as fastapi_app
    from app.services.dal_gateway import DalGateway, HealthSnapshot

    class CannedDalGateway(DalGateway):
        @property
        def is_native(self) -> bool:
            return False

        @property
        def backend_name(self) -> str:
            return "canned-dal"

        def health_snapshot(self) -> HealthSnapshot:
            snapshot = super().health_snapshot()
            return HealthSnapshot(
                backend=self.backend_name,
                is_native=self.is_native,
                evaluation_date=snapshot.evaluation_date,
            )

    gateway = CannedDalGateway()

    async def canned_gateway_dependency() -> DalGateway:
        return gateway

    fastapi_app.dependency_overrides[gateway_dependency] = canned_gateway_dependency
    return fastapi_app


app = _build_app()
