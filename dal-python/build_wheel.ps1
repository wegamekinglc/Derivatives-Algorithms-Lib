# Build Python wheel package for dal-python
#
# This script builds a binary wheel (.whl) that contains the compiled _dal extension.
# The wheel can be installed without requiring compilation or the C++ source code.
#
# Prerequisites:
# - C++ library must be built (lib\dal_public.lib and lib\dal_cpp.lib)
# - uv must be installed
# - Python 3.10+ with development headers
# - Visual Studio 2022 with C++ workload
#
# Usage:
#   .\build_wheel.ps1              # Build wheel for current platform
#   .\build_wheel.ps1 -Clean       # Clean build artifacts before building

param(
    [switch]$Clean,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$DalDir = $env:DAL_DIR
if (-not $DalDir) {
    $DalDir = Split-Path -Parent $ScriptDir
}

if ($Help) {
    Write-Host "Usage: .\build_wheel.ps1 [OPTIONS]"
    Write-Host ""
    Write-Host "Build Python wheel package for dal-python"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Clean         Clean build artifacts before building"
    Write-Host "  -Help          Show this help message"
    exit 0
}

# Check prerequisites
Write-Host "Checking prerequisites..." -ForegroundColor Yellow

if (-not (Get-Command uv -ErrorAction SilentlyContinue)) {
    Write-Host "Error: uv is not installed" -ForegroundColor Red
    Write-Host "Install uv: pip install uv    or    curl -LsSf https://astral.sh/uv/install.sh | sh"
    exit 1
}

$LibPublic = Join-Path $DalDir "lib\dal_public.lib"
$LibCpp = Join-Path $DalDir "lib\dal_cpp.lib"

if (-not ((Test-Path $LibPublic) -and (Test-Path $LibCpp))) {
    Write-Host "Error: DAL libraries not found in $DalDir\lib\" -ForegroundColor Red
    Write-Host "Build the C++ library first using the top-level CMakeLists.txt:" -ForegroundColor Red
    Write-Host ""
    Write-Host "  cd $DalDir"
    Write-Host "  mkdir build && cd build"
    Write-Host "  cmake .. -DDAL_BUILD_PUBLIC=ON -DDAL_BUILD_PYTHON=ON"
    Write-Host "  cmake --build . --config Release"
    Write-Host "  cmake --install . --config Release"
    Write-Host ""
    exit 1
}

Write-Host "  Prerequisites satisfied" -ForegroundColor Green

# Clean if requested
if ($Clean) {
    Write-Host "Cleaning build artifacts..." -ForegroundColor Yellow
    if (Test-Path (Join-Path $ScriptDir "build")) { Remove-Item -Recurse -Force (Join-Path $ScriptDir "build") }
    if (Test-Path (Join-Path $ScriptDir "dist")) { Remove-Item -Recurse -Force (Join-Path $ScriptDir "dist") }
    Get-ChildItem -Path $ScriptDir -Directory -Filter "*.egg-info" | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    Get-ChildItem -Path $ScriptDir -Directory -Filter "__pycache__" -Recurse | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "  Clean complete" -ForegroundColor Green
}

# Create build environment
Write-Host "Creating build environment..." -ForegroundColor Yellow
$VenvDir = Join-Path $ScriptDir ".venv"
if (-not (Test-Path $VenvDir)) {
    uv venv $VenvDir --python ">=3.10"
}
$VenvActivate = Join-Path $VenvDir "Scripts\Activate.ps1"
. $VenvActivate
Write-Host "  Build environment ready" -ForegroundColor Green

# Install build dependencies
Write-Host "Installing build dependencies..." -ForegroundColor Yellow
uv pip install -q scikit-build-core cmake ninja swig build
Write-Host "  Build dependencies installed" -ForegroundColor Green

# Build wheel
Write-Host "Building wheel..." -ForegroundColor Yellow
$env:DAL_DIR = $DalDir

uv build --wheel --no-build-isolation --config-settings=cmake.define.DAL_DIR="$DalDir"

# Find the built wheel
$DistDir = Join-Path $ScriptDir "dist"
$WheelFile = Get-ChildItem -Path $DistDir -Filter "*.whl" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $WheelFile) {
    Write-Host "Error: No wheel file found in dist\" -ForegroundColor Red
    exit 1
}

Write-Host "  Wheel built: $($WheelFile.Name)" -ForegroundColor Green

# Display wheel info
Write-Host ""
Write-Host "Build complete!" -ForegroundColor Green
Write-Host "Wheel location: $($WheelFile.FullName)"
Write-Host "Wheel size: $([math]::Round($WheelFile.Length / 1MB, 2)) MB"
Write-Host ""

# Show installation instructions
Write-Host "To install the wheel:"
Write-Host "  pip install $($WheelFile.FullName)"
Write-Host ""
Write-Host "Or with uv:"
Write-Host "  uv pip install $($WheelFile.FullName)"
Write-Host ""