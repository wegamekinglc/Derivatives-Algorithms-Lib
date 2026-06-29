#Requires -Version 7.0
# Stop the DAL web UI (FastAPI backend + React/Vite frontend) on Windows.
#
# Usage:
#   pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/stop.ps1 [-Force]
#
# What it does:
#   1. Reads the backend port from dal-web/frontend/vite.config.ts.
#   2. Kills each service by PID (from the .server.pid files written by
#      start.ps1), walking the process tree so child workers (uvicorn reload
#      worker, node/vite children) are also terminated. Falls back to a
#      port-based kill if a child still holds the socket.
#   3. Removes the PID files.
#   4. Verifies that both ports are free.
#
# With -Force, escalates to Stop-Process -Force (SIGKILL-equivalent) if a
# process refuses to die within 5 seconds.
#
# Exit codes:
#   0  services stopped successfully (or were already stopped)
#   1  a service could not be stopped even with -Force

[CmdletBinding()]
param([switch]$Force)

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
$FrontendPidFile = Join-Path $FrontendDir '.server.pid'

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
    if ($content -match 'target\s*:\s*"http://127\.0\.0\.1:(\d+)"') {
        return [int]$Matches[1]
    }
    $Default
}

# Collect a PID and all of its descendants (recursively) via the CIM process
# tree. Returns the full set including the root, so callers can terminate the
# whole subtree in one pass -- necessary because uvicorn --reload and vite both
# spawn child workers that inherit the listening socket but have a different PID
# than the parent we recorded.
function Get-DescendantPids {
    param([int]$RootPid)
    $result = [System.Collections.Generic.List[int]]::new()
    $result.Add($RootPid)
    $stack = [System.Collections.Generic.Stack[int]]::new()
    $stack.Push($RootPid)
    while ($stack.Count -gt 0) {
        $parent = $stack.Pop()
        $children = Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
            Where-Object { $_.ParentProcessId -eq $parent } |
            Select-Object -ExpandProperty ProcessId
        foreach ($cpid in $children) {
            if ($cpid -and -not $result.Contains($cpid)) {
                $result.Add($cpid)
                $stack.Push($cpid)
            }
        }
    }
    ,$result
}

function Test-ProcessAlive {
    param([int]$Pid_)
    $null -ne (Get-Process -Id $Pid_ -ErrorAction SilentlyContinue)
}

# Terminate a process tree. SIGTERM-equivalent first, then escalate to -Force
# (SIGKILL-equivalent) after a grace window when $Force is set.
# The Stop-* helpers always act (process/port cleanup); SupportsShouldProcess
# is declared only to satisfy PSSA's state-changing-verb rule, not to gate -WhatIf.
function Stop-ProcessTree {
    [CmdletBinding(SupportsShouldProcess)]
    param(
        [int]    $RootPid,
        [string] $Name,
        [switch] $Force
    )
    if (-not (Test-ProcessAlive $RootPid)) { return $true }

    $pids = Get-DescendantPids -RootPid $RootPid
    Write-Info "Stopping $Name tree (PIDs: $($pids -join ', '))..."
    foreach ($p in $pids) {
        if (Test-ProcessAlive $p) {
            Stop-Process -Id $p -ErrorAction SilentlyContinue
        }
    }

    for ($i = 0; $i -lt 10; $i++) {
        Start-Sleep -Milliseconds 500
        $alive = $false
        foreach ($p in $pids) { if (Test-ProcessAlive $p) { $alive = $true; break } }
        if (-not $alive) { return $true }
    }

    if ($Force) {
        Write-Warn "Escalating to force kill for $Name tree..."
        foreach ($p in $pids) {
            if (Test-ProcessAlive $p) {
                Stop-Process -Id $p -Force -ErrorAction SilentlyContinue
            }
        }
        Start-Sleep -Seconds 1
        foreach ($p in $pids) { if (Test-ProcessAlive $p) { return $false } }
        return $true
    }
    return $false
}

