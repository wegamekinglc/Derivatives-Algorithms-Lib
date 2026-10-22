"""Export the reviewed mechanical merge to a fresh native Windows root."""

import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile

repo = Path(__file__).resolve().parents[4]
evidence = Path(__file__).resolve().parent
head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip()
reviewed = "909d59d3c49b3a116e2835511cf6ebaf6fa96840"
upstream = "acadd8ee44fdb6a0bb2e70de9d559db59de066f8"
base = "bf4cabea8575206375047e51993a5aa5aeb54c0c"
root = Path("/mnt/d/multica-builds/dal419-publication-" + head[:8])
win = "D:/multica-builds/" + root.name


def git(*args, directory=repo):
    return subprocess.check_output(["git", *args], cwd=directory)


def tree(revision):
    return {name.decode(): metadata.decode() for row in git("ls-tree", "-rz", revision).split(b"\0") if row
            for metadata, name in [row.split(b"\t", 1)]}


parents = git("show", "-s", "--format=%P", head).decode().split()
assert parents == [reviewed, upstream]
old, accepted, before = tree(base), tree(upstream), tree(reviewed)
ours = {name for name in old.keys() | before.keys() if old.get(name) != before.get(name)}
assert all(old.get(name) == accepted.get(name) for name in ours)
expected = dict(accepted)
for name in ours:
    if name in before:
        expected[name] = before[name]
    else:
        expected.pop(name, None)
assert tree(head) == expected
files = ["dal-cpp/dal/script/visitor/debugger.hpp", "tests/installed-consumer/main.cpp"]
assert all(tree(head)[name] == before[name] for name in files)
review = repo / ".codex/artifacts/reviews/dal-419-installed-windows-encoding.md"
assert hashlib.sha256(review.read_bytes()).hexdigest() == "f5852d6fc9a23575530acc6ad3690da73c280364881be4a08bc18d41de655c86"
root.mkdir(exist_ok=False)
source = root / "source"


def export(repository, revision, destination):
    data = git("archive", revision, directory=repository)
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        archive.extractall(destination, filter="data")
    return hashlib.sha256(data).hexdigest()


snapshot = {"head": head, "reviewed": reviewed, "upstream": upstream, "parents": parents,
            "tree": git("rev-parse", "HEAD^{tree}").decode().strip(), "mechanical_tree_verified": True,
            "review_sha256": hashlib.sha256(review.read_bytes()).hexdigest(),
            "code_blobs": {name: before[name] for name in files}, "native_root": str(root), "native_win": win,
            "source_archive_sha256": export(repo, head, source), "dependencies": {}}
for name in ("eigen", "googletest", "machinist", "pybind11", "rapidjson"):
    relative = "dal-cpp/externals/" + name
    pin = git("rev-parse", head + ":" + relative).decode().strip()
    assert git("rev-parse", "HEAD", directory=repo / relative).decode().strip() == pin
    assert not git("status", "--porcelain", "--untracked-files=no", directory=repo / relative).strip()
    snapshot["dependencies"][name] = {"pin": pin, "archive_sha256": export(repo / relative, pin, source / relative)}
tool_root = Path("/mnt/d/multica-builds/dal419-tester-eb7d672c-696edf74/tools/cmake-4.2.3-windows-x86_64/bin")
tool = "D:/multica-builds/dal419-tester-eb7d672c-696edf74/tools/cmake-4.2.3-windows-x86_64/bin"
snapshot["cmake_423_sha256"] = hashlib.sha256((tool_root / "cmake.exe").read_bytes()).hexdigest()
shutil.copyfile(repo / ".codex/artifacts/validation/dal-419-validation-evidence/probe_environment.py", root / "probe_environment.py")
shutil.copytree(repo / ".codex/artifacts/implementation/dal-419-encoding-evidence/focused", root / "focused-source/focused")
shutil.copyfile(repo / ".codex/artifacts/implementation/dal-419-encoding-evidence/compat.cpp", root / "focused-source/compat.cpp")
python = "D:/multica-builds/dal418-eb7d672c679c-6372ede9/source/dal-python/.venv/Scripts/python.exe"


def driver(name, commands, cmake_path=tool):
    lines = ["@echo off", "setlocal", f'call :run > "{win}/{name}.log" 2>&1',
             'set "DAL419_EXIT=%ERRORLEVEL%"', f'echo %DAL419_EXIT% > "{win}/{name}.exit.txt"',
             "exit /b %DAL419_EXIT%", ":run", "if defined CL exit /b 90", "if defined _CL_ exit /b 91",
             'call "C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/Tools/VsDevCmd.bat" -startdir=none -arch=x64 -host_arch=x64',
             "if errorlevel 1 exit /b 92", "if defined CL exit /b 93", "if defined _CL_ exit /b 94",
             f'cd /d "{win}/source"', f'set "PATH={cmake_path};%PATH%"', "chcp", "ver", "where cl", "where cmake",
             "cmake --version", "ctest --version", f'"{python}" "{win}/probe_environment.py"',
             "if errorlevel 1 exit /b 95", 'set "CMAKE_BUILD_PARALLEL_LEVEL=6"', 'set "DAL_NUM_THREADS=4"']
    for command in commands:
        lines.extend([command, "if errorlevel 1 exit /b 1"])
    lines.extend(["exit /b 0", ""])
    (root / (name + ".cmd")).write_bytes("\r\n".join(lines).encode())


driver("build", [f"cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX={win}/install "
                 "-DMSVC_RUNTIME=static -DDAL_BUILD_PUBLIC=ON -DDAL_CPP_BUILD_TESTS=OFF -DDAL_CPP_BUILD_BENCHMARKS=OFF "
                 "-DDAL_PUBLIC_BUILD_TESTS=OFF -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_EXCEL=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF",
                 "cmake --build build/native --parallel 6", "cmake --install build/native"])
for version, path in (("423", tool), ("327", "C:/Program Files/CMake/bin")):
    driver("consumer-" + version, [f"cmake -DDAL_INSTALL_PREFIX={win}/install -DDAL_CONSUMER_BINARY_DIR={win}/consumer-{version} "
                                   "-DDAL_BUILD_CONFIG=Release -DDAL_GENERATOR=Ninja -P tests/installed-consumer/run.cmake"], path)
    driver("excel-" + version, [f"cmake -DDAL_INSTALL_PREFIX={win}/install -DDAL_OPTIONAL_EXCEL_BINARY_DIR={win}/excel-{version} "
                                "-P tests/optional-excel/windows-installed.cmake"], path)
driver("focused", [f"cmake -S {win}/focused-source/focused -B {win}/focused -G Ninja -DCMAKE_BUILD_TYPE=Release "
                   f"-DCMAKE_PREFIX_PATH={win}/install -DDAL419_SOURCE={win}/source",
                   f"cmake --build {win}/focused --parallel 4",
                   f"{win}/focused/debugger_tests.exe --gtest_output=xml:{win}/focused.xml",
                   f"{win}/focused/compat.exe > {win}/compat.bin"])
(evidence / "integration.json").write_text(json.dumps(snapshot, indent=2) + "\n")
print(json.dumps(snapshot, indent=2))
