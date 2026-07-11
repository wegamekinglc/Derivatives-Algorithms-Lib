@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -startdir=none -arch=x64 -host_arch=x64

call :set_variable DAL_DIR "%CD%" %DAL_DIR%
call :set_variable BUILD_TYPE "Release" %BUILD_TYPE%

echo BUILD_TYPE: %BUILD_TYPE%
echo DAL_DIR: %DAL_DIR%

rmdir /q /s bin
rmdir /q /s lib

cd dal-cpp\externals\machinist

if exist build (
  rem build folder already exists.
) else (
  mkdir build
)

echo Starting build machinist2
call ./build_windows.bat
echo End build machinist2

set MACHINIST_TEMPLATE_DIR=%CD%\template\
echo MACHINIST_TEMPLATE_DIR=%MACHINIST_TEMPLATE_DIR%
bin\Machinist.exe -c %DAL_DIR%/dal-cpp/config/dal.ifc -l %DAL_DIR%/dal-cpp/config/dal.mgl -d %DAL_DIR%/dal-cpp/dal
bin\Machinist.exe -c %DAL_DIR%/dal-cpp/config/dal.ifc -l %DAL_DIR%/dal-cpp/config/dal.mgl -d %DAL_DIR%/dal-excel

if %errorlevel% neq 0 exit /b 1


cd %DAL_DIR%

cmake --preset %BUILD_TYPE%-windows -DDAL_BUILD_PUBLIC=ON -DDAL_CPP_BUILD_EXAMPLES=ON -DDAL_CPP_BUILD_BENCHMARKS=ON

if %errorlevel% neq 0 exit /b 1

cmake --build build\%BUILD_TYPE%-windows --parallel %NUMBER_OF_PROCESSORS%
cmake --install build\%BUILD_TYPE%-windows

if %errorlevel% neq 0 exit /b 1

echo "starting run unit tests suite via ctest"
ctest --test-dir build\%BUILD_TYPE%-windows --output-on-failure -C %BUILD_TYPE%

if %errorlevel% neq 0 exit /b 1

@echo on

EXIT /B 0

:set_variable
set %~1=%~2
