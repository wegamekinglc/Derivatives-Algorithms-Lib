"""Calculate the two EUR swaps in desktop Excel and save the workbook and observed spills."""

import argparse
import json
from importlib import import_module
from pathlib import Path

import win32com.client


worksheet = import_module("run-par-rate-quote-risk")
Ref = worksheet.Ref
require = worksheet.require


def populate(sheet, *, deposit_start=None, discount_knots=Ref("")):
    inputs = worksheet.SheetInputs(sheet)
    call, table = inputs.call, inputs.table
    today = Ref("DATE(2024,1,15)")
    dates = (Ref("DATE(2026,1,15)"), Ref("DATE(2024,7,15)"))
    leg = call("RATELEGCONVENTION.NEW", "6M", "ACT_365F")
    discount_index = call("RATEINDEXCONVENTION.NEW", "6M", "ACT_365F", "OIS")
    index = call("RATEINDEXCONVENTION.NEW", "6M", "ACT_365F", "OIS", True)
    deposits = [call("DEPOSIT.NEW", today, deposit_start or today, date, rate, discount_index) for date, rate in zip(dates, (.035, .021))]
    swaps = [call("SWAP.NEW", today, today, date, rate, leg, index, leg) for date, rate in zip(dates, (.042, .028))]

    def column(values):
        return table([[value] for value in values])

    def stage(quotes, settings, knots=Ref("")):
        instruments = column(quotes)
        cells = sheet.Range(table([["parameterization", "ZERO_RATE"], *settings]))
        # Include the spare row so a scenario can change one setting at the same address.
        last = sheet.Cells(cells.Row + cells.Rows.Count, cells.Column + 1)
        settings_with_spare = sheet.Range(sheet.Cells(cells.Row, cells.Column), last)
        return call("CURVECALIBRATIONSTAGE.NEW", today, "EUR", instruments, knots, Ref(settings_with_spare.Address))

    discount = stage(deposits, [["curveName", "ESTR"], ["tolerance", 1e-13], ["initialGuess", .02]], discount_knots)
    forward = stage(swaps, [["curveName", "Euribor6M"], ["tolerance", 5e-13], ["initialGuess", .01],
                            ["calibrateDiscountCurve", False], ["targetTenor", "6M"]])
    stages = column([discount, forward])
    fitted = call("CALIBRATE.MULTICURVE", stages, True)
    unrecorded = call("CALIBRATE.MULTICURVE", stages)
    curves = column([call("MULTICURVECALIBRATIONRESULT.GET.CURVE", fitted, role, route)
                     for role, route in (("Discount", "OIS"), ("Forward", "6M"))])
    blocks, keys = column(["stage:0", "stage:1"]), column(["ESTR", "Euribor6M"])
    market = call("RATEPRICINGMARKET.NEW", Ref(f"{today}+TIME(10,0,0)"), "USD", keys, curves)
    stale = call("RATEPRICINGMARKET.NEW", Ref(f"{today}+TIME(10,1,0)"), "USD", keys, curves)
    source = call("STAGEDMULTICURVEQUOTERISKPROVENANCE.NEW", fitted, "EUR-calibration", blocks, keys, market)
    missing = call("STAGEDMULTICURVEQUOTERISKPROVENANCE.NEW", unrecorded, "unrecorded", blocks, keys, market)
    legacy = call("RATEQUOTERISKPROVENANCE.NEW", fitted, "legacy", blocks, keys, market)
    fixing = call("RATEFIXINGIDENTITY.NEW", "Euribor6M", 11, 0)
    trades = []
    for name, pay, notional, rate, end_month in (("payer", True, 1_000_000, .031, 8), ("receiver", False, 400_000, .048, 2)):
        header = call("RATETRADEHEADER.NEW", name, today, Ref("DATE(2024,2,15)"), Ref(f"DATE(2025,{end_month},15)"), "EUR")
        trades.append(call("RATEFIXEDFLOATTRADE.NEW", header, "IRS", notional, rate, pay, leg, leg, index, fixing, "Euribor6M", "ESTR"))
    trade_range = column(trades)
    for address, bound, provenance in (("A3", market, source), ("A9", stale, source), ("A12", market, missing), ("A15", market, legacy)):
        sheet.Range(address).Formula2 = f"=RATEPORTFOLIOQUOTERISK.SPILL({trade_range},{bound},{provenance})"
    sheet.Range("A1").Value2 = "EUR swaps · USD reporting currency"
    sheet.Range("A2:J2").Value2 = (("Calibration", "Axis", "Quote key", "Quote name", "Block", "PV CCY", "EUR / decimal quote", "EUR / bp",
                                     "Availability", "Reason"),)
    for cell, label in (("A8", "Valuation time moved to 10:01"), ("A11", "Capture omitted"), ("A14", "Legacy dispatcher")):
        sheet.Range(cell).Value2 = label
    sheet.Columns("A:J").ColumnWidth = 20
    sheet.Columns("J").ColumnWidth = 52
    sheet.Range("G3:H6").NumberFormat = "0.0000000000"


def read_spill(sheet, anchor, expected_range):
    cell = sheet.Range(anchor)
    require(cell.HasSpill, f"{anchor} {cell.Formula2} returned {cell.Text}")
    spill = cell.SpillingToRange
    require(spill.Address == expected_range, (cell.Formula2, spill.Address))
    return {"formula": cell.Formula2, "range": spill.Address, "values": spill.Value2}


