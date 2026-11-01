import argparse
import importlib.util
from pathlib import Path
import sys

from openpyxl import Workbook
from openpyxl.styles import Alignment, Font, PatternFill
from openpyxl.workbook.properties import CalcProperties
from openpyxl.worksheet.formula import ArrayFormula


ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("autocallable_examples", ROOT / "dal-python/examples/autocallables/common.py")
examples = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = examples
SPEC.loader.exec_module(examples)


def add_notes(book, case):
    notes = book.create_sheet("Read me")
    for row in (
        (case.title,),
        ("Load the DAL XLL, then recalculate all formulas (Ctrl+Alt+F9).",),
        ("Inputs!B2 initializes DAL; Pricing uses an explicit valuation date.",),
        ("Model inputs and path count can be edited on Inputs. Contract terms are literal scripts on Events.",),
        ("One-year note, monthly observations on the 2nd, ACT/365F, notional 1, including redemption; initial outlay excluded.",),
        ("KO: performance >= barrier. KI: performance < 65%. Phoenix coupon: performance >= 80%.",),
        ("Snowball KI is monitored monthly; Phoenix KI is tested only at maturity. These are discrete illustrative contracts.",),
        ("Phoenix pays the qualifying coupon before same-date redemption. Memory arrears are forfeited if unpaid at maturity.",),
        ("Single-asset SPOT is normalized to initial reference 1; worst-of uses MIN(A/100, B/120).",),
        ("AAD uses barrier smoothing 0.001 and can differ from the hard-barrier double price.",),
        ("Risks hold contract terms fixed. Spot derivatives use the model's spot units; rate/vol/div inputs are decimal units.",),
        ("Capital protection covers the scripted redemption only. No issuer default or funding spread is modeled.",),
    ):
        notes.append(row)
    notes.column_dimensions["A"].width = 115


def add_inputs(book, case):
    inputs = book.create_sheet("Inputs")
    for row in (
        ("Model / simulation input", "Value"),
        ("Initialize DAL", "=INIT.GLOBALDATA(1)"),
        ("Evaluation date", "=DATE(2026,1,2)"),
        ("Sobol paths", 16384),
        ("Spot A (normalized for single asset)", 100.0 if case.worst_of else 1.0),
        ("Vol A (decimal)", examples.VOL),
        ("Rate (decimal)", examples.RATE),
        ("Dividend A (decimal)", 0.0),
        ("Spot B", 120.0),
        ("Vol B (decimal)", 0.25),
        ("Dividend B (decimal)", 0.0),
        ("Correlation", 0.4),
    ):
        inputs.append(row)
    inputs["B3"].number_format = "yyyy-mm-dd"
    inputs["E1"], inputs["F1"] = "Valuation setting", "Value"
    inputs["E2"], inputs["F2"] = "evaluation_date", "=$B$3"
    inputs["E3"], inputs["F3"] = "today_fixing", "Model"
    for key_column, value_column, aad in (("E", "F", False), ("H", "I", True)):
        inputs[f"{key_column}5"] = "AAD simulation" if aad else "Plain simulation"
        for row, (key, value) in enumerate(
            (("method", "sobol"), ("use_bb", False), ("enable_aad", aad), ("smooth", examples.SMOOTH), ("compiled", True)), start=6
        ):
            inputs[f"{key_column}{row}"] = key
            inputs[f"{value_column}{row}"] = value
    if case.worst_of:
        add_correlated_inputs(inputs)
    for column in "EFGHIJKL":
        inputs.column_dimensions[column].width = 23


def add_correlated_inputs(inputs):
    for cell, value in {
        "E14": "Index", "F14": "Spot", "G14": "Vol", "H14": "Div",
        "E15": "EQ[A]", "F15": "=$B$5", "G15": "=$B$6", "H15": "=$B$8",
        "E16": "EQ[B]", "F16": "=$B$9", "G16": "=$B$10", "H16": "=$B$11",
        "K13": "Correlation", "K14": 1.0, "L14": "=$B$12", "K15": "=$B$12", "L15": 1.0,
    }.items():
        inputs[cell] = value


