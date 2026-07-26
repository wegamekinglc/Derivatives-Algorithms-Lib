from __future__ import annotations

from pathlib import Path

from alembic import command
from alembic.config import Config
from sqlalchemy import create_engine, inspect


def _config() -> Config:
    backend_dir = Path(__file__).resolve().parents[1]
    config = Config(str(backend_dir / "alembic.ini"))
    config.set_main_option("script_location", str(backend_dir / "migrations"))
    return config


def test_calibration_migration_upgrade_downgrade_upgrade(
    tmp_path: Path, monkeypatch
) -> None:
    database_url = f"sqlite:///{tmp_path / 'migration.db'}"
    monkeypatch.setenv("DAL_WEB_DB_URL", database_url)
    config = _config()

    command.upgrade(config, "head")
    engine = create_engine(database_url)
    inspector = inspect(engine)
    assert {
        "calibration_run",
        "calibration_instrument_definition",
        "curve_definition",
    } <= set(inspector.get_table_names())
    assert {index["name"] for index in inspector.get_indexes("calibration_run")} == {
        "ix_calibration_run_status_created_at"
    }
    assert {index["name"] for index in inspector.get_indexes("curve_definition")} == {
        "ix_curve_definition_base_curve_id",
        "ix_curve_definition_source_run_id",
    }
    assert {
        constraint["name"]
        for constraint in inspector.get_unique_constraints(
            "calibration_instrument_definition"
        )
    } == {
        "uq_calibration_instrument_run_group_calibration",
        "uq_calibration_instrument_run_group_input",
    }
    engine.dispose()

    command.downgrade(config, "bfa84bde2b43")
    engine = create_engine(database_url)
    table_names = set(inspect(engine).get_table_names())
    assert "valuation" in table_names
    assert "calibration_run" not in table_names
    assert "calibration_instrument_definition" not in table_names
    assert "curve_definition" not in table_names
    engine.dispose()

    command.upgrade(config, "head")
    engine = create_engine(database_url)
    assert {
        "calibration_run",
        "calibration_instrument_definition",
        "curve_definition",
    } <= set(inspect(engine).get_table_names())
    engine.dispose()