def portfolio_spill(sheet):
    result = read_spill(sheet, "A3", "$A$3:$J$6")
    for i, (row, dv01) in enumerate(zip(result["values"], (-1.13929254, -.62725918, 109.06027558, -3.41379201))):
        require((row[0], row[2], row[5], row[8]) == ("EUR-calibration", f"stage:{i // 2}:{i % 2}", "EUR", "available"), row)
        require(abs(row[6] - dv01 * 1e4) < 1e-4 and abs(row[7] - dv01) < 1e-8, row)
    return result


def forward_scenario(book, title, setting, value):
    sheet = book.Worksheets.Add()
    sheet.Name = title
    populate(sheet)
    sheet.Range("Q34:R34").Value2 = ((setting, value),)
    return sheet


def partial_fit_spill(sheet, base):
    for cell in ("Q44", "Q46", "Q57", "Q61", "A3"):
        require(sheet.Range(cell).Formula2 == base.Range(cell).Formula2, cell)
    result = read_spill(sheet, "A3", "$A$3:$J$3")
    row, = result["values"]
    require(row[0] == "EUR-calibration" and row[8] == "unavailable", row)
    require(row[6] in (None, "") and row[7] in (None, ""), row)
    require("Euribor6M" in row[9] and "Forward-curve calibration requires a preloaded discount curve for the requested collateral" in row[9], row)
    result["market_formula"] = sheet.Range("Q57").Formula2
    result["forward_setting"] = sheet.Range("Q34:R34").Value2[0]
    result["routes"] = {cell: {"formula": sheet.Range(cell).Formula2, "type": sheet.Range(cell).Value2.split("~")[1]} for cell in ("Q44", "Q46")}
    require(result["routes"]["Q44"]["type"] == "DiscountCurve" and result["routes"]["Q46"]["type"] == "UnfittedCurve", result["routes"])
    return result


def conflicting_bases(sheet):
    observed = {}
    for address in ("Q40", "Q42"):
        cell = sheet.Range(address)
        require("stage:1 (Euribor6M) uses BOND" in cell.Value2 and "stage:0 (ESTR) uses ACT_365F" in cell.Value2, cell.Value2)
        observed[address] = {"formula": cell.Formula2, "value": cell.Value2}
    return observed


def deposit_knot_cases(book, excel):
    auto, explicit = book.Worksheets.Add(), book.Worksheets.Add()
    auto.Name, explicit.Name = "T+2 instrument knots", "T+2 explicit knots"
    explicit.Range("T2").Formula2 = "=DATE(2024,7,15)"
    explicit.Range("T3").Formula2 = "=DATE(2026,1,15)"
    start = Ref("DATE(2024,1,17)")
    populate(auto, deposit_start=start)
    populate(explicit, deposit_start=start, discount_knots=Ref("$T$2:$T$3"))
    excel.CalculateFullRebuild()
    default = read_spill(auto, "A3", "$A$3:$J$3")
    row, = default["values"]
    require(row[8] == "unavailable" and "square quote-by-parameter system" in row[9], row)
    supplied = read_spill(explicit, "A3", "$A$3:$J$6")
    require(all(row[5] == "EUR" and row[8] == "available" for row in supplied["values"]), supplied)
    return {"instrument_knots": default, "explicit_knots": supplied}


def run(xll, output):
    excel = win32com.client.DispatchEx("Excel.Application")
    book = None
    try:
        excel.Visible = excel.DisplayAlerts = False
        require(excel.RegisterXLL(str(xll.resolve())), "XLL registration failed")
        book = excel.Workbooks.Add()
        sheet = book.Worksheets(1)
        sheet.Name = "EUR portfolio"
        sheet.Range("M1").Formula2 = "=INIT.GLOBALDATA(1)"
        populate(sheet)
        gc_sheet = forward_scenario(book, "GC forward", "targetCollateral", "GC")
        basis_sheet = forward_scenario(book, "Basis conflict", "liborBasis", "BOND")
        knots = deposit_knot_cases(book, excel)
        evidence = {"excel_version": excel.Version, "excel_build": excel.Build,
                    "binaries": worksheet.helpers.loaded_binaries(excel, xll), "portfolio": portfolio_spill(sheet),
                    "partial_fit": partial_fit_spill(gc_sheet, sheet), "basis_conflict": conflicting_bases(basis_sheet), "T+2": knots}
        for name, index, reason in (("stale", 9, "QUOTE_RISK_CALIBRATION_STATE_MISMATCH"),
                                    ("unrecorded", 12, "bundle QUOTE_RISK_STAGED_RECORD_NOT_REQUESTED"),
                                    ("legacy", 15, "QUOTE_RISK_NOT_AVAILABLE_FOR_STAGED_CHAIN_RULE")):
            spill = read_spill(sheet, f"A{index}", f"$A${index}:$J${index}")
            row, = spill["values"]
            require(row[6] in (None, "") and row[7] in (None, "") and row[8:] == ("unavailable", reason), row)
            evidence[name] = spill
        sheet.Activate()
        book.SaveAs(str(output.with_suffix(".xlsx").resolve()), FileFormat=51)
        output.write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    finally:
        if book is not None:
            book.Close(SaveChanges=False)
        excel.Quit()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xll", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    run(args.xll, args.output)
