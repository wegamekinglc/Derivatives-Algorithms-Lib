param([string]$Runner = (Join-Path $PSScriptRoot 'run-script-fix-settings.ps1'))

$ErrorActionPreference = 'Stop'
$tokens = $null
$parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($Runner, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count -ne 0) { throw ($parseErrors | Out-String) }
$helper = $ast.Find({ param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Remove-ComReference'
}, $true)
if ($null -eq $helper) { throw 'Missing COM cleanup helper' }
. ([scriptblock]::Create($helper.Extent.Text))

$reference = New-Object -ComObject Scripting.Dictionary
try {
    $reference.Add('retained', 42)
    Remove-ComReference $reference -WhatIf -Confirm:$false
    if ($reference.Item('retained') -ne 42) { throw 'WhatIf changed the COM object' }
} finally {
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($reference)
}
Write-Output 'PASS: WhatIf preserves a live COM reference'

$plain = [pscustomobject]@{ Value = 42 }
foreach ($value in @($null, 42, 'text', $plain)) {
    if (@(Remove-ComReference $value -Confirm:$false -WhatIf:$false).Count -ne 0) {
        throw 'Cleanup must not return pipeline output'
    }
}
if ($plain.Value -ne 42) { throw 'Cleanup changed a non-COM object' }
Write-Output 'PASS: null and non-COM cleanup are silent no-ops'

$calls = @($ast.FindAll({ param($node)
    $node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -eq 'Remove-ComReference'
}, $true))
if ($calls.Count -ne 8) { throw 'Expected all eight runner cleanup calls' }
foreach ($call in $calls) {
    $reference = New-Object -ComObject Scripting.Dictionary
    $reference.Add('retained', 42)
    Set-Variable -Name $call.CommandElements[1].VariablePath.UserPath -Value $reference
    $savedConfirm = $ConfirmPreference
    $savedWhatIf = $WhatIfPreference
    try {
        $ConfirmPreference = 'Low'
        $WhatIfPreference = $true
        . ([scriptblock]::Create($call.Extent.Text))
        $released = $false
        try { $null = $reference.Count } catch [Runtime.InteropServices.InvalidComObjectException] { $released = $true }
        if (-not $released) { throw "Runner cleanup skipped release: $($call.Extent.Text)" }
    } finally {
        $ConfirmPreference = $savedConfirm
        $WhatIfPreference = $savedWhatIf
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($reference)
    }
}
Write-Output 'PASS: all eight runner cleanup calls release COM under inherited WhatIf/Confirm preferences without prompting'
