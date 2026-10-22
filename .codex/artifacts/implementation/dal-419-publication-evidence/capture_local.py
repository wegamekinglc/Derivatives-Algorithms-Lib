"""Audit integration gates and retain exact raw evidence without binaries."""

import hashlib
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET
from zipfile import ZipFile, ZIP_DEFLATED

repo = Path(__file__).resolve().parents[4]
evidence = Path(__file__).resolve().parent
snapshot = json.loads((evidence / "integration.json").read_text())
root = Path(snapshot["native_root"])
assert not json.loads((evidence / "source-audit.json").read_text())["content_mismatches"]
results = json.loads((evidence / "run-build.json").read_text()) + json.loads((evidence / "run-audit.json").read_text())
assert len(results) == 7
summary = {}
for result in results:
    name = result["name"]
    assert result["exit"] == 0
    assert int((root / (name + ".exit.txt")).read_text().strip()) == 0
    log = (root / (name + ".log")).read_text(errors="replace")
    version = "3.27.6" if name.endswith("327") else "4.2.3"
    assert "cmake version " + version in log
    environment = next(json.loads(line) for line in log.splitlines() if line.startswith('{"platform": "win32"'))
    assert all(environment[key] == 936 for key in ("system_acp", "console_input_cp", "console_output_cp"))
    assert not environment["CL"] and not environment["_CL_"]
    summary[name] = {"exit": 0, "cmake": version, "environment": environment}
    if name.startswith("consumer"):
        assert "100% tests passed, 0 tests failed out of 3" in log
        summary[name]["passed"] = 3
    if name.startswith("excel"):
        test_log = (root / name / "excel-ON-ctest.log").read_text(errors="replace")
        passed = int(re.search(r"100% tests passed, 0 tests failed out of (\d+)", test_log)[1])
        assert "Standalone Excel tests OFF: XLL present, CTest count=0" in log
        assert f"Standalone Excel tests ON: XLL present, CTest count={passed}" in log
        for mode in ("OFF", "ON"):
            assert (root / name / ("tests-" + mode) / "dal_excel.xll").is_file()
            assert (root / name / ("tests-" + mode) / "installed-audit.log").is_file()
        summary[name].update(passed=passed, tests_off_and_on_xll_audit=True)
focused = ET.parse(root / "focused.xml").getroot()
assert focused.attrib["tests"] == "42" and focused.attrib["failures"] == "0"
summary["focused"]["passed"] = 42
with ZipFile(repo / ".codex/artifacts/implementation/dal-419-encoding-evidence/raw-evidence.zip") as archive:
    oracle = archive.read("windows/compat-baseline.bin")
native_bytes = (root / "compat.bin").read_bytes()
linux_bytes = (repo / "build/dal419-publication-compat.bin").read_bytes()
assert native_bytes == oracle and linux_bytes == oracle.replace(b"\r\n", b"\n")
assert (root / "install/include/dal/script/visitor/debugger.hpp").read_bytes() == (repo / "dal-cpp/dal/script/visitor/debugger.hpp").read_bytes()
commands = {}
for name in ("consumer-423", "consumer-327", "excel-423", "excel-327", "focused"):
    rows = (root / (name + "-commands.txt")).read_text(errors="replace").splitlines()
    compiled = [row for row in rows if re.search(r"(?:^|\s)[-/]c\s", row)]
    if name.startswith("excel"):
        compiled = [row for row in compiled if "dal_excel_tests.dir" in row]
    if name == "focused":
        compiled = [row for row in compiled if "compat.dir" in row]
    assert compiled, name
    assert all(not re.search(r"[-/](?:utf-8|source-charset|execution-charset)", row, re.I) for row in compiled), name
    commands[name] = len(compiled)
linux_log = (repo / "build/dal419-publication-linux.log").read_text()
assert "100% tests passed, 0 tests failed out of 3" in linux_log
linux_focused = ET.parse(repo / "build/dal419-publication-focused.xml").getroot()
assert linux_focused.attrib["tests"] == "42" and linux_focused.attrib["failures"] == "0"
raw = {}
for path in root.iterdir():
    if path.is_file() and path.suffix in (".cmd", ".log", ".txt", ".xml", ".bin", ".py"):
        raw["native/" + path.name] = path
folders = [root / "source/build/native", root / "focused"] + [root / (kind + "-" + version)
            for kind in ("consumer", "excel") for version in ("423", "327")]
for folder in folders:
    for path in folder.rglob("*"):
        if path.is_file() and (path.suffix in (".log", ".xml") or path.name in
                              ("CMakeCache.txt", "build.ninja", "rules.ninja", "installed-audit.log", "runtime-contract.txt")):
            raw["native/" + str(path.relative_to(root))] = path
for name in ("dal419-publication-linux.log", "dal419-publication-focused.log", "dal419-publication-focused.xml",
             "dal419-publication-compat.bin"):
    raw["linux/" + name] = repo / "build" / name
for name in ("dal419-linux-make", "dal419-linux-focused", "dal419-publication-consumer"):
    raw["linux/" + name + "/CMakeCache.txt"] = repo / "build" / name / "CMakeCache.txt"
binaries = {}
for folder in folders + [root / "install/lib"]:
    for path in folder.rglob("*"):
        if path.is_file() and path.suffix in (".exe", ".lib", ".xll"):
            binaries[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
for name in ("dal419-linux-focused/debugger_tests", "dal419-linux-focused/compat", "dal419-publication-consumer/dal_installed_consumer",
             "dal419-linux-make/dal-cpp/libdal_cpp.so", "dal419-linux-make/dal-public/libdal_public.a"):
    path = repo / "build" / name
    assert path.exists(), path
    binaries[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
with ZipFile(evidence / "raw-evidence.zip", "w", ZIP_DEFLATED, compresslevel=9) as archive:
    for name, path in sorted(raw.items()):
        archive.write(path, name)
manifest = {"tested_merge": snapshot["head"], "results": summary, "linux": {"consumer": 3, "focused": 42},
            "default_compile_rules_audited": commands, "byte_compatibility": {
                "native_bytes": len(native_bytes), "native_sha256": hashlib.sha256(native_bytes).hexdigest(),
                "linux_bytes": len(linux_bytes), "linux_sha256": hashlib.sha256(linux_bytes).hexdigest(),
                "equals_original_master_oracle": True},
            "raw_sha256": {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in raw.items()},
            "binary_sha256": binaries, "archive_sha256": hashlib.sha256((evidence / "raw-evidence.zip").read_bytes()).hexdigest()}
(evidence / "local-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps({"results": summary, "commands": commands, "raw_files": len(raw), "binaries": len(binaries)}, indent=2))
