"""Recalculate the committed script calibration workbooks with a Windows XLL."""

import argparse
import hashlib
import json
import math
import re
from pathlib import Path

from openpyxl import load_workbook
from openpyxl.worksheet.formula import ArrayFormula

ROOT = Path(__file__).resolve().parents[3]
EXAMPLES = ["012.interpolate_curve_script.xlsx", "013.xccy_script_calibration.xlsx"]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def arrays(path):
    workbook = load_workbook(path)
    result = []
    for sheet in workbook:
        for row in sheet:
            for cell in row:
                if isinstance(cell.value, ArrayFormula):
                    result.append(
                        (sheet.title, cell.value.ref, "OBSERVATIONS" in cell.value.text)
                    )
    return result


def observations(values):
    require(values[0][13] == "raw_residual", "Unexpected observation report header")
    for row in values[1:]:
        require(isinstance(row[13], (int, float)), "Missing numeric residual")
        require(
            math.isfinite(row[13]) and abs(row[13]) <= row[11],
            "Observation failed its residual tolerance",
        )
        require(row[15] is True, "Observation fit flag is false")


def parameters(values, xccy):
    require(values[0][4] == "value", "Unexpected parameter report header")
    require(
        all(
            isinstance(row[4], (int, float)) and math.isfinite(row[4])
            for row in values[1:]
        ),
        "Invalid fitted parameter",
    )
    if xccy:
        require(
            abs(values[1][4] - 0.003) < 1e-10,
            "XCCY basis does not match the native fixture",
        )


def read_reports(book, ranges, check_basis=True):
    results = []
    for name, area, is_observation in ranges:
        sheet = book.Worksheets(name)
        status = sheet.Range("F20" if name == "Calibration" else "B15").Value2
        require(status == "Succeeded", f"{name} calibration status: {status}")
        values = sheet.Range(area).Value2
        require(
            len(values) == (2 if name == "Calibration" else 14),
            "Incorrect report row count",
        )
        if is_observation:
            observations(values)
        else:
            parameters(values, check_basis and name == "Calibration")
        results.append(
            {
                "sheet": name,
                "range": area,
                "observations": is_observation,
                "values": values,
            }
        )
    return results


def bumped_formula(formula):
    return re.sub(
        r'^(=[^(]+\("[^"]+",)([^,]+)',
        lambda match: match[1] + "(" + match[2] + "+0.0001)",
        formula,
    )


def check_quote_update(before, after):
    for original, bumped in zip(before, after):
        old, new = original["values"][1], bumped["values"][1]
        if original["observations"]:
            require(
                abs(new[10] - old[10] - 0.0001) < 1e-12, "Market quote did not update"
            )
        else:
            require(new[4] != old[4], "Quote change left a fitted parameter unchanged")


def verify_workbook(excel, path):
    ranges = arrays(path)
    require(
        len(ranges) == (6 if path.name.startswith("012") else 2),
        "Missing array report ranges",
    )
    book = excel.Workbooks.Open(str(path), UpdateLinks=0, ReadOnly=True)
    try:
        excel.CalculateFullRebuild()
        before = read_reports(book, ranges)
        source = book.Worksheets(
            "Instruments" if path.name.startswith("012") else "Script Instrument"
        )
        quote = source.Range("AA23" if path.name.startswith("012") else "AA4")
        original = quote.Formula2
        quote.Formula2 = bumped_formula(original)
        excel.CalculateFullRebuild()
        after = read_reports(book, ranges, check_basis=False)
        check_quote_update(before, after)
        quote.Formula2 = original
        excel.CalculateFullRebuild()
        read_reports(book, ranges)
        return {"workbook": path.name, "reports": before, "quote_bump_reports": after}
    finally:
        book.Close(SaveChanges=False)


def run(xll, output):
    import win32com.client

    paths = [ROOT / "dal-excel" / "examples" / name for name in EXAMPLES]
    hashes = [hashlib.sha256(path.read_bytes()).hexdigest() for path in paths]
    excel = win32com.client.DispatchEx("Excel.Application")
    excel.Visible = False
    excel.DisplayAlerts = False
    try:
        require(excel.RegisterXLL(str(xll.resolve())), "XLL registration failed")
        init = excel.Workbooks.Add()
        init.Worksheets(1).Range("A1").Formula2 = "=INIT.GLOBALDATA(1)"
        init.Worksheets(1).Range("A1").Calculate()
        require(
            init.Worksheets(1).Range("A1").Value2 is True, "DAL initialization failed"
        )
        results = [verify_workbook(excel, path) for path in paths]
        init.Close(SaveChanges=False)
    finally:
        excel.Quit()
    require(
        hashes == [hashlib.sha256(path.read_bytes()).hexdigest() for path in paths],
        "A source workbook changed",
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(
            {
                "xll_sha256": hashlib.sha256(xll.read_bytes()).hexdigest(),
                "results": results,
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    print("Native Excel recalculation and quote updates passed for both workbooks.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xll", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    run(args.xll, args.output)


if __name__ == "__main__":
    main()
