"""Rebuild master plus the committed rate-option regression, without a branch."""

import hashlib
import io
import json
import pathlib
import shlex
import subprocess
import tarfile

repo = pathlib.Path(__file__).resolve().parents[4]
evidence = pathlib.Path(__file__).resolve().parent
scratch = repo / "build/dal418-master"
source = scratch / "source"
build = scratch / "build"
baseline = "aa9ba13a338f65b66f0fc2620562abfc17736666"
test = "dal-cpp/tests/model/test_gsr_rateoption_script.cpp"


def git(*args, cwd=repo):
    return subprocess.check_output(["git", *args], cwd=cwd, text=True).strip()


def run(argv, name, cwd=repo):
    with (evidence / (name + ".log")).open("w") as log:
        result = subprocess.run(argv, cwd=cwd, stdout=log, stderr=subprocess.STDOUT)
    commands.append({"argv": list(map(str, argv)), "cwd": str(cwd), "exit": result.returncode})
    return result.returncode


assert not source.exists()
source.mkdir(parents=True)
with tarfile.open(fileobj=io.BytesIO(subprocess.check_output(["git", "archive", baseline], cwd=repo))) as archive:
    archive.extractall(source, filter="data")
pins = {}
for name in ("eigen", "googletest", "rapidjson", "machinist", "pybind11"):
    relative = "dal-cpp/externals/" + name
    dependency = repo / relative
    pin = git("rev-parse", baseline + ":" + relative)
    assert git("rev-parse", "HEAD", cwd=dependency) == pin
    assert not git("status", "--porcelain", "--untracked-files=no", cwd=dependency)
    (source / relative).rmdir()
    (source / relative).symlink_to(dependency, target_is_directory=True)
    pins[name] = pin
(source / test).write_bytes((repo / test).read_bytes())
test_commit = git("rev-parse", "HEAD")
assert (source / test).read_bytes() == subprocess.check_output(["git", "show", test_commit + ":" + test], cwd=repo)
test_sha256 = hashlib.sha256((source / test).read_bytes()).hexdigest()
commands = []
assert run(["cmake", "--preset=Release-linux", "-S", str(source), "-B", str(build),
            "-DCMAKE_CXX_COMPILER=g++-14", "-DDAL_BUILD_PUBLIC=OFF", "-DDAL_CPP_BUILD_EXAMPLES=OFF",
            "-DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"], "master-configure") == 0
assert run(["cmake", "--build", str(build), "--target", "dal_cpp", "gtest", "-j", "4"], "master-build") == 0
entries = json.loads((build / "compile_commands.json").read_text())
objects = []
for relative in ("dal-cpp/tests/test_main.cpp", test):
    entry = next(row for row in entries if row["file"] == str(source / relative))
    argv = shlex.split(entry["command"])
    output = scratch / (pathlib.Path(relative).stem + ".o")
    argv[argv.index("-o") + 1] = str(output)
    assert run(argv, "master-" + output.stem, pathlib.Path(entry["directory"])) == 0
    objects.append(str(output))
binary = scratch / "rate_tests"
assert run(["g++-14", *objects, str(build / "dal-cpp/libdal_cpp.a"), str(build / "lib/libgtest.a"),
            "-pthread", "-o", str(binary)], "master-link") == 0
result = run([str(binary), "--gtest_filter=GSRRateOptionScriptTest.TestTiny*",
              "--gtest_output=xml:" + str(evidence / "master-red.xml")], "master-red")
manifest = {"baseline": baseline, "test_commit": test_commit, "dependencies": pins, "commands": commands,
            "sha256": {str(p.relative_to(repo)): hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in (binary, build / "CMakeCache.txt", build / "compile_commands.json",
                                 build / "dal-cpp/libdal_cpp.a")}}
manifest["sha256"][test] = test_sha256
(evidence / "master-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
assert result == 1
