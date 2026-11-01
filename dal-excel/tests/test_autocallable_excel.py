import gc
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import sys

import pytest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("autocallable_excel_examples", ROOT / "dal-python/examples/autocallables/common.py")
examples = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = examples
SPEC.loader.exec_module(examples)

pytestmark = pytest.mark.skipif(
    sys.platform != "win32" or not os.environ.get("DAL_EXCEL_XLL"),
    reason="requires Windows Excel and DAL_EXCEL_XLL pointing to a matching built XLL",
)


@pytest.fixture(scope="module")
def excel():
    import win32com.client

    app = win32com.client.DispatchEx("Excel.Application")
    app.Visible = False
    app.DisplayAlerts = False
    app.AskToUpdateLinks = False
    try:
        assert app.RegisterXLL(str(Path(os.environ["DAL_EXCEL_XLL"]).resolve()))
        yield app
    finally:
        gc.collect()
        app.Quit()


def read_result(sheet, address):
    rows = sheet.Range(address).Value2
    assert all(isinstance(key, str) and key for key, _ in rows), rows
    assert all(isinstance(value, (int, float)) and math.isfinite(value) and value > -1e9 for _, value in rows), rows
    result = dict(rows)
    assert len(result) == len(rows), rows
    return result


def assert_formula_results(book):
    for sheet in book.Worksheets:
        address = sheet.UsedRange.Address
        assert sheet.Evaluate(f"SUMPRODUCT(--ISERROR({address}))") == 0, sheet.Name


@pytest.mark.parametrize("number,case", enumerate(examples.CASES.values(), 1), ids=lambda item: getattr(item, "slug", str(item)))
def test_native_workbook_recalculation(excel, number, case, record_property):
    path = ROOT / "dal-excel/examples/autocallables" / f"{number:03d}.{case.slug}.xlsx"
    original = hashlib.sha256(path.read_bytes()).hexdigest()
    book = excel.Workbooks.Open(str(path), UpdateLinks=0, ReadOnly=True)
    try:
        excel.CalculateFullRebuild()
        inputs = book.Worksheets("Inputs")
        pricing = book.Worksheets("Pricing")
        assert inputs.Range("B2").Value2 == 1
        for address in ("B2", "B3", "B4", "B5", "B6"):
            assert isinstance(pricing.Range(address).Value2, str), (address, pricing.Range(address).Text)
        paths = int(inputs.Range("B4").Value2)
        expected = examples.value(case, paths)
        aad_range = "A15:B22" if case.worst_of else "A15:B19"
        plain = read_result(pricing, "A10:B10")
        aad = read_result(pricing, aad_range)
        assert plain == pytest.approx(expected["plain"], abs=1e-10)
        assert aad == pytest.approx(expected["aad"], abs=1e-10)
        record_property("excel_version", excel.Version)
        record_property("paths", paths)
        record_property("plain", json.dumps(plain, sort_keys=True))
        record_property("aad", json.dumps(aad, sort_keys=True))
        assert_formula_results(book)

        inputs.Range("B5").Value2 = (100.0 if case.worst_of else 1.0) * 0.9
        excel.CalculateFullRebuild()
        bumped = read_result(pricing, "A10:B10")
        assert abs(bumped["PV"] - plain["PV"]) > 1e-6
        inputs.Range("B5").Value2 = 100.0 if case.worst_of else 1.0
        excel.CalculateFullRebuild()
        assert read_result(pricing, "A10:B10") == pytest.approx(plain, abs=1e-10)
        assert read_result(pricing, aad_range) == pytest.approx(aad, abs=1e-10)
    finally:
        book.Close(SaveChanges=False)
        assert hashlib.sha256(path.read_bytes()).hexdigest() == original
