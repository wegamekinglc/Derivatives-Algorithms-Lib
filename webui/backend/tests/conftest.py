"""Pytest fixtures: force the in-process DAL stub so tests need no C++ build."""

from __future__ import annotations

import os

import pytest

# Ensure the gateway binds to the pure-python stub regardless of environment.
os.environ["DAL_MODULE"] = "app.services.dal_stub"
os.environ.pop("DAL_REQUIRE_NATIVE", None)
os.environ["WEBUI_SEED_DEMO"] = "0"


@pytest.fixture()
def client():
    from fastapi.testclient import TestClient

    # Reset singletons so each test starts from a clean store/gateway.
    import app.services.dal_gateway as gw
    import app.services.store as st

    gw._gateway = None
    st._store = None

    from app.main import create_app

    with TestClient(create_app()) as c:
        yield c