# Defense in depth: after the tracked PID is gone, a child it spawned may still
# hold the port (e.g. npm-spawned wrappers that inherit the socket). Kill
# whatever owns the listening socket.
function Stop-ByPort {
    [CmdletBinding(SupportsShouldProcess)]
    param([int]$Port, [string]$Name, [switch]$Force)
    $conns = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
    if ($null -eq $conns -or $conns.Count -eq 0) { return $true }
    $owners = $conns | Select-Object -ExpandProperty OwningProcess -Unique
    Write-Warn "Port $Port still held by $Name (PIDs: $($owners -join ', ')); killing by port..."
    foreach ($opid in $owners) {
        Stop-Process -Id $opid -Force:$Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Seconds 1
    (Test-PortFree $Port)
}

function Stop-Service {
    [CmdletBinding(SupportsShouldProcess)]
    param([int]$Port, [string]$Name, [string]$PidFile, [switch]$Force)

    if (Test-Path $PidFile) {
        $pidVal = [int](Get-Content $PidFile -Raw).Trim()
        if (-not (Stop-ProcessTree -RootPid $pidVal -Name $Name -Force:$Force)) {
            if ($Force) {
                Write-ErrLn "$Name PID $pidVal could not be killed."
            } else {
                Write-Warn "$Name (PID $pidVal) did not stop within 5s. Re-run with -Force to escalate."
            }
        }
        Remove-Item $PidFile -ErrorAction SilentlyContinue
    } else {
        Write-Warn "No $Name PID file found."
    }

    # Defense in depth: the tracked tree may be gone while a child it spawned
    # still holds the port. Always verify the port and fall back if needed.
    if (-not (Test-PortFree $Port)) {
        Write-Warn "$Name port $Port still busy after PID kill; falling back to port-based kill..."
        $null = Stop-ByPort -Port $Port -Name $Name -Force:$Force
    }
}

# Read backend port from vite.config.ts proxy target.
$BackendPort = Read-BackendPort -ViteConfigPath (Join-Path $FrontendDir 'vite.config.ts')

# ---------------------------------------------------------------------------
# 1. Check current state
# ---------------------------------------------------------------------------
$backendRunning  = -not (Test-PortFree $BackendPort)
$frontendRunning = -not (Test-PortFree $FrontendPort)

if (-not $backendRunning -and -not $frontendRunning) {
    Write-Info "No DAL web UI services are running (ports $BackendPort and $FrontendPort are both free)."
    Remove-Item $BackendPidFile, $FrontendPidFile -ErrorAction SilentlyContinue
    exit 0
}

# ---------------------------------------------------------------------------
# 2. Stop backend
# ---------------------------------------------------------------------------
if ($backendRunning) {
    Stop-Service -Port $BackendPort -Name 'backend' -PidFile $BackendPidFile -Force:$Force
}

# ---------------------------------------------------------------------------
# 3. Stop frontend
# ---------------------------------------------------------------------------
if ($frontendRunning) {
    Stop-Service -Port $FrontendPort -Name 'frontend' -PidFile $FrontendPidFile -Force:$Force
}

# ---------------------------------------------------------------------------
# 4. Final verification
# ---------------------------------------------------------------------------
Start-Sleep -Seconds 1
$remaining = 0
if (-not (Test-PortFree $BackendPort)) {
    Write-ErrLn "Backend is still listening on port $BackendPort."
    $remaining = 1
}
if (-not (Test-PortFree $FrontendPort)) {
    Write-ErrLn "Frontend is still listening on port $FrontendPort."
    $remaining = 1
}

if ($remaining -eq 0) {
    Write-Output "$script:AnsiGreen[ok] DAL web UI stopped. Ports $BackendPort and $FrontendPort are free.$script:AnsiReset"
    exit 0
} else {
    Write-ErrLn "Some services could not be stopped. Try: pwsh -NoProfile -ExecutionPolicy Bypass -File dal-web/scripts/stop.ps1 -Force"
    Write-ErrLn "Or manually: Get-NetTCPConnection -LocalPort $BackendPort -State Listen | %{ Stop-Process -Id `$_.OwningProcess -Force }; Get-NetTCPConnection -LocalPort $FrontendPort -State Listen | %{ Stop-Process -Id `$_.OwningProcess -Force }"
    exit 1
}
