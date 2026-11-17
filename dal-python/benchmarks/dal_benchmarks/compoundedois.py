"""Prepared OIS audit projections with the native audit driver's contracts."""

from datetime import date, timedelta

import dal

from .harness import Workload, require


def audit_fixture(mode, count):
    require(
        (mode, count) in (("None", 30), ("Lockout", 1825)),
        "unsupported OIS audit boundary",
    )
    start = date(2024, 1, 1)
    end = start
    for _ in range(count):
        end += timedelta(days=1)
        while not (
            end == date(2024, 1, 13) or (end.weekday() < 5 and end != date(2024, 1, 15))
        ):
            end += timedelta(days=1)
    native = lambda day: dal.Date_(day.year, day.month, day.day)
    payment = end + timedelta(days=2)
    last = payment + timedelta(days=20)
    today = date(2023, 12, 1)
    convention = dal.CompoundedOisObservationConvention_(
        mode, lockout_business_days=5 if mode == "Lockout" else 0
    )
    spec = dal.CompoundedOisScriptSpec_(
        instrument_id="audit-" + mode,
        direction="PayFixed",
        notional=100.0,
        coupon=0.02,
        currency="USD",
        collateral_currency="USD",
        collateral_type="OIS",
        discount_route="USD.OIS",
        projection_route="USD.SOFR",
        fixing_index="USD-SOFR",
        observation_basis="ACT_360",
        fixed_basis="ACT_365F",
        calendar=dal.HolidayCenterData_(
            center="holiday-and-working-weekend",
            holidays=[dal.Date_(2024, 1, 15)],
            work_weekends=[dal.Date_(2024, 1, 13)],
        ),
        rule_id="dal.compounded-ois-cashflows/2",
        observation_convention=convention,
        fixed_leg=[
            dal.CompoundedOisFixedRow_(
                start=native(start),
                end=native(end),
                payment=native(payment),
                accrual=(end - start).days / 365,
            )
        ],
        coupons=[
            dal.CompoundedOisCouponRow_(
                coupon_id="coupon",
                start=native(start),
                end=native(end),
                payment=native(payment),
            )
        ],
    )
    valuation = dal.ScriptValuationSettings_(
        evaluation_date=native(today), fixings=dal.MarketFixingSnapshot_New({})
    )
    prepared = dal.CompoundedOis_Prepare(spec, valuation)
    geometry = dal.CurveInstrumentGeometry_(
        today=native(today),
        currency="USD",
        knot_dates=[native(today), native(last)],
        parameterization="LOG_DISCOUNT",
        calibrate_discount_curve=False,
        target_overnight_route="USD.SOFR",
    )
    market = dal.CurveInstrumentMarket_(
        routes=[
            dal.CurveScriptRoute_(id="USD.OIS", use_target_curve=True),
            dal.CurveScriptRoute_(id="USD.SOFR", projection=True, overnight=True),
        ]
    )
    request = dal.CurveScriptEvaluationRequest_(
        geometry=geometry,
        market=market,
        valuation=valuation,
        parameters=[-0.02 * (last - today).days / 365],
        compounded_ois=prepared,
    )
    instrument = dal.CompoundedOisScriptInstrument_New(prepared)
    result = dal.CurveScriptInstrument_Evaluate(instrument, request)
    return prepared, result


def normalized(table):
    return [
        [str(value) if isinstance(value, dal.DateTime_) else value for value in row]
        for row in table
    ]


def prepare_audit(mode, count, columns):
    require(columns in (22, 26), "unsupported audit projection")
    prepared, result = audit_fixture(mode, count)
    before = dal.CompoundedOis_Get_ObservationSchedule(prepared)
    after = dal.CurveScriptEvaluation_Get_Table(result, "observation_schedule")
    require(len(before) == len(after) == count + 1, "audit row count")
    require(
        all(len(row) == 22 for row in before) and all(len(row) == 26 for row in after),
        "audit column count",
    )
    require(
        normalized(before) == normalized([row[:22] for row in after]),
        "audit public prefix changed",
    )
    require(
        after[1][3] == 0 and after[1][24:26] == [4, 1], "audit coordinate bases changed"
    )
    run = (
        (lambda: dal.CompoundedOis_Get_ObservationSchedule(prepared))
        if columns == 22
        else (
            lambda: dal.CurveScriptEvaluation_Get_Table(result, "observation_schedule")
        )
    )
    expected = normalized(before if columns == 22 else after)
    detached = run()
    detached[1][0] = "modified"
    require(normalized(run()) == expected, "audit mutation leaked into native owner")

    def validate(value):
        require(normalized(value) == expected, "detached audit projection changed")

    return Workload(run, validate)
