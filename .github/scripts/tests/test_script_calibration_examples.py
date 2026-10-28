"""Validation checks must remain active when Python assertions are disabled."""

import importlib.util
import unittest
from pathlib import Path
from types import ModuleType, SimpleNamespace
from unittest.mock import MagicMock, patch


class FakeDal(ModuleType):
    def __init__(self, case):
        super().__init__("dal")
        self.case = case
        self.yield_calls = 0
        self.xccy_calls = 0

    def __getattr__(self, name):
        return lambda *args, **kwargs: SimpleNamespace(**kwargs)

    @staticmethod
    def CollateralType_OIS():
        return "OIS"

    @staticmethod
    def PeriodLength_New(tenor):
        return tenor

    def Calibrate_CurveInstruments(self, spec, options):
        self.yield_calls += 1
        residual = {"yield_residual": 1e-8, "yield_nan": float("nan")}.get(
            self.case, 0.0
        )
        return SimpleNamespace(
            fit_within_tolerance=self.case != "yield_failed",
            parameters=[0.0] * (12 if self.case == "yield_count" else 13),
            raw_residuals=[residual] * 13,
        )

    def Calibrate_XccyInstruments(self, spec, options):
        self.xccy_calls += 1
        parameter = {"xccy_basis": 0.004, "xccy_nan": float("nan")}.get(
            self.case, 0.003
        )
        return SimpleNamespace(
            fit_within_tolerance=self.case != "xccy_failed", parameters=[parameter]
        )


class ScriptCalibrationExamplesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.examples = cls.module()

    @staticmethod
    def module():
        source = (
            Path(__file__).resolve().parents[3]
            / "scripts"
            / "gen_script_calibration_examples.py"
        )
        spec = importlib.util.spec_from_file_location("script_examples", source)
        module = importlib.util.module_from_spec(spec)
        worksheet_modules = {
            "openpyxl": MagicMock(),
            "openpyxl.styles": MagicMock(),
            "openpyxl.utils": MagicMock(),
            "openpyxl.worksheet": MagicMock(),
            "openpyxl.worksheet.formula": MagicMock(),
        }
        with patch.dict("sys.modules", worksheet_modules):
            spec.loader.exec_module(module)
        return module

    def test_valid_results_complete_all_calibrations(self):
        dal = FakeDal("valid")
        with patch.dict("sys.modules", {"dal": dal}):
            self.examples.validate_examples()
        self.assertEqual(dal.yield_calls, 3)
        self.assertEqual(dal.xccy_calls, 1)

    def test_invalid_yield_results_raise_runtime_error(self):
        for case in ("yield_failed", "yield_count", "yield_residual", "yield_nan"):
            with (
                self.subTest(case=case),
                patch.dict("sys.modules", {"dal": FakeDal(case)}),
                self.assertRaises(RuntimeError),
            ):
                self.examples.validate_examples()

    def test_invalid_xccy_results_raise_runtime_error(self):
        for case in ("xccy_failed", "xccy_basis", "xccy_nan"):
            with (
                self.subTest(case=case),
                patch.dict("sys.modules", {"dal": FakeDal(case)}),
                self.assertRaises(RuntimeError),
            ):
                self.examples.validate_examples()


if __name__ == "__main__":
    unittest.main()
