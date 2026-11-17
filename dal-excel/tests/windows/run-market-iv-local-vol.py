"""Execute the market F/D worksheet recipe in a private Excel instance."""

import argparse
import datetime
import importlib.util
import json
import math
from pathlib import Path


spec = importlib.util.spec_from_file_location("worksheet_helpers", Path(__file__).with_name("run-compounded-ois.py"))
helpers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helpers)


def block(sheet, address, rows, columns):
    first = int(address[1:])
    return sheet.sheet.Range(f"J{first}:{chr(73 + columns)}{first + rows - 1}").Value2


def verify(sheet):
    call = lambda name, *args: sheet.call(name, *args)[0]
    today = (datetime.date(2026, 11, 16) - datetime.date(1899, 12, 30)).days
    expiry = today + 365
    call("INIT.GLOBALDATA", 1)
    market = call("MARKETIVMARKET.NEW", "market", today, 100., sheet.table([
        [today + 91, 101., .995], [today + 182, 99., 1.005], [expiry, 102., .98]]))
    quotes = sheet.table([[f"atm{i}", today + days, forward, "CALL", "DECIMAL_IV", .2, None, None, None]
                          for i, (days, forward) in enumerate([(91, 101.), (182, 99.), (365, 102.)])])
    request = call("MARKETIVFITREQUEST.NEW", "request", market, quotes)
    fit = call("MARKETIVFITRESULT.NEW", "fit", request)
    snapshot = call("MARKETIVFITRESULT.GET.SNAPSHOT", fit)
    grid_request = call("MARKETIVDUPIREREQUEST.NEW", "grid-request",
                        sheet.table([[80.], [100.], [120.]]), sheet.table([[.1], [.5], [1.]]))
    grid = call("MARKETIVDUPIRERESULT.NEW", "grid", snapshot, grid_request)
    rows = [["index", "EQ[A]"], ["currency", "USD"], ["factor", "W_EQ"], ["horizon", 1.25]]
    settings, settings_address = sheet.call("MARKETIVLOCALVOLMODELSETTINGS.NEW", "settings", sheet.table(rows))
    model = call("MARKETIVLOCALVOLMODELDATA.NEW", "market", grid, settings)
    _, address = sheet.call("MARKETIVLOCALVOLMODELDATA.GET.SUMMARY", model)
    summary = dict(block(sheet, address, 23, 2))
    helpers.require(summary["schema"] == "dal.market-iv-local-vol-model-summary/1" and summary["evaluation_date"] == today, "Summary schema/date")
    _, address = sheet.call("MARKETIVLOCALVOLMODELDATA.GET.CARRY", model)
    carry = block(sheet, address, 6, 8)
    helpers.require(carry[-1][1] in (None, ""), "Fractional-day horizon must have blank date")
    valuation = call("SCRIPTVALUATIONSETTINGS.NEW", "valuation", sheet.table([["evaluation_date", today]]))
    simulation = call("MONTECARLOSETTINGS.NEW", "simulation", sheet.table([["method", "mrg32"]]))
    values = {}
    for name, event, expected, tolerance in [
        ("df", "pay PAYS 1", .98, 2e-12),
        ("forward", "pay PAYS FIX(EQ[A])", 102. * .98, .8),
        ("call", "pay PAYS MAX(FIX(EQ[A]) - 102, 0)", 102. * .98 * math.erf(.1 / math.sqrt(2)), .5),
    ]:
        product = call("PRODUCT.NEW", name, sheet.table([[expiry]]), sheet.table([[event]]))
        _, address = sheet.call("MONTECARLO.VALUEWITHSETTINGS", product, model, 16384, valuation, simulation)
        pv = dict(block(sheet, address, 1, 2))["PV"]
        helpers.require(math.isfinite(pv) and abs(pv - expected) < tolerance, f"{name}: {pv} versus {expected}")
        values[name] = {"pv": pv, "reference": expected, "tolerance": tolerance}
    _, address = sheet.call("MARKETIVLOCALVOLMODELDATA.GET.EXECUTION", product, model, valuation, simulation)
    execution = dict(block(sheet, address, 23, 2))
    helpers.require(execution["per_path_market_queries"] == 0 and execution["prepared_columns"] == execution["internal_steps"], "Execution counters")
    aad = call("MONTECARLOSETTINGS.NEW", "aad", sheet.table([["enable_aad", True]]))
    error = sheet.call("MONTECARLO.VALUEWITHSETTINGS", product, model, 16, valuation, aad, error=True)[0]
    helpers.require("DerivativeUnavailable" in str(error), "Expected passive-only rejection")
    invalid = sheet.table(rows + [["horizon", 1.25]])
    sheet.sheet.Range(settings_address).Formula2 = f'=MARKETIVLOCALVOLMODELSETTINGS.NEW("invalid",{invalid})'
    sheet.excel.CalculateFullRebuild()
    helpers.require("duplicate" in str(sheet.sheet.Range(settings_address).Value2), "Expected duplicate-key rejection")
    _, address = sheet.call("MARKETIVLOCALVOLMODELSETTINGS.GET.CONFIGURATION", settings)
    helpers.require(dict(block(sheet, address, 10, 2))["horizon"] == 1.25, "Failed overwrite must preserve handle")
    return {"summary": summary, "carry": carry, "execution": execution, "values": values, "passive_error": str(error)}


def run(args):
    import win32com.client
    excel = win32com.client.DispatchEx("Excel.Application")
    book = None
    result = {"status": "FAIL", "excel_version": excel.Version, "excel_build": excel.Build}
    try:
        excel.Visible, excel.DisplayAlerts = False, False
        helpers.require(excel.RegisterXLL(str(args.xll.resolve())), "RegisterXLL failed")
        result["loaded_binaries"] = helpers.loaded_binaries(excel, args.xll)
        result["runner_sha256"] = helpers.sha256(__file__)
        book = excel.Workbooks.Add()
        result.update(verify(helpers.Sheet(excel, book.Worksheets(1))))
        result["status"] = "PASS"
        print(json.dumps(result, indent=2))
    finally:
        try:
            args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
        finally:
            try:
                if book is not None:
                    book.Close(SaveChanges=False)
            finally:
                excel.Quit()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xll", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
