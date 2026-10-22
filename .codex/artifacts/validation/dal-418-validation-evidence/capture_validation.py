import hashlib
import json
import pathlib
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET

evidence = pathlib.Path(__file__).resolve().parent
repo = evidence.parents[3]
windows = pathlib.Path("/mnt/d/multica-builds/dal418-eb7d672c679c-6372ede9")
head = "6372ede9452b2ec912234a373666dda9b239a1bc"

def git(*args):
    return subprocess.check_output(["git", *args], cwd=repo, text=True).strip()

def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()

assert git("rev-parse", "HEAD") == head
assert not git("diff", "--name-only")
copied = ["windows-python.log", "windows-python-exit.txt", "windows-standalone-python.log",
          "windows-standalone-python-exit.txt", "windows-consumer-default-red.log", "windows-consumer-default-exit.txt",
          "windows-consumer-utf8-build.log", "windows-consumer-utf8-tests.log", "windows-consumer-repro.log",
          "windows-source-audit.json", "windows-snapshots.xml", "run-validation.cmd", "run-python.cmd",
          "run-standalone-python.cmd", "run-consumer-repro.cmd", "verify_sources.py",
          "run-installed-excel.cmd", "run-installed-excel-control.cmd",
          "windows-installed-excel.log", "windows-installed-excel-exit.txt",
          "windows-installed-excel-control.log", "windows-installed-excel-control-exit.txt",
          "windows-installed-excel-narrow-red.log", "windows-installed-excel-narrow-exit.txt",
          "windows-installed-excel-utf8-build.log", "windows-installed-excel-utf8-tests.log"]
for name in copied:
    shutil.copyfile(windows / name, evidence / name)
for path in sorted((windows / "installed-excel").glob("*.log")):
    shutil.copyfile(path, evidence / ("installed-excel-" + path.name))

parent_equal = {}
for relative in ("dal-cpp/dal/script/visitor/debugger.hpp", "tests/installed-consumer/CMakeLists.txt",
                 "tests/installed-consumer/main.cpp", "CMakeLists.txt", "dal-excel/CMakeLists.txt",
                 "dal-excel/tests/test_curvature_request.cpp", "tests/optional-excel/windows-installed.cmake"):
    parent_bytes = subprocess.check_output(["git", "show", "9c547ab6:" + relative], cwd=repo)
    assert parent_bytes == (repo / relative).read_bytes(), relative
    parent_equal[relative] = hashlib.sha256(parent_bytes).hexdigest()
(evidence / "consumer-parent-identity.json").write_text(json.dumps(parent_equal, indent=2) + "\n")

def snapshot(name):
    root = ET.parse(evidence / name).getroot()
    case = root.find(".//testcase[@name='TestAccrualCompatibilitySnapshots']")
    assert case is not None and case.find("failure") is None
    return {p.get("name"): p.get("value") for p in case.findall("./properties/property")}

parent = snapshot("independent-parent-snapshots.xml")
assert len(parent) == 504
assert parent == snapshot("fresh-linux-ac.xml") == snapshot("windows-snapshots.xml")
summaries = {}
for name, count in (("linux-full-test_output.txt", 3883), ("windows-full-test_output.txt", 3967),
                    ("linux-examples.log", 37), ("linux-installed-consumer.log", 3),
                    ("windows-consumer-utf8-tests.log", 3), ("windows-python.log", 1)):
    text = (evidence / name).read_text(errors="replace")
    assert f"100% tests passed, 0 tests failed out of {count}" in text, name
    summaries[name] = {"ctest_tests": count, "failures": 0,
                       "elapsed": re.findall(r"Total Test time \(real\) =\s*([^\r\n]+)", text)[-1]}
    if "python" in name:
        summaries[name]["pytest_summary"] = re.findall(r"[^\r\n]*\d+ passed[^\r\n]*", text)[-1]
standalone = (evidence / "windows-standalone-python.log").read_text(errors="replace")
assert "1460 passed, 4 skipped" in standalone
assert "error C2001" in standalone
assert (evidence / "windows-consumer-default-exit.txt").read_text().strip() != "0"
excel = (evidence / "windows-installed-excel.log").read_text(errors="replace")
assert "excel-OFF-build: passed" in excel and "excel-OFF-ctest: passed" in excel
assert "excel-ON-build failed (2)" in excel and "error C2001" in excel
assert (evidence / "windows-installed-excel-exit.txt").read_text().strip() != "0"
assert (evidence / "windows-installed-excel-narrow-exit.txt").read_text().strip() != "0"
assert "error C2001" in (evidence / "windows-installed-excel-narrow-red.log").read_text(errors="replace")
assert (evidence / "windows-installed-excel-control-exit.txt").read_text().strip() == "0"
excel_control = (evidence / "windows-installed-excel-utf8-tests.log").read_text(errors="replace")
excel_count = int(re.findall(r"100% tests passed, 0 tests failed out of (\d+)", excel_control)[-1])
summaries["windows-installed-excel-utf8-tests.log"] = {
    "ctest_tests": excel_count, "failures": 0,
    "elapsed": re.findall(r"Total Test time \(real\) =\s*([^\r\n]+)", excel_control)[-1],
    "diagnostic_control_only": True}