def add_events(book, case):
    events = book.create_sheet("Events")
    events.append(("Event date", "Script (editable contract terms)"))
    dates, scripts = examples.build_events(case)
    for day, script in zip(dates, scripts):
        events.append((day, script))
        events.cell(events.max_row, 1).number_format = "yyyy-mm-dd"
        events.cell(events.max_row, 2).alignment = Alignment(wrap_text=True, vertical="top")
        events.row_dimensions[events.max_row].height = 16 * (script.count("\n") + 1)
    events.column_dimensions["B"].width = 125
    return len(dates)


def add_pricing(book, case, event_count):
    pricing = book.create_sheet("Pricing")
    pricing.append((case.title,))
    model = (
        'CORRELATEDBSMODELDATA.NEW("model",Inputs!E15:E16,Inputs!F15:F16,Inputs!G15:G16,Inputs!H15:H16,Inputs!B7,Inputs!K14:L15)'
        if case.worst_of else 'BSMODELDATA.NEW("model",Inputs!B5,Inputs!B6,Inputs!B7,Inputs!B8)'
    )
    for row, (label, formula) in enumerate((
        ("Product", f'PRODUCT.NEW("{case.slug}",Events!A2:A{event_count + 1},Events!B2:B{event_count + 1})'),
        ("Model", model),
        ("Valuation", 'SCRIPTVALUATIONSETTINGS.NEW("valuation",Inputs!E2:F3)'),
        ("Plain settings", 'MONTECARLOSETTINGS.NEW("plain",Inputs!E6:F10)'),
        ("AAD settings", 'MONTECARLOSETTINGS.NEW("aad",Inputs!H6:I10)'),
    ), start=2):
        pricing.cell(row, 1, label)
        pricing.cell(row, 2, f'=IF(Inputs!$B$2,{formula},"")')
    pricing["A8"] = "MC double"
    pricing["A9"], pricing["B9"] = "Output key", "Value (notional 1)"
    pricing["A10"] = ArrayFormula(ref="A10:B10", text="=MONTECARLO.VALUEWITHSETTINGS(B2,B3,Inputs!B4,B4,B5)")
    pricing["A13"] = "MC AAD (smoothed barriers)"
    pricing["A14"], pricing["B14"] = "Output key / input derivative", "Value"
    last_row = 22 if case.worst_of else 19
    pricing["A15"] = ArrayFormula(ref=f"A15:B{last_row}", text="=MONTECARLO.VALUEWITHSETTINGS(B2,B3,Inputs!B4,B4,B6)")
    for row in range(15, last_row + 1):
        pricing.cell(row, 2).number_format = "0.000000"
    pricing["B10"].number_format = "0.000000"


def format_sheets(book):
    for sheet in book:
        sheet.freeze_panes = "B2"
        if sheet.title != "Read me":
            sheet.column_dimensions["A"].width = 40
            if sheet.title != "Events":
                sheet.column_dimensions["B"].width = 30
        for cell in sheet[1]:
            cell.font = Font(color="FFFFFF", bold=True)
            cell.fill = PatternFill("solid", fgColor="203864")


def workbook(case):
    book = Workbook()
    book.remove(book.active)
    book.calculation = CalcProperties(calcMode="auto", fullCalcOnLoad=True)
    add_notes(book, case)
    add_inputs(book, case)
    event_count = add_events(book, case)
    add_pricing(book, case, event_count)
    format_sheets(book)
    return book


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, default=Path(__file__).resolve().parent)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for number, case in enumerate(examples.CASES.values(), start=1):
        path = args.output_dir / f"{number:03d}.{case.slug}.xlsx"
        workbook(case).save(path)
        print(path)


if __name__ == "__main__":
    main()
