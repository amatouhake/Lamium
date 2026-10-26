param(
    [string]$Catalog = (Join-Path $PSScriptRoot '../src/app/Capabilities.h'),
    [string]$Inventory = (Join-Path $PSScriptRoot '../docs/GAME-UPDATES.md')
)
$ErrorActionPreference = 'Stop'
$projectDirectory = Split-Path $PSScriptRoot -Parent
$catalogText = Get-Content -LiteralPath $Catalog -Raw
$inventoryText = Get-Content -LiteralPath $Inventory -Raw
$definitions = [regex]::Matches($catalogText, '\{Id::(?<id>\w+),\s*"(?<name>[^"]+)",\s*"(?<feature>[^"]*)",\s*"(?<key>[^"]*)",\s*Impact::')
if ($definitions.Count -eq 0) { throw 'No capability definitions found.' }
$names = @{}
foreach ($definition in $definitions) {
    if (-not $definition.Groups['feature'].Value) { continue }
    $name = $definition.Groups['name'].Value
    $names[$name] = $true
    if ($inventoryText -notmatch ('(?m)^\| ' + [regex]::Escape($name) + ' \|')) {
        throw "Capability is missing from the game inventory: $name"
    }
}
foreach ($row in [regex]::Matches($inventoryText, '(?m)^\| (?<name>[^|\r\n]+) \| `[^|\r\n]+ \|')) {
    $name = $row.Groups['name'].Value
    if (-not $names.ContainsKey($name)) { throw "Stale capability in game inventory: $name" }
}
$policy = [regex]::Match($catalogText, 'loaderMajor\s*=\s*(\d+),\s*loaderMinor\s*=\s*(\d+),\s*minimumLoaderPatch\s*=\s*(\d+)')
$xmake = Get-Content -LiteralPath (Join-Path $projectDirectory 'xmake.lua') -Raw
$sdk = [regex]::Match($xmake, 'add_requires\("levilamina-client-sdk\s+(\d+)\.(\d+)\.(\d+)"')
if (-not $policy.Success -or -not $sdk.Success) { throw 'Shared loader policy or SDK version not found.' }
foreach ($part in 1..2) {
    if ($policy.Groups[$part].Value -ne $sdk.Groups[$part].Value) {
        throw 'The shared loader series/minimum must match the SDK baseline; review both on an update.'
    }
}
$minimumPatch = [int]$policy.Groups[3].Value
$sdkPatch = [int]$sdk.Groups[3].Value
if ($minimumPatch -gt $sdkPatch) {
    throw 'The minimum supported loader patch is newer than the compiled SDK baseline.'
}
if ($minimumPatch -lt $sdkPatch) {
    # An older minimum needs a recorded Check-LoaderImports.ps1 run for this SDK.
    $series = "$($sdk.Groups[1].Value).$($sdk.Groups[2].Value)"
    $record = "SDK $series.$sdkPatch imports checked against loader $series.$minimumPatch"
    if ($inventoryText -notmatch [regex]::Escape($record)) {
        throw "The minimum loader patch is older than the SDK; run Check-LoaderImports.ps1 and record '$record' in GAME-UPDATES.md."
    }
}
$tooth = Get-Content -LiteralPath (Join-Path $projectDirectory 'tooth.json') -Raw | ConvertFrom-Json
$expectedSeries = "$($policy.Groups[1].Value).$($policy.Groups[2].Value).*"
if ($tooth.variants[0].dependencies.'github.com/LiteLDev/LeviLamina#client' -ne $expectedSeries) {
    throw "The package must target the shared loader series $expectedSeries."
}
$sourceRoot = Join-Path $projectDirectory 'src'
foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Include '*.cpp','*.h') {
    if ($file.Name -eq 'Versions.cpp' -and $file.Directory.Name -eq 'app') { continue }
    $text = Get-Content -LiteralPath $file.FullName -Raw
    if ($text -match 'versionSensitiveAllowed\s*\(\s*"' -or $text -match '\bgetLoaderVersion\s*\(') {
        throw "Feature-local version policy or an untyped gate found: $($file.FullName)"
    }
}
Write-Host "Capability catalog matches: $($names.Count) production capabilities; loader $expectedSeries, minimum patch $($policy.Groups[3].Value)."
