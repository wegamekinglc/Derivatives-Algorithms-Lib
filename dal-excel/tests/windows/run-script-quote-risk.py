"""Recalculate a two-quote risk workbook using a native Windows XLL."""

import argparse
import hashlib
import json
from pathlib import Path


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def populate(sheet, fixture):
    formulas = {
        "A1": "=INIT.GLOBALDATA(1)",
        "B1": "=DATE(2026,10,21)", "B2": "=DATE(2027,10,21)", "B3": "=DATE(2028,10,23)",
        "B5": "=CALIBRATIONINSTRUMENT.LOADJSON(A5)", "B6": "=CALIBRATIONINSTRUMENT.LOADJSON(A6)",
        "D5": '=CALIBRATIONMARKETQUOTE.NEW("first",C5,"Rate",)',
        "D6": '=CALIBRATIONMARKETQUOTE.NEW("last",C6,"Rate",)',
        "E5": '=CALIBRATIONOBSERVATION.NEW("first",B5,"ParRate",D5,1E-12)',
        "E6": '=CALIBRATIONOBSERVATION.NEW("last",B6,"ParRate",D6,1E-12)',
        "G1": "=B1", "G2": "=B2", "G3": "=B3",
        "I1": "=CALIBRATIONDATES.NEW(G1:G3)",
        "I2": "=CURVEINSTRUMENTGEOMETRY.NEW(L1:M4)",
        "I3": "=CURVESCRIPTROUTES.NEW(L7:O8)",
        "I4": "=CURVEINSTRUMENTMARKET.NEW(L10:M10)",
        "I5": "=MARKETFIXINGSNAPSHOT.NEW(,,)",
        "I6": '=SCRIPTVALUATIONSETTINGS.NEW("risk-valuation",L12:M12,I5)',
        "I7": "=CALIBRATIONOBSERVATIONS.NEW(E5:E6)",
        "I8": "=CURVEINSTRUMENTCALIBRATIONSPEC.NEW(L15:M18)",
        "I9": "=CALIBRATE.CURVEINSTRUMENTS(I8,)",
        "I10": "=CALIBRATIONNUMBERS.NEW(G5:G6)",
        "I11": "=SCRIPTCURVEQUOTERISKACCURACY.NEW(L21:M23)",
        "I12": "=SCRIPTCURVEQUOTERISKSNAPSHOT.NEW(I9,I11)",
        "I13": "=SCRIPTCURVEQUOTEIDS.NEW(G8:G9)",
        "I14": "=SCRIPTCURVEQUOTERISKREQUEST.NEW(L26:M26)",
        "I15": "=CALIBRATIONINSTRUMENT.VALUEWITHQUOTERISK(B5,I12,I8,I14)",
        "I16": "=SCRIPTCURVEQUOTERISKRESULT.GET.STATUS(I15)",
        "I17": "=SCRIPTCURVEQUOTERISKRESULT.GET.PV(I15)",
        "B12": "=SCRIPTCURVEQUOTERISKRESULT.GET.QUOTEAXIS(I15)",
        "B20": "=SCRIPTCURVEQUOTERISKRESULT.GET.SENSITIVITY(I15)",
        "I20": "=SCRIPTCURVEQUOTERISKREQUEST.NEW(L29:M29)",
        "I21": "=CALIBRATIONINSTRUMENT.VALUEWITHQUOTERISK(B5,I12,I8,I20)",
        "I22": "=SCRIPTCURVEQUOTERISKRESULT.GET.STATUS(I21)",
        "I23": "=SCRIPTCURVEQUOTERISKRESULT.GET.PV(I21)",
        "B40": "=SCRIPTCURVEQUOTERISKRESULT.GET.DIAGNOSTICS(I21)",
        "B31": "=SCRIPTCURVEQUOTERISKRESULT.GET.SENSITIVITY(I21)",
    }
    values = {
        "A5": fixture[0], "A6": fixture[1], "C5": .035, "C6": .04,
        "G5": 1e-12, "G6": 1e-12, "G8": "last", "G9": "first",
        "L1": "today", "L2": "currency", "M2": "USD", "L3": "knot_dates",
        "L4": "parameterization", "M4": "LOG_DISCOUNT",
        "L7": "id", "M7": "projection", "N7": "collateral", "O7": "tenor",
        "L8": "USD.OIS", "M8": False, "N8": "OIS", "L10": "routes",
        "L12": "evaluation_date", "L15": "geometry", "L16": "market", "L17": "valuation",
        "L18": "observations", "L21": "residual_absolute_limits", "L22": "min_reciprocal_condition",
        "M22": 1e-10, "L23": "response_backward_error_limit", "M23": 1e-12,
        "L26": "quote_ids", "L29": "live_quote_payoff", "M29": True,
    }
    formulas.update({
        "M1": "=B1", "M3": "=I1", "M10": "=I3", "M12": "=B1",
        "M15": "=I2", "M16": "=I4", "M17": "=I6", "M18": "=I7",
        "M21": "=I10", "M26": "=I13",
    })
    for address, value in values.items():
        sheet.Range(address).Value2 = value
    for address, formula in formulas.items():
        sheet.Range(address).Formula2 = formula