source_audit = json.loads((evidence / "windows-source-audit.json").read_text())
assert not source_audit["content_mismatches"]
base = json.loads((evidence / "windows-base-binaries.json").read_text())
for name, expected in base.items():
    assert digest(windows / name) == expected, name
assert all("Passed" in line for name in ("linux-full-test_output.txt", "windows-full-test_output.txt")
           for line in (evidence / name).read_text(errors="replace").splitlines()
           if "Test #" in line and "GSRScriptAcceptanceTest." in line)

dependencies = {}
for name in ("eigen", "googletest", "machinist", "pybind11", "rapidjson"):
    relative = "dal-cpp/externals/" + name
    pin = git("rev-parse", head + ":" + relative)
    actual = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo / relative, text=True).strip()
    assert pin == actual
    assert not subprocess.check_output(["git", "status", "--porcelain=v1", "--untracked-files=no"], cwd=repo / relative)
    dependencies[name] = pin

files = [repo / "dal-cpp/dal/model/gsrrateoption.cpp", repo / "dal-cpp/tests/model/test_gsr_rateoption_script.cpp",
         repo / "dal-cpp/tests/model/test_gsr_script_acceptance.cpp", repo / "build/Release-linux/CMakeCache.txt",
         repo / "build/Release-linux/compile_commands.json", repo / "build/Release-linux/dal-cpp/libdal_cpp.a",
         repo / "build/Release-linux/dal-cpp/dal_cpp_tests", repo / "build/Release-linux/dal-public/dal_public_tests",
         repo / "build/Release-linux/dal-excel/dal_excel_portable_tests"]
files += list((repo / "build/Release-linux/dal-python/dal").glob("_dal*.so"))
native_files = [windows / "source/build/Release-windows/CMakeCache.txt",
                windows / "source/build/Release-windows/compile_commands.json"]
native_files += list((windows / "source/build/Release-windows/dal-python/dal").glob("_dal*.pyd"))
native_files += list((windows / "source/build/Release-windows/dal-python").glob("_dal_quote_risk_test*.pyd"))
native_files += [windows / "python-standalone/CMakeCache.txt",
                 windows / "installed-consumer/CMakeCache.txt",
                 windows / "installed-excel/tests-OFF/CMakeCache.txt",
                 windows / "installed-excel/tests-ON/CMakeCache.txt",
                 windows / "installed-excel/tests-OFF/dal_excel.xll",
                 windows / "installed-excel/tests-ON/dal_excel.xll",
                 windows / "installed-excel/tests-ON/dal_excel_tests.exe"]
manifest = {"tested_head": head, "merge_parents": git("show", "-s", "--format=%P", head).split(),
            "source_scope": git("diff", "--name-status", "0ba2e154", head).splitlines(),
            "dependencies": dependencies, "fresh_results": summaries,
            "snapshot_records": 504, "parent_linux_windows_snapshots_equal": True,
            "native_base_binaries_preserved_after_python_enable": True,
            "windows_source_audit": source_audit,
            "initial_linux": {"exit": 8, "tests": 3882, "failures": 3, "cause": "configure subprocess/tool discovery with inherited Windows PATH; unchanged case scripts pass with Linux-only PATH"},
            "windows_standalone_python": {"pytest_passed": 1460, "pytest_skipped": 4, "ctest_passed": 1},
            "unresolved_gate": {"name": "Default native Windows installed consumer", "locale_codepage": 936,
                                "error": "debugger.hpp:308 C2001", "introduced_by_bg1": False,
                                "utf8_diagnostic_control": "CL=/utf-8: 3/3 PASS; no repository repair"},
            "unresolved_excel_gate": {
                "name": "Default native Windows installed Excel tests-ON", "locale_codepage": 936,
                "tests_OFF_xll": "PASS", "tests_ON_error": "debugger.hpp:308 C2001",
                "parent_scope_identity_verified": True,
                "utf8_diagnostic_control_passed_cases": excel_count,
                "default_gate_still_failed": True},
            "linux_sha256": {str(path.relative_to(repo)): digest(path) for path in files},
            "windows_sha256": {str(path.relative_to(windows)): digest(path) for path in native_files},
            "windows_base_sha256": base,
            "evidence_sha256": {path.name: digest(path) for path in evidence.iterdir()
                                if path.is_file() and path.name not in ("manifest.json", "raw-evidence.zip", "archive.json", "capture-validation.log")}}
(evidence / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps({"tested_head": head, "results": summaries, "snapshot_records": 504,
                  "unresolved_gate": manifest["unresolved_gate"]}, indent=2))
