"""Database-backed store package.

Holds the SQLAlchemy engine/session plumbing, ORM models, and the
:class:`DbStore` implementation that lives behind the ``Store`` seam. Routers
never import this package directly -- they go through ``app.services.store``.
"""

from __future__ import annotations

from app.services.db.session import default_db_url, engine_from_url
from app.services.db.store_db import DbStore

__all__ = ["DbStore", "default_db_url", "engine_from_url"]
