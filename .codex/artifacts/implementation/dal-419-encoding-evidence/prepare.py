import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile

repo = Path(__file__).resolve().parents[4]
evidence = Path(__file__).resolve().parent
parent = repo.parents[2] / "dal-408-e2075e8549aa/workdir/Derivatives-Algorithms-Lib"
root = Path("/mnt/d/multica-builds/dal419-3103ab04")
root.mkdir(exist_ok=False)
source = root / "source"
source.mkdir()
master = subprocess.check_output(["git", "rev-parse", "origin/master"], cwd=repo, text=True).strip()


def export(repository, revision, destination):
    data = subprocess.check_output(["git", "archive", revision], cwd=repository)
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        archive.extractall(destination, filter="data")
    return hashlib.sha256(data).hexdigest()


manifest = {"master": master, "source": str(source), "archive_sha256": export(repo, master, source), "dependencies": {}}
for name in ("eigen", "googletest", "machinist", "pybind11", "rapidjson"):
    relative = "dal-cpp/externals/" + name
    pin = subprocess.check_output(["git", "rev-parse", master + ":" + relative], cwd=repo, text=True).strip()
    origin = parent / relative
    assert subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=origin, text=True).strip() == pin
    manifest["dependencies"][name] = {"pin": pin, "archive_sha256": export(origin, pin, source / relative)}
    subprocess.run(["git", "clone", "--shared", "--no-checkout", str(origin), str(repo / relative)], check=True)
    subprocess.run(["git", "checkout", "--detach", pin], cwd=repo / relative, check=True)

(evidence / "snapshot.json").write_text(json.dumps(manifest, indent=2) + "\n")


def driver(name, commands):
    win = "D:\\multica-builds\\dal419-3103ab04"
    lines = [
        "@echo off", "setlocal", f'cd /d "{win}\\source"',
        'call "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\Tools\\VsDevCmd.bat" -startdir=none -arch=x64 -host_arch=x64',
        "if errorlevel 1 exit /b 1", "chcp", "ver", "where cl", "cmake --version",
        'if defined CL (echo Unexpected CL environment & exit /b 9)',
        'if defined _CL_ (echo Unexpected _CL_ environment & exit /b 9)',
        'set "CMAKE_BUILD_PARALLEL_LEVEL=6"', 'set "DAL_NUM_THREADS=4"',
    ]
    for command in commands:
        lines += [command, "if errorlevel 1 exit /b 1"]
    lines += ["exit /b 0", ""]
    data = "\r\n".join(lines).encode()
    (root / name).write_bytes(data)
    (evidence / name).write_bytes(data)


driver("build-master.cmd", [
    'cmake -S . -B build/dal419 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=D:/multica-builds/dal419-3103ab04/install -DMSVC_RUNTIME=static -DDAL_BUILD_PUBLIC=ON -DDAL_CPP_BUILD_TESTS=OFF -DDAL_PUBLIC_BUILD_TESTS=OFF -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_EXCEL=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF',
    'cmake --build build/dal419 --parallel 6',
    'cmake --install build/dal419',
])
driver("consumer-red.cmd", [
    'cmake -DDAL_INSTALL_PREFIX=D:/multica-builds/dal419-3103ab04/install -DDAL_CONSUMER_BINARY_DIR=D:/multica-builds/dal419-3103ab04/consumer-red -DDAL_BUILD_CONFIG=Release -DDAL_GENERATOR=Ninja -P tests/installed-consumer/run.cmake',
])
driver("excel-red.cmd", [
    'cmake -DDAL_INSTALL_PREFIX=D:/multica-builds/dal419-3103ab04/install -DDAL_OPTIONAL_EXCEL_BINARY_DIR=D:/multica-builds/dal419-3103ab04/excel-red -P tests/optional-excel/windows-installed.cmake',
])
print(json.dumps(manifest, indent=2))
