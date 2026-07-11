"""Pytest fixtures: a fake ``dal`` module so tests need no C++ build.

The backend imports the compiled ``dal`` package (dal-python) directly. For
tests we register a minimal fake ``dal`` in :data:`sys.modules` before the app
imports the gateway, so the FastAPI wiring can be exercised without building the
C++ extension. In production the real ``dal`` is imported.
"""

from __future__ import annotations

import os
import sys

import pytest

from tests.fake_dal import build_fake_dal

# Seed a demo portfolio only when explicitly requested.
os.environ.setdefault("WEBUI_SEED_DEMO", "0")


# Install before any `import dal` inside the app under test.
sys.modules["dal"] = build_fake_dal()


def _reset_singletons() -> None:
    import app.services.dal_gateway as gw
    import app.services.store as st

    gw._gateway_box[0] = None
    st._store_box[0] = None


@pytest.fixture()
def store(tmp_path, monkeypatch):
    """A fresh ``DbStore`` against a per-test temp SQLite file.

    Persistence tests exercise the real DB path -- there is no fake store for
    the persistence layer. The env is rewired so ``get_store()`` inside the app
    would resolve to the same database.
    """
    _reset_singletons()
    db_path = tmp_path / "test.db"
    monkeypatch.delenv("DAL_WEB_STORE", raising=False)
    monkeypatch.setenv("DAL_WEB_DB_URL", f"sqlite:///{db_path}")

    from app.services.db.store_db import DbStore

    s = DbStore(url=f"sqlite:///{db_path}")
    s.create_all()
    yield s
    s.close()


@pytest.fixture()
def client(tmp_path, monkeypatch):
    from fastapi.testclient import TestClient

    _reset_singletons()
    # Point the app at a per-test SQLite file so router tests exercise the
    # real DbStore path end-to-end. ``DAL_WEB_STORE`` stays unset.
    monkeypatch.delenv("DAL_WEB_STORE", raising=False)
    monkeypatch.setenv("DAL_WEB_DB_URL", f"sqlite:///{tmp_path / 'api.db'}")
    from app.main import create_app

    with TestClient(create_app()) as c:
        yield c
