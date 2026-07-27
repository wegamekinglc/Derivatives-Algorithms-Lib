"""add Curve Lab risk runs and matrix blobs

Revision ID: e9c31a7b6d42
Revises: d8a4217f6c90
Create Date: 2026-07-28
"""

from __future__ import annotations

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "e9c31a7b6d42"
down_revision: str | None = "d8a4217f6c90"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.create_table(
        "curve_risk_runs",
        sa.Column("id", sa.String(32), primary_key=True),
        sa.Column(
            "curve_version_id",
            sa.String(32),
            sa.ForeignKey("curve_versions.id", ondelete="RESTRICT"),
            nullable=False,
        ),
        sa.Column("calibration_run_id", sa.String(32), nullable=True),
        sa.Column("import_job_id", sa.String(32), nullable=True),
        sa.Column("source_kind", sa.String(32), nullable=False),
        sa.Column("request_json", sa.JSON(), nullable=False),
        sa.Column("target_fingerprint", sa.String(64), nullable=False),
        sa.Column("quote_axis_json", sa.JSON(), nullable=True),
        sa.Column("parameter_axis_json", sa.JSON(), nullable=False),
        sa.Column("estimated_work_json", sa.JSON(), nullable=False),
        sa.Column("state", sa.String(32), nullable=False),
        sa.Column("result_json", sa.JSON(), nullable=True),
        sa.Column("error_json", sa.JSON(), nullable=True),
        sa.Column("created_at", sa.String(40), nullable=False),
        sa.Column("finished_at", sa.String(40), nullable=True),
    )
    op.create_table(
        "curve_matrix_blobs",
        sa.Column(
            "risk_run_id",
            sa.String(32),
            sa.ForeignKey("curve_risk_runs.id", ondelete="CASCADE"),
            primary_key=True,
        ),
        sa.Column("matrix_id", sa.String(128), primary_key=True),
        sa.Column("envelope_json", sa.JSON(), nullable=False),
        sa.Column("values_blob", sa.LargeBinary(), nullable=True),
    )


def downgrade() -> None:
    op.drop_table("curve_matrix_blobs")
    op.drop_table("curve_risk_runs")
