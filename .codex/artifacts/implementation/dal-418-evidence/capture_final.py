"""Capture exact source, dependency, configuration and executable identities."""

import hashlib
import json
import pathlib
import subprocess
import xml.etree.ElementTree as ET
import zipfile

repo = pathlib.Path(__file__).resolve().parents[4]
evidence = pathlib.Path(__file__).resolve().parent


def git(*args):
    return subprocess.check_output(["git", *args], cwd=repo, text=True).strip()


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


master = json.loads((evidence / "master-manifest.json").read_text())
test = "dal-cpp/tests/model/test_gsr_rateoption_script.cpp"
assert sha(repo / "build/dal418-master/source" / test) == master["sha256"][test]
subprocess.run(["git", "merge-base", "--is-ancestor", master["baseline"], "HEAD"], cwd=repo, check=True)
pins = {}
for name in ("eigen", "googletest", "rapidjson", "machinist", "pybind11"):
    path = repo / "dal-cpp/externals" / name
    actual = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=path, text=True).strip()
    assert actual == master["dependencies"][name]
    assert not subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=no"], cwd=path, text=True).strip()
    pins[name] = actual
files = ["dal-cpp/dal/model/gsrrateoption.cpp", test, "build/dal408/dal-cpp/libdal_cpp.a",
         "build/dal408/dal-cpp/dal_cpp_tests", "build/dal408/CMakeCache.txt", "build/dal408/compile_commands.json",
         "build/dal418-parent/focused_tests"]
results = {}
for name in ("master-red.xml", "latest-master-red.xml", "parent-red.xml", "parent-expanded-red.xml", "target-green.xml"):
    if (evidence / name).exists():
        root = ET.parse(evidence / name).getroot()
    else:
        with zipfile.ZipFile(evidence / "raw-evidence.zip") as archive:
            root = ET.fromstring(archive.read(name))
    results[name] = {key: root.attrib.get(key) for key in ("tests", "failures", "errors")}
data = {
    "initial_master": master["baseline"],
    "latest_master": json.loads((evidence / "latest-master-manifest.json").read_text())["latest_master"],
    "production_parent": "9c547ab64ff2a53ef444ac928440e443fc720c24",
    "test_commits": [git("rev-parse", "0ac14fb7"), git("rev-parse", "4b5e49e7")],
    "fix_commit": git("rev-parse", "a4386247"), "dependencies": pins,
    "toolchain": {
        "compiler": subprocess.check_output(["g++-14", "--version"], text=True).splitlines()[0],
        "cmake": subprocess.check_output(["cmake", "--version"], text=True).splitlines()[0],
        "platform": subprocess.check_output(["uname", "-srmo"], text=True).strip(),
        "cpu": "13th Gen Intel Core i9-13900HX", "threads": 1,
        "build": "Release; O3; C++17; native AAD; Eigen ON; native arch OFF; static"},
    "results": results, "sha256": {name: sha(repo / name) for name in files},
    "evidence_sha256": {p.name: sha(p) for p in evidence.iterdir()
                        if p.suffix in (".xml", ".log", ".json", ".py") and p.name != "final-manifest.json"}}
with zipfile.ZipFile(evidence / "raw-evidence.zip") as archive:
    data["evidence_sha256"].update({name: hashlib.sha256(archive.read(name)).hexdigest() for name in archive.namelist()})
data["raw_archive_sha256"] = sha(evidence / "raw-evidence.zip")
(evidence / "final-manifest.json").write_text(json.dumps(data, indent=2) + "\n")
print(json.dumps(results, indent=2))
