"""add curve calibration persistence

Revision ID: c2d8f43a9e71
Revises: bfa84bde2b43
Create Date: 2026-07-26

"""

from __future__ import annotations

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "c2d8f43a9e71"
down_revision: str | None = "bfa84bde2b43"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.create_table(
        "calibration_run",
        sa.Column("id", sa.String(length=32), nullable=False),
        sa.Column("schema_version", sa.Integer(), nullable=False),
        sa.Column("kind", sa.String(length=16), nullable=False),
        sa.Column("name", sa.String(length=128), nullable=False),
        sa.Column("status", sa.String(length=16), nullable=False),
        sa.Column("phase", sa.String(length=16), nullable=False),
        sa.Column("request_payload", sa.JSON(), nullable=False),
        sa.Column("solver_payload", sa.JSON(), nullable=False),
        sa.Column("options_payload", sa.JSON(), nullable=False),
        sa.Column("resolved_knot_plan", sa.JSON(), nullable=True),
        sa.Column("resolved_knot_plan_hash", sa.String(length=64), nullable=True),
        sa.Column("expected_execution_identity", sa.JSON(), nullable=True),
        sa.Column("expected_execution_identity_hash", sa.String(length=64), nullable=True),
        sa.Column("actual_jacobian_mode", sa.String(length=16), nullable=True),
        sa.Column("actual_execution_identity", sa.JSON(), nullable=True),
        sa.Column("actual_execution_identity_hash", sa.String(length=64), nullable=True),
        sa.Column("result_payload", sa.JSON(), nullable=True),
        sa.Column("error_payload", sa.JSON(), nullable=True),
        sa.Column("backend", sa.String(length=64), nullable=False),
        sa.Column("is_native", sa.Boolean(), nullable=False),
        sa.Column("created_at", sa.String(length=40), nullable=False),
        sa.Column("started_at", sa.String(length=40), nullable=True),
        sa.Column("finished_at", sa.String(length=40), nullable=True),
        sa.Column("native_solve_ms", sa.Float(), nullable=True),
        sa.Column("serialization_ms", sa.Float(), nullable=True),
        sa.CheckConstraint(
            "kind IN ('single','xccy_staged','xccy_joint')",
            name="ck_calibration_run_kind",
        ),
        sa.CheckConstraint(
            "status IN ('running','completed','failed')",
            name="ck_calibration_run_status",
        ),
        sa.CheckConstraint(
            "(status = 'running' AND phase IN "
            "('queued','solving','serializing','persisting')) OR "
            "(status IN ('completed','failed') AND phase = 'finished')",
            name="ck_calibration_run_status_phase",
        ),
        sa.CheckConstraint(
            "actual_jacobian_mode IS NULL OR actual_jacobian_mode IN ('ANALYTIC','BUMPED')",
            name="ck_calibration_run_actual_jacobian_mode",
        ),
        sa.PrimaryKeyConstraint("id"),
    )
    op.create_index(
        "ix_calibration_run_status_created_at",
        "calibration_run",
        ["status", "created_at"],
        unique=False,
    )

    op.create_table(
        "curve_definition",
        sa.Column("id", sa.String(length=32), nullable=False),
        sa.Column("dto_version", sa.Integer(), nullable=False),
        sa.Column("name", sa.String(length=128), nullable=False),
        sa.Column("currency", sa.String(length=128), nullable=False),
        sa.Column("role", sa.String(length=16), nullable=False),
        sa.Column("source_run_id", sa.String(length=32), nullable=False),
        sa.Column("base_curve_id", sa.String(length=32), nullable=True),
        sa.Column("payload", sa.JSON(), nullable=False),
        sa.Column("created_at", sa.String(length=40), nullable=False),
        sa.CheckConstraint(
            "role IN ('discount','forward','basis','base')",
            name="ck_curve_definition_role",
        ),
        sa.ForeignKeyConstraint(["base_curve_id"], ["curve_definition.id"], ondelete="RESTRICT"),
        sa.ForeignKeyConstraint(["source_run_id"], ["calibration_run.id"], ondelete="RESTRICT"),
        sa.PrimaryKeyConstraint("id"),
    )
    op.create_index(
        "ix_curve_definition_source_run_id",
        "curve_definition",
        ["source_run_id"],
        unique=False,
    )
    op.create_index(
        "ix_curve_definition_base_curve_id",
        "curve_definition",
        ["base_curve_id"],
        unique=False,
    )

    op.create_table(
        "calibration_instrument_definition",
        sa.Column("id", sa.String(length=32), nullable=False),
        sa.Column("run_id", sa.String(length=32), nullable=False),
        sa.Column("group_name", sa.String(length=160), nullable=False),
        sa.Column("input_index", sa.Integer(), nullable=False),
        sa.Column("calibration_index", sa.Integer(), nullable=False),
        sa.Column("kind", sa.String(length=32), nullable=False),
        sa.Column("label", sa.String(length=128), nullable=False),
        sa.Column("native_name", sa.String(length=128), nullable=False),
        sa.Column("payload", sa.JSON(), nullable=False),
        sa.CheckConstraint(
            "calibration_index >= 0",
            name="ck_calibration_instrument_calibration_index",
        ),
        sa.CheckConstraint("input_index >= 0", name="ck_calibration_instrument_input_index"),
        sa.ForeignKeyConstraint(["run_id"], ["calibration_run.id"], ondelete="CASCADE"),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint(
            "run_id",
            "group_name",
            "calibration_index",
            name="uq_calibration_instrument_run_group_calibration",
        ),
        sa.UniqueConstraint(
            "run_id",
            "group_name",
            "input_index",
            name="uq_calibration_instrument_run_group_input",
        ),
    )


def downgrade() -> None:
    op.drop_table("calibration_instrument_definition")
    op.drop_index("ix_curve_definition_base_curve_id", table_name="curve_definition")
    op.drop_index("ix_curve_definition_source_run_id", table_name="curve_definition")
    op.drop_table("curve_definition")
    op.drop_index("ix_calibration_run_status_created_at", table_name="calibration_run")
    op.drop_table("calibration_run")
