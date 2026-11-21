@echo off
setlocal

rem Optional first argument: --configure-only stops after the CMake configure
rem (CI smoke-tests this script that way without paying for a full build).
rem --full-tests includes examples and long-running qualification cases.
set "CONFIGURE_ONLY="
if /i "%~1"=="--configure-only" set "CONFIGURE_ONLY=1"

rem Keep the toolchain selected by an x64 Visual Studio developer shell.
if defined VSCMD_VER (
    if /i not "%VSCMD_ARG_TGT_ARCH%"=="x64" (
        echo Run this script from an x64 Visual Studio developer shell or a regular terminal
        exit /b 1
    )
    goto :check_compiler
)

rem Discover Visual Studio with C++ build tools via vswhere. Prefer a stable
rem installation and fall back to Insiders if no stable installation is found.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo vswhere.exe was not found at "%VSWHERE%"
    exit /b 1
)
set "VS_PATH="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"
)
if not defined VS_PATH (
    echo Visual Studio with C++ build tools was not found
    exit /b 1
)
set "VSDEVCMD=%VS_PATH%\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" (
    echo VsDevCmd.bat was not found at "%VSDEVCMD%"
    exit /b 1
)
call "%VSDEVCMD%" -startdir=none -arch=x64 -host_arch=x64
if errorlevel 1 (
    echo Failed to initialize the Visual Studio x64 build environment
    exit /b 1
)

:check_compiler
where cl >nul 2>&1
if errorlevel 1 (
    echo The MSVC compiler was not found on PATH
    exit /b 1
)

call :set_variable DAL_DIR "%CD%" "%DAL_DIR%"
call :set_variable BUILD_TYPE "Release" "%BUILD_TYPE%"

echo BUILD_TYPE: %BUILD_TYPE%
echo DAL_DIR: %DAL_DIR%

if exist "%DAL_DIR%\bin" rmdir /q /s "%DAL_DIR%\bin"
if exist "%DAL_DIR%\lib" rmdir /q /s "%DAL_DIR%\lib"

rem Build Machinist the same way the Windows CI workflow does -- cmake preset,
rem build, install into the submodule's own bin\ -- then regenerate sources.
pushd "%DAL_DIR%\dal-cpp\externals\machinist"
if exist build rmdir /q /s build
mkdir build
pushd build
cmake --preset %BUILD_TYPE%-windows ..
if errorlevel 1 (
    echo Machinist configure failed
    popd
    popd
    exit /b 1
)
cmake --build .
if errorlevel 1 (
    echo Machinist build failed
    popd
    popd
    exit /b 1
)
cmake --install .
if errorlevel 1 (
    echo Machinist install failed
    popd
    popd
    exit /b 1
)
popd

set "MACHINIST_TEMPLATE_DIR=%CD%\template\"
echo MACHINIST_TEMPLATE_DIR=%MACHINIST_TEMPLATE_DIR%
bin\Machinist.exe -c "%DAL_DIR%\dal-cpp\config\dal.ifc" -l "%DAL_DIR%\dal-cpp\config\dal.mgl" -d "%DAL_DIR%\dal-cpp\dal"
if errorlevel 1 (
    echo Machinist code generation for dal-cpp failed
    popd
    exit /b 1
)
cmake "-DDAL_REPOSITORY_ROOT=%DAL_DIR%" -P "%DAL_DIR%\dal-cpp\cmake\normalize-calibration-generated-enums.cmake"
if errorlevel 1 (
    echo Generated enum normalization failed
    popd
    exit /b 1
)
bin\Machinist.exe -c "%DAL_DIR%\dal-cpp\config\dal.ifc" -l "%DAL_DIR%\dal-cpp\config\dal.mgl" -d "%DAL_DIR%\dal-excel"
if errorlevel 1 (
    echo Machinist code generation for dal-excel failed
    popd
    exit /b 1
)
popd

cd /d "%DAL_DIR%"

cmake --preset %BUILD_TYPE%-windows -DDAL_BUILD_PUBLIC=ON -DDAL_CPP_BUILD_EXAMPLES=ON -DDAL_CPP_BUILD_BENCHMARKS=OFF
if errorlevel 1 (
    echo CMake configure failed
    exit /b 1
)

if defined CONFIGURE_ONLY (
    echo Configure-only mode: skipping build, install, and tests
    exit /b 0
)

if not defined CMAKE_BUILD_PARALLEL_LEVEL set "CMAKE_BUILD_PARALLEL_LEVEL=%NUMBER_OF_PROCESSORS%"
cmake --build "build\%BUILD_TYPE%-windows"
if errorlevel 1 (
    echo CMake build failed
    exit /b 1
)

cmake --install "build\%BUILD_TYPE%-windows"
if errorlevel 1 (
    echo CMake install failed
    exit /b 1
)

echo Starting unit test suite via ctest
if not defined CTEST_PARALLEL_LEVEL set "CTEST_PARALLEL_LEVEL=4"
if /i "%~1"=="--full-tests" (
    ctest --test-dir "build\%BUILD_TYPE%-windows" --output-on-failure -C %BUILD_TYPE% -LE benchmark
) else (
    ctest --test-dir "build\%BUILD_TYPE%-windows" --output-on-failure -C %BUILD_TYPE% -LE "benchmark|examples|slow_qualification"
)
if errorlevel 1 (
    echo ctest failed
    exit /b 1
)

endlocal
exit /b 0

:set_variable
if "%~3"=="" (set "%~1=%~2") else (set "%~1=%~3")
exit /b 0
