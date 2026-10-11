param(
    [string]$Inventory = (Join-Path $PSScriptRoot '../docs/GAME-UPDATES.md')
)
# L-137: every source file with a game hook is listed in the inventory with
# exactly its hooks, so the list of what an update can break stays complete.
$ErrorActionPreference = 'Stop'
$projectDirectory = Split-Path $PSScriptRoot -Parent
$sourceRoot = Join-Path $projectDirectory 'src'
$hookPattern = 'LL_(?:AUTO_)?(?:TYPE_)?(?:INSTANCE|STATIC)_HOOK\(\s*(\w+)'

$inCode = @{}
foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Include '*.cpp', '*.h') {
    $text = Get-Content -LiteralPath $file.FullName -Raw
    if (-not $text) { continue }
    $names = [regex]::Matches($text, $hookPattern) | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
    if ($names) {
        $relative = [System.IO.Path]::GetRelativePath($projectDirectory, $file.FullName).Replace('\', '/')
        $inCode[$relative] = @($names)
    }
}

$listed = @{}
$current = $null
foreach ($line in Get-Content -LiteralPath $Inventory) {
    if ($line -match '^### `(src/[^`]+)`') { $current = $Matches[1]; $listed[$current] = @(); continue }
    if ($current -and $line -match '^Hooks: (.*)$') {
        $listed[$current] = @([regex]::Matches($Matches[1], '`(\w+)`') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    }
}

$problems = @()
foreach ($file in $inCode.Keys | Sort-Object) {
    if (-not $listed.ContainsKey($file)) { $problems += "not in the inventory: $file ($($inCode[$file] -join ', '))"; continue }
    $missing = $inCode[$file] | Where-Object { $listed[$file] -notcontains $_ }
    $stale = $listed[$file] | Where-Object { $inCode[$file] -notcontains $_ }
    if ($missing) { $problems += "${file}: hooks not listed: $($missing -join ', ')" }
    if ($stale) { $problems += "${file}: listed hooks not in the code: $($stale -join ', ')" }
}
foreach ($file in $listed.Keys | Sort-Object) {
    if (-not $inCode.ContainsKey($file)) { $problems += "listed but has no hooks (or is gone): $file" }
}
if ($problems) {
    $problems | ForEach-Object { Write-Host $_ }
    throw "The game inventory (docs/GAME-UPDATES.md) does not match the hooks in src."
}
$count = ($inCode.Values | ForEach-Object { $_.Count } | Measure-Object -Sum).Sum
Write-Host "Game inventory matches: $($inCode.Count) files, $count hooks."
