"""add immutable Curve Lab fixing snapshots

Revision ID: f4a27c91d6e3
Revises: e9c31a7b6d42
Create Date: 2026-07-28
"""

from __future__ import annotations

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "f4a27c91d6e3"
down_revision: str | None = "e9c31a7b6d42"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.create_table(
        "curve_fixing_snapshots",
        sa.Column("id", sa.String(256), primary_key=True),
        sa.Column("observations_json", sa.JSON(), nullable=False),
        sa.Column("content_hash", sa.String(64), nullable=False),
        sa.Column("created_at", sa.String(40), nullable=False),
    )
    op.add_column(
        "curve_risk_runs",
        sa.Column(
            "fixing_snapshot_hash",
            sa.String(64),
            nullable=False,
            server_default="0" * 64,
        ),
    )


def downgrade() -> None:
    op.drop_column("curve_risk_runs", "fixing_snapshot_hash")
    op.drop_table("curve_fixing_snapshots")
