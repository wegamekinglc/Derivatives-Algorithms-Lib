# Build and test the DAL Python binding using a fresh uv-managed environment.
#
# Usage:
#   cd dal-python && .\run_tests.ps1              # run all tests
#   cd dal-python && .\run_tests.ps1 -v            # verbose pytest output
#   cd dal-python && .\run_tests.ps1 -k test_date  # run specific tests
#
# Prerequisites:
#   - uv (https://docs.astral.sh/uv/)
#   - The C++ library must be built first (run ..\build_win.ps1 or build from top-level CMake)
#   - SWIG 4.x and Python 3.10+ development headers
#   - Visual Studio 2022 with C++ workload

param(
    [switch]$Clean,
    [switch]$Help,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$PytestArgs
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$DalDir = Split-Path -Parent $ScriptDir

if ($Help) {
    Write-Host "Usage: .\run_tests.ps1 [OPTIONS] [-- pytest_args...]"
    Write-Host ""
    Write-Host "Build and test the DAL Python binding"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Clean         Clean build artifacts before building"
    Write-Host "  -Help          Show this help message"
    Write-Host ""
    Write-Host "All other arguments are forwarded to pytest."
    exit 0
}

Write-Host "================================================" -ForegroundColor Cyan
Write-Host " DAL Python Binding -- Build & Test (uv-managed)" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "  DAL root:    $DalDir"
Write-Host "  Working dir: $ScriptDir"
Write-Host ""

# ---- Step 1: Verify C++ library is built ------------------------------------

$LibPublic = Join-Path $DalDir "lib\dal_public.lib"
$LibCpp = Join-Path $DalDir "lib\dal_cpp.lib"

if (-not ((Test-Path $LibPublic) -and (Test-Path $LibCpp))) {
    Write-Host "ERROR: DAL C++ libraries not found in $DalDir\lib\" -ForegroundColor Red
    Write-Host "       Build the C++ library first using the top-level CMakeLists.txt:" -ForegroundColor Red
    Write-Host ""
    Write-Host "         cd $DalDir"
    Write-Host "         mkdir build && cd build"
    Write-Host "         cmake .. -DDAL_BUILD_PUBLIC=ON -DDAL_BUILD_PYTHON=ON"
    Write-Host "         cmake --build . --config Release"
    Write-Host "         cmake --install . --config Release"
    Write-Host ""
    exit 1
}
Write-Host "[OK] C++ libraries found" -ForegroundColor Green

# ---- Step 2: Create or reuse uv virtual environment -------------------------

$VenvDir = Join-Path $ScriptDir ".venv"

if ($Clean) {
    Write-Host "Cleaning build artifacts..." -ForegroundColor Yellow
    if (Test-Path $VenvDir) { Remove-Item -Recurse -Force $VenvDir }
    if (Test-Path (Join-Path $ScriptDir "build")) { Remove-Item -Recurse -Force (Join-Path $ScriptDir "build") }
    if (Test-Path (Join-Path $ScriptDir "dist")) { Remove-Item -Recurse -Force (Join-Path $ScriptDir "dist") }
    Get-ChildItem -Path $ScriptDir -Directory -Filter "__pycache__" -Recurse | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    Get-ChildItem -Path $ScriptDir -Directory -Filter "*.egg-info" -Recurse | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "[OK] Clean complete" -ForegroundColor Green
}

if (-not (Test-Path $VenvDir)) {
    Write-Host "Creating fresh uv virtual environment..."
    uv venv $VenvDir --python ">=3.10"
}
else {
    Write-Host "Reusing existing virtual environment at .venv/"
}

# Activate the venv
$VenvActivate = Join-Path $VenvDir "Scripts\Activate.ps1"
$VenvPython = Join-Path $VenvDir "Scripts\python.exe"
if (-not (Test-Path $VenvPython)) {
    Write-Host "ERROR: Virtual environment python not found at $VenvPython" -ForegroundColor Red
    Write-Host "       Try running with -Clean to recreate the venv." -ForegroundColor Red
    exit 1
}

. $VenvActivate
Write-Host "  Python: $(python --version)"
Write-Host "  Path:   $(Get-Command python | Select-Object -ExpandProperty Source)"

# ---- Step 3: Install build + test dependencies ------------------------------

Write-Host ""
Write-Host "Installing dependencies (scikit-build-core, pytest, numpy)..." -ForegroundColor Yellow
uv pip install scikit-build-core pytest numpy

# ---- Step 4: Install the package in editable mode ---------------------------

Write-Host ""
Write-Host "Installing dal-python in editable mode..." -ForegroundColor Yellow
Write-Host "  DAL_DIR=$DalDir"

$env:DAL_DIR = $DalDir

uv pip install `
    --no-build-isolation `
    --config-settings=cmake.define.DAL_DIR="$DalDir" `
    -e ".[test]"

Write-Host "[OK] Package installed" -ForegroundColor Green

# ---- Step 5: Verify import works --------------------------------------------

Write-Host ""
Write-Host "Verifying import..." -ForegroundColor Yellow
$importResult = python -c "import dal; print(f'  dal {dal.__version__} loaded successfully')" 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "[OK] Import successful" -ForegroundColor Green
    Write-Host $importResult
}
else {
    Write-Host "ERROR: Failed to import dal module" -ForegroundColor Red
    Write-Host $importResult
    exit 1
}

# ---- Step 6: Run tests ------------------------------------------------------

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host " Running tests" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

$pytestArgs = @("tests/", "-v")
if ($PytestArgs) {
    $pytestArgs += $PytestArgs
}

python -m pytest @pytestArgs