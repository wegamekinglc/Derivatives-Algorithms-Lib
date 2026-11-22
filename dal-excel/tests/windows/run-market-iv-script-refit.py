"""Capture script quote-refit rows from a recalculated Excel worksheet."""

import argparse
import datetime
import json
import math
from importlib import import_module
from pathlib import Path

import win32com.client

helpers = import_module("run-compounded-ois")
SheetInputs = import_module("run-par-rate-quote-risk").SheetInputs
FIXTURES = json.loads((Path(__file__).resolve().parents[3] / "tests/market-iv-local-vol/fixtures/script_refit.json").read_text())


def populate(sheet, fixture, iv_shift=0.):
    inputs = SheetInputs(sheet)
    inputs.row = 20
    call, table = inputs.call, inputs.table

    def column(values):
        return table([[value] for value in values])

    case = sheet.Name
    market_case, ids, steps = FIXTURES["cases"][fixture]
    data = FIXTURES[market_case]
    today = (datetime.date(2026, 11, 16) - datetime.date(1899, 12, 30)).days
    expiry = today + 365
    market = call("MARKETIVMARKET.NEW", case, today, 100, table([[expiry, data["forward"], .96]]))
    quotes = table([[name, expiry, strike, option, kind, mid + iv_shift, bid, ask, weight]
                    for name, strike, option, kind, mid, bid, ask, weight in data["quotes"]])
    fit_request = call("MARKETIVFITREQUEST.NEW", case, market, quotes)
    fit = call("MARKETIVFITRESULT.NEW", case, fit_request)
    snapshot = call("MARKETIVFITRESULT.GET.SNAPSHOT", fit)
    grid = call("MARKETIVDUPIREREQUEST.NEW", case, column(data["spots"]), column(data["times"]))
    model = call("MARKETIVLOCALVOLMODELSETTINGS.NEW", case, table([
        ["index", "EQ[A]"], ["currency", "USD"], ["factor", "W_EQ"], ["horizon", 1], ["max_step", data["max_step"]]]))
    product = call("PRODUCT.NEW", case, table([[expiry]]), table([[f"pay PAYS MAX(spot() - {data['strike']}, 0)"]]))
    valuation = call("SCRIPTVALUATIONSETTINGS.NEW", case, table([["evaluation_date", today]]))
    counts = table([["blocks", 16], ["paths_per_block", 131072 if iv_shift else 257]])
    request = call("MARKETIVSCRIPTREFITREQUEST.NEW", case, snapshot, grid, model, product, column(ids),
                   column(steps) if steps else helpers.Ref(""), valuation, helpers.Ref(""), counts)
    result = call("MARKETIVSCRIPTREFITRISKRESULT.NEW", case, request)
    sheet.Range("A1").Formula2 = f"=MARKETIVSCRIPTREFITRISKRESULT.GET.ROWS({result})"
    sheet.Range("A7").Formula2 = f"=MARKETIVSCRIPTREFITRISKRESULT.GET.EXECUTION({result})"
    if iv_shift:
        sheet.Range("D7").Formula2 = f"=MARKETIVSCRIPTREFITRISKRESULT.GET.BASEBLOCKS({result})"
    return request, result


def read_rows(sheet, anchor):
    cells = sheet.Range(anchor)
    helpers.require(cells.Value2 == "quote_index", f"{sheet.Name}!{anchor} expected row headers, got {cells.Value2}")
    headers, *rows = cells.SpillingToRange.Value2
    return [dict(zip(headers, row)) for row in rows]


def verify_row(sheet_name, row):
    if row["status"] == "SIDE_FAILED":
        helpers.require(sheet_name == "partial" and row["id"] == "atm" and row["plus_fit_status"] == "INCONSISTENT_QUOTES",
                        f"{sheet_name} unexpected fit failure")
        helpers.require(all(row[k] in (None, "") for k in ("plus_pv", "slope", "standard_error")), "Misplaced missing values")
        helpers.require(math.isfinite(row["minus_pv"]), "Missing surviving PV")
    else:
        helpers.require(math.isclose(row["slope"], (row["plus_pv"] - row["minus_pv"]) / (2 * row["step"]), abs_tol=1e-8),
                        f"{sheet_name} slope")


