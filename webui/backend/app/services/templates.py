"""Product templates and demo seed data for the portfolio workbench."""

from __future__ import annotations

from datetime import date
from typing import Dict, List

from app.schemas import (
    BSModelParams,
    EventRow,
    ModelCreate,
    ModelDefinition,
    Portfolio,
    ProductCreate,
    ProductDefinition,
    Trade,
)
from app.services.store import Store


def product_templates() -> List[Dict]:
    """Return ready-to-use scripted-product templates for the UI builder."""
    return [
        {
            "key": "european_call",
            "name": "European Call",
            "description": "Vanilla European call paying MAX(spot-K, 0) at maturity.",
            "rows": [
                {"date_kind": "label", "label": "STRIKE", "event": "120.00"},
                {"date_kind": "date", "date": "2025-09-15",
                 "event": "call pays MAX(spot() - STRIKE, 0.0)"},
            ],
        },
        {
            "key": "up_and_out_call",
            "name": "Up-and-Out Call",
            "description": "Barrier call knocked out when spot crosses the barrier.",
            "rows": [
                {"date_kind": "label", "label": "STRIKE", "event": "120.00"},
                {"date_kind": "label", "label": "BARRIER", "event": "150.00"},
                {"date_kind": "date", "date": "2022-09-25", "event": "alive = 1"},
                {"date_kind": "label",
                 "label": "START: 2022-09-25 END: 2025-09-25 FREQ: 1W",
                 "event": "IF spot() > BARRIER:0.1 THEN alive = 0 END"},
                {"date_kind": "date", "date": "2025-09-25",
                 "event": "IF spot() > BARRIER:0.1 THEN alive = 0 END "
                          "uoc pays alive * MAX(spot() - STRIKE, 0.0)"},
            ],
        },
        {
            "key": "snowball",
            "name": "Snowball",
            "description": "Autocallable snowball with knock-in / knock-out coupons.",
            "rows": [
                {"date_kind": "label", "label": "KI", "event": "0.88"},
                {"date_kind": "label", "label": "KO", "event": "1.00"},
                {"date_kind": "label", "label": "STRIKE", "event": "1.00"},
                {"date_kind": "label", "label": "COUPON", "event": "0.069"},
                {"date_kind": "date", "date": "2023-03-01",
                 "event": "alive = 1 is_ki = 0"},
                {"date_kind": "label",
                 "label": "START: 2023-06-01 END: 2025-02-01 FREQ: 1M",
                 "event": "if spot() < KI:0.001 then is_ki = 1 end\n"
                          "if spot() > KO:0.001 then call pays alive * COUPON * "
                          "DCF(ACT365F, 2023-03-01, PeriodEnd) alive = 0 end"},
                {"date_kind": "date", "date": "2025-03-01",
                 "event": "if spot() < KI:0.001 then is_ki = 1 end\n"
                          "if spot() > KO:0.001 then call pays alive * COUPON * "
                          "DCF(ACT365F, 2023-03-01, 2025-03-01) alive = 0 end\n"
                          "call pays alive * is_ki * (spot() - STRIKE) + "
                          "alive * (1.000000 - is_ki) * COUPON * "
                          "DCF(ACT365F, 2023-03-01, 2025-03-01)"},
            ],
        },
    ]


def _rows_from_template(tpl: Dict) -> List[EventRow]:
    rows: List[EventRow] = []
    for r in tpl["rows"]:
        rows.append(
            EventRow(
                date_kind=r["date_kind"],
                date=date.fromisoformat(r["date"]) if r.get("date") else None,
                label=r.get("label"),
                event=r["event"],
            )
        )
    return rows


def seed_demo_data(store: Store) -> None:
    """Populate the store with one portfolio, model, product and trade."""
    if store.list_portfolios():
        return  # already seeded

    tpl = product_templates()[0]  # european call
    product = ProductDefinition(
        name="Demo European Call",
        description=tpl["description"],
        template=tpl["key"],
        rows=_rows_from_template(tpl),
    )
    store.add_product(product)

    model = ModelDefinition(
        name="BS spot=100 vol=15%",
        kind="BSModelData_",
        bs=BSModelParams(spot=100.0, vol=0.15, rate=0.0, div=0.0),
    )
    store.add_model(model)

    trade = Trade(
        name="EQ Call 120 2025",
        book="EQ-EXOTICS",
        counterparty="ACME",
        notional=1_000_000.0,
        quantity=1.0,
        product_id=product.id,
        model_id=model.id,
        tags=["demo", "european"],
    )
    store.add_trade(trade)

    portfolio = Portfolio(
        name="Demo Equity Book",
        description="Seeded demo portfolio",
        trade_ids=[trade.id],
    )
    store.add_portfolio(portfolio)
