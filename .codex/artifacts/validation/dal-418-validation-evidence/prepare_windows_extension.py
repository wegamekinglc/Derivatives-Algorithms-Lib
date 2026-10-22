import pathlib

evidence = pathlib.Path(__file__).resolve().parent
root = pathlib.Path("/mnt/d/multica-builds/dal418-eb7d672c679c-6372ede9")
win = str(pathlib.PureWindowsPath("D:/multica-builds/dal418-eb7d672c679c-6372ede9"))
source = win + "\\source"
python = source + "\\dal-python\\.venv\\Scripts\\python.exe"
lines = [
    "@echo off", f'call :run > "{win}\\windows-python.log" 2>&1',
    'set "DAL418_EXIT=%ERRORLEVEL%"', f'echo %DAL418_EXIT% > "{win}\\windows-python-exit.txt"',
    'exit /b %DAL418_EXIT%', ':run',
    'call "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\Tools\\VsDevCmd.bat" -startdir=none -arch=x64 -host_arch=x64',
    'if errorlevel 1 exit /b 1', f'cd /d "{source}"', 'set "DAL_NUM_THREADS=4"',
    f'set "PATH={source}\\dal-python\\.venv\\Scripts;%PATH%"',
    f'"{python}" --version',
    'build\\Release-windows\\dal-cpp\\dal_cpp_tests.exe --gtest_filter="GSRRateOptionScriptTest.TestAccrualCompatibilitySnapshots:GSRRateOptionScriptTest.TestSubnormalAccrualUsesIndependentDecimalParserBoundary" ' +
        '--gtest_output="xml:' + win.replace('\\', '/') + '/windows-snapshots.xml"',
    'if errorlevel 1 exit /b 1',
    'cmake --preset Release-windows -DDAL_BUILD_PUBLIC=ON -DDAL_CPP_BUILD_EXAMPLES=ON -DDAL_CPP_BUILD_BENCHMARKS=OFF -DDAL_BUILD_PYTHON=ON ' + f'"-DPython3_EXECUTABLE={python}"',
    'if errorlevel 1 exit /b 1', 'cmake --build build\\Release-windows --parallel 6', 'if errorlevel 1 exit /b 1',
    'cmake --install build\\Release-windows', 'if errorlevel 1 exit /b 1',
    'ctest --test-dir build\\Release-windows --verbose --output-on-failure --no-tests=error -C Release -R "^dal_python_pytest$"',
    'if errorlevel 1 exit /b 1', 'exit /b 0', '',
]
driver = "\r\n".join(lines).encode("utf-8")
(root / "run-python.cmd").write_bytes(driver)
(evidence / "windows-python-driver.cmd").write_bytes(driver)
print(win + "\\run-python.cmd")