def read(sheet):
    require(sheet.Range("I16").Value2 == "Available", "PV risk is not Available")
    axis = sheet.Range("B12:G14").Value2
    require(axis[1][0:2] == (1., "last") and axis[2][0:2] == (0., "first"), "Quote order or ordinals changed")
    require(axis[2][3:5] == ("Rate", "DecimalRate"), "Decimal Rate metadata changed")
    values = sheet.Range("B20:G22").Value2
    require(values[2][3] == "USD", "PV currency changed")
    require(abs(values[2][5] - values[2][4] * 1e-4) < 1e-14, "DV01 scaling changed")
    require(sheet.Range("I22").Value2 == "Unavailable", "Live quote payoff was admitted")
    diagnostics = sheet.Range("B40:O41").Value2
    require(diagnostics[1][0] == "ScriptCurveQuoteRiskLiveQuoteUnsupported", "Failure diagnostic missing")
    require(sheet.Range("F32").Value2 in (None, "") and sheet.Range("G32").Value2 in (None, ""), "Unavailable metrics are not blank")
    return {"axis": axis, "sensitivities": values, "pv": sheet.Range("I17").Value2,
            "diagnostics": diagnostics, "guarded_pv_error": sheet.Range("I23").Value2}


def run(args):
    import win32com.client

    fixture = json.loads(args.fixture.read_text(encoding="utf-8"))
    excel = win32com.client.DispatchEx("Excel.Application")
    excel.Visible = False
    excel.DisplayAlerts = False
    book = None
    try:
        require(excel.RegisterXLL(str(args.xll.resolve())), "Native XLL registration failed")
        book = excel.Workbooks.Add()
        sheet = book.Worksheets(1)
        sheet.Name = "ScriptQuoteRisk"
        populate(sheet, fixture)
        excel.CalculateFullRebuild()
        before = read(sheet)
        accrual = 365. / 360.
        denominator = 1. + .035 * accrual
        require(abs(before["pv"] - (-100. + 100. * (1. + .025 * accrual) / denominator)) < 1e-9, "PV differs from independent oracle")
        sheet.Range("C5").Value2 = .0351
        excel.CalculateFullRebuild()
        after = read(sheet)
        response = before["sensitivities"][2][5] * denominator / (denominator + .0001 * accrual)
        require(abs(after["pv"] - before["pv"] - response) < 1e-9, "+1bp recalibration response differs from fixed-coupon oracle")
        require(abs(after["axis"][2][2] - .0351) < 1e-14, "Recalculation did not refresh the quote axis")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        workbook = args.output.with_suffix(".xlsx")
        book.SaveAs(str(workbook.resolve()), FileFormat=51)
        evidence = {"excel_version": excel.Version, "xll_sha256": hashlib.sha256(args.xll.read_bytes()).hexdigest(),
                    "fixture_sha256": hashlib.sha256(args.fixture.read_bytes()).hexdigest(),
                    "before": before, "after_plus_1bp": after, "workbook": workbook.name}
        args.output.write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    finally:
        if book is not None:
            book.Close(SaveChanges=False)
        excel.Quit()
    print("Native Excel quote axis, fixed-coupon +1bp response, diagnostics and blanks passed.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xll", required=True, type=Path)
    parser.add_argument("--fixture", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    run(parser.parse_args())
