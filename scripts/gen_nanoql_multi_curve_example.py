#!/usr/bin/env python3
"""Generate the recalculating DAL Excel workbook for the NanoQL example."""

from pathlib import Path
from runpy import run_path

from openpyxl import Workbook
from openpyxl.styles import Font
from openpyxl.utils import get_column_letter

from gen_script_calibration_examples import write_table

ROOT = Path(__file__).resolve().parents[1]
EXAMPLE = ROOT / "dal-python/examples/021.nanoql_multi_curve.py"
OUTPUT = ROOT / "dal-excel/examples/014.nanoql_multi_curve.xlsx"
PERCENT = "0.00000%"
DATE = "yyyy-mm-dd"


def format_column(sheet, column, first, last, format_code):
    for row in range(first, last + 1):
        sheet.cell(row, column).number_format = format_code


def write_rate_conventions(sheet):
    conventions = [
        ("Overnight", '=RATEINDEXCONVENTION.NEW("1M","ACT_360","OIS",FALSE)'),
        ("Euribor 6M", '=RATEINDEXCONVENTION.NEW("6M","ACT_360","OIS",TRUE)'),
        ("OIS leg", '=RATELEGCONVENTION.NEW("12M","ACT_360","TARGET","ModifiedFollowing","Following")'),
        ("Fixed leg", '=RATELEGCONVENTION.NEW("12M","BOND","TARGET","Unadjusted","ModifiedFollowing")'),
        (
            "Floating leg",
            '=RATELEGCONVENTION.NEW("6M","ACT_360","TARGET","ModifiedFollowing","ModifiedFollowing")',
        ),
    ]
    write_table(sheet, 2, 13, ["Convention", "Handle"], conventions)


def write_calibration_quotes(sheet, anchor, quotes, projection):
    definitions = {
        "Deposit": ("DEPOSIT", "$N$4" if projection else "$N$3"),
        "FRA": ("FRA", "$N$4"),
        "OIS": ("OISSWAP", "$N$5,$N$3,$N$5"),
        "IRS": ("SWAP", "$N$6,$N$4,$N$7"),
    }

    rows = [("Curve anchor", None, None, f"={anchor}", None, None)]
    for row, (kind, label, start, end, rate) in enumerate(quotes, 10):
        function, arguments = definitions[kind]
        knot = f'=NEXT.BIZDAY("TARGET",C{row})'
        base_rate = "=Results!$B$4" if label == "IRS 5Y" else rate
        details = f"{anchor},B{row},C{row}"
        base = f"={function}.NEW({details},E{row},{arguments})"
        values = (label, start, end, knot, base_rate, base)
        if projection:
            shocked_rate = "=Results!$B$5" if label == "IRS 5Y" else f"=E{row}"
            shocked = f"={function}.NEW({details},G{row},{arguments})"
            values += (shocked_rate, shocked)
        rows.append(values)

    headers = ["Instrument", "Start", "Maturity", "Pillar", "Quote", "Instrument handle"]
    if projection:
        headers.extend(["Shocked quote", "Shocked handle"])
    write_table(sheet, 8, 1, headers, rows)
    return 9 + len(quotes)


def calibration_sheet(sheet, anchor, quotes, projection=False):
    sheet["A1"] = f"{sheet.title} calibration"
    sheet["A1"].font = Font(bold=True, size=14)
    for row, label in enumerate(("Calibration", "Curve", "Max residual (bp)"), 2):
        sheet.cell(row, 1, label)
    settings = [
        ("curveName", sheet.title),
        ("parameterization", "LOG_DISCOUNT"),
        ("logDfScheme", "LOG_CUBIC_NATURAL"),
        ("calibrateDiscountCurve", not projection),
        ("initialGuess", 0.01),
        ("tolerance", 1e-11),
        ("fitTolerance", 1e-11),
    ]
    if projection:
        settings.append(("targetTenor", "6M"))
    settings_cells = write_table(sheet, 2, 10, ["Setting", "Value"], settings, include_header=False)

    write_rate_conventions(sheet)
    last = write_calibration_quotes(sheet, anchor, quotes, projection)
    discount = ",ESTR!$B$3" if projection else ""
    for result_col, handle_col in (("B", "F"), ("E", "H")) if projection else (("B", "F"),):
        sheet[f"{result_col}2"] = (
            f'=CALIBRATE.SINGLECURVE({anchor},"EUR",{handle_col}10:{handle_col}{last},D9:D{last},{settings_cells}{discount})'
        )
        sheet[f"{result_col}3"] = f"=CALIBRATIONRESULT.GET.CURVE({result_col}2)"
        sheet[f"{result_col}4"] = f'=1E4*CALIBRATIONRESULT.GET({result_col}2,"maxAbsResidual")'
        sheet[f"{result_col}4"].number_format = "0.00E+00"
    if projection:
        sheet["D2"] = "Shocked calibration"
        sheet["D3"] = "Shocked curve"
        sheet["D4"] = "Max residual (bp)"
        format_column(sheet, 7, 10, last, PERCENT)
    format_column(sheet, 4, 9, last, DATE)
    format_column(sheet, 5, 10, last, PERCENT)
    sheet.freeze_panes = "B10"


