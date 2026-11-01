import importlib.util
from datetime import date
from pathlib import Path
import xml.etree.ElementTree as ET
from zipfile import ZipFile

import pytest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("autocallable_examples", ROOT / "dal-python/examples/autocallables/common.py")
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)


@pytest.mark.parametrize("number,case", list(enumerate(examples.CASES.values(), start=1)), ids=lambda item: getattr(item, "slug", str(item)))
def test_committed_excel_contracts_match_python(number, case):
    path = ROOT / "dal-excel/examples/autocallables" / f"{number:03d}.{case.slug}.xlsx"
    ns = {"m": "http://schemas.openxmlformats.org/spreadsheetml/2006/main"}
    with ZipFile(path) as archive:
        events = ET.fromstring(archive.read("xl/worksheets/sheet3.xml"))
        scripts = [
            "".join(cell.itertext())
            for cell in events.findall(".//m:c[@t='inlineStr']/m:is", ns)
        ][2:]
        assert scripts == examples.build_events(case)[1]
        serials = [int(cell.text) for cell in events.findall(".//m:c[@t='n']/m:v", ns)]
        assert serials == [(day - date(1899, 12, 30)).days for day in examples.build_events(case)[0]]
        pricing = ET.fromstring(archive.read("xl/worksheets/sheet4.xml"))
        formulas = {cell.attrib["r"]: cell.find("m:f", ns) for cell in pricing.findall(".//m:c[m:f]", ns)}
        assert formulas["A10"].attrib["ref"] == "A10:B10"
        assert formulas["A15"].attrib["ref"] == ("A15:B22" if case.worst_of else "A15:B19")
        assert formulas["A10"].text == "MONTECARLO.VALUEWITHSETTINGS(B2,B3,Inputs!B4,B4,B5)"
        assert formulas["A15"].text == "MONTECARLO.VALUEWITHSETTINGS(B2,B3,Inputs!B4,B4,B6)"
