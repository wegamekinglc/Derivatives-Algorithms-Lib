#!/usr/bin/env python3
"""Smoke-test the web gateway against the compiled dal-python extension."""

from __future__ import annotations

import math
from pathlib import Path

import dal._dal as native_extension
from app.services.dal_gateway import DalGateway, ValuationRequest


def main() -> None:
    extension_path = Path(native_extension.__file__ or "")
    if extension_path.suffix not in {".so", ".pyd", ".dylib"}:
        raise AssertionError(f"expected a compiled DAL extension, got {extension_path}")

    gateway = DalGateway()
    if not gateway.is_native or gateway.backend_name != "dal":
        raise AssertionError("the web gateway did not select the native DAL backend")

    gateway.set_evaluation_date(2022, 9, 15)
    if gateway.get_evaluation_date() != "2022-09-15":
        raise AssertionError("native evaluation-date round trip failed")

    result = gateway.value(
        ValuationRequest(
            event_dates=["STRIKE", {"date": "2023-09-15"}],
            events=["100.0", "call pays MAX(spot() - STRIKE, 0.0)"],
            model_kind="BSModelData_",
            model_params={"spot": 100.0, "vol": 0.2, "rate": 0.0, "div": 0.0},
            num_paths=256,
            enable_aad=False,
            evaluation_date=(2022, 9, 15),
        )
    )
    pv = result.get("PV")
    if pv is None or not math.isfinite(pv) or pv <= 0.0:
        raise AssertionError(f"native gateway returned an invalid PV: {result}")
    print(f"Native web gateway smoke test passed (PV={pv:.10g}).")


if __name__ == "__main__":
    main()
