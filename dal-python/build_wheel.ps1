# Build Python wheel package for dal-python
#
# This script builds a binary wheel (.whl) that contains the compiled _dal extension.
# The wheel can be installed without requiring compilation or the C++ source code.
#
# Prerequisites:
# - C++ library must be installed (run ..\build_windows.bat)
# - uv must be installed
# - CPython 3.9-3.13 with development headers
# - Visual Studio 2022 with C++ workload
#
# Usage:
#   .\build_wheel.ps1              # Build wheel for current platform
#   .\build_wheel.ps1 -Python 3.9  # Select an exact supported CPython
#   .\build_wheel.ps1 -Clean       # Clean build artifacts before building

param(
    [switch]$Clean,
    [switch]$Help,
    [string]$Python,
    [string]$DalInstallPrefix
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
    Write-Output "Usage: .\build_wheel.ps1 [OPTIONS]"
    Write-Output ""
    Write-Output "Build Python wheel package for dal-python"
    Write-Output ""
    Write-Output "Options:"
    Write-Output "  -Clean         Clean build artifacts before building"
    Write-Output "  -Python <minor> Select CPython 3.9, 3.10, 3.11, 3.12, or 3.13"
    Write-Output "  -DalInstallPrefix <path>  Installed DAL prefix"
    Write-Output "  -Help          Show this help message"
    exit 0
}

# Check prerequisites
Write-Output "Checking prerequisites..."
if (-not (Get-Command uv -ErrorAction SilentlyContinue)) {
    Write-Output "Error: uv is not installed"
    Write-Output "Install uv: pip install uv    or    curl -LsSf https://astral.sh/uv/install.sh | sh"
    exit 1
}

$LibPublic = Join-Path $DalInstallPrefix "lib\dal_public.lib"
$LibCpp = Join-Path $DalInstallPrefix "lib\dal_cpp.lib"

if (-not ((Test-Path $LibPublic) -and (Test-Path $LibCpp))) {
    Write-Output "Error: DAL libraries not found in $DalInstallPrefix\lib\"
    Write-Output "Build and install DAL first:"
    Write-Output ""
    Write-Output "  cd $DalDir"
    Write-Output "  .\build_windows.bat"
    Write-Output ""
    exit 1
}

Write-Output "  Prerequisites satisfied"

# Clean if requested
$VenvDir = Join-Path $ScriptDir ".venv"
if ($Clean) {
    Write-Output "Cleaning build artifacts..."
    if (Test-Path (Join-Path $ScriptDir "build")) { Remove-Item -Recurse -Force (Join-Path $ScriptDir "build") }
    if (Test-Path (Join-Path $ScriptDir "dist")) { Remove-Item -Recurse -Force (Join-Path $ScriptDir "dist") }
    if (Test-Path $VenvDir) { Remove-Item -Recurse -Force $VenvDir }
    Get-ChildItem -Path $ScriptDir -Directory -Filter "*.egg-info" | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    Get-ChildItem -Path $ScriptDir -Directory -Filter "__pycache__" -Recurse | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    Write-Output "  Clean complete"
}

# Create build environment
Write-Output "Creating build environment..."
$VenvPython = Join-Path $VenvDir "Scripts\python.exe"
if (-not (Test-Path $VenvPython)) {
    if (Test-Path $VenvDir) {
        Write-Error "build_wheel.ps1: environment '$VenvDir' has no executable Python; rerun with -Clean to recreate it"
    }
    $PythonRequest = if ($PythonRequested) { $Python } else { ">=3.9,<3.14" }
    uv venv $VenvDir --python $PythonRequest
}
$CompatArgs = @(
    (Join-Path $ScriptDir "scripts\python_compat.py"),
    "--entry-point", "dal-python/build_wheel.ps1",
    "--environment", $VenvDir,
    "--remediation", "rerun with -Clean to recreate $VenvDir"
)
if ($PythonRequested) { $CompatArgs += @("--requested", $Python) }
& $VenvPython @CompatArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$VenvActivate = Join-Path $VenvDir "Scripts\Activate.ps1"
. $VenvActivate
Write-Output "  Build environment ready"

# Install build dependencies
Write-Output "Installing build dependencies..."
uv pip install -q "scikit-build-core==1.0.3" cmake ninja build
Write-Output "  Build dependencies installed"

# Build wheel
Write-Output "Building wheel..."
$env:DAL_INSTALL_PREFIX = $DalInstallPrefix

uv build --wheel --no-build-isolation --config-settings=cmake.define.DAL_INSTALL_PREFIX="$DalInstallPrefix"

# Find the built wheel
$DistDir = Join-Path $ScriptDir "dist"
$WheelFile = Get-ChildItem -Path $DistDir -Filter "*.whl" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $WheelFile) {
    Write-Output "Error: No wheel file found in dist\"
    exit 1
}

Write-Output "  Wheel built: $($WheelFile.Name)"

# Display wheel info
Write-Output ""
Write-Output "Build complete!"
Write-Output "Wheel location: $($WheelFile.FullName)"
Write-Output "Wheel size: $([math]::Round($WheelFile.Length / 1MB, 2)) MB"
Write-Output ""

# Show installation instructions
Write-Output "To install the wheel:"
Write-Output "  pip install $($WheelFile.FullName)"
Write-Output ""
Write-Output "Or with uv:"
Write-Output "  uv pip install $($WheelFile.FullName)"
Write-Output ""
