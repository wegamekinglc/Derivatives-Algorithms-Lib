import pathlib

evidence = pathlib.Path(__file__).resolve().parent
root = pathlib.Path("/mnt/d/multica-builds/dal418-eb7d672c679c-6372ede9")
win = str(pathlib.PureWindowsPath("D:/multica-builds/dal418-eb7d672c679c-6372ede9"))
source = win + "\\source"
python = source + "\\dal-python\\.venv\\Scripts\\python.exe"
lines = [
    '@echo off', f'call :run > "{win}\\windows-standalone-python.log" 2>&1',
    'set "DAL418_EXIT=%ERRORLEVEL%"', f'echo %DAL418_EXIT% > "{win}\\windows-standalone-python-exit.txt"',
    'exit /b %DAL418_EXIT%', ':run',
    'call "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\Tools\\VsDevCmd.bat" -startdir=none -arch=x64 -host_arch=x64',
    'if errorlevel 1 exit /b 1', f'cd /d "{source}"', 'set "DAL_NUM_THREADS=4"',
    f'set "PATH={source}\\dal-python\\.venv\\Scripts;%PATH%"',
    'build\\Release-windows\\dal-cpp\\dal_cpp_tests.exe --gtest_filter="GSRRateOptionScriptTest.TestAccrualCompatibilitySnapshots:GSRRateOptionScriptTest.TestSubnormalAccrualUsesIndependentDecimalParserBoundary" ' +
        '--gtest_output="xml:' + win.replace('\\', '/') + '/windows-snapshots.xml"',
    'if errorlevel 1 exit /b 1',
    f'cmake -S dal-python -B "{win}\\python-standalone" -G Ninja -DCMAKE_BUILD_TYPE=Release -DDAL_PYTHON_BUILD_TESTS=ON ' +
        f'"-DDAL_INSTALL_PREFIX={source}\\build\\stage\\Release-windows" "-DPython3_EXECUTABLE={python}"',
    'if errorlevel 1 exit /b 1', f'cmake --build "{win}\\python-standalone" --parallel 6',
    'if errorlevel 1 exit /b 1',
    f'ctest --test-dir "{win}\\python-standalone" --verbose --output-on-failure --no-tests=error -C Release -R "^dal_python_pytest$"',
    'if errorlevel 1 exit /b 1',
    f'cmake "-DDAL_INSTALL_PREFIX={source}\\build\\stage\\Release-windows" "-DDAL_CONSUMER_BINARY_DIR={win}\\installed-consumer" -DDAL_BUILD_CONFIG=Release -DDAL_GENERATOR=Ninja -P tests/installed-consumer/run.cmake',
    'if errorlevel 1 exit /b 1', 'exit /b 0', '',
]
driver = "\r\n".join(lines).encode()
(root / "run-standalone-python.cmd").write_bytes(driver)
(evidence / "windows-standalone-driver.cmd").write_bytes(driver)
print(win + "\\run-standalone-python.cmd")
