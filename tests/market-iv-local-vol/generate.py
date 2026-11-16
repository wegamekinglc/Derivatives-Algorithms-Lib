"""Emit deterministic 90-digit quotes and the frozen grid/probe requests."""

import argparse
import json
from pathlib import Path

import mpmath as mp
import numpy as np

from reference import (
    QUOTE_DAYS,
    RATIOS,
    TERM_DAYS,
    black,
    carry,
    pchip_slopes,
    ssvi,
    theta_at,
)


def generating_curve(nonlinear):
    times = [mp.mpf(day) / 365 for day in (0, *QUOTE_DAYS)]
    values = (
        list(map(mp.mpf, ("0", ".009", ".018", ".04", ".09")))
        if nonlinear
        else [mp.mpf(".04") * time for time in times]
    )
    return times, values, pchip_slopes(times, values)


def fixture(name, flat, smile, nonlinear=False):
    times, values, slopes = generating_curve(nonlinear)
    quotes = []
    for days in QUOTE_DAYS:
        time = mp.mpf(days) / 365
        forward, discount = carry(time, flat)
        theta, _ = theta_at(time, times, values, slopes)
        for ratio in RATIOS:
            ordinal = len(quotes)
            strike = forward * mp.mpf(ratio)
            variance = (
                ssvi(theta, mp.mpf("-.35"), mp.mpf(".60"), mp.log(mp.mpf(ratio)))
                if smile
                else theta
            )
            call = ordinal % 2 == 0
            price = black(forward, discount, strike, variance, call)
            price_unit = ordinal % 4 < 2
            quotes.append(
                dict(
                    id=f"q{ordinal:02}",
                    days=days,
                    strike=mp.nstr(strike, 90),
                    option="CALL" if call else "PUT",
                    unit="PRICE" if price_unit else "DECIMAL_IV",
                    mid=mp.nstr(price if price_unit else mp.sqrt(variance / time), 90),
                    generating_price=mp.nstr(price, 90),
                    generating_variance=mp.nstr(variance, 90),
                )
            )
    probe_times = sorted(
        {
            mp.mpf(1) / 36500,
            mp.mpf(1) / 146000,
            mp.mpf("2.25"),
            *(mp.mpf(day) / 365 for day in TERM_DAYS[1:]),
            *(
                mp.mpf(day) / 365 + shift
                for day in QUOTE_DAYS
                for shift in (mp.mpf("-1e-10"), mp.mpf("1e-10"))
            ),
        }
    )
    probes = [
        dict(
            time=mp.nstr(time, 90),
            strike=mp.nstr(carry(time, flat)[0] * mp.mpf(ratio), 90),
        )
        for time in probe_times
        for ratio in RATIOS
    ]
    probes.extend(
        dict(time=mp.nstr(mp.mpf(1) / 36500, 90), strike=str(strike))
        for strike in (20, 400)
    )
    return dict(id=name, flat=flat, strict=not smile, quotes=quotes, probes=probes)


def grid(name, spots=81, columns=49, tau=1 / 36500, low=20, high=400, end=2.25):
    # The union is formed before enforcing the production 100000 point ceiling.
    times = sorted(
        {
            *np.linspace(tau, end, columns),
            *(day / 365 for day in TERM_DAYS[1:] if day / 365 <= end),
        }
    )
    if spots * len(times) > 100000:
        raise ValueError("Frozen grid exceeds maxGridPoints")
    return dict(id=name, spots=np.geomspace(low, high, spots).tolist(), times=times)


def request(minimal=False):
    with mp.workdps(90):
        fixtures = [fixture("F1_knots", False, False)]
        if minimal:
            fixtures[0]["probes"] = [
                dict(time=mp.nstr(mp.mpf(1) / 36500, 90), strike="20")
            ]
            grids = [
                dict(
                    id="minimal_tau_wing",
                    spots=[20.0, 100.0],
                    times=[1 / 36500, 91 / 365],
                )
            ]
        else:
            fixtures += [
                fixture("F1_flat", True, False),
                fixture("F2_linear", False, True),
                fixture("F2_nonlinear", False, True, True),
            ]
            grids = [
                grid("base"),
                grid("space161", 161),
                grid("space321", 321),
                grid("time97", 321, 97),
                grid("time193", 321, 193),
                grid("tau_half", 321, 193, 1 / 73000),
                grid("tau_quarter", 321, 193, 1 / 146000),
                grid("expanded", 321, 193, 1 / 36500, 10, 600),
                grid("time_tail", 321, 193, end=2),
            ]
        return dict(
            schema="dal.market-iv-independent-request/1",
            decimal_digits=90,
            fixtures=fixtures,
            grids=grids,
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--minimal", action="store_true")
    args = parser.parse_args()
    args.output.write_text(
        json.dumps(request(args.minimal), indent=2) + "\n", encoding="utf-8"
    )
