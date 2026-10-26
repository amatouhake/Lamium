param(
    [Parameter(Mandatory)][string[]]$Loader,
    [string]$Dll = (Join-Path $PSScriptRoot '../bin/Lamium/Lamium.dll')
)
# Lamium links against an import library made from the SDK's runtime DLL. An
# older loader in the series can run it only if it exports every symbol
# Lamium imports; a missing one stops the DLL from loading.
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -property installationPath
$dumpbin = Get-ChildItem (Join-Path $vs 'VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe') | Select-Object -First 1 -ExpandProperty FullName
if (-not $dumpbin) { throw 'dumpbin not found.' }

$imports = @()
$inLoader = $false
foreach ($line in & $dumpbin /nologo /imports $Dll) {
    if ($line -match '^\s+(\S+\.dll)$') { $inLoader = $Matches[1] -ieq 'LeviLamina.dll'; continue }
    if ($inLoader -and $line -match '^\s+[0-9A-F]+\s+(\S+)$') { $imports += $Matches[1] }
}
if ($imports.Count -eq 0) { throw "No LeviLamina.dll imports found in $Dll." }

$failed = $false
foreach ($path in $Loader) {
    $exports = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($line in & $dumpbin /nologo /exports $path) {
        if ($line -match '^\s*\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)') { [void]$exports.Add($Matches[1]) }
    }
    $missing = @($imports | Where-Object { -not $exports.Contains($_) })
    Write-Host "$path`: $($imports.Count) imports, $($missing.Count) missing."
    $missing | ForEach-Object { Write-Host "  $_" }
    if ($missing.Count -gt 0) { $failed = $true }
}
if ($failed) { exit 1 }
