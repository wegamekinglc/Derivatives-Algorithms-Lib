import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile

repo = Path(__file__).resolve().parents[4]
evidence = Path(__file__).resolve().parent
native = Path("/mnt/d/multica-builds/dal419-3103ab04")


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


raw = {}
for path in evidence.iterdir():
    if path.suffix in {".log", ".xml", ".bin", ".patch", ".cmd", ".py", ".cpp"} or (
        path.suffix == ".json" and path.name not in {"archive.json", "manifest.json"}
    ):
        raw["local/" + path.name] = path
raw["local/focused/CMakeLists.txt"] = evidence / "focused/CMakeLists.txt"
for path in native.iterdir():
    if path.suffix in {".log", ".xml", ".bin", ".cmd", ".cpp"}:
        raw["windows/" + path.name] = path
for folder in ("excel-red", "excel-green"):
    for path in (native / folder).glob("*.log"):
        raw["windows/" + str(path.relative_to(native))] = path
native_builds = ["source/build/dal419", "consumer-red", "consumer-green", "focused-baseline", "focused-green"]
native_builds += [f"excel-{mode}/{case}" for mode in ("red", "green") for case in ("tests-OFF", "tests-ON", "gtest-build")]
for folder in native_builds:
    for name in ("CMakeCache.txt", "compile_commands.json", "build.ninja", "CTestTestfile.cmake", "installed-audit.log", "runtime-contract.txt",
                 "Testing/Temporary/LastTest.log"):
        path = native / folder / name
        if path.exists():
            raw["windows/" + folder + "/" + name] = path
for folder in ("build/dal419-linux-make", "build/dal419-linux-focused", "build/dal419-linux-consumer"):
    for name in ("CMakeCache.txt", "compile_commands.json", "CTestTestfile.cmake", "Testing/Temporary/LastTest.log"):
        path = repo / folder / name
        if path.exists():
            raw["linux/" + folder + "/" + name] = path

consumer_rules = (native / "consumer-green/build.ninja").read_text()
excel_rules = (native / "excel-green/tests-ON/build.ninja").read_text()
for rules, target in ((consumer_rules, "dal_installed_consumer"), (excel_rules, "dal_excel_tests")):
    blocks = re.findall(r"build [^\n]*: CXX_COMPILER__" + target + r"_[^\n]*\n(.*?)(?=\n\n)", rules, flags=re.S)
    assert blocks and all("/utf-8" not in block and "charset" not in block for block in blocks), target
for relative in ("dal-cpp/dal/script/visitor/debugger.hpp", "tests/installed-consumer/main.cpp"):
    assert (repo / relative).read_bytes() == (native / "source" / relative).read_bytes(), relative
assert (repo / "dal-cpp/dal/script/visitor/debugger.hpp").read_bytes() == (native / "install/include/dal/script/visitor/debugger.hpp").read_bytes()

binary_paths = [native / "install/lib" / name for name in ("dal_cpp.lib", "dal_public.lib")]
binary_paths += [native / "consumer-green" / (name + ".exe") for name in ("dal_installed_consumer", "dal_installed_aad_consumer", "dal_installed_buffer_consumer")]
binary_paths += [native / f"focused-{mode}" / (name + ".exe") for mode in ("baseline", "green") for name in ("debugger_tests", "compat")]
binary_paths += [native / "excel-green" / mode / "dal_excel.xll" for mode in ("tests-OFF", "tests-ON")]
binary_paths += [native / "excel-green/tests-ON/dal_excel_tests.exe"]
binary_paths += [repo / "build/dal419-linux-install/lib" / name for name in ("libdal_cpp.so.1.0.0", "libdal_public.a")]
binary_paths += [repo / "build/dal419-linux-focused" / name for name in ("debugger_tests", "compat")]
binary_paths += [repo / "build/dal419-linux-consumer" / name for name in ("dal_installed_consumer", "dal_installed_aad_consumer", "dal_installed_buffer_consumer")]
assert all(path.exists() for path in binary_paths)

results = {
    "master_build": {"exit": 0},
    "master_probe": {"exit": 2, "expected": "debugger.hpp:308/309 C2001"},
    "original_installed_consumer_red": {"exit": 1, "expected": "debugger.hpp:308/309 C2001"},
    "original_installed_excel_red": {"exit": 1, "tests_off": "XLL build/audit passed", "expected": "tests-ON debugger.hpp C2001"},
    "test_only_consumer_red": {"exit": 1, "expected": "debugger.hpp:308/309 C2001"},
    "baseline_focused_utf8": {"exit": 0, "passed": 42},
    "default_windows_consumer_green": {"exit": 0, "passed": 3},
    "default_windows_excel_green": {"exit": 0, "passed": 259, "tests_off": "XLL build/audit passed", "tests_on": "XLL build/audit passed"},
    "windows_focused_green": {"exit": 0, "passed": 42},
    "linux_build": {"exit": 0},
    "linux_focused_green": {"exit": 0, "passed": 42},
    "linux_installed_consumer_green": {"exit": 0, "passed": 3},
}
(evidence / "execution-results.json").write_text(json.dumps(results, indent=2) + "\n")
raw["local/execution-results.json"] = evidence / "execution-results.json"

manifest = {
    "captured_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
    "master": subprocess.check_output(["git", "rev-parse", "origin/master"], cwd=repo, text=True).strip(),
    "test_commit": "a4ab9bef1dff4ac580d6bc9c6680bb7e9caacd1c",
    "tested_production_commit": "1bbb195aab407ed6e63f6b9dd5490c28d2f4066d",
    "binary_sha256": {str(path): sha256(path) for path in binary_paths},
    "raw_sha256": {name: sha256(path) for name, path in raw.items()},
    "metadata_sha256": {path.name: sha256(path) for path in evidence.glob("*.json") if path.name not in {"manifest.json", "archive.json"}},
    "default_caller_flags_audited": True,
}
(evidence / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
with zipfile.ZipFile(evidence / "raw-evidence.zip", "w", compression=zipfile.ZIP_DEFLATED) as archive:
    for name, path in sorted(raw.items()):
        archive.write(path, name)
    archive.write(evidence / "manifest.json", "manifest.json")
archive_record = {"file": "raw-evidence.zip", "bytes": (evidence / "raw-evidence.zip").stat().st_size, "sha256": sha256(evidence / "raw-evidence.zip")}
(evidence / "archive.json").write_text(json.dumps(archive_record, indent=2) + "\n")
print(json.dumps({"raw_files": len(raw), "binaries_hashed": len(binary_paths), "archive": archive_record}, indent=2))
