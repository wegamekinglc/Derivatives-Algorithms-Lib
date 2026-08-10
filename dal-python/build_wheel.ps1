# Build Python wheel package for dal-python
#
# This script builds a binary wheel (.whl) that contains the compiled _dal extension.
# The wheel can be installed without requiring compilation or the C++ source code.
#
# Prerequisites:
# - C++ library must be installed (run ..\build_windows.bat)
# - uv must be installed
# - CPython 3.10-3.13 with development headers
# - Visual Studio 2022 with C++ workload
#
# Usage:
#   .\build_wheel.ps1              # Build wheel for current platform
#   .\build_wheel.ps1 -Clean       # Clean build artifacts before building

param(
    [switch]$Clean,
    [switch]$Help,
    [string]$DalInstallPrefix
)

$ErrorActionPreference = "Stop"

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
if ($Clean) {
    Write-Output "Cleaning build artifacts..."
    if (Test-Path (Join-Path $ScriptDir "build")) { Remove-Item -Recurse -Force (Join-Path $ScriptDir "build") }
    if (Test-Path (Join-Path $ScriptDir "dist")) { Remove-Item -Recurse -Force (Join-Path $ScriptDir "dist") }
    Get-ChildItem -Path $ScriptDir -Directory -Filter "*.egg-info" | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    Get-ChildItem -Path $ScriptDir -Directory -Filter "__pycache__" -Recurse | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    Write-Output "  Clean complete"
}

# Create build environment
Write-Output "Creating build environment..."
$VenvDir = Join-Path $ScriptDir ".venv"
if (-not (Test-Path $VenvDir)) {
    uv venv $VenvDir --python ">=3.10,<3.14"
}
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
