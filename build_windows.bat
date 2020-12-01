@echo off

set BUILD_TYPE=Release
set DAL_DIR="%CD%"
set ADDRESS_MODEL=
set MSVC_RUNTIME=dynamic
set MSVC_VERSION=Visual Studio 15 2017

if exist build (
  rem build folder already exists.
) else (
  mkdir build
)

cd build

if "%ADDRESS_MODEL%"=="Win64" (
  if "%MSVC_VERSION%"=="Visual Studio 16 2019" (
    set ADDRESS_MODEL=
  )
)

if "%ADDRESS_MODEL%"=="Win64" (
  set PLATFORM=x64
) else (
  set PLATFORM=Win32
)

if "%ADDRESS_MODEL%" =="Win64" (
cmake -G "%MSVC_VERSION% %ADDRESS_MODEL%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%DAL_DIR% -DMSVC_RUNTIME=%MSVC_RUNTIME% --target install ..
) else (
cmake -G "%MSVC_VERSION%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%DAL_DIR% -DMSVC_RUNTIME=%MSVC_RUNTIME% --target install ..
)

if %errorlevel% neq 0 exit /b 1

msbuild dal.sln /m /p:Configuration=%BUILD_TYPE% /p:Platform=%PLATFORM%
msbuild INSTALL.vcxproj /m:%NUMBER_OF_PROCESSORS% /p:Configuration=%BUILD_TYPE% /p:Platform=%PLATFORM%

if %errorlevel% neq 0 exit /b 1

cd ..

bin\test_suite.exe

if %errorlevel% neq 0 exit /b 1

@echo on