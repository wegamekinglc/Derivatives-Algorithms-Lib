"""add persisted Curve Lab soft deadlines

Revision ID: a13c8e4b7d92
Revises: f4a27c91d6e3
Create Date: 2026-07-28
"""

from __future__ import annotations

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "a13c8e4b7d92"
down_revision: str | None = "f4a27c91d6e3"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    for table in ("curve_build_runs", "curve_import_jobs", "curve_risk_runs"):
        op.add_column(
            table,
            sa.Column(
                "deadline_at",
                sa.String(40),
                nullable=False,
                server_default="1970-01-01T00:00:00Z",
            ),
        )


def downgrade() -> None:
    for table in ("curve_risk_runs", "curve_import_jobs", "curve_build_runs"):
        op.drop_column(table, "deadline_at")
