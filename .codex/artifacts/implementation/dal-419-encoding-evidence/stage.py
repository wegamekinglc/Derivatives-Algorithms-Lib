from pathlib import Path
import shutil
import sys

repo = Path(__file__).resolve().parents[4]
evidence = Path(__file__).resolve().parent
root = Path("/mnt/d/multica-builds/dal419-3103ab04")
win = "D:/multica-builds/dal419-3103ab04"


def driver(name, commands):
    lines = [
        "@echo off", "setlocal", f'cd /d "{win}/source"',
        'call "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\Tools\\VsDevCmd.bat" -startdir=none -arch=x64 -host_arch=x64',
        "if errorlevel 1 exit /b 1", "chcp",
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


def focus_commands(mode):
    flags = ' -DDAL419_BASELINE_UTF8=ON -DCMAKE_CXX_FLAGS="/DWIN32 /D_WINDOWS /GR /EHsc"' if mode == "baseline" else ""
    return [
        f'cmake -S {win}/focused-source/focused -B {win}/focused-{mode} -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH={win}/install -DDAL419_SOURCE={win}/source{flags}',
        f'cmake --build {win}/focused-{mode} --parallel 4',
        f'{win}/focused-{mode}/debugger_tests.exe --gtest_output=xml:{win}/focused-{mode}.xml',
        f'{win}/focused-{mode}/compat.exe > {win}/compat-{mode}.bin',
    ]


mode = sys.argv[1]
if mode == "baseline":
    shutil.copytree(evidence / "focused", root / "focused-source/focused", dirs_exist_ok=True)
    shutil.copy2(evidence / "compat.cpp", root / "focused-source/compat.cpp")
    driver("focused-baseline.cmd", focus_commands("baseline"))
elif mode == "test":
    shutil.copy2(repo / "tests/installed-consumer/main.cpp", root / "source/tests/installed-consumer/main.cpp")
    driver("consumer-test-red.cmd", [f'cmake --build {win}/consumer-red --target dal_installed_consumer --parallel 1'])
elif mode == "green":
    for relative in ("dal-cpp/dal/script/visitor/debugger.hpp", "tests/installed-consumer/main.cpp"):
        shutil.copy2(repo / relative, root / "source" / relative)
    driver("green.cmd", [
        'cmake --build build/dal419 --parallel 6',
        'cmake --install build/dal419',
        f'cmake -DDAL_INSTALL_PREFIX={win}/install -DDAL_CONSUMER_BINARY_DIR={win}/consumer-green -DDAL_BUILD_CONFIG=Release -DDAL_GENERATOR=Ninja -P tests/installed-consumer/run.cmake',
    ])
    driver("excel-green.cmd", [
        f'cmake -DDAL_INSTALL_PREFIX={win}/install -DDAL_OPTIONAL_EXCEL_BINARY_DIR={win}/excel-green -P tests/optional-excel/windows-installed.cmake',
    ])
    driver("focused-green.cmd", focus_commands("green"))
else:
    raise ValueError(mode)
