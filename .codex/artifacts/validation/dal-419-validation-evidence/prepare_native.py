import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request
from zipfile import ZipFile

evidence = Path(__file__).resolve().parent
repo = evidence.parents[3]
head = '696edf742f4c9024e7745f55c65923b56d98abca'
root = Path('/mnt/d/multica-builds/dal419-tester-eb7d672c-696edf74')
win = 'D:/multica-builds/dal419-tester-eb7d672c-696edf74'
assert subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo, text=True).strip() == head
assert not subprocess.check_output(['git', 'diff', '--name-only'], cwd=repo)
root.mkdir(exist_ok=True)
tools = root / 'tools'
tools.mkdir(exist_ok=True)
filename = 'cmake-4.2.3-windows-x86_64.zip'
base = 'https://github.com/Kitware/CMake/releases/download/v4.2.3/'
checksums = urllib.request.urlopen(base + 'cmake-4.2.3-SHA-256.txt', timeout=60).read()
expected = next(line.split()[0].decode() for line in checksums.splitlines() if line.split()[-1].decode() == filename)
package = tools / filename
if not package.exists():
    with urllib.request.urlopen(base + filename, timeout=60) as source, package.open('wb') as target:
        shutil.copyfileobj(source, target)
assert hashlib.sha256(package.read_bytes()).hexdigest() == expected
tool_directory = tools / 'cmake-4.2.3-windows-x86_64'
if not tool_directory.exists():
    with ZipFile(package) as archive:
        archive.extractall(tools)
tool = win + '/tools/cmake-4.2.3-windows-x86_64/bin'
(evidence / 'cmake-4.2.3-SHA-256.txt').write_bytes(checksums)


def git(*args, directory=repo):
    return subprocess.check_output(['git', *args], cwd=directory)


def export(repository, revision, destination):
    data = git('archive', revision, directory=repository)
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        archive.extractall(destination, filter='data')
    return hashlib.sha256(data).hexdigest()


source = root / 'source'
assert not source.exists(), 'Preserve existing validation source/build trees'
snapshot = {'tested_head': head, 'source_archive_sha256': export(repo, head, source),
            'tool_url': base + filename, 'tool_sha256': expected, 'dependencies': {}}
for name in ('eigen', 'googletest', 'machinist', 'pybind11', 'rapidjson'):
    relative = 'dal-cpp/externals/' + name
    pin = git('rev-parse', head + ':' + relative).decode().strip()
    snapshot['dependencies'][name] = {'pin': pin, 'archive_sha256': export(repo / relative, pin, source / relative)}
master_header = root / 'master-include/dal/script/visitor/debugger.hpp'
master_header.parent.mkdir(parents=True)
master_header.write_bytes(git('show', 'bf4cabea:dal-cpp/dal/script/visitor/debugger.hpp'))
shutil.copytree(repo / '.codex/artifacts/implementation/dal-419-encoding-evidence/focused', root / 'focused-source/focused')
shutil.copyfile(repo / '.codex/artifacts/implementation/dal-419-encoding-evidence/compat.cpp', root / 'focused-source/compat.cpp')
(root / 'master-probe.cpp').write_text('#include <dal/platform/platform.hpp>\n#include <dal/script/visitor/debugger.hpp>\nint main() { return Dal::Script::TreeStyle(false).tee[0] == 0; }\n')


def driver(name, commands):
    lines = ['@echo off', 'setlocal', f'call :run > "{win}/{name}.log" 2>&1',
             'set "DAL419_EXIT=%ERRORLEVEL%"', f'echo %DAL419_EXIT% > "{win}/{name}.exit.txt"',
             'exit /b %DAL419_EXIT%', ':run',
             'if defined CL exit /b 90', 'if defined _CL_ exit /b 91',
             'call "C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/Tools/VsDevCmd.bat" -startdir=none -arch=x64 -host_arch=x64',
             'if errorlevel 1 exit /b 92', 'if defined CL exit /b 93', 'if defined _CL_ exit /b 94',
             f'cd /d "{win}/source"', f'set "PATH={tool};%PATH%"',
             'chcp', 'ver', 'where cl', 'where cmake', 'cmake --version', 'ctest --version',
             'echo CL and _CL_ are undefined before and after VS setup',
             'set "CMAKE_BUILD_PARALLEL_LEVEL=6"', 'set "DAL_NUM_THREADS=4"']
    for command in commands:
        lines.extend([command, 'if errorlevel 1 exit /b 1'])
    lines.extend(['exit /b 0', ''])
    data = '\r\n'.join(lines).encode()
    (root / (name + '.cmd')).write_bytes(data)
    (evidence / (name + '.cmd')).write_bytes(data)


driver('build-fixed', [
    f'cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX={win}/install -DMSVC_RUNTIME=static -DDAL_BUILD_PUBLIC=ON -DDAL_CPP_BUILD_TESTS=OFF -DDAL_PUBLIC_BUILD_TESTS=OFF -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_EXCEL=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF',
    'cmake --build build/native --parallel 6', 'cmake --install build/native'])
driver('consumer-default', [f'cmake -DDAL_INSTALL_PREFIX={win}/install -DDAL_CONSUMER_BINARY_DIR={win}/consumer-default -DDAL_BUILD_CONFIG=Release -DDAL_GENERATOR=Ninja -P tests/installed-consumer/run.cmake'])
driver('excel-default', [f'cmake -DDAL_INSTALL_PREFIX={win}/install -DDAL_OPTIONAL_EXCEL_BINARY_DIR={win}/excel-default -P tests/optional-excel/windows-installed.cmake'])
driver('focused-fixed', [
    f'cmake -S {win}/focused-source/focused -B {win}/focused-fixed -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH={win}/install -DDAL419_SOURCE={win}/source',
    f'cmake --build {win}/focused-fixed --parallel 4',
    f'{win}/focused-fixed/debugger_tests.exe --gtest_output=xml:{win}/focused-fixed.xml',
    f'{win}/focused-fixed/compat.exe > {win}/compat-fixed.bin'])
driver('master-probe', [f'cl /nologo /DWIN32 /D_WINDOWS /EHsc /std:c++17 /I{win}/master-include /I{win}/install/include /c {win}/master-probe.cpp /Fo{win}/master-probe.obj'])
(evidence / 'native-snapshot.json').write_text(json.dumps(snapshot, indent=2) + '\n')
print(json.dumps(snapshot, indent=2))
