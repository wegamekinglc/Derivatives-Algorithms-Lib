#Requires -Version 7.0
# Start the DAL web UI (FastAPI backend + React/Vite frontend) on Windows.
#
# Usage:
#   pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/start.ps1
#
# What it does:
#   1. Verifies prerequisites (python 3.13+, uv, node, npm, curl).
#   2. Reads the backend port from dal-web/frontend/vite.config.ts.
#   3. Checks that both ports (backend + 5173) are free.
#   4. Verifies the native DAL Python package is installed.
#   5. Starts the backend (uvicorn) in the background.
#   6. Starts the frontend (vite) in the background.
#   7. Waits for both to be ready, then runs a smoke test.
#   8. Prints the URLs.
#
# Logs are written under each server directory: .server.log (stdout) and
# .server.log.err (stderr) for both dal-web/backend/ and dal-web/frontend/.
# PIDs are stored in .server.pid next to the respective server directory, so
# stop.ps1 can kill them cleanly.
#
# Exit codes:
#   0  both services started successfully
#   1  prerequisites missing or ports already in use
#   2  backend failed to start
#   3  frontend failed to start

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Resolve paths (this script lives in dal-web/scripts/)
# ---------------------------------------------------------------------------
$ScriptDir    = $PSScriptRoot
$WebRoot      = Split-Path -Parent $ScriptDir        # dal-web
$BackendDir   = Join-Path $WebRoot 'backend'
$FrontendDir  = Join-Path $WebRoot 'frontend'
$FrontendPort = 5173

$BackendPidFile  = Join-Path $BackendDir  '.server.pid'
$BackendLogFile  = Join-Path $BackendDir  '.server.log'
$FrontendPidFile = Join-Path $FrontendDir '.server.pid'
$FrontendLogFile = Join-Path $FrontendDir '.server.log'

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
# Status output uses Write-Output with embedded ANSI color rather than Write-Host
# or [Console]::WriteLine -- both trip PSScriptAnalyzer's PSAvoidUsingWriteHost,
# while Write-Output keeps the color and stays green for these interactive scripts.
$script:AnsiReset  = [char]27 + '[0m'
$script:AnsiGreen  = [char]27 + '[32m'
$script:AnsiYellow = [char]27 + '[33m'
$script:AnsiRed    = [char]27 + '[31m'

function Write-Colored {
    param([string]$Prefix, [string]$Color, [string]$Msg)
    Write-Output "$Color$Prefix$Msg$script:AnsiReset"
}
function Write-Info  { param([string]$Msg) Write-Colored '[info]  ' $script:AnsiGreen  $Msg }
function Write-Warn  { param([string]$Msg) Write-Colored '[warn]  ' $script:AnsiYellow $Msg }
function Write-ErrLn { param([string]$Msg) Write-Colored '[error] ' $script:AnsiRed    $Msg }

function Test-PortFree {
    param([int]$Port)
    $c = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
    $null -eq $c -or $c.Count -eq 0
}

function Read-BackendPort {
    param([string]$ViteConfigPath, [int]$Default = 8001)
    if (-not (Test-Path $ViteConfigPath)) { return $Default }
    $content = Get-Content -Raw $ViteConfigPath
    # Matches lines like: target: "http://127.0.0.1:<port>"
    if ($content -match 'target\s*:\s*"http://127\.0\.0\.1:(\d+)"') {
        return [int]$Matches[1]
    }
    $Default
}

function Get-Health {
    param([string]$Uri, [int]$TimeoutSec = 3)
    try {
        Invoke-WebRequest -UseBasicParsing -Uri $Uri -TimeoutSec $TimeoutSec
    } catch {
        $null
    }
}

# Read backend port from vite.config.ts proxy target.
$BackendPort = Read-BackendPort -ViteConfigPath (Join-Path $FrontendDir 'vite.config.ts')

# ---------------------------------------------------------------------------
# 1. Prerequisites
# ---------------------------------------------------------------------------
Write-Info "Checking prerequisites..."

function Assert-Command {
    param([string]$Name)
    if ($null -eq (Get-Command $Name -ErrorAction SilentlyContinue)) {
        Write-ErrLn "$Name is not installed. Please install it and retry."
        return $false
    }
    $true
}

$failed = $false
foreach ($c in 'python','uv','node','npm','curl') {
    if (-not (Assert-Command $c)) { $failed = $true }
}
if ($failed) { exit 1 }

