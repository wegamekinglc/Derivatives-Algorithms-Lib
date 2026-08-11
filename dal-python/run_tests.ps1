# Build and test the DAL Python binding using a fresh uv-managed environment.
#
# Usage:
#   cd dal-python && .\run_tests.ps1              # run all tests
#   cd dal-python && .\run_tests.ps1 -Python 3.9   # select an exact CPython
#   cd dal-python && .\run_tests.ps1 -v            # verbose pytest output
#   cd dal-python && .\run_tests.ps1 -k test_date  # run specific tests
#
# Prerequisites:
#   - uv (https://docs.astral.sh/uv/)
#   - The C++ library must be installed first (run ..\build_windows.bat)
#   - pybind11 (vendored as a git submodule at dal-cpp/externals/pybind11, v2.11.1) and CPython 3.9-3.13 development headers
#   - Visual Studio 2022 with C++ workload

param(
    [switch]$Clean,
    [switch]$Help,
    [string]$Python,
    [string]$DalInstallPrefix,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$PytestArgs
)

$ErrorActionPreference = "Stop"
$SupportedPythons = @("3.9", "3.10", "3.11", "3.12", "3.13")
$PythonRequested = $PSBoundParameters.ContainsKey("Python")
if ($PythonRequested -and ((-not $Python) -or ($Python -notin $SupportedPythons))) {
    Write-Error "-Python: unsupported value '$Python'; expected one of $($SupportedPythons -join ', ')"
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$DalDir = Split-Path -Parent $ScriptDir
if (-not $DalInstallPrefix) {
    $DalInstallPrefix = $env:DAL_INSTALL_PREFIX
}
if (-not $DalInstallPrefix) {
    $DalInstallPrefix = Join-Path $DalDir "build\stage\Release-windows"
}

if ($Help) {
    Write-Output "Usage: .\run_tests.ps1 [OPTIONS] [-- pytest_args...]"
    Write-Output ""
    Write-Output "Build and test the DAL Python binding"
    Write-Output ""
    Write-Output "Options:"
    Write-Output "  -Clean         Clean build artifacts before building"
    Write-Output "  -Python <minor> Select CPython 3.9, 3.10, 3.11, 3.12, or 3.13"
    Write-Output "  -DalInstallPrefix <path>  Installed DAL prefix"
    Write-Output "  -Help          Show this help message"
    Write-Output ""
    Write-Output "All other arguments are forwarded to pytest."
    exit 0
}

if (-not (Get-Command uv -ErrorAction SilentlyContinue)) {
    Write-Error "Error: uv is required by dal-python/run_tests.ps1"
}

Write-Output "================================================"
Write-Output " DAL Python Binding -- Build & Test (uv-managed)"
Write-Output "================================================"
Write-Output "  DAL root:    $DalDir"
Write-Output "  DAL install: $DalInstallPrefix"
Write-Output "  Working dir: $ScriptDir"
Write-Output ""

# ---- Step 1: Verify C++ library is built ------------------------------------

$LibPublic = Join-Path $DalInstallPrefix "lib\dal_public.lib"
$LibCpp = Join-Path $DalInstallPrefix "lib\dal_cpp.lib"

if (-not ((Test-Path $LibPublic) -and (Test-Path $LibCpp))) {
    Write-Output "ERROR: DAL C++ libraries not found in $DalInstallPrefix\lib\"
    Write-Output "       Build and install DAL first:"
    Write-Output ""
    Write-Output "         cd $DalDir"
    Write-Output "         .\build_windows.bat"
    Write-Output ""
    exit 1
}
Write-Output "[OK] C++ libraries found"

# ---- Step 2: Create or reuse uv virtual environment -------------------------

$VenvDir = Join-Path $ScriptDir ".venv"

if ($Clean) {
    Write-Output "Cleaning build artifacts..."
    if (Test-Path $VenvDir) { Remove-Item -Recurse -Force $VenvDir }
    if (Test-Path (Join-Path $ScriptDir "build")) { Remove-Item -Recurse -Force (Join-Path $ScriptDir "build") }
    if (Test-Path (Join-Path $ScriptDir "dist")) { Remove-Item -Recurse -Force (Join-Path $ScriptDir "dist") }
    Get-ChildItem -Path $ScriptDir -Directory -Filter "__pycache__" -Recurse | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    Get-ChildItem -Path $ScriptDir -Directory -Filter "*.egg-info" -Recurse | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    Write-Output "[OK] Clean complete"
}

if (-not (Test-Path $VenvDir)) {
    Write-Output "Creating fresh uv virtual environment..."
    $PythonRequest = if ($PythonRequested) { $Python } else { ">=3.9,<3.14" }
    uv venv $VenvDir --python $PythonRequest
}
else {
    Write-Output "Reusing existing virtual environment at .venv/"
}

# Activate the venv
$VenvActivate = Join-Path $VenvDir "Scripts\Activate.ps1"
$VenvPython = Join-Path $VenvDir "Scripts\python.exe"
if (-not (Test-Path $VenvPython)) {
    Write-Output "ERROR: Virtual environment python not found at $VenvPython"
    Write-Output "       Try running with -Clean to recreate the venv."
    exit 1
}

$CompatArgs = @(
    (Join-Path $ScriptDir "scripts\python_compat.py"),
    "--entry-point", "dal-python/run_tests.ps1",
    "--environment", $VenvDir,
    "--remediation", "rerun with -Clean to recreate $VenvDir"
)
if ($PythonRequested) { $CompatArgs += @("--requested", $Python) }
& $VenvPython @CompatArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

. $VenvActivate
Write-Output "  Python: $(python --version)"
Write-Output "  Path:   $(Get-Command python | Select-Object -ExpandProperty Source)"

# ---- Step 3: Install build + test dependencies ------------------------------

Write-Output ""
Write-Output "Installing dependencies (scikit-build-core, pytest, numpy)..."
uv pip install "scikit-build-core==1.0.3" pytest numpy

# ---- Step 4: Install the package in editable mode ---------------------------

Write-Output ""
Write-Output "Installing dal-python in editable mode..."
Write-Output "  DAL_INSTALL_PREFIX=$DalInstallPrefix"

$env:DAL_INSTALL_PREFIX = $DalInstallPrefix

uv pip install `
    --no-build-isolation `
    --config-settings=cmake.define.DAL_INSTALL_PREFIX="$DalInstallPrefix" `
    -e ".[test]"

Write-Output "[OK] Package installed"

# ---- Step 5: Verify import works --------------------------------------------

Write-Output ""
Write-Output "Verifying import..."
$importResult = python -c "import dal; print(f'  dal {dal.__version__} loaded successfully')" 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Output "[OK] Import successful"
    Write-Output $importResult
}
else {
    Write-Output "ERROR: Failed to import dal module"
    Write-Output $importResult
    exit 1
}

# ---- Step 6: Run tests ------------------------------------------------------

Write-Output ""
Write-Output "================================================"
Write-Output " Running tests"
Write-Output "================================================"
Write-Output ""

$pytestArgs = @("tests/", "-v")
if ($PytestArgs) {
    $pytestArgs += $PytestArgs
}

python -m pytest @pytestArgs
