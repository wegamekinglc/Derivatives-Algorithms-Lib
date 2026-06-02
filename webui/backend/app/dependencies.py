"""Shared FastAPI dependencies."""

from __future__ import annotations

from app.services.dal_gateway import DalGateway, get_gateway
from app.services.store import Store, get_store


def store_dependency() -> Store:
    return get_store()


def gateway_dependency() -> DalGateway:
    return get_gateway()
