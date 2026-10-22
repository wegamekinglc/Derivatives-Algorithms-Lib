import pathlib

evidence = pathlib.Path(__file__).resolve().parent
root = pathlib.Path("/mnt/d/multica-builds/dal418-eb7d672c679c-6372ede9")
win = str(pathlib.PureWindowsPath("D:/multica-builds/dal418-eb7d672c679c-6372ede9"))
lines = [
    '@echo off', 'setlocal', f'call :run > "{win}\\windows-consumer-repro.log" 2>&1',
    'set "DAL418_EXIT=%ERRORLEVEL%"', f'echo %DAL418_EXIT% > "{win}\\windows-consumer-repro-exit.txt"',
    'exit /b %DAL418_EXIT%', ':run',
    'call "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\Tools\\VsDevCmd.bat" -startdir=none -arch=x64 -host_arch=x64',
    'if errorlevel 1 exit /b 1', 'chcp',
    f'cmake --build "{win}\\installed-consumer" --target dal_installed_consumer --parallel 1 > "{win}\\windows-consumer-default-red.log" 2>&1',
    'set "DAL418_DEFAULT_EXIT=%ERRORLEVEL%"', f'echo %DAL418_DEFAULT_EXIT% > "{win}\\windows-consumer-default-exit.txt"',
    'if "%DAL418_DEFAULT_EXIT%"=="0" exit /b 2',
    'set "CL=/utf-8 %CL%"',
    f'cmake --build "{win}\\installed-consumer" --parallel 3 > "{win}\\windows-consumer-utf8-build.log" 2>&1',
    'if errorlevel 1 exit /b 1',
    f'ctest --test-dir "{win}\\installed-consumer" --output-on-failure -C Release > "{win}\\windows-consumer-utf8-tests.log" 2>&1',
    'if errorlevel 1 exit /b 1', 'exit /b 0', '',
]
driver = "\r\n".join(lines).encode()
(root / "run-consumer-repro.cmd").write_bytes(driver)
(evidence / "windows-consumer-repro-driver.cmd").write_bytes(driver)
