"""Database-persistence tests for the SQLAlchemy-backed store.

These exercise the real :class:`DbStore` against a fresh on-disk SQLite file per
test. They cover the four guarantees called out in the spec:

- data survives a session close/reopen (the core persistence guarantee);
- deleting a product still referenced by a trade raises ``ConflictError``;
- JSON columns round-trip nested structures (product rows, model params,
  valuation trades);
- ``get_store()`` selects the legacy in-memory store under
  ``DAL_WEB_STORE=memory`` and a :class:`DbStore` otherwise.
"""

from __future__ import annotations

from datetime import date
from pathlib import Path

import pytest

from app.schemas import (
    BSModelParams,
    EventRow,
    ModelDefinition,
    Portfolio,
    ProductDefinition,
    Trade,
    TradeValuation,
    ValuationConfig,
    ValuationResult,
)
from app.services.db.store_db import DbStore
from app.services.store import ConflictError, Store, get_store


def _make_product(name: str = "P") -> ProductDefinition:
    return ProductDefinition(
        name=name,
        description="d",
        template="european_call",
        rows=[
            EventRow(date_kind="label", label="STRIKE", event="120.00"),
            EventRow(date_kind="date", date=date(2025, 9, 15), event="call pays MAX(spot() - STRIKE, 0.0)"),
        ],
    )


def _make_bs_model(name: str = "M") -> ModelDefinition:
    return ModelDefinition(
        name=name,
        kind="BSModelData_",
        bs=BSModelParams(spot=100.0, vol=0.15, rate=0.01, div=0.0),
    )


def _open_store(db_path: Path) -> DbStore:
    """Build a fresh DbStore against ``db_path`` and create its schema."""
    store = DbStore(url=f"sqlite:///{db_path}")
    store.create_all()
    return store


def test_data_survives_session_reopen(tmp_path: Path) -> None:
    db_path = tmp_path / "dalweb.db"

    first = _open_store(db_path)
    product = first.add_product(_make_product())
    model = first.add_model(_make_bs_model())
    trade = first.add_trade(
        Trade(
            name="t",
            book="B",
            counterparty="ACME",
            notional=2.0,
            quantity=3.0,
            product_id=product.id,
            model_id=model.id,
            tags=["a", "b"],
        )
    )
    portfolio = first.add_portfolio(Portfolio(name="PF", trade_ids=[trade.id]))
    first.close()

    # A brand-new store pointing at the same file must see everything written
    # by the closed one -- the persistence guarantee.
    reopened = _open_store(db_path)
    products = reopened.list_products()
    models = reopened.list_models()
    trades = reopened.list_trades()
    portfolios = reopened.list_portfolios()

    assert len(products) == 1
    assert products[0].id == product.id
    assert products[0].rows == product.rows
    assert len(models) == 1
    assert models[0].bs == model.bs
    assert len(trades) == 1
    assert trades[0].tags == ["a", "b"]
    assert len(portfolios) == 1
    assert portfolios[0].id == portfolio.id
    assert reopened.portfolio_trades(portfolio.id)[0].id == trade.id


def test_delete_product_referenced_by_trade_raises_conflict(tmp_path: Path) -> None:
    store = _open_store(tmp_path / "dalweb.db")
    product = store.add_product(_make_product())
    model = store.add_model(_make_bs_model())
    store.add_trade(
        Trade(name="t", product_id=product.id, model_id=model.id)
    )

    with pytest.raises(ConflictError):
        store.delete_product(product.id)
    # The product is still there.
    assert store.get_product(product.id).id == product.id


def test_delete_model_referenced_by_trade_raises_conflict(tmp_path: Path) -> None:
    store = _open_store(tmp_path / "dalweb.db")
    product = store.add_product(_make_product())
    model = store.add_model(_make_bs_model())
    store.add_trade(
        Trade(name="t", product_id=product.id, model_id=model.id)
    )

    with pytest.raises(ConflictError):
        store.delete_model(model.id)


