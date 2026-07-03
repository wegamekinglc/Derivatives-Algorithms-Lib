"""Alembic environment for the dal-web backend.

The database URL is read from ``DAL_WEB_DB_URL`` (falling back to the default
SQLite file) so a single migration set serves every backend without editing
``alembic.ini``. The ``offline`` and ``online`` modes follow Alembic's standard
pattern; ``online`` builds a short-lived ``NullPool`` engine for the migration,
so running ``alembic upgrade head`` from inside the app process does not hold a
long-lived second pool alongside the store's engine.
"""

from __future__ import annotations

import os
import sys
from logging.config import fileConfig
from pathlib import Path

from alembic import context
from sqlalchemy import engine_from_config, pool

# Make ``app`` importable when alembic runs from the backend directory.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from app.services.db.models import Base  # noqa: E402
from app.services.db.session import default_db_url  # noqa: E402

config = context.config

if config.config_file_name is not None:
    fileConfig(config.config_file_name)

config.set_main_option("sqlalchemy.url", os.environ.get("DAL_WEB_DB_URL") or default_db_url())

target_metadata = Base.metadata


def run_migrations_offline() -> None:
    url = config.get_main_option("sqlalchemy.url")
    context.configure(
        url=url,
        target_metadata=target_metadata,
        literal_binds=True,
        dialect_opts={"paramstyle": "named"},
    )
    with context.begin_transaction():
        context.run_migrations()


def run_migrations_online() -> None:
    connectable = engine_from_config(
        config.get_section(config.config_ini_section, {}),
        prefix="sqlalchemy.",
        poolclass=pool.NullPool,
    )
    with connectable.connect() as connection:
        context.configure(connection=connection, target_metadata=target_metadata)
        with context.begin_transaction():
            context.run_migrations()


if context.is_offline_mode():
    run_migrations_offline()
else:
    run_migrations_online()
