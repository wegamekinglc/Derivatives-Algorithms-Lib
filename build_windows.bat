@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -startdir=none -arch=x64 -host_arch=x64

call :set_variable DAL_DIR "%CD%" %DAL_DIR%
call :set_variable BUILD_TYPE "Release" %BUILD_TYPE%
call :set_variable MSVC_VERSION "Visual Studio 17 2022" %MSVC_VERSION%

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

if exist build (
  rem build folder already exists.
) else (
  mkdir build
)

cd build
cmake -G "%MSVC_VERSION%" --preset %BUILD_TYPE%-windows -DDAL_BUILD_PUBLIC=ON -DDAL_CPP_BUILD_EXAMPLES=ON -DDAL_CPP_BUILD_BENCHMARKS=ON ..

if %errorlevel% neq 0 exit /b 1

msbuild dal.sln /m /p:Configuration=%BUILD_TYPE% /p:Platform=x64
msbuild INSTALL.vcxproj /m:%NUMBER_OF_PROCESSORS% /p:Configuration=%BUILD_TYPE% /p:Platform=x64

if %errorlevel% neq 0 exit /b 1

echo "starting run unit tests suite via ctest"
ctest --output-on-failure -C %BUILD_TYPE%

cd ..

if %errorlevel% neq 0 exit /b 1

@echo on

EXIT /B 0

:set_variable
set %~1=%~2
