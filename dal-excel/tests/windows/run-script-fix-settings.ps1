param(
    [Parameter(Mandatory=$true)][string]$Xll,
    [string]$Manifest = (Join-Path $PSScriptRoot '../../examples/010.script_fix_settings.json'),
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$SourceSha = 'working-tree',
    [string]$SourceTree = 'working-tree'
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path $OutputDirectory).Path
$Xll = (Resolve-Path $Xll).Path
$fixture = Get-Content -Raw -Encoding UTF8 $Manifest | ConvertFrom-Json
$existing = @(Get-Process EXCEL -ErrorAction SilentlyContinue | ForEach-Object { $_.Id })
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class ExcelProcess {
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
}
'@
$excel = $null
$book = $null
$ownPid = 0
$sheets = @{}
$results = [ordered]@{ utc=[DateTime]::UtcNow.ToString('o'); sourceSha=$SourceSha; sourceTree=$SourceTree;
    xll=$Xll; xllSha256=(Get-FileHash $Xll -Algorithm SHA256).Hash; manifestSha256=(Get-FileHash $Manifest -Algorithm SHA256).Hash;
    registered=$false; passed=$false; assertions=0; outputs=[ordered]@{} }

function Check([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    $script:results.assertions++
}
function Release-Com($Object) {
    if ($null -ne $Object -and [Runtime.InteropServices.Marshal]::IsComObject($Object)) {
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($Object)
    }
}
function Put([string]$Sheet, [string]$Address, $Value) {
    $range = $script:sheets[$Sheet].Range($Address)
    try {
        if ($Value -is [string] -and $Value.StartsWith('=')) {
            $range.Formula2 = $Value
        } elseif ($Value -is [string]) {
            $range.NumberFormat = '@'
            $range.Value2 = $Value
        } else { $range.Value2 = [double]$Value }
    } finally { Release-Com $range }
}
function Calculate([string]$Sheet, [string]$Address) {
    $range = $script:sheets[$Sheet].Range($Address)
    try { $range.Calculate() } finally { Release-Com $range }
}
function Output([string]$Sheet, [string]$Address) {
    $range = $script:sheets[$Sheet].Range($Address)
    $spill = $null
    try {
        try { $spill = $range.SpillingToRange } catch { $spill = $null }
        if ($null -eq $spill) { $value = $range.Value2; $rows = 1; $cols = 1 }
        else { $value = $spill.Value2; $rows = $spill.Rows.Count; $cols = $spill.Columns.Count }
        $data = @()
        for ($i=0; $i -lt $rows; $i++) {
            $row = @()
            for ($j=0; $j -lt $cols; $j++) {
                if ($value -is [Array]) { $row += $value.GetValue($i+1,$j+1) } else { $row += $value }
            }
            $data += ,$row
        }
        $out = [ordered]@{rows=$rows; columns=$cols; values=$data}
        $script:results.outputs["$Sheet!$Address"] = $out
        return $out
    } finally { Release-Com $spill; Release-Com $range }
}
function Price([string]$Address) {
    $out = Output 'Values' $Address
    Check ($out.columns -eq 2) "Values!$Address must spill two columns: $($out | ConvertTo-Json -Compress -Depth 8)"
    $values = @{}
    foreach ($row in $out.values) {
        Check ($row[0] -is [string] -and ($row[0] -eq 'PV' -or $row[0].StartsWith('d_'))) "Invalid price key at $Address"
        Check ($row[1] -is [double] -and -not [double]::IsNaN($row[1]) -and -not [double]::IsInfinity($row[1])) "Invalid numeric result at $Address"
        Check (-not $values.ContainsKey($row[0])) "Duplicate result key at $Address"
        $values[$row[0]] = $row[1]
    }
    return $values
}
function Near([double]$Actual, [double]$Expected, [double]$Tolerance, [string]$Label) {
    Check ([Math]::Abs($Actual-$Expected) -le $Tolerance) "$Label actual=$Actual expected=$Expected tolerance=$Tolerance"
}
function Diagnostic([string]$Address) {
    $out = Output 'Diagnostics' $Address
    Check ($out.columns -eq 1) "Diagnostic $Address must spill one column"
    $text = [Text.StringBuilder]::new()
    foreach ($row in $out.values) {
        Check ($row[0] -is [string] -and $row[0].Length -le 30000) "Invalid chunk at $Address"
        Check ($row[0] -notmatch '[^\x00-\x7F]') "Non-ASCII diagnostic at $Address"
        [void]$text.Append($row[0])
    }
    $raw = $text.ToString()
    [IO.File]::WriteAllText((Join-Path $script:OutputDirectory "diagnostic-$Address.json"), $raw, [Text.UTF8Encoding]::new($false))
    return ($raw | ConvertFrom-Json)
}

try {
    $excel = New-Object -ComObject Excel.Application
    [uint32]$createdPid = 0
    [void][ExcelProcess]::GetWindowThreadProcessId([IntPtr]$excel.Hwnd, [ref]$createdPid)
    Check ($existing -notcontains [int]$createdPid) 'COM reused an existing Excel process; refusing to change it'
    $ownPid = [int]$createdPid
    $results.excelPid = $ownPid
    $results.excelHwnd = $excel.Hwnd
    $results.excelVersion = $excel.Version
    $results.excelBuild = $excel.Build
    $excel.Visible = $false
    $excel.DisplayAlerts = $false
    $book = $excel.Workbooks.Add()
    $excel.Calculation = -4135
    $book.Date1904 = $false
    $results.registered = $excel.RegisterXLL($Xll)
    Check $results.registered 'RegisterXLL returned false'
    foreach ($name in @('Inputs','Control','Handles','Values','Diagnostics','Long')) {
        $sheet = $book.Worksheets.Add()
        $sheet.Name = $name
        $sheets[$name] = $sheet
    }
    foreach ($sheet in $fixture.sheets.PSObject.Properties) {
        foreach ($cell in $sheet.Value.PSObject.Properties) { Put $sheet.Name $cell.Name $cell.Value }
    }
    $sheets['Inputs'].Calculate()
    foreach ($address in @('B2','B3')) {
        Calculate 'Control' $address
        $control = Output 'Control' $address
        Check ($control.values[0][0] -is [bool] -or $control.values[0][0] -is [double]) "Control $address failed"
        $range = $sheets['Control'].Range($address)
        try { $range.Value2 = [double]$control.values[0][0] } finally { Release-Com $range }
    }
    foreach ($cell in $fixture.sheets.Handles.PSObject.Properties) {
        Calculate 'Handles' $cell.Name
        $out = Output 'Handles' $cell.Name
        $tag = $out.values[0][0]
        Check ($out.rows -eq 1 -and $out.columns -eq 1 -and $tag -is [string] -and $tag.Length -gt 0 -and -not $tag.StartsWith('#Error')) "Handle $($cell.Name) failed: $tag"
    }
    foreach ($cell in $fixture.sheets.Values.PSObject.Properties) { Calculate 'Values' $cell.Name }
    foreach ($entry in $fixture.expectedErrors.PSObject.Properties) {
        $out = Output 'Values' $entry.Name
        $message = $out.values[0][0]
        Check ($message -is [string] -and $message.StartsWith('#Error:')) "Expected DAL error text at $($entry.Name), received $message"
        foreach ($field in $entry.Value) { Check ($message.Contains($field)) "Missing '$field' at $($entry.Name): $message" }
    }
    foreach ($address in @('D2','G2','J2','A60','D60','G60','M60')) {
        $p = Price $address
        Check ($p.Count -eq 1) "Expected PV only at $address"
        Near $p.PV 100 1e-10 "default $address"
    }
    $mixed = Price 'A2'
    Check ($mixed.Count -eq 6) 'Expected five AAD risks and PV'
    Near $mixed.PV 260 2.6e-10 'mixed PV'
    Near $mixed.d_SCALE 80 1e-10 'mixed scale'
    Near $mixed.d_spot 1 1e-10 'mixed spot'
    Near (Price 'M2').PV 260 2.6e-10 'null simulation'
    Near (Price 'A20').PV 100 1e-10 'today model'
    Near (Price 'D20').PV 80 8e-11 'today history'
    $tp = 10.0 / 365.0; $tf = 3.0 / 365.0
    $historical = Price 'G20'; $pv = 160 * [Math]::Exp(-.05*$tp)
    Near $historical.PV $pv (1e-12*$pv) 'historical PV'
    Near $historical.d_SCALE ($pv/2) 1e-10 'historical scale'
    Near $historical.d_rate (-$tp*$pv) 1e-10 'historical rate'
    foreach ($key in @('d_spot','d_vol','d_div')) { Near $historical[$key] 0 1e-10 "historical $key" }
    $future = Price 'J20'; $pv = 100 * [Math]::Exp(.03*$tf-.05*$tp)
    Near $future.PV $pv (1e-12*$pv) 'retained PV'
    Near $future.d_spot ($pv/100) 1e-10 'retained spot'
    Near $future.d_rate (($tf-$tp)*$pv) 1e-10 'retained rate'
    Near $future.d_div (-$tf*$pv) 1e-10 'retained dividend'
    Check ((Price 'J60').Count -eq 5) 'null valuation with explicit AAD settings'
    foreach ($address in @('A2','D2')) { Calculate 'Diagnostics' $address }
    $description = Diagnostic 'A2'; $explanation = Diagnostic 'D2'
    Check ($description.schema -ceq 'dal.script-product/2' -and $description.events.Count -eq 2) 'Describe schema/events'
    Check ($explanation.schema -ceq 'dal.script-valuation/1' -and $explanation.requests.Count -eq 2) 'Explain schema/requests'
    Check ($explanation.event_to_sample[0] -eq 1 -and $explanation.requests[1].model_slot.sample_id -eq 0) 'Distinct fixing/payment sample IDs'
    Near $explanation.requests[0].value 80 0 'resolved history'
    Put 'Control' 'B5' '=EVALUATIONDATE.SET(DATE(2026,9,23))'
    Calculate 'Control' 'B5'
    Calculate 'Values' 'A2'; Calculate 'Diagnostics' 'D2'
    Near (Price 'A2').PV 260 2.6e-10 'explicit date after global date change'
    Check ((Diagnostic 'D2').evaluation_date -ceq '2026-09-12') 'Explain explicit date changed'
    Put 'Control' 'B5' '=EVALUATIONDATE.SET(Inputs!B2)'
    Calculate 'Control' 'B5'
    # Preserve UTF-8 bytes through the existing byte-oriented Excel input converter.
    $unicodeName = 'quote"slash\newline' + "`n" + [char]0x4e2d + [char]0xd83d + [char]0xde00 + '-END'
    $transportName = -join ([Text.Encoding]::UTF8.GetBytes($unicodeName) | ForEach-Object { [char]$_ })
    Put 'Long' 'D1' $transportName
    for ($i=1; $i -le 500; $i++) {
        Put 'Long' "A$i" 46287
        Put 'Long' "B$i" 'pay PAYS FIX(EQ[AAPL], 2026-09-11)'
    }
    Put 'Long' 'D2' '=PRODUCT.NEW(D1,A1:A500,B1:B500)'
    Calculate 'Long' 'D2'
    Put 'Diagnostics' 'G2' '=PRODUCT.DESCRIBE(Long!D2)'
    Put 'Diagnostics' 'J2' '=SCRIPTVALUATION.EXPLAIN(Long!D2,Handles!B6,Handles!B4)'
    foreach ($address in @('G2','J2')) { Calculate 'Diagnostics' $address }
    $longDescription = Diagnostic 'G2'; $longExplanation = Diagnostic 'J2'
    Check ($longDescription.name -ceq $unicodeName) 'Unicode/quote/backslash/newline roundtrip'
    Check ($longDescription.input_rows.Count -eq 500) 'Long Describe tail lost'
    Check ($longExplanation.requests[0].uses.Count -eq 500) 'Long Explain tail lost'
    foreach ($address in @('G2','J2')) {
        $raw = Get-Content -Raw -Encoding UTF8 (Join-Path $OutputDirectory "diagnostic-$address.json")
        Check ($raw.Length -gt 65535) "Long diagnostic $address must exceed 65535"
    }
    $results.passed = $true
} catch {
    $results.error = $_.ToString()
    $results.stack = $_.ScriptStackTrace
} finally {
    if ($null -ne $book) {
        try { $book.SaveAs((Join-Path $OutputDirectory '010.script_fix_settings.executed.xlsx'), 51) } catch { $results.saveError = $_.ToString() }
        $book.Close($false)
    }
    if ($ownPid -ne 0) { $excel.Quit() }
    foreach ($sheet in $sheets.Values) { Release-Com $sheet }
    Release-Com $book; Release-Com $excel
    [GC]::Collect(); [GC]::WaitForPendingFinalizers()
    if ($ownPid -ne 0) {
        $process = Get-Process -Id $ownPid -ErrorAction SilentlyContinue
        if ($process -and -not $process.WaitForExit(5000)) {
            Stop-Process -Id $ownPid
            [void]$process.WaitForExit(5000)
        }
        $results.cleanedOwnProcess = ($null -eq (Get-Process -Id $ownPid -ErrorAction SilentlyContinue))
    }
    $results | ConvertTo-Json -Depth 30 | Set-Content -Encoding UTF8 (Join-Path $OutputDirectory 'results.json')
}
if (-not $results.passed) { throw $results.error }
Write-Output "PASS: $($results.assertions) assertions; $OutputDirectory"