def test_delete_trade_cascades_portfolio_membership(tmp_path: Path) -> None:
    store = _open_store(tmp_path / "dalweb.db")
    product = store.add_product(_make_product())
    model = store.add_model(_make_bs_model())
    trade = store.add_trade(Trade(name="t", product_id=product.id, model_id=model.id))
    portfolio = store.add_portfolio(Portfolio(name="PF", trade_ids=[trade.id]))
    assert [t.id for t in store.portfolio_trades(portfolio.id)] == [trade.id]

    store.delete_trade(trade.id)
    assert store.portfolio_trades(portfolio.id) == []


def test_json_columns_round_trip(tmp_path: Path) -> None:
    store = _open_store(tmp_path / "dalweb.db")
    product = store.add_product(_make_product())
    model = store.add_model(_make_bs_model())
    trade = store.add_trade(
        Trade(
            name="t",
            product_id=product.id,
            model_id=model.id,
            tags=["alpha", "beta"],
        )
    )

    config = ValuationConfig(num_paths=1024, method="sobol", enable_aad=True, smooth=0.02)
    result = ValuationResult(
        target_kind="trade",
        target_id=trade.id,
        backend="native",
        is_native=True,
        config=config,
        total_pv=42.5,
        total_greeks={"d_spot": 0.7},
        trades=[
            {
                "trade_id": trade.id,
                "trade_name": "t",
                "pv": 42.5,
                "scaled_pv": 42.5,
                "greeks": {"d_spot": 0.7},
                "error": None,
            }
        ],
        created_at="2026-07-03T00:00:00+00:00",
        status="completed",
    )
    saved = store.add_valuation(result)
    store.close()

    reopened = _open_store(tmp_path / "dalweb.db")
    loaded = reopened.get_valuation(saved.id)
    # Nested JSON blobs must survive the serializer round-trip intact.
    assert loaded.config == config
    assert loaded.total_greeks == {"d_spot": 0.7}
    assert len(loaded.trades) == 1
    assert loaded.trades[0].greeks == {"d_spot": 0.7}
    assert loaded.trades[0].trade_id == trade.id
    # And the product / model JSON columns.
    assert reopened.get_product(product.id).rows == product.rows
    assert reopened.get_model(model.id).bs == model.bs


def test_update_valuation_persists_across_reopen(tmp_path: Path) -> None:
    # The async pricer writes results back exclusively via update_valuation
    # (status running -> completed/failed), so a patch must survive a reopen.
    store = _open_store(tmp_path / "dalweb.db")
    product = store.add_product(_make_product())
    model = store.add_model(_make_bs_model())
    trade = store.add_trade(Trade(name="t", product_id=product.id, model_id=model.id))

    pending = ValuationResult(
        target_kind="trade",
        target_id=trade.id,
        backend="native",
        is_native=True,
        config=ValuationConfig(num_paths=512),
        total_pv=0.0,
        trades=[],
        created_at="2026-07-03T00:00:00+00:00",
        status="running",
    )
    saved = store.add_valuation(pending)

    tv = TradeValuation(
        trade_id=trade.id,
        trade_name="t",
        pv=11.0,
        scaled_pv=22.0,
        greeks={"d_spot": 0.9},
    )
    store.update_valuation(
        saved.id,
        {
            "status": "completed",
            "total_pv": 22.0,
            "total_greeks": {"d_spot": 0.9},
            "trades": [tv],
        },
    )
    store.close()

    reopened = _open_store(tmp_path / "dalweb.db")
    loaded = reopened.get_valuation(saved.id)
    assert loaded.status == "completed"
    assert loaded.total_pv == 22.0
    assert loaded.total_greeks == {"d_spot": 0.9}
    assert len(loaded.trades) == 1
    assert loaded.trades[0].scaled_pv == 22.0
    assert loaded.trades[0].greeks == {"d_spot": 0.9}


def test_get_store_returns_memory_store_when_env_set(monkeypatch: pytest.MonkeyPatch) -> None:
    import app.services.store as st

    monkeypatch.setenv("DAL_WEB_STORE", "memory")
    monkeypatch.delenv("DAL_WEB_DB_URL", raising=False)
    st._store_box[0] = None
    store = get_store()
    assert isinstance(store, Store)
    assert not isinstance(store, DbStore)


