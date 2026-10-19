import copy
import math
from concurrent.futures import ThreadPoolExecutor

import pytest

import dal


def request(*, flat=False, nonflat=False, terms=3, strikes=9, bands=True):
    today = dal.Date_(2026, 10, 19)
    dates = [dal.Date_(2027, 4, 19), dal.Date_(2027, 10, 19), dal.Date_(2028, 10, 19)][:terms]
    times = [182 / 365, 1.0, 731 / 365][:terms]
    forwards = [100 * math.exp(.02 * t) for t in times]
    discounts = [math.exp(-.03 * t) for t in times]
    if nonflat:
        forwards = [101.0, 100.5, 103.0][:terms]
        discounts = [.99, 1.01, .98][:terms]
        market = dal.MarketIvMarket_(today, 100.0, [dal.MarketIvTerm_(d, f, b) for d, f, b in zip(dates, forwards, discounts)])
    else:
        market = dal.MarketIvFlatMarket_New(today, 100.0, .03, .01, dates)
    quotes = []
    for j, (date, t, f, discount) in enumerate(zip(dates, times, forwards, discounts)):
        for i in range(strikes):
            k = (i - (strikes - 1) / 2) * .08
            theta, rho, eta = .04 * t, -.4, 0.0 if flat else .8
            phi = eta / math.sqrt(theta * (1 + theta))
            w = theta / 2 * (1 + rho * phi * k + math.sqrt((phi * k + rho) ** 2 + 1 - rho * rho))
            sigma, strike = math.sqrt(w / t), f * math.exp(k)
            put = i % 2 == 1
            price = i % 3 == 1
            d1, d2 = -k / math.sqrt(w) + math.sqrt(w) / 2, -k / math.sqrt(w) - math.sqrt(w) / 2
            ncdf = lambda x: .5 * math.erfc(-x / math.sqrt(2))
            mid = discount * (strike * ncdf(-d2) - f * ncdf(-d1) if put else f * ncdf(d1) - strike * ncdf(d2)) if price else sigma
            quotes.append(dal.MarketIvQuote_(id=f"{j}:{i}", expiry=date, strike=strike, option_type="PUT" if put else "CALL",
                          quote_type="PRICE" if price else "DECIMAL_IV", mid=mid, bid=mid - (1e-5 if price else 1e-6) if bands else None, ask=mid + (1e-5 if price else 1e-6) if bands else None))
    return dal.MarketIvFitRequest_(market, quotes)


def fit(req):
    result = dal.MarketIvFitResult_New(req)
    assert result.is_success, (result.status, result.failure, [(s.termination, s.projected_gradient) for s in result.starts])
    return result


def test_synthetic_mixed_quotes_flat_and_owning():
    for flat in (True, False):
        result = fit(request(flat=flat, nonflat=True))
        snapshot = copy.deepcopy(result.snapshot)
        assert snapshot.market.carry_mode == "FORWARD_DISCOUNT_KNOTS"
        if not flat:
            assert snapshot.rho == pytest.approx(-.4, abs=1e-7)
            assert snapshot.eta == pytest.approx(.8, abs=1e-7)
        assert max(abs(q.normalized_residual) for q in result.quotes) < 1e-10
        rows = result.request.quotes
        rows.clear()
        assert len(snapshot.request.quotes) == 27
        del result
        point = dal.MarketIvPointResult_New(snapshot, dal.MarketIvQuery_(100., .8))
        assert point.derivative_status == point.dupire_status == "SUCCESS"
        if flat:
            assert point.local_vol == pytest.approx(.2, abs=1e-10)
        with pytest.raises(RuntimeError, match="FLAT_CARRY"):
            dal.MarketIvSurfaceAsLegacyIVS(snapshot)


