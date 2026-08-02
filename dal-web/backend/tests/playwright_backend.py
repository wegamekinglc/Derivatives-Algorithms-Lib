"""Playwright-only FastAPI entry point backed by the canned DAL test double.

Importing this module is guarded by ``DAL_PLAYWRIGHT_TEST_BACKEND=1`` so it
cannot silently become a development or production fallback.  The regular
``app.main`` entry point remains native-only.
"""

from __future__ import annotations

import os
import sys
from decimal import Decimal

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
    from tests.fake_gateway import FakeDalGateway

    class CannedDalGateway(FakeDalGateway):
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

        def price_curve_lab_trades(
            self,
            document,
            trades,
            _evaluation_time,
            base_currency,
            **kwargs,
        ):
            check_deadline = kwargs.get("check_deadline")
            if callable(check_deadline):
                check_deadline()
            quote_total = sum(
                (
                    Decimal(str(instrument["raw_quote"]))
                    for instrument in document["instruments"]
                    if instrument.get("included", True)
                ),
                Decimal(0),
            )
            parameter_shift = sum(
                (Decimal(str(bump)) for _, bump in (kwargs.get("parameter_bumps") or ())),
                Decimal(0),
            )
            parameter_axis = kwargs.get("parameter_axis") or ()
            include_node_sensitivities = bool(kwargs.get("include_node_sensitivities"))
            component_keys = [
                str(declaration["component_key"]) for declaration in document["declarations"]
            ]
            return [
                {
                    "trade_id": trade["trade_id"],
                    "instrument_type": trade["instrument_type"],
                    "succeeded": True,
                    "pv": format(
                        Decimal("100") + quote_total + parameter_shift,
                        "f",
                    ),
                    "currency": base_currency,
                    "required_historical_fixings": [],
                    "missing_historical_fixings": [],
                    "dependency_component_keys": component_keys,
                    "error": "",
                    "aad_node_gradient": (
                        ["1" for _ in parameter_axis] if include_node_sensitivities else None
                    ),
                    "aad_ineligibility_reason": None,
                }
                for trade in trades
            ]

    gateway = CannedDalGateway()

    async def canned_gateway_dependency() -> DalGateway:
        return gateway

    fastapi_app.dependency_overrides[gateway_dependency] = canned_gateway_dependency
    return fastapi_app


app = _build_app()
