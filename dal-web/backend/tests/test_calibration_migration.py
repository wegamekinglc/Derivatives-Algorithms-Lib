from __future__ import annotations

import json
from pathlib import Path

import pytest
from alembic import command
from alembic.config import Config
from sqlalchemy import create_engine, inspect
from sqlalchemy.engine import Engine
from sqlalchemy.exc import IntegrityError

_LEGACY_TABLES = (
    "model",
    "portfolio",
    "portfolio_trade",
    "product",
    "trade",
    "valuation",
)


def _config() -> Config:
    backend_dir = Path(__file__).resolve().parents[1]
    config = Config(str(backend_dir / "alembic.ini"))
    config.set_main_option("script_location", str(backend_dir / "migrations"))
    return config


def _insert_legacy_entities(engine: Engine) -> None:
    with engine.begin() as connection:
        connection.exec_driver_sql("PRAGMA foreign_keys=ON")
        connection.exec_driver_sql(
            "INSERT INTO product "
            "(id, name, description, template, rows) VALUES (?, ?, ?, ?, ?)",
            (
                "legacy-product",
                "Legacy product",
                "must survive",
                "european_call",
                json.dumps(
                    [
                        {
                            "date_kind": "label",
                            "label": "STRIKE",
                            "event": "100.0",
                        }
                    ],
                    separators=(",", ":"),
                ),
            ),
        )
        connection.exec_driver_sql(
            "INSERT INTO model (id, name, kind, params) VALUES (?, ?, ?, ?)",
            (
                "legacy-model",
                "Legacy model",
                "BSModelData_",
                json.dumps(
                    {"spot": 100.0, "vol": 0.2, "rate": 0.01, "div": 0.0},
                    separators=(",", ":"),
                ),
            ),
        )
        connection.exec_driver_sql(
            "INSERT INTO trade "
            "(id, name, book, counterparty, notional, quantity, "
            "product_id, model_id, tags) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
            (
                "legacy-trade",
                "Legacy trade",
                "BOOK",
                "COUNTERPARTY",
                1_000_000.0,
                2.0,
                "legacy-product",
                "legacy-model",
                '["migration","fixture"]',
            ),
        )
        connection.exec_driver_sql(
            "INSERT INTO portfolio (id, name, description) VALUES (?, ?, ?)",
            ("legacy-portfolio", "Legacy portfolio", "must survive"),
        )
        connection.exec_driver_sql(
            "INSERT INTO portfolio_trade "
            "(portfolio_id, trade_id, position) VALUES (?, ?, ?)",
            ("legacy-portfolio", "legacy-trade", 0),
        )
        connection.exec_driver_sql(
            "INSERT INTO valuation "
            "(id, target_kind, target_id, backend, is_native, config, "
            "total_pv, total_greeks, trades, created_at, status, error_message) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            (
                "legacy-valuation",
                "trade",
                "legacy-trade",
                "dal",
                True,
                '{"method":"sobol"}',
                123.45,
                '{"delta":0.5}',
                '[{"trade_id":"legacy-trade","pv":123.45}]',
                "2026-01-02T10:00:00",
                "completed",
                None,
            ),
        )


def _legacy_rows(engine: Engine) -> dict[str, tuple[tuple[object, ...], ...]]:
    with engine.connect() as connection:
        return {
            table: tuple(
                tuple(row)
                for row in connection.exec_driver_sql(
                    f"SELECT * FROM {table} ORDER BY 1"
                ).all()
            )
            for table in _LEGACY_TABLES
        }


def _legacy_foreign_keys(
    engine: Engine,
) -> dict[str, tuple[tuple[tuple[str, ...], str, tuple[str, ...], str | None], ...]]:
    inspector = inspect(engine)
    return {
        table: tuple(
            sorted(
                (
                    tuple(foreign_key["constrained_columns"]),
                    foreign_key["referred_table"],
                    tuple(foreign_key["referred_columns"]),
                    foreign_key["options"].get("ondelete"),
                )
                for foreign_key in inspector.get_foreign_keys(table)
            )
        )
        for table in _LEGACY_TABLES
    }


def _assert_legacy_fk_enforcement(engine: Engine) -> None:
    with pytest.raises(IntegrityError), engine.begin() as connection:
        connection.exec_driver_sql("PRAGMA foreign_keys=ON")
        connection.exec_driver_sql(
            "INSERT INTO trade "
            "(id, name, book, counterparty, notional, quantity, "
            "product_id, model_id, tags) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
            (
                "orphan",
                "orphan",
                "BOOK",
                "COUNTERPARTY",
                1.0,
                1.0,
                "missing-product",
                "missing-model",
                "[]",
            ),
        )


def _assert_legacy_unchanged(
    engine: Engine,
    rows: dict[str, tuple[tuple[object, ...], ...]],
    foreign_keys: dict[
        str,
        tuple[tuple[tuple[str, ...], str, tuple[str, ...], str | None], ...],
    ],
) -> None:
    assert _legacy_rows(engine) == rows
    assert _legacy_foreign_keys(engine) == foreign_keys
    _assert_legacy_fk_enforcement(engine)


def test_calibration_migration_upgrade_downgrade_upgrade(
    tmp_path: Path, monkeypatch
) -> None:
    database_url = f"sqlite:///{tmp_path / 'migration.db'}"
    monkeypatch.setenv("DAL_WEB_DB_URL", database_url)
    config = _config()

    command.upgrade(config, "bfa84bde2b43")
    engine = create_engine(database_url)
    _insert_legacy_entities(engine)
    legacy_rows = _legacy_rows(engine)
    legacy_foreign_keys = _legacy_foreign_keys(engine)
    assert {
        table: len(legacy_rows[table])
        for table in ("product", "model", "trade", "portfolio", "valuation")
    } == {
        "product": 1,
        "model": 1,
        "trade": 1,
        "portfolio": 1,
        "valuation": 1,
    }
    assert legacy_foreign_keys["trade"] == (
        (("model_id",), "model", ("id",), "RESTRICT"),
        (("product_id",), "product", ("id",), "RESTRICT"),
    )
    assert legacy_foreign_keys["portfolio_trade"] == (
        (("portfolio_id",), "portfolio", ("id",), "CASCADE"),
        (("trade_id",), "trade", ("id",), "CASCADE"),
    )
    _assert_legacy_fk_enforcement(engine)
    engine.dispose()

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
    _assert_legacy_unchanged(engine, legacy_rows, legacy_foreign_keys)
    engine.dispose()

    command.downgrade(config, "bfa84bde2b43")
    engine = create_engine(database_url)
    table_names = set(inspect(engine).get_table_names())
    assert "valuation" in table_names
    assert "calibration_run" not in table_names
    assert "calibration_instrument_definition" not in table_names
    assert "curve_definition" not in table_names
    _assert_legacy_unchanged(engine, legacy_rows, legacy_foreign_keys)
    engine.dispose()

    command.upgrade(config, "head")
    engine = create_engine(database_url)
    assert {
        "calibration_run",
        "calibration_instrument_definition",
        "curve_definition",
    } <= set(inspect(engine).get_table_names())
    _assert_legacy_unchanged(engine, legacy_rows, legacy_foreign_keys)
    engine.dispose()
