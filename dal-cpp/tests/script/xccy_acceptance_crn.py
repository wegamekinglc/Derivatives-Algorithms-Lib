"""Qualify h/h2/h4 with the accepted independent scalar path reference.

Raw binary64 subtraction failures remain in the output. Wider cashflows reuse
the existing Brownian integration/Cholesky reference, not production pricing.
"""

import argparse
import json
from pathlib import Path

import mpmath as mp

from xccy_acceptance_reference import day, number, require
from xccy_mc_reference import Cashflows


def reference_input(data):
    model = data["model"]
    curves = [{"currency": c["currency"], "route": c["route"], "days": list(map(day, c["dates"])),
               "labels": c["labels"], "values": c["values"], "jacobian": c["jacobian"]} for c in model["curves"]]
    return {"grid": list(map(day, data["grid"])), "bucket_days": list(map(day, model["bucket_dates"])),
            "bucket_dates": model["bucket_dates"], "a": model["a"], "correlation": model["correlation"],
            "sigma": model["sigma"], "normals": data["normals"], "curves": curves, "spot": model["spot"], "foreign_spread": False}


def evaluate(data, digits):
    with mp.workdps(digits):
        oracle = Cashflows(reference_input(data))
        rows = []
        for actual in data["rows"]:
            scale = number(actual["scale"])
            values = []
            for divisor in (1, 2, 4):
                step = scale / 1000 / divisor
                up, down = oracle.price(actual["label"], step), oracle.price(actual["label"], -step)
                up2, down2 = oracle.price(actual["label"], 2 * step), oracle.price(actual["label"], -2 * step)
                values.append((8 * (up - down) - (up2 - down2)) / (12 * step))
            rows.append(values)
        return oracle.price(), rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = json.loads(args.capture.read_text())
    coarse_price, coarse = evaluate(data, 50)
    price, fine = evaluate(data, 80)
    with mp.workdps(90):
        require(abs(number(data["price"]) - price) <= mp.mpf("1e-10"), "independent fixed-path price")
        rows = []
        for actual, low, high in zip(data["rows"], coarse, fine):
            rounding = 4 * max(abs(a - b) for a, b in zip(low, high))
            refinement = 4 * max(abs(high[0] - high[1]), abs(high[1] - high[2]))
            wide_error = max(abs(number(actual["aad"]) - v) for v in high)
            raw_errors = [abs(step["gradient"] - actual["aad"]) for step in actual["steps"]]
            rows.append({"label": actual["label"], "raw_double_errors": raw_errors,
                         "raw_double_passed": all(x <= 1e-8 for x in raw_errors),
                         "wide_fd": list(map(str, high)), "refinement_indicator": str(refinement), "rounding_indicator": str(rounding),
                         "aad_error": str(wide_error), "wide_passed": bool(wide_error + rounding + refinement <= mp.mpf("1e-8"))})
        output = {"price": str(price), "price_refinement": str(abs(price - coarse_price)), "mapping": data["mapping"],
                  "reservation": data["reservation"], "rows": rows}
        args.output.write_text(json.dumps(output, indent=2) + "\n")
        require(all(row["wide_passed"] for row in rows), "independent CRN native gradients; raw artifact retained")
    print("PASS: independent 50/80-digit h/h2/h4 gradients; raw binary64 subtraction failures retained")


if __name__ == "__main__":
    main()