def cashflows(sheet, first_row, start, period_data, add_months):
    fixed_dates, float_periods = period_data
    rows = []
    for index, pay in enumerate(fixed_dates, 1):
        row = first_row + len(rows) + 1
        first, last = add_months(start, 12 * (index - 1)), add_months(start, 12 * index)
        rows.append(
            (
                "Fixed",
                first,
                last,
                pay,
                f"=DAYS360(B{row},C{row},TRUE)/360",
                f"=DISCOUNTCURVE.GET.FACTOR(ESTR!$B$3,Results!$B$3,D{row})",
                f"=E{row}*F{row}",
            )
        )
    float_first = first_row + len(rows) + 1
    for first, last in float_periods:
        row = first_row + len(rows) + 1
        rows.append(
            (
                "Floating",
                first,
                last,
                last,
                f"=(C{row}-B{row})/360",
                f"=DISCOUNTCURVE.GET.FACTOR(ESTR!$B$3,Results!$B$3,D{row})",
                f"=E{row}*F{row}",
                f"=DISCOUNTCURVE.GET.FACTOR('Euribor 6M'!$B$3,B{row},C{row})",
                f"=F{row}*(1/H{row}-1)",
                f"=DISCOUNTCURVE.GET.FACTOR('Euribor 6M'!$E$3,B{row},C{row})",
                f"=F{row}*(1/J{row}-1)",
            )
        )
    headers = [
        "Leg",
        "Accrual start",
        "Accrual end",
        "Pay date",
        "Year fraction",
        "€STR DF",
        "PV annuity",
        "Base forward DF",
        "Base floating PV",
        "Shocked forward DF",
        "Shocked floating PV",
    ]
    write_table(sheet, first_row, 1, headers, rows)
    end = first_row + len(rows)
    return (
        f"SUM(Cashflows!G{first_row + 1}:G{float_first - 1})",
        f"SUM(Cashflows!G{float_first}:G{end})",
        f"SUM(Cashflows!I{float_first}:I{end})",
        f"SUM(Cashflows!K{float_first}:K{end})",
    )


def main():
    example = run_path(str(EXAMPLE))
    calendar = example["dal"].Holidays_("TARGET")
    today, add_months = example["TODAY"], example["add_months"]
    spot = example["advance_business_days"](today, 2, calendar)
    forward = example["adjust"](add_months(spot, 12), calendar)
    wb = Workbook()
    results = wb.active
    results.title = "Results"
    results["A1"] = "€STR discounting and Euribor 6M forecasting"
    results["A1"].font = Font(bold=True, size=14)
    results["A2"] = "Load the DAL Excel add-in and recalculate. Edit either 5Y quote to reprice."
    for row, (label, value, fmt) in enumerate(
        (
            ("Valuation date", today, DATE),
            ("Base 5Y quote", example["BASE_QUOTE"], PERCENT),
            ("Shocked 5Y quote", example["SHOCKED_QUOTE"], PERCENT),
            ("Notional (EUR)", example["NOTIONAL"], "#,##0"),
            ("Fixed coupon", example["FIXED_RATE"], PERCENT),
            ("Settlement date", spot, DATE),
        ),
        3,
    ):
        results.cell(row, 1, label)
        results.cell(row, 2, value).number_format = fmt
    calibration_sheet(wb.create_sheet("ESTR"), "Results!$B$3", example["estr_quotes"](spot, calendar))
    calibration_sheet(
        wb.create_sheet("Euribor 6M"),
        "Results!$B$8",
        example["euribor_quotes"](spot, calendar, example["BASE_QUOTE"]),
        projection=True,
    )

    flow_sheet = wb.create_sheet("Cashflows")
    flow_sheet["A1"], flow_sheet["A21"] = "5Y spot", "1Y forward 5Y"
    spot_flows = cashflows(flow_sheet, 2, spot, example["swap_periods"](spot, calendar), add_months)
    forward_flows = cashflows(flow_sheet, 22, forward, example["swap_periods"](forward, calendar), add_months)
    cases = (
        ("Base 5Y spot", "$B$4", spot_flows, 2, 3076.04),
        ("Base 1Y forward 5Y", "$B$4", forward_flows, 2, 19202.54),
        ("Shocked 5Y spot", "$B$5", spot_flows, 3, 9922.71),
        ("Shocked 1Y forward 5Y", "$B$5", forward_flows, 3, 19202.54),
    )
    rows = []
    for row, (label, quote_cell, summary, float_index, reference) in enumerate(cases, 11):
        fixed_annuity, float_annuity = summary[:2]
        floating_pv = summary[float_index]
        rows.append(
            (
                label,
                f"={quote_cell}",
                f"=$B$6*({floating_pv}-$B$7*{fixed_annuity})",
                f"=-C{row}/($B$6*{float_annuity})",
                f"={floating_pv}/{fixed_annuity}",
                reference,
            )
        )
    write_table(
        results,
        10,
        1,
        ["Trade", "5Y quote", "DAL NPV (EUR)", "Fair spread", "Fair fixed rate", "NanoQL NPV (EUR)"],
        rows,
    )
    for column in (2, 4, 5):
        format_column(results, column, 11, 14, PERCENT)
    for column in (3, 6):
        format_column(results, column, 11, 14, "#,##0.00")
    results["A16"] = "Published reference values use the original 0.762% and 0.900% 5Y quotes."
    for sheet in wb:
        for column in range(1, sheet.max_column + 1):
            sheet.column_dimensions[get_column_letter(column)].width = 30 if column == 1 else 22
    wb.calculation.fullCalcOnLoad = True
    wb.save(OUTPUT)


if __name__ == "__main__":
    main()
