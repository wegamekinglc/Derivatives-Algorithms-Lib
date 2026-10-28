"""Check workbook inputs and reserved array outputs without executing the XLL."""

import importlib.util
import io
import re
import unittest
import zipfile
from pathlib import Path

from openpyxl.utils.cell import range_boundaries
from openpyxl.worksheet.formula import ArrayFormula


class ScriptCalibrationWorkbooksTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = (
            Path(__file__).resolve().parents[1] / "gen_script_calibration_examples.py"
        )
        spec = importlib.util.spec_from_file_location("workbook_examples", source)
        cls.examples = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.examples)

    def workbooks(self):
        return [
            self.examples.generate_yield_workbook(),
            self.examples.generate_xccy_workbook(),
        ]

    @staticmethod
    def package_parts(source):
        with zipfile.ZipFile(source) as package:
            return {
                name: package.read(name)
                for name in package.namelist()
                if name != "docProps/core.xml"
            }

    def test_committed_workbooks_match_generator(self):
        directory = Path(__file__).resolve().parents[2] / "dal-excel" / "examples"
        names = (
            "012.interpolate_curve_script.xlsx",
            "013.xccy_script_calibration.xlsx",
        )
        for name, workbook in zip(names, self.workbooks()):
            with self.subTest(workbook=name):
                generated = io.BytesIO()
                workbook.save(generated)
                self.assertEqual(
                    self.package_parts(generated), self.package_parts(directory / name)
                )

    def test_handle_cells_are_formulas(self):
        for workbook in self.workbooks():
            for sheet in workbook:
                for row in sheet:
                    for cell in row:
                        if isinstance(cell.value, str):
                            self.assertIsNone(
                                re.fullmatch(r"\$[A-Z]+\$[0-9]+", cell.value),
                                (sheet.title, cell.coordinate),
                            )

    def test_settings_ranges_exclude_column_headers(self):
        for workbook in self.workbooks():
            for sheet in workbook:
                formulas = (
                    getattr(cell.value, "text", cell.value)
                    for row in sheet
                    for cell in row
                    if cell.data_type == "f"
                )
                for formula in formulas:
                    for start, end in re.findall(
                        r"(\$[A-Z]+\$[0-9]+):(\$[A-Z]+\$[0-9]+)", formula
                    ):
                        self.assertNotEqual(
                            sheet[start].value, "field", (sheet.title, formula, end)
                        )

    def test_discount_route_has_no_projection_tenor(self):
        workbook = self.examples.generate_yield_workbook()
        for name in ("LOG_LINEAR", "LOG_CUBIC_NATURAL", "MIXED"):
            sheet = workbook[name]
            self.assertFalse(sheet["E18"].value)
            self.assertIsNone(sheet["G18"].value)

    def test_reports_have_reserved_array_ranges(self):
        count = 0
        for workbook in self.workbooks():
            for sheet in workbook:
                count += self.check_report_ranges(sheet)
        self.assertEqual(count, 8)

    def check_report_ranges(self, sheet):
        count = 0
        for row in sheet:
            for cell in row:
                if not isinstance(cell.value, ArrayFormula):
                    continue
                count += 1
                first_col, first_row, last_col, last_row = range_boundaries(
                    cell.value.ref
                )
                self.assertEqual(first_col, 1)
                self.assertIn(last_col, (9, 22))
                self.assertIn(last_row - first_row, (1, 13))
                self.assert_reserved_cells(sheet, cell, first_row, last_row, last_col)
        return count

    def assert_reserved_cells(self, sheet, anchor, first_row, last_row, last_col):
        for row in sheet.iter_rows(
            min_row=first_row, max_row=last_row, max_col=last_col
        ):
            for cell in row:
                if cell.coordinate != anchor.coordinate:
                    self.assertIsNone(
                        cell.value, (sheet.title, anchor.coordinate, cell.coordinate)
                    )


if __name__ == "__main__":
    unittest.main()
