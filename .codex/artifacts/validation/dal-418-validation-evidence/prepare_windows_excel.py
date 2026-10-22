from pathlib import Path

root = Path('/mnt/d/multica-builds/dal418-eb7d672c679c-6372ede9')
drive = r'D:\multica-builds\dal418-eb7d672c679c-6372ede9'
body = rf'''@echo off
setlocal
call :run > "{drive}\windows-installed-excel.log" 2>&1
set "DAL418_EXIT=%ERRORLEVEL%"
echo %DAL418_EXIT% > "{drive}\windows-installed-excel-exit.txt"
exit /b %DAL418_EXIT%
:run
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -startdir=none -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1
cd /d "{drive}\source"
set "CMAKE_BUILD_PARALLEL_LEVEL=6"
set "DAL_NUM_THREADS=4"
cmake "-DDAL_INSTALL_PREFIX={drive}\source\build\stage\Release-windows" "-DDAL_OPTIONAL_EXCEL_BINARY_DIR={drive}\installed-excel" -P tests/optional-excel/windows-installed.cmake
exit /b %ERRORLEVEL%
'''
(root / 'run-installed-excel.cmd').write_bytes(body.replace('\n', '\r\n').encode())
control = rf'''@echo off
setlocal
call :run > "{drive}\windows-installed-excel-control.log" 2>&1
set "DAL418_EXIT=%ERRORLEVEL%"
echo %DAL418_EXIT% > "{drive}\windows-installed-excel-control-exit.txt"
exit /b %DAL418_EXIT%
:run
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -startdir=none -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1
chcp
set "DAL_NUM_THREADS=4"
cmake --build "{drive}\installed-excel\tests-ON" --target CMakeFiles/dal_excel_tests.dir/tests/test_curvature_request.cpp.obj --parallel 1 > "{drive}\windows-installed-excel-narrow-red.log" 2>&1
set "DAL418_DEFAULT_EXIT=%ERRORLEVEL%"
echo %DAL418_DEFAULT_EXIT% > "{drive}\windows-installed-excel-narrow-exit.txt"
if "%DAL418_DEFAULT_EXIT%"=="0" exit /b 2
set "CL=/utf-8 %CL%"
cmake --build "{drive}\installed-excel\tests-ON" --parallel 6 > "{drive}\windows-installed-excel-utf8-build.log" 2>&1
if errorlevel 1 exit /b 1
ctest --test-dir "{drive}\installed-excel\tests-ON" -C Release --output-on-failure --no-tests=error --parallel 4 > "{drive}\windows-installed-excel-utf8-tests.log" 2>&1
exit /b %ERRORLEVEL%
'''
(root / 'run-installed-excel-control.cmd').write_bytes(control.replace('\n', '\r\n').encode())