def test_high_precision_independent_price_derivatives():
    mp = pytest.importorskip("mpmath")
    mp.mp.dps = 80
    snapshot = fit(request(nonflat=True)).snapshot
    ts, ys = list(map(mp.mpf, snapshot.times)), list(map(mp.mpf, snapshot.theta))
    hs = [b - a for a, b in zip(ts, ts[1:])]
    ds = [(b - a) / h for a, b, h in zip(ys, ys[1:], hs)]

    def endpoint(h0, h1, d0, d1):
        return max(mp.mpf(0), min(3 * d0, ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1)))

    slopes = [endpoint(hs[0], hs[1], ds[0], ds[1])]
    for i in range(1, len(ts) - 1):
        w1, w2 = 2 * hs[i] + hs[i - 1], hs[i] + 2 * hs[i - 1]
        slopes.append((w1 + w2) / (w1 / ds[i - 1] + w2 / ds[i]) if ds[i - 1] and ds[i] else mp.mpf(0))
    slopes.append(endpoint(hs[-1], hs[-2], ds[-1], ds[-2]))
    assert all(abs(a - b) < mp.mpf("1e-15") for a, b in zip(slopes, snapshot.slopes))

    def theta(t):
        if t > ts[-1]:
            return ys[-1] + slopes[-1] * (t - ts[-1])
        i = next(i for i in range(len(hs)) if t < ts[i + 1])
        u = (t - ts[i]) / hs[i]
        return (2 * u**3 - 3 * u**2 + 1) * ys[i] + (-2 * u**3 + 3 * u**2) * ys[i + 1] + hs[i] * ((u**3 - 2 * u**2 + u) * slopes[i] + (u**3 - u**2) * slopes[i + 1])

    rho, eta = mp.mpf(snapshot.rho), mp.mpf(snapshot.eta)
    def variance(k, t):
        th = theta(t)
        z = eta * k / mp.sqrt(th * (1 + th))
        return th / 2 * (1 + rho * z + mp.sqrt((z + rho) ** 2 + 1 - rho**2))

    forwards = [mp.mpf(100)] + [mp.mpf(x.forward) for x in snapshot.market.terms]
    discounts = [mp.mpf(1)] + [mp.mpf(x.discount) for x in snapshot.market.terms]
    def carry(t, values):
        i = next((i for i in range(len(hs)) if t < ts[i + 1]), len(hs) - 1)
        return mp.exp(mp.log(values[i]) + (mp.log(values[i + 1]) - mp.log(values[i])) / hs[i] * (t - ts[i]))

    ncdf = lambda x: mp.erfc(-x / mp.sqrt(2)) / 2
    def call(kstrike, t):
        f, d = carry(t, forwards), carry(t, discounts)
        w = variance(mp.log(kstrike / f), t)
        d1 = mp.log(f / kstrike) / mp.sqrt(w) + mp.sqrt(w) / 2
        return d * (f * ncdf(d1) - kstrike * ncdf(d1 - mp.sqrt(w)))

    for t in (.0001, .2, .75, 1.5, 3.0):
        for k in ((0.,) if t < .01 else (-.5, 0., .5)):
            time = mp.mpf(t)
            strike = float(carry(time, forwards) * mp.exp(k))
            K = mp.mpf(strike)
            kval = mp.log(K / carry(time, forwards))
            point = dal.MarketIvPointResult_New(snapshot, dal.MarketIvQuery_(strike, t))
            refs = [variance(kval, time), mp.diff(lambda x: variance(x, time), kval), mp.diff(lambda x: variance(x, time), kval, 2),
                    t * mp.diff(lambda x: variance(kval, x), time), call(K, time), K * mp.diff(lambda x: call(x, time), K),
                    K**2 * mp.diff(lambda x: call(x, time), K, 2), t * mp.diff(lambda x: call(K, x), time)]
            df = carry(time, forwards) * carry(time, discounts)
            actual = [point.total_variance, point.variance_k, point.variance_kk, t * point.variance_t, point.call_price, K * point.call_k, K**2 * point.call_kk, t * point.call_t]
            for i, (got, expected) in enumerate(zip(actual, refs)):
                scale = df if i >= 4 else 1
                assert abs((mp.mpf(got) - expected) / scale) <= mp.mpf("1e-11") + mp.mpf("1e-9") * abs(expected / scale), (t, k, i)
            w, wk, wkk, wt = refs[:4]
            g = (1 - kval * wk / (2 * w)) ** 2 - wk**2 / 4 * (1 / w + mp.mpf(".25")) + wkk / 2
            assert point.local_variance == pytest.approx(float(wt / t / g), abs=1e-10, rel=1e-8)


def test_full_refit_independently_rebuilds_raw_request():
    req = request(bands=False)
    base = fit(req).snapshot
    outputs = [dal.MarketIvRefitOutput_(dal.MarketIvQuery_(105., .8), q) for q in ("CALL_PRICE", "TOTAL_VARIANCE", "LOCAL_VOL")]
    for h in (1e-5, 5e-6, 2.5e-6):
        risk = dal.MarketIvRefitRiskResult_New(base, dal.MarketIvRefitRiskRequest_(["1:4"], outputs, steps=[h]))
        assert risk.complete
        for sign in (-1, 1):
            rows = [dal.MarketIvQuote_(id=q.id, expiry=q.expiry, strike=q.strike, option_type=q.option_type, quote_type=q.quote_type,
                    mid=q.mid + (sign * h if q.id == "1:4" else 0), bid=None if q.bid is None else q.bid + (sign * h if q.id == "1:4" else 0),
                    ask=None if q.ask is None else q.ask + (sign * h if q.id == "1:4" else 0), weight=q.weight) for q in req.quotes]
            independent = fit(dal.MarketIvFitRequest_(req.market, rows, settings=req.settings)).snapshot
            point = dal.MarketIvPointResult_New(independent, outputs[0].query)
            for row, expected in zip(risk.responses, [point.call_price, point.total_variance, point.local_vol]):
                assert (row.plus if sign == 1 else row.minus) == pytest.approx(expected, abs=1e-11)
            if base.regime_signature != independent.regime_signature:
                assert risk.axis[0].regime_changed
        assert all(row.symmetric_bump_slope is not None for row in risk.responses)
    limited = dal.MarketIvRefitRiskResult_New(base, dal.MarketIvRefitRiskRequest_(["1:4"], outputs,
              settings=dal.MarketIvRefitRiskSettings_(max_total_model_row_evaluations=1)))
    assert not limited.complete and limited.execution.model_row_evaluations == 1