def test_get_store_returns_db_store_by_default(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    import app.services.store as st

    monkeypatch.delenv("DAL_WEB_STORE", raising=False)
    monkeypatch.setenv("DAL_WEB_DB_URL", f"sqlite:///{tmp_path / 'default.db'}")
    st._store_box[0] = None
    try:
        store = get_store()
        assert isinstance(store, DbStore)
    finally:
        st._store_box[0] = None


def test_db_store_satisfies_store_protocol() -> None:
    # StoreProtocol is a typing.Protocol; DbStore must be usable wherever a
    # Store is. The in-memory Store already satisfies it; DbStore must too.
    from app.services.store import StoreProtocol

    def _accepts(_store: StoreProtocol) -> None:
        pass

    _accepts(DbStore(url="sqlite:///:memory:"))
    _accepts(Store())


def test_seed_demo_data_is_idempotent(tmp_path: Path) -> None:
    # Imported here so the module's top-level store annotation does not pin a
    # concrete Store at import time.
    from app.services.templates import seed_demo_data

    store = _open_store(tmp_path / "dalweb.db")
    seed_demo_data(store)
    n_products_after_first = len(store.list_products())
    n_portfolios_after_first = len(store.list_portfolios())
    assert n_products_after_first >= 1
    assert n_portfolios_after_first == 1

    seed_demo_data(store)
    assert len(store.list_products()) == n_products_after_first
    assert len(store.list_portfolios()) == n_portfolios_after_first


def test_remove_trade_from_portfolio_excludes_removed_trade(tmp_path: Path) -> None:
    # Under expire_on_commit=False, session.delete(m) used to leave the row in
    # pf.memberships, so the returned Portfolio still named the removed trade.
    # The return value (and a fresh read) must reflect the removal.
    store = _open_store(tmp_path / "dalweb.db")
    product = store.add_product(_make_product())
    model = store.add_model(_make_bs_model())
    t1 = store.add_trade(Trade(name="t1", product_id=product.id, model_id=model.id))
    t2 = store.add_trade(Trade(name="t2", product_id=product.id, model_id=model.id))
    t3 = store.add_trade(Trade(name="t3", product_id=product.id, model_id=model.id))
    portfolio = store.add_portfolio(Portfolio(name="PF", trade_ids=[t1.id, t2.id, t3.id]))

    after = store.remove_trade_from_portfolio(portfolio.id, t2.id)

    assert t2.id not in after.trade_ids
    assert after.trade_ids == [t1.id, t3.id]
    assert store.get_portfolio(portfolio.id).trade_ids == [t1.id, t3.id]
    assert [t.id for t in store.portfolio_trades(portfolio.id)] == [t1.id, t3.id]


def test_init_database_runs_alembic_under_auto_migrate(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    # DAL_WEB_AUTO_MIGRATE=1 must build the schema via Alembic. Previously
    # _build_store() called create_all() first, so the initial Alembic migration
    # collided with tables that already existed. Proof that Alembic ran (rather
    # than create_all()): an `alembic_version` table is present.
    from sqlalchemy import create_engine, inspect

    import app.services.store as st

    db_url = f"sqlite:///{tmp_path / 'migrate.db'}"
    monkeypatch.delenv("DAL_WEB_STORE", raising=False)
    monkeypatch.setenv("DAL_WEB_DB_URL", db_url)
    monkeypatch.setenv("DAL_WEB_AUTO_MIGRATE", "1")
    st._store_box[0] = None
    try:
        from app.main import _init_database

        _init_database()

        engine = create_engine(db_url)
        names = set(inspect(engine).get_table_names())
        engine.dispose()
    finally:
        if st._store_box[0] is not None and hasattr(st._store_box[0], "close"):
            st._store_box[0].close()
        st._store_box[0] = None

    assert "product" in names
    assert "alembic_version" in names
