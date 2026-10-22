"""Revalidate RED when master advances only in unrelated validation files."""

import hashlib
import json
import pathlib
import shlex
import subprocess

repo = pathlib.Path(__file__).resolve().parents[4]
evidence = pathlib.Path(__file__).resolve().parent
scratch = repo / "build/dal418-master"
source, build = scratch / "source", scratch / "build"
previous = "aa9ba13a338f65b66f0fc2620562abfc17736666"
current = subprocess.check_output(["git", "rev-parse", "origin/master"], cwd=repo, text=True).strip()
delta = subprocess.check_output(["git", "diff", "--name-status", previous, current], cwd=repo, text=True).splitlines()
expected = {".codex/artifacts/validation/dal-413-gsr-script-numerical-validation.md",
            "dal-cpp/tests/model/test_gsr_script_acceptance.cpp"}
assert {row.split("\t")[1] for row in delta} == expected
assert all(row.startswith("A\t") for row in delta)
for name in expected:
    path = source / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(subprocess.check_output(["git", "show", current + ":" + name], cwd=repo))
test = "dal-cpp/tests/model/test_gsr_rateoption_script.cpp"
assert (source / test).read_bytes() == subprocess.check_output(["git", "show", "0ac14fb7:" + test], cwd=repo)
commands = []


def run(argv, name, cwd=repo):
    with (evidence / (name + ".log")).open("w") as log:
        result = subprocess.run(argv, cwd=cwd, stdout=log, stderr=subprocess.STDOUT)
    commands.append({"argv": list(map(str, argv)), "cwd": str(cwd), "exit": result.returncode})
    return result.returncode


assert run(["cmake", "--preset=Release-linux", "-S", str(source), "-B", str(build),
            "-DCMAKE_CXX_COMPILER=g++-14", "-DDAL_BUILD_PUBLIC=OFF", "-DDAL_CPP_BUILD_EXAMPLES=OFF",
            "-DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"], "latest-master-configure") == 0
assert run(["cmake", "--build", str(build), "--target", "dal_cpp", "gtest", "-j", "4"], "latest-master-build") == 0
entries = json.loads((build / "compile_commands.json").read_text())
objects = []
for relative in ("dal-cpp/tests/test_main.cpp", test):
    entry = next(row for row in entries if row["file"] == str(source / relative))
    argv = shlex.split(entry["command"])
    obj = scratch / ("latest-" + pathlib.Path(relative).stem + ".o")
    argv[argv.index("-o") + 1] = str(obj)
    assert run(argv, obj.stem, pathlib.Path(entry["directory"])) == 0
    objects.append(str(obj))
binary = scratch / "latest_rate_tests"
assert run(["g++-14", *objects, str(build / "dal-cpp/libdal_cpp.a"), str(build / "lib/libgtest.a"),
            "-pthread", "-o", str(binary)], "latest-master-link") == 0
result = run([str(binary), "--gtest_filter=GSRRateOptionScriptTest.TestTiny*",
              "--gtest_output=xml:" + str(evidence / "latest-master-red.xml")], "latest-master-red")
manifest = {"previous": previous, "latest_master": current, "delta": delta,
            "production_and_dependency_tree_unchanged": True, "commands": commands,
            "test_commit": "0ac14fb74696b1d0e80e2426e6270555028410fe",
            "sha256": {str(p.relative_to(repo)): hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in (source / test, binary, build / "dal-cpp/libdal_cpp.a",
                                 build / "CMakeCache.txt", build / "compile_commands.json")}}
(evidence / "latest-master-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
assert result == 1