$pyVer   = & python -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')"
$pyParts = $pyVer.Split('.')
$pyMajor = [int]$pyParts[0]
$pyMinor = [int]$pyParts[1]
if ($pyMajor -lt 3 -or ($pyMajor -eq 3 -and $pyMinor -lt 13)) {
    Write-ErrLn "Python >= 3.13 is required (found $pyVer)"
    exit 1
}
$uvVer  = (& uv --version).Split(' ')[1]
$nodeV  = & node --version
$npmVer = & npm --version
Write-Info "  python $pyVer, uv $uvVer, node $nodeV, npm $npmVer"

# ---------------------------------------------------------------------------
# 2. Check ports are free
# ---------------------------------------------------------------------------
Write-Info "Checking ports (backend=$BackendPort, frontend=$FrontendPort)..."

if (-not (Test-PortFree $BackendPort)) {
    Write-ErrLn "Port $BackendPort is already in use. Run dal-web/scripts/stop.ps1 first, or pick a different port in frontend/vite.config.ts."
    exit 1
}
if (-not (Test-PortFree $FrontendPort)) {
    Write-ErrLn "Port $FrontendPort is already in use. Run dal-web/scripts/stop.ps1 first."
    exit 1
}

# ---------------------------------------------------------------------------
# 3. Backend setup
# ---------------------------------------------------------------------------
Write-Info "Installing backend dependencies (uv sync)..."
Push-Location $BackendDir
try {
    uv sync --quiet --inexact
    if ($LASTEXITCODE -ne 0) {
        Write-ErrLn "Backend dependency installation failed."
        exit 1
    }

    Write-Info "Checking native DAL Python package..."
    $nativeOutput = @(& uv run --no-sync python -m app.native_runtime 2>&1)
    if ($LASTEXITCODE -ne 0) {
        Write-ErrLn "Native DAL preflight failed:"
        $nativeOutput | ForEach-Object { Write-Output $_ }
        exit 1
    }
} finally { Pop-Location }

Write-Info "Starting backend on port $BackendPort..."
# uv run launches uvicorn; with --reload uvicorn spawns a reloader parent plus
# a worker child. We capture the parent PID and rely on stop.ps1's process-tree
# walk plus port-based fallback to clean up the worker holding the socket.
$backendProc = Start-Process -FilePath 'uv' `
    -ArgumentList @('run','--no-sync','python','-m','uvicorn','app.main:app','--reload','--host','127.0.0.1','--port',"$BackendPort",'--log-config','log_config.json') `
    -WorkingDirectory $BackendDir `
    -WindowStyle Hidden `
    -RedirectStandardOutput $BackendLogFile `
    -RedirectStandardError  "$BackendLogFile.err" `
    -PassThru
$BackendPid = $backendProc.Id
Set-Content -Path $BackendPidFile -Value $BackendPid -NoNewline
Write-Info "  backend PID $BackendPid, log: backend/.server.log (+ .server.log.err)"

# Wait for backend to accept connections (up to 20s).
Write-Info "Waiting for backend health check..."
$backendUp = $false
for ($i = 0; $i -lt 40; $i++) {
    if (Get-Health "http://127.0.0.1:$BackendPort/api/health") {
        Write-Info "  backend is up"
        $backendUp = $true
        break
    }
    if ($backendProc.HasExited) {
        Write-ErrLn "Backend process exited before becoming healthy. Check $BackendLogFile :"
        if (Test-Path $BackendLogFile) { Get-Content $BackendLogFile -Tail 20 | ForEach-Object { Write-Output $_ } }
        Remove-Item $BackendPidFile -ErrorAction SilentlyContinue
        exit 2
    }
    Start-Sleep -Milliseconds 500
}
if (-not $backendUp) {
    Write-ErrLn "Backend did not become healthy within 20s. Check $BackendLogFile."
    Stop-Process -Id $BackendPid -Force -ErrorAction SilentlyContinue
    Remove-Item $BackendPidFile -ErrorAction SilentlyContinue
    exit 2
}

# ---------------------------------------------------------------------------
# 4. Frontend setup
# ---------------------------------------------------------------------------
Write-Info "Installing frontend dependencies (npm install)..."
Push-Location $FrontendDir
try { npm install --silent --no-audit --no-fund *> $null } finally { Pop-Location }

