"""Engine plumbing for the DB-backed store.

For SQLite URLs the engine is created with ``check_same_thread=False`` (sessions
are still short-lived and never shared across threads) and a connection event
installs the WAL journal mode and foreign-key enforcement pragmas -- both
essential for the async valuation path that offloads C++ pricing to worker
threads via ``asyncio.to_thread``.
"""

from __future__ import annotations

from pathlib import Path

from sqlalchemy import create_engine, event
from sqlalchemy.engine import Engine


def _backend_dir() -> Path:
    """Directory of the backend package, used for the default SQLite file."""
    # This module lives at app/services/db/session.py; the backend root is two
    # levels up from app/.
    import app

    return Path(app.__file__).resolve().parent.parent


def default_db_url() -> str:
    """The SQLAlchemy URL used when ``DAL_WEB_DB_URL`` is unset.

    Points at a local SQLite file under ``dal-web/backend/.data/`` so a fresh
    checkout persists out of the box without provisioning a database server.
    """
    data_dir = _backend_dir() / ".data"
    data_dir.mkdir(parents=True, exist_ok=True)
    return f"sqlite:///{data_dir / 'dalweb.db'}"


def _is_sqlite(url: str) -> bool:
    return url.startswith("sqlite")


def engine_from_url(url: str) -> Engine:
    """Build a configured :class:`Engine` for ``url``.

    SQLite engines get ``check_same_thread=False`` (passed to the DBAPI via
    ``connect_args``) plus WAL and foreign-key pragmas applied on every
    connection.
    """
    kwargs: dict = {"future": True}
    if _is_sqlite(url):
        kwargs["connect_args"] = {"check_same_thread": False}
    engine = create_engine(url, **kwargs)

    if _is_sqlite(url):
        @event.listens_for(engine, "connect")
        def _set_sqlite_pragmas(dbapi_connection, _connection_record):  # noqa: ANN001
            cursor = dbapi_connection.cursor()
            try:
                cursor.execute("PRAGMA journal_mode=WAL")
                cursor.execute("PRAGMA foreign_keys=ON")
            finally:
                cursor.close()

    return engine
