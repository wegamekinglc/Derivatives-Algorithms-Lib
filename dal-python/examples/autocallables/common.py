import argparse
from dataclasses import dataclass
from datetime import date
import math
from pathlib import Path
import sys


@dataclass(frozen=True)
class Case:
    slug: str
    title: str
    coupon: float = 0.12
    knock_in: float = 0.65
    knock_out: float = 1.0
    first_call: int = 1
    step_down: float = 0.0
    phoenix: bool = False
    memory: bool = False
    airbag: bool = False
    protected: bool = False
    participation: float = 0.0
    worst_of: bool = False


CASES = {
    case.slug: case
    for case in (
        Case("classic_snowball", "Classic snowball"),
        Case("stepdown_snowball", "Step-down snowball", knock_out=1.05, step_down=0.015),
        Case("lockout_snowball", "Six-month lockout snowball", first_call=6),
        Case("phoenix", "Phoenix conditional coupon", phoenix=True),
        Case("memory_phoenix", "Phoenix with coupon memory", phoenix=True, memory=True),
        Case("stepdown_memory_phoenix", "Step-down memory Phoenix", knock_out=1.05, step_down=0.015, phoenix=True, memory=True),
        Case("airbag_snowball", "Airbag snowball", airbag=True),
        Case("capital_protected", "Capital-protected autocall", coupon=0.04, phoenix=True, protected=True),
        Case("upside_participation", "Upside-participating autocall", coupon=0.06, knock_out=1.2, participation=1.0),
        Case("worst_of_phoenix", "Two-asset worst-of Phoenix", phoenix=True, worst_of=True),
    )
}


TODAY = date(2026, 1, 2)
OBSERVATIONS = tuple(date(2026 + month // 12, month % 12 + 1, 2) for month in range(1, 13))
RATE = 0.02
VOL = 0.20
SMOOTH = 0.001


def build_events(case):
    dates = [TODAY, *OBSERVATIONS]
    events = ["alive = 1 is_ki = 0 unpaid = 0"]
    previous = TODAY
    for month, observation in enumerate(OBSERVATIONS, start=1):
        accrued = (observation - TODAY).days / 365.0
        period = (observation - previous).days / 365.0
        performance = "MIN(FIX(EQ[A]) / 100, FIX(EQ[B]) / 120)" if case.worst_of else "SPOT()"
        script = [f"performance = {performance}"]
        if not case.phoenix:
            script.append(f"if performance < {case.knock_in:.12f} then is_ki = 1 end")
        else:
            coupon = case.coupon * period
            if case.memory:
                script.append(f"unpaid = unpaid + {coupon:.12f}")
            amount = "unpaid" if case.memory else f"{coupon:.12f}"
            payment = f"pay PAYS alive * {amount}"
            if case.memory:
                payment += " unpaid = 0"
            if case.protected:
                script.append(payment)
            else:
                script.append(f"if performance >= 0.8 then {payment} end")
        if month >= case.first_call:
            barrier = max(0.85, case.knock_out - case.step_down * (month - 1))
            redemption = 1.0 if case.phoenix else 1.0 + case.coupon * accrued
            script.append(f"if performance >= {barrier:.12f} then pay PAYS alive * {redemption:.12f} alive = 0 end")
        if month == 12:
            if case.protected:
                redemption = "1"
            elif case.phoenix:
                script.append(f"is_ki = 0 if performance < {case.knock_in:.12f} then is_ki = 1 end")
                redemption = "1 - is_ki + is_ki * MIN(performance, 1)"
            else:
                downside = f"MIN(performance / {case.knock_in:.12f}, 1)" if case.airbag else "MIN(performance, 1)"
                redemption = f"(1 - is_ki) * {1.0 + case.coupon * accrued:.12f} + is_ki * {downside}"
                if case.participation:
                    redemption += f" + {case.participation:.12f} * MAX(performance - 1, 0)"
            script.append(f"pay PAYS alive * ({redemption})")
        events.append("\n".join(script))
        previous = observation
    return dates, events


def value(case, paths):
    import dal

    dal.EvaluationDate_Set(dal.Date_(TODAY.year, TODAY.month, TODAY.day))
    dates, events = build_events(case)
    product = dal.Product_New([dal.Date_(d.year, d.month, d.day) for d in dates], events)
    if case.worst_of:
        model = dal.CorrelatedBSModelData_New(
            ["EQ[A]", "EQ[B]"], [100.0, 120.0], [VOL, 0.25], [0.0, 0.0], RATE,
            dal.DoubleMatrix_([[1.0, 0.4], [0.4, 1.0]]),
        )
    else:
        model = dal.BSModelData_New(1.0, VOL, RATE, 0.0)
    results = {}
    for name, aad in (("plain", False), ("aad", True)):
        simulation = dal.MonteCarloSettings_(method="sobol", use_bb=False, enable_aad=aad, smooth=SMOOTH, compiled=True)
        result = dal.MonteCarlo_ValueWithSettings(product, model, paths, simulation=simulation)
        if not all(math.isfinite(number) for number in result.values()):
            raise ValueError(f"{case.slug}: non-finite {name} result")
        results[name] = result
    return results


def run(slug):
    parser = argparse.ArgumentParser(description=CASES[slug].title)
    parser.add_argument("--paths", type=int, default=16384)
    args = parser.parse_args()
    if not 1 <= args.paths <= 2147483647:
        parser.error("--paths must be in [1, 2147483647]")
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from float_format import format_float

    case = CASES[slug]
    results = value(case, args.paths)
    print(f"\n{case.title}; notional 1; ACT/365F; {args.paths} Sobol paths")
    print(f"{'Method / input derivative':<42}{'PV / derivative':>20}")
    print("-" * 62)
    print(f"{'MC double':<42}{format_float(results['plain']['PV'], 6):>20}")
    print(f"{'MC AAD (smoothed barriers)':<42}{format_float(results['aad']['PV'], 6):>20}")
    for key, number in results["aad"].items():
        if key != "PV":
            print(f"{key:<42}{format_float(number, 6):>20}")
    print("-" * 62)
    print("Derivatives hold contract terms fixed; rate/vol/div inputs are decimal units.")
