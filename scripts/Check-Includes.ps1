# Keeps the includes between source areas from growing (L-404). An area is
# src/app, src/settings, src/input, src/ui, src/overlay or one folder under
# src/features. The area-to-area edges that exist are listed in
# IncludeBaseline.txt; an include that makes a new edge fails until the edge is
# added on purpose (-Update), and an edge that is gone fails too, so the
# baseline shrinks with the code. Includes of the SDK and libraries are not
# areas and are not counted.
param(
    [switch]$Update,
    [string]$Baseline = (Join-Path $PSScriptRoot 'IncludeBaseline.txt')
)
$ErrorActionPreference = 'Stop'
$projectDirectory = Split-Path $PSScriptRoot -Parent
$source = Join-Path $projectDirectory 'src'
function Get-Area([string]$path) {
    $parts = $path -split '/'
    if ($parts[0] -eq 'features' -and $parts.Count -gt 2) { return "features/$($parts[1])" }
    if ($parts.Count -gt 1 -and $parts[0] -in 'app', 'settings', 'input', 'ui', 'overlay') { return $parts[0] }
    return $null
}
$edges = [ordered]@{}
foreach ($file in Get-ChildItem -LiteralPath $source -Recurse -File -Include '*.cpp', '*.h' | Sort-Object FullName) {
    $relative = [IO.Path]::GetRelativePath($source, $file.FullName).Replace('\', '/')
    $from = Get-Area $relative
    if (-not $from) { continue }
    foreach ($match in [regex]::Matches((Get-Content -LiteralPath $file.FullName -Raw), '(?m)^\s*#\s*include\s+"([^"]+)"')) {
        $to = Get-Area $match.Groups[1].Value
        if ($to -and $to -ne $from) {
            $key = "$from $to"
            if (-not $edges.Contains($key)) { $edges[$key] = $relative }
        }
    }
}
$sorted = $edges.Keys | Sort-Object
if ($Update) {
    $lines = @('# Check-Includes.ps1 baseline: one "from to" area edge per line. Add an edge on purpose; it only shrinks otherwise.')
    $lines += $sorted
    Set-Content -LiteralPath $Baseline -Value $lines -Encoding utf8NoBOM
    Write-Host "Include baseline written: $($sorted.Count) edges."
    return
}
$allowed = @{}
foreach ($line in Get-Content -LiteralPath $Baseline) {
    if ($line -match '^\s*(#|$)') { continue }
    $allowed[$line.Trim()] = $true
}
$problems = @()
foreach ($key in $sorted) {
    if (-not $allowed.ContainsKey($key)) {
        $problems += "new include edge $key (first in src/$($edges[$key])): include from a shared area instead, or add the edge on purpose with Check-Includes.ps1 -Update"
    }
}
foreach ($key in $allowed.Keys) {
    if (-not $edges.Contains($key)) { $problems += "include edge $key is gone: run Check-Includes.ps1 -Update" }
}
if ($problems) { $problems | ForEach-Object { Write-Host $_ }; throw "Include check failed ($($problems.Count))." }
Write-Host "Include edges match the baseline: $($sorted.Count) edges between areas."
