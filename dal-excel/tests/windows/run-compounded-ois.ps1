[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Xll,
    [Parameter(Mandatory = $true)][string]$PythonEvidence,
    [Parameter(Mandatory = $true)][string]$BuildCache,
    [Parameter(Mandatory = $true)][string]$Output
)
$ErrorActionPreference = "Stop"
$sourceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$gitPath = (Get-Command git.exe -CommandType Application).Source
$pythonPath = (Get-Command python.exe -CommandType Application).Source

function Invoke-GitRead {
    param([string[]]$GitArguments)
    $result = & $gitPath -C $sourceRoot @GitArguments
    if ($LASTEXITCODE -ne 0) { throw "Git metadata read failed" }
    return $result
}

$head = ((Invoke-GitRead -GitArguments @("rev-parse", "HEAD")) -join "`n").Trim()
$tracked = Invoke-GitRead -GitArguments @("status", "--porcelain", "--untracked-files=no")
if ($tracked) { throw "Tracked source changes prevent a single-SHA acceptance claim" }
$manifest = @{
    source_head = $head
    source_root = $sourceRoot
    clean_tracked = $true
    git_executable = $gitPath
    dependencies = (Invoke-GitRead -GitArguments @("submodule", "status", "--recursive")) -join "`n"
    xll_sha256 = (Get-FileHash -LiteralPath $Xll -Algorithm SHA256).Hash
    build_cache_sha256 = (Get-FileHash -LiteralPath $BuildCache -Algorithm SHA256).Hash
}
$outputPath = [IO.Path]::GetFullPath($Output)
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($outputPath)) | Out-Null
$manifestPath = [IO.Path]::ChangeExtension($outputPath, "build.json")
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
$runner = Join-Path $PSScriptRoot "run-compounded-ois.py"
& $pythonPath $runner --xll $Xll --python-evidence $PythonEvidence --build-cache $BuildCache --build-manifest $manifestPath --output $outputPath
exit $LASTEXITCODE