def test_strict_inputs_failures_and_concurrent_ownership():
    for value in (True, "0.2", dal.OptionType_.CALL):
        with pytest.raises((TypeError, RuntimeError)):
            dal.MarketIvFitSettings_(rho_limit=value)
    with pytest.raises(TypeError):
        dal.MarketIvFitSettings_(unknown=1)
    with pytest.raises((TypeError, RuntimeError)):
        dal.MarketIvFitSettings_(max_iterations_per_start=2**80)
    req = request(flat=True)
    with pytest.raises(RuntimeError, match="duplicate"):
        dal.MarketIvFitRequest_(req.market, req.quotes + req.quotes)
    no_work = dal.MarketIvFitResult_New(dal.MarketIvFitRequest_(req.market, req.quotes, settings=dal.MarketIvFitSettings_(max_model_row_evaluations=0)))
    assert no_work.status == "WORK_LIMIT"
    assert no_work.selected_start is None
    assert no_work.starts[0].rank is None
    assert len(no_work.starts[0].parameters) == 5
    with pytest.raises(RuntimeError, match="SnapshotUnavailable"):
        _ = no_work.snapshot
    def evaluate(_):
        s = fit(req).snapshot
        return dal.MarketIvPointResult_New(s, dal.MarketIvQuery_(100., .8)).local_vol
    with ThreadPoolExecutor(max_workers=2) as pool:
        assert list(pool.map(evaluate, range(4))) == pytest.approx([.2] * 4, abs=1e-10)


def test_positive_iv_underflow_is_explicit_numeric_failure():
    req = request(flat=True, terms=1, strikes=1, bands=False)
    original = req.quotes[0]
    quote = dal.MarketIvQuote_(id=original.id, expiry=original.expiry, strike=original.strike, option_type="CALL", quote_type="DECIMAL_IV", mid=1e-300)
    result = dal.MarketIvFitResult_New(dal.MarketIvFitRequest_(req.market, [quote]))
    assert result.status == "NUMERIC_RANGE"
    with pytest.raises(RuntimeError, match="SnapshotUnavailable"):
        _ = result.snapshot


def test_discrete_regime_switch_and_requested_output_failures():
    output = dal.MarketIvRefitOutput_(dal.MarketIvQuery_(100., .8), "LOCAL_VOL")
    for flat, quote_id in ((False, "1:4"), (True, "1:8")):
        base = fit(request(flat=flat, bands=False)).snapshot
        risk = dal.MarketIvRefitRiskResult_New(base, dal.MarketIvRefitRiskRequest_([quote_id], [output], steps=[1e-4]))
        assert risk.complete
        assert risk.axis[0].regime_changed is flat
        assert risk.responses[0].status == ("REGIME_CHANGED" if flat else "SUCCESS")
    base = fit(request(bands=False)).snapshot
    short = dal.MarketIvRefitOutput_(dal.MarketIvQuery_(100., 1e-12), "LOCAL_VOL")
    unavailable = dal.MarketIvRefitRiskResult_New(base, dal.MarketIvRefitRiskRequest_(["1:4"], [short], steps=[1e-4]))
    assert not unavailable.complete
    assert unavailable.responses[0].symmetric_bump_slope is None
    assert unavailable.responses[0].plus_output_status == "SINGULAR_DENSITY"
    invalid = dal.MarketIvRefitRiskResult_New(base, dal.MarketIvRefitRiskRequest_(["1:4"], [output], steps=[1e-300]))
    assert not invalid.complete
    assert invalid.axis[0].plus_status == invalid.axis[0].minus_status == "BUMP_INVALID"


def test_exact_grid_export_and_legacy_ownership():
    snapshot = fit(request(flat=True)).snapshot
    legacy = dal.MarketIvSurfaceAsLegacyIVS(snapshot)
    grid_request = dal.MarketIvDupireRequest_([80., 100., 120.], [.25, 1.])
    grid = dal.MarketIvDupireResult_New(snapshot, grid_request)
    assert grid.is_success and len(grid.nodes) == 6
    assert [p.local_vol for p in grid.nodes] == pytest.approx([.2] * 6, abs=1e-10)
    copied = grid.request.spots
    copied.append(200.)
    assert grid.request.spots == [80., 100., 120.]
    surface = grid.surface
    del grid, snapshot
    assert isinstance(surface, dal.LocalVolSurfaceData_)
    assert legacy.implied_vol(100., .8) == pytest.approx(.2, abs=1e-10)
    with pytest.raises(RuntimeError, match="max_grid_points"):
        dal.MarketIvDupireRequest_([80., 100., 120.], [.25, 1.], settings=dal.MarketIvDupireSettings_(max_grid_points=5))
    with pytest.raises(RuntimeError, match="strictly increasing"):
        dal.MarketIvDupireRequest_([100., 80.], [.25, 1.])
