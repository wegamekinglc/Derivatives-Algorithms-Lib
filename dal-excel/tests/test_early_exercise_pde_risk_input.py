"""Isolated generated-entry checks; this does not exercise the Windows XLL ABI."""

import hashlib
import json
import os
from pathlib import Path
import subprocess

import pytest


ROOT = Path(__file__).resolve().parents[2]


def declaration(path, signature):
    source = (ROOT / path).read_text()
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


@pytest.fixture(scope="module")
def probe(tmp_path_factory):
    work = Path(os.environ.get("DAL_S4_PROBE_DIR", tmp_path_factory.mktemp("s4-input")))
    work.mkdir(parents=True, exist_ok=True)
    helpers = [
        ("dal-excel/src/__scriptinput.hpp", "class ScriptSettingsInput_"),
        ("dal-excel/src/__scriptinput.hpp", "inline String_ ScriptSettingLocation("),
        ("dal-excel/src/__scriptinput.hpp", "inline const OPER_* ScriptScalarInput("),
        ("dal-excel/src/__scriptinput.hpp", "inline bool ScriptInputBlank("),
        ("dal-excel/src/__dupireinput.hpp", "inline void ValidateNativeTextInput("),
        ("dal-excel/src/__earlyexercisepdeinput.hpp", "inline bool IsEarlyExercisePdeCell("),
    ]
    raw_header = (ROOT / "dal-excel/src/__earlyexercisepderiskinput.hpp").read_text()
    raw_helpers = raw_header.split("#ifdef _WIN32", 1)[1].split("#endif", 1)[0]
    pieces = [declaration(path, signature) + (";" if signature.startswith("class ") else "")
              for path, signature in helpers]
    converter = declaration("dal-excel/src/_excel.cpp", "String_ FromPascalString(wchar_t* src)")
    string_input = declaration("dal-excel/src/_excel.cpp", "String_ Excel::ToString(const OPER_* src, bool optional)")
    typed_handle = declaration("dal-excel/src/_excel.hpp", "template <class T_> Handle_<T_> ToHandle(")
    template = (ROOT / "dal-excel/tests/earlyexerciseinputprobe.in").read_text()
    source = template.replace("@HELPERS@", "\n".join(pieces) + "\n" + typed_handle + "\n" + raw_helpers)
    source = source.replace("@CONVERTER@", converter).replace("@STRING_INPUT@", string_input)
    generated = sorted((ROOT / "dal-excel/auto").glob("MG_EarlyExercisePdeRisk*_public.inc"))
    source = source.replace("@GENERATED@", "\n".join(f'#include "{path}"' for path in generated))
    (work / "probe.cpp").write_text(source)
    build = ROOT / "build/s4"
    command = [
        "g++-15", "-std=c++17", "-O2", f"-I{ROOT}", f"-I{ROOT / 'dal-cpp'}",
        str(work / "probe.cpp"),
        str(build / "dal-excel/CMakeFiles/dal_excel_portable_tests.dir/src/__earlyexercisepderisk.cpp.o"),
        f"-L{build / 'dal-cpp'}", f"-Wl,-rpath,{build / 'dal-cpp'}", "-ldal_cpp",
        "-o", str(work / "probe"),
    ]
    subprocess.run(command, check=True, capture_output=True, text=True)
    (work / "identity.json").write_text(json.dumps({
        "compile_command": command,
        "source_sha256": hashlib.sha256(source.encode()).hexdigest(),
        "executable_sha256": hashlib.sha256((work / "probe").read_bytes()).hexdigest(),
        "generated_sha256": {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in generated},
        "scope": "Production generated bodies and input helpers with isolated OPER/registry/return adapters; not Windows ABI.",
    }, indent=2) + "\n")
    return work / "probe"


@pytest.mark.parametrize("case,field", [
    ("unicode_key", "row=1"), ("unicode_id", "risk_id"), ("nul_key", "row=1"),
    ("numeric_id", "risk_id"), ("boolean_id", "risk_id"), ("error_id", "risk_id"),
    ("numeric_name", "name"), ("boolean_name", "name"), ("error_name", "name"),
    ("wrong_result", "result"), ("wrong_request", "request"),
    ("wrong_valuation", "valuation"), ("wrong_risk", "risk"), ("wrong_settings", "settings"),
])
def test_generated_entry_rejects_raw_input_with_context(probe, case, field):
    result = subprocess.run([str(probe), case], check=True, capture_output=True, text=True)
    assert "InvalidEarlyExercisePdeRiskRequest" in result.stdout, result.stdout
    assert field in result.stdout, result.stdout


@pytest.mark.parametrize("case", ["valid_key", "valid_id", "valid_factory"])
def test_generated_entry_accepts_canonical_input(probe, case):
    result = subprocess.run([str(probe), case], check=True, capture_output=True, text=True)
    assert result.stdout.startswith("OK"), result.stdout