Write-Info "Starting frontend on port $FrontendPort..."
# Run vite directly rather than `npm run dev` so the PID we save is the actual
# node process holding the port. npm wraps the script in a parent process that
# doesn't forward termination to its child, which previously left orphaned node
# processes behind on stop. On Windows we invoke vite's JS entry via node so the
# recorded PID is the single port-holding node process (and stop.ps1 walks its
# children as defense in depth).
$viteJs   = Join-Path $FrontendDir 'node_modules\vite\bin\vite.js'
$viteShim = Join-Path $FrontendDir 'node_modules\.bin\vite.cmd'
if (Test-Path $viteJs) {
    $frontendProc = Start-Process -FilePath 'node' `
        -ArgumentList @("`"$viteJs`"") `
        -WorkingDirectory $FrontendDir `
        -WindowStyle Hidden `
        -RedirectStandardOutput $FrontendLogFile `
        -RedirectStandardError  "$FrontendLogFile.err" `
        -PassThru
} elseif (Test-Path $viteShim) {
    $frontendProc = Start-Process -FilePath 'cmd.exe' `
        -ArgumentList @('/c', "`"$viteShim`"") `
        -WorkingDirectory $FrontendDir `
        -WindowStyle Hidden `
        -RedirectStandardOutput $FrontendLogFile `
        -RedirectStandardError  "$FrontendLogFile.err" `
        -PassThru
} else {
    Write-ErrLn "vite binary not found under $FrontendDir\node_modules. Run npm install first."
    Stop-Process -Id $BackendPid -Force -ErrorAction SilentlyContinue
    Remove-Item $BackendPidFile -ErrorAction SilentlyContinue
    exit 3
}
$FrontendPid = $frontendProc.Id
Set-Content -Path $FrontendPidFile -Value $FrontendPid -NoNewline
Write-Info "  frontend PID $FrontendPid, log: frontend/.server.log (+ .server.log.err)"

# Wait for frontend to accept connections (up to 30s).
Write-Info "Waiting for frontend to be ready..."
$frontendUp = $false
for ($i = 0; $i -lt 60; $i++) {
    if (Get-Health "http://localhost:$FrontendPort") {
        Write-Info "  frontend is up"
        $frontendUp = $true
        break
    }
    if ($frontendProc.HasExited) {
        Write-ErrLn "Frontend process exited before becoming healthy. Check $FrontendLogFile :"
        if (Test-Path $FrontendLogFile) { Get-Content $FrontendLogFile -Tail 20 | ForEach-Object { Write-Output $_ } }
        Remove-Item $FrontendPidFile -ErrorAction SilentlyContinue
        Stop-Process -Id $BackendPid -Force -ErrorAction SilentlyContinue
        Remove-Item $BackendPidFile -ErrorAction SilentlyContinue
        exit 3
    }
    Start-Sleep -Milliseconds 500
}
if (-not $frontendUp) {
    Write-ErrLn "Frontend did not become ready within 30s. Check $FrontendLogFile."
    Stop-Process -Id $FrontendPid -Force -ErrorAction SilentlyContinue
    Remove-Item $FrontendPidFile -ErrorAction SilentlyContinue
    Stop-Process -Id $BackendPid -Force -ErrorAction SilentlyContinue
    Remove-Item $BackendPidFile -ErrorAction SilentlyContinue
    exit 3
}

# ---------------------------------------------------------------------------
# 5. Smoke test -- proxy should forward /api to the backend
# ---------------------------------------------------------------------------
Write-Info "Smoke test (frontend -> backend proxy)..."
$smoke = Get-Health "http://localhost:$FrontendPort/api/health"
if ($null -eq $smoke) {
    Write-Warn "Frontend is up but /api proxy is not forwarding to the backend. Check vite.config.ts."
} else {
    Write-Info "  /api/health via proxy -> $($smoke.Content)"
}

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------
Write-Output ''
Write-Output "$script:AnsiGreen[ok] DAL web UI is running$script:AnsiReset"
Write-Output ''
Write-Output "  Frontend:  http://localhost:$FrontendPort"
Write-Output "  Backend:   http://127.0.0.1:$BackendPort"
Write-Output "  API docs:  http://127.0.0.1:$BackendPort/docs"
Write-Output "  Backend:   PID $BackendPid"
Write-Output "  Frontend:  PID $FrontendPid"
Write-Output ''
Write-Output "To stop:     pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/stop.ps1"
Write-Output "Logs:        backend/{.server.log, .server.log.err}, frontend/{.server.log, .server.log.err}"
exit 0