def verify(sheet):
    rows = read_rows(sheet, "A1")
    expected = {"all_buckets": ["left", "atm", "right"], "partial": ["atm", "right"]}.get(sheet.Name, ["atm"])
    helpers.require([r["id"] for r in rows] == expected, f"{sheet.Name} quote order")
    for row in rows:
        verify_row(sheet.Name, row)
    if sheet.Name == "partial":
        helpers.require(rows[0]["status"] == "SIDE_FAILED" and rows[1]["status"] != "SIDE_FAILED", "Wrong failed bucket")
        for cell in ("G2", "I2", "J2"):
            helpers.require(sheet.Range(cell).Text == "", f"{sheet.Name}!{cell} must display blank")
    return {"rows": rows, "execution": dict(sheet.Range("A7").SpillingToRange.Value2)}


def verify_release(excel, sheet, request, result, rows):
    request_tag = sheet.Range(request).Value2
    tag = sheet.Range(result).Value2
    sheet.UsedRange.ClearContents()
    sheet.Range("AA15").Value2 = request_tag
    erased, = excel.Run("REPOSITORY.ERASE", sheet.Range("AA15"))
    helpers.require(erased == 1, "Expected to erase the request")
    cell = sheet.Range("A15")
    cell.Formula2 = f'=MARKETIVSCRIPTREFITRISKRESULT.GET.ROWS("{tag}")'
    cell.Calculate()
    helpers.require(read_rows(sheet, "A15") == rows, "Rows changed after request release")
    return erased


def parallel_shift(book):
    sheet = book.Worksheets.Add()
    sheet.Name = "Black"
    sheet.Range("A1:C1").Value2 = (("PV plus", "PV minus", "Paired slope"),)
    for i in range(16):
        sheet.Cells(i + 2, 1).Formula2 = f"=iv_plus!D{i + 7}"
        sheet.Cells(i + 2, 2).Formula2 = f"=iv_minus!D{i + 7}"
        sheet.Cells(i + 2, 3).Formula2 = f"=(A{i + 2}-B{i + 2})/(2*0.0001)"
    sheet.Range("E1:I1").Value2 = (("PV plus", "PV minus", "Slope", "Paired SE", "Black"),)
    for col, formula in enumerate(("AVERAGE(A2:A17)", "AVERAGE(B2:B17)", "AVERAGE(C2:C17)", "STDEV.S(C2:C17)/SQRT(16)",
                                   "0.96*105*(ERF(0.2001/SQRT(8))-ERF(0.1999/SQRT(8)))/0.0002"), 5):
        sheet.Cells(2, col).Formula2 = "=" + formula
    return sheet


def populate_workbook(book):
    cases = []
    shifts = {"iv_plus": 1e-4, "iv_minus": -1e-4}
    for i, name in enumerate([*FIXTURES["cases"], *shifts]):
        sheet = book.Worksheets.Add() if i else book.Worksheets(1)
        sheet.Name = name
        cases.append((sheet, *populate(sheet, "full" if name in shifts else name, shifts.get(name, 0.))))
    return cases, parallel_shift(book)


def run(xll, output):
    excel = win32com.client.DispatchEx("Excel.Application")
    book = None
    try:
        excel.Visible = excel.DisplayAlerts = False
        helpers.require(excel.RegisterXLL(str(xll.resolve())), "RegisterXLL failed")
        book = excel.Workbooks.Add()
        excel.Calculation = -4135  # manual while constructing cells and erasing repository entries
        book.Worksheets(1).Range("A19").Formula2 = "=INIT.GLOBALDATA(1)"
        book.Worksheets(1).Range("A19").Calculate()
        cases, black = populate_workbook(book)
        excel.CalculateFullRebuild()
        evidence = {"excel_version": excel.Version, "excel_build": excel.Build, "binaries": helpers.loaded_binaries(excel, xll)}
        prices = black.Range("E2:I2").Value2[0]
        slope, se, reference = prices[2:]
        helpers.require(0 < se / 100 <= .001 and abs(slope - reference) / 100 <= .001 + 4.5 * se / 100, f"Flat IV Black check: {prices}")
        evidence["black"] = dict(zip(("plus", "minus", "slope", "standard_error", "reference"), prices))
        for sheet, _, _ in cases:
            evidence[sheet.Name] = verify(sheet)
        book.SaveAs(str(output.with_suffix(".xlsx").resolve()), FileFormat=51)
        for sheet, request, result in cases:
            case = evidence[sheet.Name]
            case["erased_requests"] = verify_release(excel, sheet, request, result, case["rows"])
        output.write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    finally:
        if book is not None:
            book.Close(SaveChanges=False)
        excel.Quit()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xll", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.xll, args.output)
