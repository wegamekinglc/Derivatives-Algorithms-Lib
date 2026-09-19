"""Verify the workbook's common snapshot using the installed Python API and analytic oracles."""

import argparse
import json
import math
from pathlib import Path

import dal

parser = argparse.ArgumentParser()
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--excel-results", type=Path)
parser.add_argument("--native-diagnostics", type=Path)
args = parser.parse_args()
D, H, F, P = (dal.Date_(2026, 9, day) for day in (12, 11, 15, 22))
snapshot = dal.MarketFixingSnapshot_New({"EQ[AAPL]": {dal.DateTime_(H, 0): 80., dal.DateTime_(D, 0): 80.}})
valuation = dal.ScriptValuationSettings_(evaluation_date=D, fixings=snapshot)
simulation = dal.MonteCarloSettings_(enable_aad=True, compiled=True)
model = dal.BSModelData_New(100., 0., .05, .02)
zero = dal.BSModelData_New(100., 0., 0., 0.)
contract = dal.ScriptProductSettings_(default_index="EQ[AAPL]")
history = dal.Product_New(["SCALE", H, P], ["2.0", "x = SCALE * FIX(EQ[AAPL])", "pay PAYS x"], settings=contract)
mixed = dal.Product_New(["SCALE", H, P], ["2.0", "x = SCALE * FIX(EQ[AAPL])", "pay PAYS x + FIX(EQ[AAPL], 2026-09-15)"], settings=contract)
future = dal.Product_New([P], ["pay PAYS FIX(EQ[AAPL], 2026-09-15)"])
results = {name: dal.MonteCarlo_ValueWithSettings(product, market, 257, valuation=valuation, simulation=simulation)
           for name, product, market in (("historical", history, model), ("historical_zero", history, zero),
                                        ("mixed", mixed, zero), ("future", future, model))}
tp, tf = 10./365., 3./365.
hpv, fpv = 160.*math.exp(-.05*tp), 100.*math.exp(.03*tf-.05*tp)
oracles = {
    "historical": {"PV": hpv, "d_SCALE": hpv/2., "d_rate": -tp*hpv, "d_spot": 0., "d_vol": 0., "d_div": 0.},
    "historical_zero": {"PV": 160., "d_SCALE": 80., "d_rate": -tp*160., "d_spot": 0., "d_vol": 0., "d_div": 0.},
    "mixed": {"PV": 260., "d_SCALE": 80., "d_spot": 1.},
    "future": {"PV": fpv, "d_spot": fpv/100., "d_rate": (tf-tp)*fpv, "d_div": -tf*fpv},
}
for name, expected in oracles.items():
    for key, value in expected.items():
        tolerance = 1e-12*max(1., abs(value)) if key == "PV" else 1e-10
        if not math.isfinite(results[name][key]) or abs(results[name][key]-value) > tolerance:
            raise AssertionError((name, key, results[name][key], value, tolerance))
if args.native_diagnostics:
    native_prices = json.loads((args.native_diagnostics / "prices.json").read_text(encoding="utf-8"))
    if set(native_prices) != set(results):
        raise AssertionError("different native scenario names")
    for name, actual in native_prices.items():
        if set(actual) != set(results[name]):
            raise AssertionError((name, "different native result keys"))
        for key, value in actual.items():
            if not math.isfinite(value) or not math.isfinite(results[name][key]) or abs(value-results[name][key]) > 1e-8:
                raise AssertionError((name, key, value, results[name][key]))
if args.excel_results:
    excel = json.loads(args.excel_results.read_text(encoding="utf-8-sig"))
    if not excel["passed"]:
        raise AssertionError("Excel runner did not pass")
    for name, cell in (("historical", "G20"), ("historical_zero", "M20"), ("mixed", "A2"), ("future", "J20")):
        rows = dict(excel["outputs"][f"Values!{cell}"]["values"])
        if set(rows) != set(results[name]):
            raise AssertionError((name, "different result keys"))
        for key, value in results[name].items():
            if not math.isfinite(rows[key]) or not math.isfinite(value) or abs(rows[key]-value) > 1e-8:
                raise AssertionError((name, key, rows[key], value))
    if args.native_diagnostics:
        for address in ("A2", "D2", "G2", "J2"):
            filename = f"diagnostic-{address}.json"
            native = json.loads((args.native_diagnostics / filename).read_text(encoding="utf-8"))
            actual = json.loads((args.excel_results.parent / filename).read_text(encoding="utf-8"))
            if actual != native:
                raise AssertionError((address, "Excel JSON differs from installed public C++ JSON"))
args.output.write_text(json.dumps({"module": dal.__file__, "native_module": dal._dal.__file__, "results": results,
                                  "oracles": oracles, "passed": True,
                                  "excel_results": str(args.excel_results) if args.excel_results else None,
                                  "native_diagnostics": str(args.native_diagnostics) if args.native_diagnostics else None},
                                 indent=2) + "\n", encoding="utf-8")
print("PASS: common AAPL snapshot, independent PV/AAD oracles" + (", Excel comparison" if args.excel_results else ""))
