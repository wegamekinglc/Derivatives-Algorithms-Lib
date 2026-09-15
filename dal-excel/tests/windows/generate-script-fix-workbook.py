"""Generate the Excel FIX fixture; the Windows runner recalculates with its XLL."""

import argparse
import json
from pathlib import Path

import openpyxl
from openpyxl.utils.datetime import CALENDAR_WINDOWS_1900

parser = argparse.ArgumentParser()
parser.add_argument("--manifest", type=Path, default=Path(__file__).resolve().parents[2] / "examples/010.script_fix_settings.json")
parser.add_argument("--output", type=Path)
args = parser.parse_args()
manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
book = openpyxl.Workbook()
book.remove(book.active)
book.epoch = CALENDAR_WINDOWS_1900
book.calculation = openpyxl.workbook.properties.CalcProperties(calcMode="manual")
for name, cells in manifest["sheets"].items():
    sheet = book.create_sheet(name)
    for address, value in cells.items():
        sheet[address] = value
    sheet.freeze_panes = "B2"
    for column in "ABCDEFGHIJKLM":
        sheet.column_dimensions[column].width = 24
output = args.output or args.manifest.with_suffix(".xlsx")
book.save(output)
print(output)
