"""Owning market-grid reports and fixing input through the public bindings."""

import math

import dal

from .harness import Workload, require


def model_for(terms=1):
    today = dal.Date_(2026, 11, 16)
    expiry = today.AddDays(365)
    if terms == 1:
        carry = [dal.MarketIvTerm_(expiry, 102.0, 0.98)]
        forward = 102.0
    else:
        carry = []
        for i in range(1, terms + 1):
            days = 365 * i // terms
            time = days / 365
            carry.append(
                dal.MarketIvTerm_(
                    today.AddDays(days),
                    100 * math.exp(0.02 * time),
                    math.exp(-0.02 * time),
                )
            )
        forward = 100 * math.exp(0.02)
    market = dal.MarketIvMarket_(today, 100.0, carry)
    quote = dal.MarketIvQuote_(
        id="atm",
        expiry=expiry,
        strike=forward,
        option_type="CALL",
        quote_type="DECIMAL_IV",
        mid=0.2,
    )
    fit = dal.MarketIvFitResult_New(dal.MarketIvFitRequest_(market, [quote]))
    require(fit.is_success, "market-IV fixture fit failed")
    grid = dal.MarketIvDupireResult_New(
        fit.snapshot, dal.MarketIvDupireRequest_([80.0, 100.0, 120.0], [0.1, 1.0])
    )
    require(grid.is_success, "market-IV fixture grid failed")
    return dal.MarketIvLocalVolModelData_New(
        "market", grid, dal.MarketIvLocalVolModelSettings_("EQ[A]", "USD", "W_EQ", 1.25)
    )


def fixing_rows(count):
    today = dal.Date_(2026, 11, 16)
    return {
        "EQ[A]": {
            dal.DateTime_(today.AddDays(-i), 12): 99.0 for i in range(1, count + 1)
        }
    }


def prepare_execution(count):
    require(count in (0, 1, 365), "unsupported fixing count")
    model = model_for()
    today = dal.Date_(2026, 11, 16)
    product = dal.Product_New([today.AddDays(365)], ["pay PAYS 1"])
    simulation = dal.MonteCarloSettings_(method="mrg32")

    def valuation(rows):
        snapshot = dal.MarketFixingSnapshot_New(fixing_rows(rows)) if rows else None
        return dal.ScriptValuationSettings_(evaluation_date=today, fixings=snapshot)

    frozen = valuation(count)

    def inspect(settings):
        return dal.MarketIvLocalVolModelData_Get_Execution(
            product, model, valuation=settings, simulation=simulation
        )

    expected = inspect(frozen)
    require(
        expected == inspect(valuation(count)), "independent fixing copy changed report"
    )
    if count:
        require(
            expected["request_identity"] != inspect(valuation(0))["request_identity"],
            "fixing history missing from identity",
        )
    if count == 365:
        require(
            expected["request_identity"] != inspect(valuation(1))["request_identity"],
            "history length missing from identity",
        )
    pv = dal.MonteCarlo_ValueWithSettings(
        product, model, 16, valuation=frozen, simulation=simulation
    )["PV"]
    require(abs(pv - 0.98) <= 2e-12, "unit payment preflight failed")

    def validate(value):
        require(value == expected and len(value) == 23, "execution report changed")
        require(
            value["schema"] == "dal.market-iv-local-vol-execution/1",
            "execution schema changed",
        )
        require(
            value["prepared_columns"] == value["internal_steps"]
            and value["per_path_market_queries"] == 0,
            "execution counters",
        )
        require(
            value["numeric_payload_bytes"] <= value["planned_numeric_payload_bytes"],
            "payload ledger",
        )
        require(
            value["peak_workspace_bytes"] <= value["planned_workspace_bytes"],
            "workspace ledger",
        )

    return Workload(lambda: inspect(frozen), validate)


def prepare_fixing_input(count=365):
    rows = fixing_rows(count)

    def validate(snapshot):
        require(
            all(
                snapshot.find("EQ[A]", time) == value
                for time, value in rows["EQ[A]"].items()
            ),
            "fixing input lost history",
        )

    return Workload(lambda: dal.MarketFixingSnapshot_New(rows), validate)


def prepare_carry(terms):
    model = model_for(terms)
    expected = model.carry_terms
    require(
        len(expected) == terms + 2 and all(len(row) == 8 for row in expected),
        "carry report shape",
    )

    def validate(value):
        require(value == expected, "detached carry report changed")

    return Workload(lambda: model.carry_terms, validate)
