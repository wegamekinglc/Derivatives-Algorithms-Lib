"""add Curve Lab V2 lifecycle persistence

Revision ID: d8a4217f6c90
Revises: c2d8f43a9e71
Create Date: 2026-07-28
"""

from __future__ import annotations

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "d8a4217f6c90"
down_revision: str | None = "c2d8f43a9e71"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.create_table(
        "curve_drafts",
        sa.Column("id", sa.String(32), primary_key=True),
        sa.Column("schema_version", sa.Integer(), nullable=False),
        sa.Column("revision", sa.Integer(), nullable=False),
        sa.Column("fingerprint", sa.String(64), nullable=False),
        sa.Column("document_json", sa.JSON(), nullable=False),
        sa.Column("state", sa.String(32), nullable=False),
        sa.Column("created_at", sa.String(40), nullable=False),
        sa.Column("updated_at", sa.String(40), nullable=False),
    )
    op.create_table(
        "curve_build_runs",
        sa.Column("id", sa.String(32), primary_key=True),
        sa.Column(
            "draft_id",
            sa.String(32),
            sa.ForeignKey("curve_drafts.id", ondelete="RESTRICT"),
            nullable=False,
        ),
        sa.Column("draft_revision", sa.Integer(), nullable=False),
        sa.Column("draft_fingerprint", sa.String(64), nullable=False),
        sa.Column("request_json", sa.JSON(), nullable=False),
        sa.Column("resolved_plan_json", sa.JSON(), nullable=True),
        sa.Column("quote_axis_json", sa.JSON(), nullable=True),
        sa.Column("parameter_axis_json", sa.JSON(), nullable=True),
        sa.Column("dependency_manifest_json", sa.JSON(), nullable=False),
        sa.Column("state", sa.String(32), nullable=False),
        sa.Column("native_payload", sa.LargeBinary(), nullable=True),
        sa.Column("native_payload_hash", sa.String(64), nullable=True),
        sa.Column("diagnostics_json", sa.JSON(), nullable=True),
        sa.Column("error_json", sa.JSON(), nullable=True),
        sa.Column("created_at", sa.String(40), nullable=False),
        sa.Column("finished_at", sa.String(40), nullable=True),
    )
    op.create_table(
        "curve_versions",
        sa.Column("id", sa.String(32), primary_key=True),
        sa.Column("idempotency_key", sa.String(256), nullable=False, unique=True),
        sa.Column("source_kind", sa.String(16), nullable=False),
        sa.Column(
            "build_run_id",
            sa.String(32),
            sa.ForeignKey("curve_build_runs.id", ondelete="RESTRICT"),
            nullable=True,
        ),
        sa.Column("import_job_id", sa.String(32), nullable=True),
        sa.Column("native_payload", sa.LargeBinary(), nullable=False),
        sa.Column("native_payload_length", sa.Integer(), nullable=False),
        sa.Column("native_payload_hash", sa.String(64), nullable=False),
        sa.Column("archive_numeric_format", sa.String(32), nullable=False),
        sa.Column("root_kind", sa.String(32), nullable=False),
        sa.Column("build_validation_state", sa.String(32), nullable=False),
        sa.Column("visibility_state", sa.String(16), nullable=False),
        sa.Column("metadata_json", sa.JSON(), nullable=False),
        sa.Column("verification_json", sa.JSON(), nullable=False),
        sa.Column("created_at", sa.String(40), nullable=False),
    )
    op.create_table(
        "curve_import_jobs",
        sa.Column("id", sa.String(32), primary_key=True),
        sa.Column("request_hash", sa.String(64), nullable=False),
        sa.Column("compressed_payload_length", sa.Integer(), nullable=False),
        sa.Column("expanded_payload_length", sa.Integer(), nullable=False),
        sa.Column("state", sa.String(16), nullable=False),
        sa.Column("phase", sa.String(64), nullable=False),
        sa.Column("error_json", sa.JSON(), nullable=True),
        sa.Column("resulting_version_id", sa.String(32), nullable=True),
        sa.Column("created_at", sa.String(40), nullable=False),
        sa.Column("finished_at", sa.String(40), nullable=True),
    )
    op.create_table(
        "curve_audit_events",
        sa.Column("id", sa.String(32), primary_key=True),
        sa.Column("action", sa.String(64), nullable=False),
        sa.Column("actor", sa.String(128), nullable=False),
        sa.Column("target_type", sa.String(64), nullable=False),
        sa.Column("target_id", sa.String(32), nullable=False),
        sa.Column("input_hash", sa.String(64), nullable=False),
        sa.Column("outcome", sa.String(32), nullable=False),
        sa.Column("details_json", sa.JSON(), nullable=False),
        sa.Column("created_at", sa.String(40), nullable=False),
    )


def downgrade() -> None:
    op.drop_table("curve_audit_events")
    op.drop_table("curve_import_jobs")
    op.drop_table("curve_versions")
    op.drop_table("curve_build_runs")
    op.drop_table("curve_drafts")
